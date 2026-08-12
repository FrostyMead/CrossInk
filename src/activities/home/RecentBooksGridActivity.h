#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "../Activity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

enum class LibrarySection { Books, Comics };

class RecentBooksGridActivity final : public Activity {
 public:
  static constexpr int BOOKS_PER_PAGE = 9;  // 3 cols x 3 rows
  static constexpr int MAX_LIBRARY_BOOKS = BOOKS_PER_PAGE * 10;
  static constexpr int COVER_HEIGHT = 158;
  static constexpr int COVER_WIDTH = 112;

  explicit RecentBooksGridActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   LibrarySection section = LibrarySection::Books)
      : Activity(section == LibrarySection::Books ? "Books" : "Comics", renderer, mappedInput),
        section(section),
        libraryRoot(section == LibrarySection::Books ? "/Books" : "/Comics"),
        currentDirectory(libraryRoot) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct BookState {
    RecentBook book;
    float progress = -1.0f;
    bool progressLoaded = false;
    bool inRecentStore = false;
    bool isDirectory = false;
    bool isUnsortedCollection = false;
  };
  static constexpr int NO_PAGE_LOADED = -1;
  static constexpr size_t MAX_SCAN_DIRECTORIES = 48;
  static constexpr size_t SCAN_NAME_BUFFER_SIZE = 512;

  ButtonNavigator buttonNavigator;
  LibrarySection section;
  std::string libraryRoot;
  std::string currentDirectory;
  bool showingUnsorted = false;
  int selectorIndex = 0;
  bool longPressFired = false;
  bool pendingCacheDeletedFeedback = false;
  unsigned long cacheDeletedFeedbackShowTime = 0UL;
  std::vector<BookState> recentBooks;
  std::array<std::string, MAX_SCAN_DIRECTORIES> scanDirectories;
  std::unique_ptr<char[]> scanNameBuffer;
  int loadedPageStart = NO_PAGE_LOADED;

  void loadBooks();
  void discoverBooks();
  bool hasUnsortedBooks();
  void loadDirectory();
  void appendBookFile(const std::string& path);
  void clearScanDirectories();
  void loadPageCovers(int pageStart);
  void ensureProgressLoaded(int index);
  void openEntry(int index);
  void navigateBack();
  bool isAtLibraryRoot() const;
  const char* sectionTitle() const;
  std::string headerTitle() const;
  void reloadAfterBookAction();
  void promptDeleteBook(const RecentBook& book);
  void promptRemoveBook(const std::string& path, const std::string& title);
  void showBookActionMenu(int bookIndex, bool ignoreInitialConfirmRelease = false);
  int bookIndexFromPoint(int x, int y);
};
