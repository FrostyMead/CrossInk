#pragma once

#include "components/themes/lyra/LyraTheme.h"

namespace FrostMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics values = LyraMetrics::values;
  values.headerHeight = 78;
  values.verticalSpacing = 12;
  values.contentSidePadding = 22;
  values.listRowHeight = 44;
  values.listWithSubtitleRowHeight = 66;
  values.listRowGap = 6;
  values.listRowRadius = 14;
  values.listInset = 18;
  values.listSidePadding = 12;
  values.listSelectionStyle = 1;
  values.listScrollWidth = 2;
  values.listTitleBold = true;
  values.headerSidePadding = 22;
  values.headerUnderlineSize = 1;
  values.menuRowHeight = 54;
  values.menuSpacing = 6;
  values.tabSpacing = 8;
  values.tabBarHeight = 46;
  values.tabBarAppearance = ThemeTabBarAppearance::Pill;
  values.scrollBarWidth = 2;
  values.scrollBarRightOffset = 4;
  values.homeTopPadding = 52;
  values.popupCornerRadius = 18;
  values.optionPopupSelectionRadius = 16;
  values.optionPopupSelectionLight = true;
  return values;
}

constexpr ThemeMetrics values = makeValues();
}  // namespace FrostMetrics

class FrostTheme final : public LyraTheme {
 public:
  void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                     const char* rightLabel = nullptr) const override;
  void drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                  bool selected) const override;
  Rect drawLoadingPopup(const GfxRenderer& renderer, const char* message) const override;
  bool tabIndexFromPoint(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs, int x, int y,
                         int& index) const override;
  int getListRowStep(bool hasSubtitle, int rowHeightScale = 1) const override;
  int getListPageItems(int contentHeight, bool hasSubtitle, int rowHeightScale = 1) const override;
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle,
                const std::function<UIIcon(int index)>& rowIcon, const std::function<std::string(int index)>& rowValue,
                bool highlightValue, const std::function<bool(int index)>& rowDimmed = nullptr,
                const std::function<bool(int index)>& isHeader = nullptr, int rowHeightScale = 1,
                bool showSelection = true) const override;
  void drawEmptyRecents(const GfxRenderer& renderer, Rect rect) const override;
};
