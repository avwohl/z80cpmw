/*
 * HelpAssets.h - The state-free half of the help system.
 *
 * Nothing here touches a window, a thread or a socket, so tests\test_help.cpp
 * can call all of it on a machine with no interactive desktop and without
 * either sibling checkout - which is why the help suite can sit second in
 * tests\run_tests.bat, ahead of the two blocks that exit /b 1 when
 * ..\romwbw_emu or ..\cpmemu are missing.
 *
 * The renderer and the name and text helpers take strings and return strings.
 * The cache half below does touch the file system, which is the point of it,
 * and is kept testable by setCacheRoot(): the suite gives it a scratch
 * directory, so the round trip that runs in the suite is the real one and not a
 * simulation of it.
 *
 * The split is not tidying for its own sake. todo.txt asks for the seven remote
 * topics to be bundled and cached on disk, and both halves of that job are
 * built out of exactly these functions: bundling makes markdownToText's output
 * the only thing a reader ever sees of those topics, in every build and with
 * the network down, and caching turns a filename that arrived over the network
 * into a path on the user's machine. Whatever the renderer gets wrong today is
 * shipped and taken offline by the first of those, so it is fixed and pinned
 * before either is written.
 */

#pragma once

#include <string>
#include <vector>

namespace help_assets {

// One entry from help_index.json.
struct HelpTopic {
    std::string id;
    std::string title;
    std::string description;
    std::string filename;
};

// Read the "topics" array out of help_index.json. Returns false and sets error
// when nothing usable was found; topics is cleared on entry either way.
//
// This is a scanner rather than a JSON parser: it takes each brace-delimited
// run after the "topics" key and pulls four quoted values out of it by name. It
// therefore gets a nested object or an escaped quote wrong, and does not stop
// at the closing bracket of the array. That is the behaviour that has shipped;
// it is moved here unchanged, and the suite pins the shape the published index
// actually has rather than asserting anything about the shapes it would
// mishandle. nlohmann::json is already in the tree (Config.cpp uses it) and
// would fix all three, but swapping the parser is a behaviour change that
// belongs in its own commit.
bool parseIndexJson(const std::string& json,
                    std::vector<HelpTopic>& topics,
                    std::string& error);

// Render markdown to the plain text the help pane's read-only EDIT control
// shows. Line endings are CRLF because that is what an EDIT control needs to
// break a line at all.
std::string markdownToText(const std::string& markdown);

// True when name is safe to paste into a download URL and into a cache path.
//
// Both of those are built out of HelpTopic::filename, which is a string that
// arrived over the network in help_index.json. It is a whitelist - a plain file
// name, no separators, no drive letter, no dot-dot - because a blacklist of the
// ways a path escapes a directory on Windows is a list nobody finishes.
//
// Its callers are cachePath() and cacheTempPath() below, which return empty
// rather than a path when it says no, and HelpWindow::fetchTopic, which refuses
// to build a download URL out of a name it rejects.
bool isSafeAssetName(const std::string& name);

// Did the WHOLE document arrive? False means the caller must not treat the
// bytes it has as the document.
//
//   declaredLength  the Content-Length the response carried, or -1 for a
//                   response that carried none.
//   assembled       how many bytes the read loop actually put together.
//   readError       the WinHTTP error code from a WinHttpQueryDataAvailable or
//                   WinHttpReadData that returned FALSE, or 0 when every read
//                   returned.
//   error           set to a sentence for the reader on false; left alone on
//                   true, so a caller cannot print a stale reason next to a
//                   document that arrived intact.
//
// Outside tests\test_help.cpp its one caller is HelpWindow::downloadToString,
// which clears its result and reports failure when this says no. fetchTopic
// then hands resolveTopic an empty download - a failed download and an empty
// one take the same path there - so the reader gets the CACHED copy under a
// status line reading "offline copy", or, with nothing cached and nothing
// bundled, the failure page carrying the sentence set in error below. What
// they no longer get is a fragment called "(downloaded)".
//
// Before this existed, downloadToString ended its read loop with an
// unconditional "success = true", so a body cut off part-way was cached by
// resolveTopic ON TOP OF the complete copy and labelled "(downloaded)": the
// fragment became the durable offline copy of the topic.
//
// It is a free function rather than three lines inside that loop because
// downloadToString needs a live WinHTTP session and tests\test_help.cpp cannot
// call it at all. The RULE is the part that can be wrong, and it is covered
// there by test_download_completeness and
// test_download_truncation_leaves_the_cache_alone. What those two cannot reach
// is the WIRING - that downloadToString queries the real Content-Length, counts
// the real bytes and passes on a real error code - which was measured with a
// throwaway probe instead and is argued in the comment on that read loop.
//
// BOTH halves of the rule are needed; each catches a case the other cannot.
// Measured 2026-08-28 against a throwaway localhost server, with a WinHTTP
// probe that replicated downloadToString's read loop:
//
//   Content-Length: 5468, 2000 bytes sent, socket closed - WinHTTP reported NO
//   error at all. The next WinHttpQueryDataAvailable returned TRUE with 0
//   bytes available, exactly as it does at a clean end of body. Nothing but
//   comparing 2000 against 5468 tells those two apart, which is why "treat a
//   failed read as failure" was not enough on its own.
//
//   Transfer-Encoding: chunked, one 500-byte chunk, socket closed with no
//   terminating chunk - WinHttpQueryDataAvailable returned FALSE with
//   ERROR_WINHTTP_INVALID_SERVER_RESPONSE (12152). A chunked response carries
//   no Content-Length (the same probe got ERROR_WINHTTP_HEADER_NOT_FOUND,
//   12150), so the length half is blind to this one and the read-error half is
//   what catches it.
//
// A missing Content-Length is therefore NOT fatal. "No length, no document"
// would take the whole remote help system offline the day the asset host
// switched to chunked, and it does not need to be fatal: measured the same day
// against https://github.com/avwohl/ioscpm/releases/latest/download/, the final
// 200 arrives from release-assets.githubusercontent.com with a Content-Length
// and no Transfer-Encoding - help_index.json 1266, help_cpm22.md 5147,
// help_file_transfer.md 1145. The two GitHub 302s in front of it are followed
// by WinHTTP itself before downloadToString looks at anything: the probe asked
// for the help_cpm22.md URL above and WinHttpQueryHeaders answered 200 with
// Content-Length 5147, so what is being judged is the asset and not a redirect.
//
// An empty body with "Content-Length: 0" is COMPLETE here, and stays the
// caller's problem rather than becoming this one's: resolveTopic treats an
// empty download as absent, which is what stops an empty HTTP 200 from
// displacing the cached copy.
bool downloadIsComplete(long long declaredLength,
                        unsigned long long assembled,
                        unsigned long readError,
                        std::string& error);

//=============================================================================
// The on-disk help cache
//
// todo.txt: "the seven remote help topics have no bundled copy and no on-disk
// cache, so an offline user - or a release whose assets were not attached -
// gets none of them." This is the cache half of that item and not the bundling
// half; see resolveTopic below for why the other half is still blocked.
//
// What it buys: HelpWindow's m_cache keeps a topic in RAM for CACHE_TTL_MS
// (fifteen minutes) and loses it at exit, so a reader who read the CP/M 2.2
// guide yesterday and opens it on a train today gets "This topic could not be
// downloaded." A topic that has been read once should stay readable, and stay
// readable across a restart, which is the one thing an in-memory cache cannot
// do.
//
// Everything here is keyed on the ASSET NAME - HelpTopic::filename, e.g.
// "help_cpm22.md" - and not on HelpTopic::id, because the asset name is what
// isSafeAssetName was written to judge and it is the only one of the two that
// has to survive being turned into a path. Both arrived over the network.
//=============================================================================

// Point the cache at a directory. Call it once, at startup, before a help
// window can exist: cacheDir() is read on the thread fetchTopic() detaches and
// there is no lock, so a later call would be a data race rather than a
// reconfiguration.
//
// This seam exists so that HelpAssets does not become the FOURTH copy of the
// SHGetKnownFolderPath(FOLDERID_LocalAppData) + WideCharToMultiByte snippet.
// The three already in the tree are EmulatorEngine::getUserDataDirectory,
// DiskCatalog's constructor and emu_io_windows.cpp's getDataFolder; a fourth
// would be a fourth place to edit when the packaged layout moves, and three is
// already one too many. It is also what lets tests\test_help.cpp exercise a
// real round trip in a scratch directory instead of writing into the user's
// profile.
//
// MainWindow should call setCacheRoot(EmulatorEngine::getUserDataDirectory() +
// "\\help") before ShowHelpWindow can run. NOTHING IN THIS REPOSITORY CALLS IT
// YET - this commit owns HelpAssets and HelpWindow only - so until that line
// exists the default below is what runs.
//
// An empty string puts the cache back on that default, which is how
// tests\test_help.cpp gets at it.
void setCacheRoot(const std::string& dir);

// The directory the cache lives in: whatever setCacheRoot was given, or, when
// it was never called, %LOCALAPPDATA%\z80cpmw\help read from the environment.
//
// The default is a fallback and not the intended configuration. It normally
// names the same directory getUserDataDirectory() + "\\help" would, since
// LOCALAPPDATA and FOLDERID_LocalAppData normally agree - "normally" because
// that equivalence has NOT been checked inside an MSIX package here, and this
// says so rather than implying it was measured. If they ever disagreed the
// cache would simply sit somewhere else valid; nothing else reads it.
//
// Returns empty if LOCALAPPDATA is unset, which makes cachePath() empty, which
// makes readCached() and writeCached() fail cleanly rather than writing to a
// relative path in whatever the current directory happens to be.
std::string cacheDir();

// The cache file for an asset, and the scratch name writeCached() renames onto
// it. Both return empty for a name isSafeAssetName() refuses - that call is the
// first statement of cachePath(), so every path this file builds has been
// through it, and there is no way to reach the file system here without one.
//
// The temp name carries the process id because two copies of z80cpmw can run at
// once and each would otherwise pick the same scratch name for the same topic;
// with the id they write different files and the losing rename is simply
// overwritten by the winner, which is fine because both wrote the same asset.
// cacheTempPath is public for tests\test_help.cpp, which parks a half-written
// file there and asserts it is not readable under the real name.
std::string cachePath(const std::string& assetName);
std::string cacheTempPath(const std::string& assetName);

// Read a cached asset. False, with content cleared, whenever a whole topic did
// not come back: no such file, an unsafe name, an empty file, an implausibly
// large one, or a read that came up short.
//
// An empty file is reported as absent rather than as an empty topic: zero bytes
// is what a killed writer or a write to a device name leaves behind, and a
// reader shown a blank pane cannot tell it from a topic that loaded and said
// nothing. The size ceiling is 1 MB against a largest published asset of 5,147
// bytes - help_cpm22.md, fetched from the release URL HelpWindow reads, where
// all eight assets together come to 23,997. It is not a tight bound, it is a
// bound: a file some other process grew cannot be read whole into memory here.
bool readCached(const std::string& assetName, std::string& content);

// Write an asset to the cache, or refuse. Bytes are stored exactly as given:
// no BOM handling, no line-ending translation, so readCached returns what was
// downloaded and markdownToText sees the same input either way.
//
// The write goes to cacheTempPath() and is renamed onto cachePath() with
// MoveFileExW(MOVEFILE_REPLACE_EXISTING), so a reader never sees a partial file
// under the real name - the name appears with all of its content or not at all.
// Writing in place was rejected outright: the file being overwritten is the
// only offline copy the user has, and a truncating open that then fails leaves
// them with less than they had before opening the app.
//
// Empty content is refused, because it would replace a good cached copy with a
// file readCached() then reports as absent. That case is real: downloadToString
// returns success for an HTTP 200 with an empty body.
bool writeCached(const std::string& assetName, const std::string& content);

// Which copy of a topic the reader is looking at.
enum class TopicSource {
    None,        // no copy anywhere - the failure page
    Downloaded,  // fetched this time
    Cached,      // read back from cacheDir()
    Bundled,     // compiled into this binary
};

struct ResolvedTopic {
    TopicSource source = TopicSource::None;
    std::string content;
    // Local "YYYY-MM-DD HH:MM" of the cache file's last write, set for Cached
    // only and empty if the timestamp could not be read. The status line shows
    // it: "offline copy" alone does not tell a reader whether they are looking
    // at yesterday's text or last year's.
    std::string savedWhen;
};

// The order todo.txt asks for, in one place: download, then cache, then the
// copy shipped in the binary. A successful download is cached on the way past,
// which is the only place anything writes the cache.
//
// downloaded is empty when the fetch failed or was not attempted, and bundled
// is empty when this build ships no copy of the topic. Empty means absent for
// both - a zero-byte topic and a missing one are the same thing to a reader,
// and treating them alike is what stops an empty HTTP 200 from displacing the
// cached copy.
//
// TODAY BUNDLED IS ALWAYS EMPTY FOR THE SEVEN REMOTE TOPICS, so their chain
// really ends at the cache. The third step is written anyway because the
// bundling commit is a change to what HelpWindow passes here and to nothing
// else. That commit is blocked on wording, not on plumbing: three of the seven
// published topics are written for iOS ("tap the gear icon", "Cmd+C copies the
// screen text", an "iOS / iPadOS" heading) and the published index calls Quick
// Start "Getting started with iOSCPM", so compiling them into a Windows binary
// would make the wrong wording durable and offline - which is the opposite of
// what the todo item wants. The cache carries no such question: it stores the
// text the user has already been shown.
ResolvedTopic resolveTopic(const std::string& assetName,
                           const std::string& downloaded,
                           const std::string& bundled);

// A short phrase naming a source, for the status line. Never null.
const char* sourceLabel(TopicSource source);

// Decode UTF-8 to UTF-16 for the Win32 W entry points.
//
// The thing this replaces is std::wstring(s.begin(), s.end()), which widens
// each char one at a time; char is signed on MSVC, so every byte over 0x7F
// becomes a negative value and then a wchar_t up near 0xFF80-0xFFFF. Of the
// eight assets published in avwohl/ioscpm exactly one is not ASCII -
// help_file_transfer.md, 30 bytes making 10 characters: U+2014 twice, U+2026
// twice, U+2192 six times - so the old conversion turned each of those into two
// or three pieces of garbage.
//
// Invalid UTF-8 is not rejected. MultiByteToWideChar is called without
// MB_ERR_INVALID_CHARS, so a bad sequence becomes U+FFFD and the reader still
// gets the rest of the topic; failing the call would hand the pane an empty
// string, which looks exactly like a topic that loaded and said nothing.
std::wstring toWide(const std::string& utf8);

}  // namespace help_assets
