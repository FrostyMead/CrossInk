#ifdef SIMULATOR
#include "OtaUpdater.h"

bool OtaUpdater::isUpdateNewer() const { return false; }
const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }
OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() { return NO_UPDATE; }
OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate(ProgressCallback, void*, std::atomic<bool>*) { return NO_UPDATE; }
#else
#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ReleaseJsonParser.h>
#include <strings.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <utility>

#include "AppVersion.h"
#include "OtaUpdater.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "mbedtls/sha256.h"
#include "network/FirmwareFlasher.h"
#include "network/HttpDownloader.h"
#include "network/OtaAssetName.h"
#include "network/WifiPowerSaveGuard.h"

namespace {
#ifndef CROSSINK_OTA_RELEASE_URL
#define CROSSINK_OTA_RELEASE_URL "https://api.github.com/repos/FrostyMead/CrossInk/releases/latest"
#endif

constexpr char latestReleaseUrl[] = CROSSINK_OTA_RELEASE_URL;

#ifdef CROSSINK_FIRMWARE_DEVICE_TYPE
constexpr char firmwareAssetStem[] = "firmware-" CROSSINK_FIRMWARE_DEVICE_TYPE;
constexpr char firmwareAssetName[] = "firmware-" CROSSINK_FIRMWARE_DEVICE_TYPE ".bin";
#else
constexpr char firmwareAssetStem[] = "firmware";
constexpr char firmwareAssetName[] = "firmware.bin";
#endif

constexpr char otaStagingDirectory[] = "/.crosspoint";
constexpr char otaStagingPath[] = "/.crosspoint/frostink-ota.bin";
constexpr size_t VERSION_SEGMENT_COUNT = 4;
constexpr size_t OTA_HASH_BUFFER_BYTES = 4096;
constexpr int OTA_CHECK_TIMEOUT_MS = 30000;

struct ParsedVersion {
  int segments[VERSION_SEGMENT_COUNT] = {0, 0, 0, 0};
  bool valid = false;
  bool releaseCandidate = false;
};

bool isDigit(const char c) { return c >= '0' && c <= '9'; }

bool startsWithNumberAfterOptionalV(const char* version) {
  if (version == nullptr) return false;
  if ((version[0] == 'v' || version[0] == 'V') && isDigit(version[1])) return true;
  return isDigit(version[0]);
}

bool containsRcMarker(const char* version) {
  if (version == nullptr) return false;
  for (const char* p = version; p[0] != '\0' && p[1] != '\0' && p[2] != '\0'; ++p) {
    if (p[0] == '-' && (p[1] == 'r' || p[1] == 'R') && (p[2] == 'c' || p[2] == 'C')) {
      return true;
    }
  }
  return false;
}

ParsedVersion parseVersion(const char* version) {
  ParsedVersion parsed;
  if (!startsWithNumberAfterOptionalV(version)) return parsed;

  const char* p = version;
  if (p[0] == 'v' || p[0] == 'V') ++p;

  size_t segmentIndex = 0;
  while (segmentIndex < VERSION_SEGMENT_COUNT) {
    if (!isDigit(*p)) return parsed;

    int value = 0;
    while (isDigit(*p)) {
      value = value * 10 + (*p - '0');
      ++p;
    }
    parsed.segments[segmentIndex] = value;
    ++segmentIndex;

    if (*p != '.') break;
    ++p;
  }

  parsed.valid = true;
  parsed.releaseCandidate = containsRcMarker(version);
  return parsed;
}

int compareVersions(const char* latestVersion, const char* currentVersion) {
  const ParsedVersion latest = parseVersion(latestVersion);
  const ParsedVersion current = parseVersion(currentVersion);
  if (!latest.valid || !current.valid) return 0;

  for (size_t i = 0; i < VERSION_SEGMENT_COUNT; ++i) {
    if (latest.segments[i] != current.segments[i]) {
      return latest.segments[i] > current.segments[i] ? 1 : -1;
    }
  }

  if (current.releaseCandidate && !latest.releaseCandidate) return 1;
  return 0;
}

char lowerHex(const uint8_t value) {
  return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('a' + value - 10);
}

char asciiLower(const char c) { return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 'a') : c; }

bool isHexChar(const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

bool isSha256Hex(const char* value) {
  if (value == nullptr || strlen(value) != 64) return false;
  for (size_t i = 0; i < 64; ++i) {
    if (!isHexChar(value[i])) return false;
  }
  return true;
}

bool sha256Matches(const uint8_t digest[32], const char* expectedHex) {
  if (!isSha256Hex(expectedHex)) return false;

  for (size_t i = 0; i < 32; ++i) {
    const char high = lowerHex((digest[i] >> 4) & 0x0F);
    const char low = lowerHex(digest[i] & 0x0F);
    if (high != asciiLower(expectedHex[i * 2]) || low != asciiLower(expectedHex[i * 2 + 1])) return false;
  }
  return true;
}

void formatSha256(const uint8_t digest[32], char output[65]) {
  for (size_t i = 0; i < 32; ++i) {
    output[i * 2] = lowerHex((digest[i] >> 4) & 0x0F);
    output[i * 2 + 1] = lowerHex(digest[i] & 0x0F);
  }
  output[64] = '\0';
}

bool isHttpsUrl(const std::string& url) { return url.rfind("https://", 0) == 0; }

bool isMatchingFirmwareAssetName(const char* assetName) {
  return ota_asset_name::matches(assetName, firmwareAssetStem, firmwareAssetName);
}

/*
 * When esp_crt_bundle.h included, it is pointing wrong header file
 * which is something under WifiClientSecure because of our framework based on arduno platform.
 * To manage this obstacle, don't include anything, just extern and it will point correct one.
 */
extern "C" {
extern esp_err_t esp_crt_bundle_attach(void* conf);
}

size_t totalBytesReceived = 0;

struct OtaFlashContext {
  size_t* processedSize = nullptr;
  size_t imageSize = 0;
  OtaUpdater::ProgressCallback onProgress = nullptr;
  void* progressCtx = nullptr;
};

enum class StagedHashResult {
  OK,
  OPEN_FAILED,
  SIZE_MISMATCH,
  READ_FAILED,
  OUT_OF_MEMORY,
  HASH_MISMATCH,
};

StagedHashResult verifyStagedFileHash(const char* path, const size_t expectedSize, const char* expectedSha256) {
  HalFile file;
  if (!Storage.openFileForRead("OTA", path, file) || !file) return StagedHashResult::OPEN_FAILED;

  const size_t fileSize = file.fileSize();
  if (fileSize != expectedSize) {
    LOG_ERR("OTA", "Staged firmware size mismatch: got=%zu expected=%zu", fileSize, expectedSize);
    file.close();
    return StagedHashResult::SIZE_MISMATCH;
  }

  auto buffer = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[OTA_HASH_BUFFER_BYTES]);
  if (!buffer) {
    file.close();
    return StagedHashResult::OUT_OF_MEMORY;
  }

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, /*is224=*/0);

  size_t remaining = fileSize;
  while (remaining > 0) {
    const size_t requested = std::min(remaining, OTA_HASH_BUFFER_BYTES);
    const int bytesRead = file.read(buffer.get(), requested);
    if (bytesRead <= 0 || static_cast<size_t>(bytesRead) != requested) {
      LOG_ERR("OTA", "Failed to hash staged firmware: got=%d expected=%zu", bytesRead, requested);
      mbedtls_sha256_free(&shaCtx);
      file.close();
      return StagedHashResult::READ_FAILED;
    }
    mbedtls_sha256_update(&shaCtx, buffer.get(), requested);
    remaining -= requested;
  }

  uint8_t computedSha256[32];
  mbedtls_sha256_finish(&shaCtx, computedSha256);
  mbedtls_sha256_free(&shaCtx);
  file.close();

  if (!sha256Matches(computedSha256, expectedSha256)) {
    char computedSha256Hex[65];
    formatSha256(computedSha256, computedSha256Hex);
    LOG_ERR("OTA", "Staged firmware sha256 mismatch: expected=%s actual=%s", expectedSha256, computedSha256Hex);
    return StagedHashResult::HASH_MISMATCH;
  }
  return StagedHashResult::OK;
}

void flashProgress(const size_t written, const size_t, void* ctx) {
  auto* flashCtx = static_cast<OtaFlashContext*>(ctx);
  if (flashCtx == nullptr || flashCtx->processedSize == nullptr) return;
  *flashCtx->processedSize = flashCtx->imageSize + written;
  if (flashCtx->onProgress != nullptr) flashCtx->onProgress(flashCtx->progressCtx);
}

void removeStagedFirmware() {
  if (Storage.exists(otaStagingPath) && !Storage.remove(otaStagingPath)) {
    LOG_ERR("OTA", "Could not remove staged firmware: %s", otaStagingPath);
  }
}

esp_err_t release_manifest_event_handler(esp_http_client_event_t* event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  if (event->data_len <= 0) return ESP_OK;

  auto* parser = static_cast<ReleaseJsonParser*>(event->user_data);
  if (parser == nullptr) {
    LOG_ERR("OTA", "HTTP client parser missing");
    return ESP_ERR_INVALID_ARG;
  }

  totalBytesReceived += static_cast<size_t>(event->data_len);
  parser->feed(static_cast<const char*>(event->data), event->data_len);
  return ESP_OK;
}

}  // namespace

OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() {
  WifiPowerSaveGuard wifiPowerSaveGuard;

  updateAvailable = false;
  latestVersion.clear();
  otaUrl.clear();
  otaSha256.clear();
  otaSize = 0;
  processedSize = 0;
  totalSize = 0;
  installPhase = IDLE;

  esp_err_t esp_err;
  ReleaseJsonParser releaseParser(isMatchingFirmwareAssetName);

  esp_http_client_config_t client_config = {
      .url = latestReleaseUrl,
      .timeout_ms = OTA_CHECK_TIMEOUT_MS,
      .event_handler = release_manifest_event_handler,
      // 4096 holds the API response headers; the 32KB body streams through the
      // parser in chunks so RX needn't be larger. TX only carries our GET.
      // Both free before installUpdate, so smaller leaves it less fragmentation.
      .buffer_size = 4096,
      .buffer_size_tx = 1024,
      .user_data = &releaseParser,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  totalBytesReceived = 0;
  LOG_DBG("OTA", "Checking for update (current: %s)", CROSSINK_VERSION);

  esp_http_client_handle_t client_handle = esp_http_client_init(&client_config);
  if (!client_handle) {
    LOG_ERR("OTA", "HTTP Client Handle Failed");
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_http_client_set_header(client_handle, "User-Agent", "FrostInk-ESP32-" CROSSINK_VERSION);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_http_client_set_header Failed : %s", esp_err_to_name(esp_err));
    esp_http_client_cleanup(client_handle);
    return INTERNAL_UPDATE_ERROR;
  }

  esp_err = esp_http_client_perform(client_handle);
  if (esp_err != ESP_OK) {
    const int transportErrno = esp_http_client_get_errno(client_handle);
    const bool timedOut =
        esp_err == ESP_ERR_HTTP_EAGAIN || esp_err == ESP_ERR_HTTP_READ_TIMEOUT || transportErrno == ETIMEDOUT;
    LOG_ERR("OTA", "esp_http_client_perform Failed: %s (errno=%d)", esp_err_to_name(esp_err), transportErrno);
    esp_http_client_cleanup(client_handle);
    return timedOut ? UPDATE_CHECK_TIMEOUT_ERROR : HTTP_ERROR;
  }

  esp_err = esp_http_client_cleanup(client_handle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_http_client_cleanup Failed : %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  LOG_DBG("OTA", "Response received: %zu bytes total", totalBytesReceived);
  LOG_DBG("OTA", "Parser results: tag=%s firmware=%s", releaseParser.foundTag() ? "yes" : "no",
          releaseParser.foundFirmware() ? "yes" : "no");

  if (!releaseParser.foundTag()) {
    LOG_ERR("OTA", "No tag_name in release JSON");
    return JSON_PARSE_ERROR;
  }

  latestVersion = releaseParser.getTagName();

  if (!releaseParser.foundFirmware()) {
    LOG_ERR("OTA", "No matching %s asset found for release %s", firmwareAssetStem, latestVersion.c_str());
    return NO_UPDATE;
  }

  otaUrl = releaseParser.getFirmwareUrl();
  otaSha256 = releaseParser.getFirmwareSha256();
  otaSize = releaseParser.getFirmwareSize();
  if (!isHttpsUrl(otaUrl) || !isSha256Hex(otaSha256.c_str()) || otaSize == 0) {
    LOG_ERR("OTA", "Release asset is missing required HTTPS, size, or sha256 metadata");
    otaUrl.clear();
    otaSha256.clear();
    otaSize = 0;
    return JSON_PARSE_ERROR;
  }
  totalSize = otaSize * 2;
  updateAvailable = true;

  LOG_DBG("OTA", "Found update: tag=%s size=%zu sha256=%s", latestVersion.c_str(), otaSize,
          otaSha256.empty() ? "missing" : "present");
  LOG_DBG("OTA", "Firmware URL: %s", otaUrl.c_str());
  return OK;
}

bool OtaUpdater::isUpdateNewer() const {
  if (!updateAvailable || latestVersion.empty() || latestVersion == CROSSINK_VERSION) {
    return false;
  }

  const int comparison = compareVersions(latestVersion.c_str(), CROSSINK_VERSION);
  LOG_DBG("OTA", "Version comparison latest=%s current=%s result=%d", latestVersion.c_str(), CROSSINK_VERSION,
          comparison);
  return comparison > 0;
}

const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }

OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate(ProgressCallback onProgress, void* ctx,
                                                      std::atomic<bool>* cancelRequested) {
  const auto isCancellationRequested = [cancelRequested]() -> bool {
    return cancelRequested != nullptr && cancelRequested->load(std::memory_order_relaxed);
  };

  if (!isUpdateNewer()) return UPDATE_OLDER_ERROR;
  if (isCancellationRequested()) return CANCELLED_ERROR;
  if (!isHttpsUrl(otaUrl) || !isSha256Hex(otaSha256.c_str()) || otaSize == 0) {
    LOG_ERR("OTA", "Refusing release without HTTPS, size, and sha256 metadata");
    return JSON_PARSE_ERROR;
  }

  const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (updatePartition == nullptr) {
    LOG_ERR("OTA", "No OTA update partition found");
    return INTERNAL_UPDATE_ERROR;
  }
  if (otaSize > updatePartition->size) {
    LOG_ERR("OTA", "Firmware too large: %zu > %zu", otaSize, updatePartition->size);
    return INTERNAL_UPDATE_ERROR;
  }

  Storage.mkdir(otaStagingDirectory);
  removeStagedFirmware();
  processedSize = 0;
  totalSize = otaSize * 2;
  installPhase = DOWNLOADING;

  HttpDownloader::DownloadOptions downloadOptions;
  downloadOptions.shouldCancel = isCancellationRequested;
  // SecureNet's wolfSSL transport is used for the large asset to avoid TLS heap
  // fragmentation. The GitHub API response is CA-verified and its mandatory
  // digest pins every downloaded byte before the staged file is accepted.
  downloadOptions.transport = HttpDownloader::Transport::WOLFSSL;
  const auto transferResult = HttpDownloader::downloadToFile(
      otaUrl, otaStagingPath,
      [this, onProgress, ctx](const size_t downloaded, const size_t) {
        processedSize = std::min(downloaded, otaSize);
        if (onProgress != nullptr) onProgress(ctx);
      },
      nullptr, "", "", std::move(downloadOptions));

  if (transferResult != HttpDownloader::OK) {
    removeStagedFirmware();
    installPhase = IDLE;
    if (transferResult == HttpDownloader::ABORTED || isCancellationRequested()) {
      LOG_INF("OTA", "Update cancelled during download");
      return CANCELLED_ERROR;
    }
    LOG_ERR("OTA", "Firmware staging download failed: %d", static_cast<int>(transferResult));
    return transferResult == HttpDownloader::FILE_ERROR ? INTERNAL_UPDATE_ERROR : HTTP_ERROR;
  }

  if (isCancellationRequested()) {
    removeStagedFirmware();
    installPhase = IDLE;
    return CANCELLED_ERROR;
  }

  installPhase = VERIFYING;
  processedSize = otaSize;
  if (onProgress != nullptr) onProgress(ctx);

  const StagedHashResult hashResult = verifyStagedFileHash(otaStagingPath, otaSize, otaSha256.c_str());
  if (hashResult != StagedHashResult::OK) {
    removeStagedFirmware();
    installPhase = IDLE;
    if (hashResult == StagedHashResult::OUT_OF_MEMORY) return OOM_ERROR;
    if (hashResult == StagedHashResult::HASH_MISMATCH) return HASH_MISMATCH_ERROR;
    return INTERNAL_UPDATE_ERROR;
  }
  LOG_INF("OTA", "Staged firmware release sha256 verified");

  const auto validationResult = firmware_flash::validateImageFile(otaStagingPath, updatePartition->size);
  if (validationResult != firmware_flash::Result::OK) {
    LOG_ERR("OTA", "Staged firmware image validation failed: %s", firmware_flash::resultName(validationResult));
    removeStagedFirmware();
    installPhase = IDLE;
    return validationResult == firmware_flash::Result::OOM ? OOM_ERROR : INVALID_FIRMWARE_ERROR;
  }

  if (isCancellationRequested()) {
    removeStagedFirmware();
    installPhase = IDLE;
    return CANCELLED_ERROR;
  }

  installPhase = INSTALLING;
  OtaFlashContext flashCtx{&processedSize, otaSize, onProgress, ctx};
  const auto flashResult = firmware_flash::flashFromSdPath(otaStagingPath, flashProgress, &flashCtx);
  removeStagedFirmware();
  installPhase = IDLE;
  if (flashResult != firmware_flash::Result::OK) {
    LOG_ERR("OTA", "Staged firmware flash failed: %s", firmware_flash::resultName(flashResult));
    if (flashResult == firmware_flash::Result::OOM) return OOM_ERROR;
    return INTERNAL_UPDATE_ERROR;
  }

  processedSize = totalSize;
  if (onProgress != nullptr) onProgress(ctx);
  LOG_INF("OTA", "FrostInk OTA completed: %zu verified bytes", otaSize);
  return OK;
}
#endif
