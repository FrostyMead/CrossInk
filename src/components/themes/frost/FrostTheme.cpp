#include "FrostTheme.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstddef>

#include "components/TouchRegistry.h"
#include "fontIds.h"
#include "images/FrostInkLogo120.h"

namespace {
constexpr int kTabHorizontalPadding = 10;
constexpr int kMaxRightLabelWidth = 240;
constexpr int kLoadingLogoSize = 120;
constexpr int kLoadingCardPadding = 12;
constexpr int kLoadingCardBorder = 2;
constexpr int kLoadingCardRadius = 18;
constexpr int kLoadingPulseWidth = 14;
constexpr int kLoadingPulseHeight = 3;
constexpr int kLoadingPulseGap = 6;
constexpr int kEmptyCardRadius = 16;
constexpr int kEmptyCardMarkerWidth = 4;
constexpr int kEmptyCardTextInset = 28;
}  // namespace

void FrostTheme::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                               const char* rightLabel) const {
  const ThemeMetrics& metrics = FrostMetrics::values;
  const int titleLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int titleY = rect.y + std::max(0, (rect.height - titleLineHeight) / 2);
  const int titleX = rect.x + metrics.contentSidePadding;
  int rightReserve = metrics.contentSidePadding;

  if (rightLabel != nullptr && rightLabel[0] != '\0') {
    const auto right = renderer.truncatedText(SMALL_FONT_ID, rightLabel, kMaxRightLabelWidth, EpdFontFamily::REGULAR);
    const int rightWidth = renderer.getTextWidth(SMALL_FONT_ID, right.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - metrics.contentSidePadding - rightWidth,
                      rect.y + std::max(0, (rect.height - renderer.getLineHeight(SMALL_FONT_ID)) / 2), right.c_str());
    rightReserve += rightWidth + metrics.verticalSpacing;
  }

  const int availableWidth = std::max(0, rect.width - (titleX - rect.x) - rightReserve);
  const auto title = renderer.truncatedText(UI_10_FONT_ID, label, availableWidth, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, titleX, titleY, title.c_str(), true, EpdFontFamily::BOLD);
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

void FrostTheme::drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                            const bool selected) const {
  const ThemeMetrics& metrics = FrostMetrics::values;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int textY = rect.y + std::max(0, (rect.height - lineHeight) / 2);
  int currentX = rect.x + metrics.contentSidePadding;

  for (size_t i = 0; i < tabs.size(); ++i) {
    const auto& tab = tabs[i];
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, tab.label, EpdFontFamily::BOLD);
    const int tabWidth = textWidth + 2 * kTabHorizontalPadding;
    const Rect tabRect{currentX, rect.y + 5, tabWidth, rect.height - 11};
    TouchRegistry::getInstance().add(tabRect, static_cast<int>(i), TouchRegistry::Tab);

    if (tab.selected) {
      if (selected) {
        renderer.fillRoundedRect(tabRect.x, tabRect.y, tabRect.width, tabRect.height,
                                 std::min(FrostMetrics::values.listRowRadius, tabRect.height / 2), Color::Black);
      } else {
        renderer.fillRoundedRect(tabRect.x, tabRect.y, tabRect.width, tabRect.height,
                                 std::min(FrostMetrics::values.listRowRadius, tabRect.height / 2), Color::LightGray);
      }
    }

    renderer.drawText(UI_10_FONT_ID, currentX + kTabHorizontalPadding, textY, tab.label, !(tab.selected && selected),
                      tab.selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    currentX += tabWidth + metrics.tabSpacing;
  }

  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

Rect FrostTheme::drawLoadingPopup(const GfxRenderer& renderer, const char*) const {
  // A single static paint is deliberate: animated loaders cause distracting
  // flashes on e-ink. The FrostInk stamp plus a compact three-beat pulse makes
  // the retained transition frame look intentional until the next screen lands.
  const int pulseRowWidth = kLoadingPulseWidth * 3 + kLoadingPulseGap * 2;
  const int cardWidth = kLoadingLogoSize + kLoadingCardPadding * 2;
  const int cardHeight = kLoadingLogoSize + kLoadingCardPadding * 2 + kLoadingPulseHeight + kLoadingPulseGap;
  const int x = (renderer.getScreenWidth() - cardWidth) / 2;
  const int y = static_cast<int>(renderer.getScreenHeight() * FrostMetrics::values.popupTopOffsetRatio);

  renderer.fillRoundedRect(x - kLoadingCardBorder, y - kLoadingCardBorder, cardWidth + kLoadingCardBorder * 2,
                           cardHeight + kLoadingCardBorder * 2, kLoadingCardRadius + kLoadingCardBorder, Color::Black);
  renderer.fillRoundedRect(x, y, cardWidth, cardHeight, kLoadingCardRadius, Color::White);
  renderer.drawImage(FrostInkLogo120, x + kLoadingCardPadding, y + kLoadingCardPadding, kLoadingLogoSize,
                     kLoadingLogoSize);

  const int pulseX = x + (cardWidth - pulseRowWidth) / 2;
  const int pulseY = y + cardHeight - kLoadingCardPadding;
  for (int i = 0; i < 3; ++i) {
    renderer.fillRect(pulseX + i * (kLoadingPulseWidth + kLoadingPulseGap), pulseY, kLoadingPulseWidth,
                      kLoadingPulseHeight, true);
  }

  renderer.displayBuffer();
  return Rect{x, y, cardWidth, cardHeight};
}

bool FrostTheme::tabIndexFromPoint(const GfxRenderer& renderer, const Rect rect, const std::vector<TabInfo>& tabs,
                                   const int x, const int y, int& index) const {
  if (tabs.empty() || y < rect.y || y >= rect.y + rect.height) return false;

  const ThemeMetrics& metrics = FrostMetrics::values;
  int currentX = rect.x + metrics.contentSidePadding;
  for (size_t i = 0; i < tabs.size(); ++i) {
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, tabs[i].label, EpdFontFamily::BOLD);
    const int tabWidth = textWidth + 2 * kTabHorizontalPadding;
    const int left = i == 0 ? rect.x : currentX - metrics.tabSpacing / 2;
    const int right = currentX + tabWidth + metrics.tabSpacing / 2;
    if (x >= left && x < right) {
      index = static_cast<int>(i);
      return true;
    }
    currentX += tabWidth + metrics.tabSpacing;
  }
  return false;
}

int FrostTheme::getListRowStep(const bool hasSubtitle, const int rowHeightScale) const {
  const int scale = std::max(1, rowHeightScale);
  const int rowHeight =
      hasSubtitle ? FrostMetrics::values.listWithSubtitleRowHeight : FrostMetrics::values.listRowHeight;
  return rowHeight * scale;
}

int FrostTheme::getListPageItems(const int contentHeight, const bool hasSubtitle, const int rowHeightScale) const {
  const int rowStep = getListRowStep(hasSubtitle, rowHeightScale);
  return rowStep > 0 ? std::max(1, contentHeight / rowStep) : 1;
}

void FrostTheme::drawList(const GfxRenderer& renderer, Rect rect, const int itemCount, const int selectedIndex,
                          const std::function<std::string(int index)>& rowTitle,
                          const std::function<std::string(int index)>& rowSubtitle,
                          const std::function<UIIcon(int index)>& rowIcon,
                          const std::function<std::string(int index)>& rowValue, const bool highlightValue,
                          const std::function<bool(int index)>& rowDimmed,
                          const std::function<bool(int index)>& isHeader, const int rowHeightScale,
                          const bool showSelection) const {
  drawListWithMetrics(renderer, rect, itemCount, selectedIndex, rowTitle, rowSubtitle, rowIcon, rowValue,
                      highlightValue, rowDimmed, isHeader, FrostMetrics::values, false, rowHeightScale, showSelection);
}

void FrostTheme::drawEmptyRecents(const GfxRenderer& renderer, const Rect rect) const {
  const int cardX = rect.x + FrostMetrics::values.contentSidePadding;
  const int cardWidth = std::max(0, rect.width - FrostMetrics::values.contentSidePadding * 2);
  const int titleLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int subtitleLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int cardHeight = titleLineHeight + subtitleLineHeight + 30;
  const int cardY = rect.y + std::max(0, (rect.height - cardHeight) / 2);

  if (cardWidth <= kEmptyCardMarkerWidth + kEmptyCardTextInset) return;
  renderer.drawRoundedRect(cardX, cardY, cardWidth, cardHeight, 1, kEmptyCardRadius, true);
  renderer.fillRoundedRect(cardX + 12, cardY + 12, kEmptyCardMarkerWidth, cardHeight - 24, kEmptyCardMarkerWidth / 2,
                           Color::Black);
  renderer.drawText(UI_12_FONT_ID, cardX + kEmptyCardTextInset, cardY + 10, tr(STR_NO_OPEN_BOOK), true,
                    EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, cardX + kEmptyCardTextInset, cardY + 14 + titleLineHeight, tr(STR_START_READING),
                    true);
}
