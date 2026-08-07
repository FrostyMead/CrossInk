#include "FrostTheme.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstddef>

#include "components/TouchRegistry.h"
#include "fontIds.h"

namespace {
constexpr int kMarkerWidth = 3;
constexpr int kMarkerGap = 9;
constexpr int kTabHorizontalPadding = 10;
constexpr int kMaxRightLabelWidth = 240;
}  // namespace

void FrostTheme::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                               const char* rightLabel) const {
  const ThemeMetrics& metrics = FrostMetrics::values;
  const int titleLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int titleY = rect.y + std::max(0, (rect.height - titleLineHeight) / 2);
  const int markerX = rect.x + metrics.contentSidePadding;
  const int titleX = markerX + kMarkerWidth + kMarkerGap;
  int rightReserve = metrics.contentSidePadding;

  renderer.fillRect(markerX, titleY, kMarkerWidth, titleLineHeight, true);

  if (rightLabel != nullptr && rightLabel[0] != '\0') {
    const auto right = renderer.truncatedText(SMALL_FONT_ID, rightLabel, kMaxRightLabelWidth, EpdFontFamily::REGULAR);
    const int rightWidth = renderer.getTextWidth(SMALL_FONT_ID, right.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - metrics.contentSidePadding - rightWidth,
                      rect.y + std::max(0, (rect.height - renderer.getLineHeight(SMALL_FONT_ID)) / 2), right.c_str());
    rightReserve += rightWidth + kMarkerGap;
  }

  const int availableWidth = std::max(0, rect.width - (titleX - rect.x) - rightReserve);
  const auto title = renderer.truncatedText(UI_10_FONT_ID, label, availableWidth, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, titleX, titleY, title.c_str(), true, EpdFontFamily::BOLD);
  renderer.drawLine(rect.x, rect.y + rect.height - 2, rect.x + rect.width - 1, rect.y + rect.height - 2, 2, true);
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
    const Rect tabRect{currentX, rect.y + 2, tabWidth, rect.height - 5};
    TouchRegistry::getInstance().add(tabRect, static_cast<int>(i), TouchRegistry::Tab);

    if (tab.selected) {
      if (selected) {
        renderer.fillRect(tabRect.x, tabRect.y, tabRect.width, tabRect.height, true);
      } else {
        renderer.drawRect(tabRect.x, tabRect.y, tabRect.width, tabRect.height, true);
      }
    }

    renderer.drawText(UI_10_FONT_ID, currentX + kTabHorizontalPadding, textY, tab.label, !(tab.selected && selected),
                      tab.selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    currentX += tabWidth + metrics.tabSpacing;
  }

  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
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
                      highlightValue, rowDimmed, isHeader, FrostMetrics::values, true, rowHeightScale, showSelection);
}
