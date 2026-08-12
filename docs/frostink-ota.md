---
title: Safe FrostInk OTA
---

# Safe FrostInk OTA

FrostInk checks the latest **published, non-prerelease** release in
`FrostyMead/CrossInk`. Draft releases are deliberately invisible to the device,
which creates a manual hardware-test gate before an update can reach OTA users.

## Safety model

An OTA install must pass every gate below before FrostInk changes the selected
boot partition:

1. The release metadata is fetched from GitHub over certificate-verified HTTPS.
2. The release must contain the X3/X4 firmware asset, its exact byte size, and
   GitHub's SHA-256 asset digest.
3. The complete download is staged at `/.crosspoint/frostink-ota.bin` on the SD
   card. The currently running firmware remains selected during this step.
4. FrostInk verifies the staged size and SHA-256 digest.
5. FrostInk validates the ESP image header, segment bounds, XOR checksum, and
   appended image SHA-256 before writing the inactive OTA slot.
6. The shared firmware flasher validates the same staged image again, writes
   only the inactive slot, and changes `otadata` only after the complete write.
7. On the first boot, the new slot remains rollback-pending until hardware
   detection, SD mounting, settings loading, display setup, activity routing,
   and one visible UI paint have succeeded.

An interrupted download, invalid digest, malformed image, failed validation,
or interrupted inactive-slot write does not select that slot. The temporary SD
file is removed after success or failure.

## Publishing a release

1. Push the exact tested FrostInk commit. Production releases should normally
   come from the repository's default branch; a feature branch may be used for
   a private-device draft test when the release remains unpublished.
2. Run **Build FrostInk Release** from that branch in GitHub Actions with a
   version higher than the installed version, such as `1.0.1`.
3. The workflow builds the X3/X4 firmware, checks formatting, runs host tests and
   the Frost simulator smoke test, verifies the partition-size limit, and creates
   a **draft** GitHub release.
4. Download the draft asset and flash it over USB to the X4 first.
5. Test cold boot, opening a book, sleep/wake, File Transfer, and OTA Update.
6. Only after that hardware test, publish the draft release. Published stable
   releases become visible to the device's OTA Update screen.

Never publish a release asset that was built or renamed manually outside the
release workflow. Keep the previous published release available so its firmware
remains downloadable for USB recovery.

## Recovery

- Preferred recovery: use the CrossPoint browser flash tool over USB and choose
  the last known-good FrostInk `.bin`.
- SD recovery: place a known-good X3/X4 `.bin` on the SD card, then hold the X4
  side-up button together with Power while booting to open firmware recovery.

OTA reduces update risk but cannot remove every hardware or power-loss risk.
Keep the device charged and do not remove the SD card or power during the final
install phase.
