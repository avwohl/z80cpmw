/*
 * HelpWindow.h - Remote Help System Window
 *
 * Fetches help documentation from GitHub releases and displays in a window.
 * Implements an index-based discovery model with on-demand loading.
 */

#pragma once

#include "HelpAssets.h"
#include "CatalogV0.h"

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

#pragma comment(lib, "winhttp.lib")

// Ids for the two LOCAL help topics, served from the app itself, and the
// markdown behind them.
//
// "Bundled" now means two different things and they are worth keeping apart.
// These two are prose written here, compiled in as string literals, listed by
// seedLocalTopics() and shown by isLocalTopic()'s branch in fetchTopic() with
// no network step at all. The seven REMOTE topics are also in the binary since
// the bundling commit, but as RCDATA copied from ..\ioscpm\release_assets, and
// they reach the reader only as resolveTopic's third step, behind a download
// and behind the on-disk cache. Both end up labelled "bundled with the app" on
// the status line, which is the truth as the reader needs it.
//
// The two getters were file-static in HelpWindow.cpp and are declared here so
// tests\test_help.cpp can read what the pane will show. They stay in
// HelpWindow.cpp rather than moving to HelpAssets.cpp with the renderer: they
// are prose this application wrote about this application, not an asset it
// fetches or a transformation it performs. The Getting Started one in
// particular carries a long comment recording what was measured about the
// R8 and W8 on the shipped disk images and which of its blocks come out when,
// and moving the prose away from that record would strand it.
namespace help_topics {
    inline constexpr const char* GettingStarted = "local:gettingstarted";
    inline constexpr const char* Configuration  = "local:configuration";

    std::string gettingStartedMarkdown();
    std::string configurationMarkdown();
}

// Help content cache entry
struct HelpCache {
    std::string topicId;
    std::string content;
    DWORD timestamp;
};

class HelpWindow {
public:
    HelpWindow();
    ~HelpWindow();

    // Show the help window (creates if needed). If topicId is non-empty, that
    // topic is displayed once the window is up (bundled topics show instantly).
    bool show(HWND parent, const std::string& topicId = "");

    // Close the help window
    void close();

    // Check if window is visible
    bool isVisible() const;

    // The Catalog index setting, empty for the index this build ships with.
    // Supplied by the caller so this class needs no configuration layer - see
    // resolveHelpCatalog(). $ROMWBW_INDEX_URL still wins over it, inside
    // catalogv0::indexUrl().
    void setCatalogIndexSetting(const std::string& url) { m_catalogIndexSetting = url; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Create child controls
    void createControls();

    // Fetch help index from GitHub
    void fetchIndex();

    // Fetch a specific topic content
    void fetchTopic(const std::string& topicId);

    // Update topic list UI
    void updateTopicList();

    // Display topic content
    void displayContent(const std::string& markdown);

    // HTTP helper
    bool downloadToString(const std::wstring& url, std::string& result, std::string& error);

    // Fetch the catalog index and take the `help` block out of it, filling
    // m_helpCatalog. False - leaving it empty - for a dead network, an
    // unparseable index, or an index with no `help` key, which are one case as
    // far as this window is concerned: no live topics, show the bundled ones.
    // Called on the worker thread from fetchIndex().
    bool resolveHelpCatalog();

    // Local (bundled) help topics, shown even when the online index is
    // unavailable. Served from the app rather than fetched over the network.
    void seedLocalTopics();
    bool isLocalTopic(const std::string& topicId) const;
    std::string localTopicContent(const std::string& topicId) const;

    // Highlight a topic in the list box (no-op if not found)
    void selectTopicInList(const std::string& topicId);

    // Find cached content
    std::string* findCachedContent(const std::string& topicId);

    // Cache topic content
    void cacheContent(const std::string& topicId, const std::string& content);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_topicList = nullptr;
    HWND m_contentView = nullptr;
    HWND m_statusLabel = nullptr;

    std::vector<help_assets::HelpTopic> m_topics;
    std::vector<HelpCache> m_cache;
    std::string m_currentTopicId;
    std::atomic<bool> m_loading{false};

    // Cache TTL: 15 minutes
    static constexpr DWORD CACHE_TTL_MS = 15 * 60 * 1000;

    // GitHub release URLs
    // WHERE THE HELP LIVES IS NOT COMPILED IN. These held
    // avwohl/ioscpm/releases/latest/download/... until 1.0.32, and that pair of
    // constants was the last thing in this application that could only be
    // changed by shipping a new client to Windows, Android, iOS and Linux. The
    // location now comes out of the catalog index's `help` block, so
    // romwbw_disks can rename the tag, re-cut it or move hosts with no client
    // release - and a custom $ROMWBW_INDEX_URL redirects help along with
    // everything else. See catalogv0::HelpCatalog.
    //
    // Empty until the index has been read, and legitimately still empty after:
    // an index published before the block existed has no `help` key. Empty
    // means the Help window shows the topics compiled into the binary, which is
    // the same thing it shows when the network is down.
    catalogv0::HelpCatalog m_helpCatalog;

    // What Settings holds for the catalog index; empty means the built-in one.
    std::string m_catalogIndexSetting;
};

// Show help window (creates singleton instance if needed). Optionally open a
// specific topic (e.g. help_topics::GettingStarted).
void ShowHelpWindow(HWND parent, const std::string& topicId = "",
                    const std::string& catalogIndexSetting = "");
