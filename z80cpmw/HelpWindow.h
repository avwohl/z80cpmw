/*
 * HelpWindow.h - Remote Help System Window
 *
 * Fetches help documentation from GitHub releases and displays in a window.
 * Implements an index-based discovery model with on-demand loading.
 */

#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

#pragma comment(lib, "winhttp.lib")

// Help topic entry from index
struct HelpTopic {
    std::string id;
    std::string title;
    std::string description;
    std::string filename;
};

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

    // Show the help window (creates if needed)
    bool show(HWND parent);

    // Close the help window
    void close();

    // Check if window is visible
    bool isVisible() const;

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

    // Convert markdown to plain text (simple conversion)
    std::string markdownToText(const std::string& markdown);

    // HTTP helper
    bool downloadToString(const std::wstring& url, std::string& result, std::string& error);

    // Parse JSON index (simple parser)
    bool parseIndexJson(const std::string& json, std::vector<HelpTopic>& topics, std::string& error);

    // Find cached content
    std::string* findCachedContent(const std::string& topicId);

    // Cache topic content
    void cacheContent(const std::string& topicId, const std::string& content);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_topicList = nullptr;
    HWND m_contentView = nullptr;
    HWND m_statusLabel = nullptr;

    std::vector<HelpTopic> m_topics;
    std::vector<HelpCache> m_cache;
    std::string m_currentTopicId;
    std::atomic<bool> m_loading{false};

    // Cache TTL: 15 minutes
    static constexpr DWORD CACHE_TTL_MS = 15 * 60 * 1000;

    // GitHub release URLs
    static const std::wstring INDEX_URL;
    static const std::wstring CONTENT_BASE_URL;
};

// Show help window (creates singleton instance if needed)
void ShowHelpWindow(HWND parent);
