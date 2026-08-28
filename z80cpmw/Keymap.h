/*
 * Keymap.h - Configurable terminal key bindings ("termcap in reverse")
 *
 * CP/M is pure ASCII and has no native notion of function or navigation keys;
 * every CP/M terminal historically defined its own escape sequences for them.
 * There is therefore no single "standard" set of bytes to send for F-keys or
 * Insert/PageUp/PageDown - it depends on the terminal the user's CP/M software
 * expects (VT100, ADM-3A, Televideo, Kaypro, ...).
 *
 * This module maps Windows virtual-key codes to byte sequences using
 * termcap-style escape strings, so a binding can be copied straight out of (or
 * into) a termcap/terminfo entry. That is the "termcap in reverse" idea: termcap
 * tells an application what bytes a key produces; here we use the same notation
 * to decide what bytes a key should produce.
 *
 * Default bindings follow the DEC VT220 / xterm convention, which matches the
 * VT100-style sequences the terminal already emits for the arrow keys. Every
 * binding is overridable from the config file (z80cpmw.json, "keyboard.keys").
 */
#pragma once

#include <windows.h>
#include <string>
#include <map>
#include <cctype>
#include <cstdlib>

namespace keymap {

// Decode a termcap-style escape string into the raw bytes to send to the guest.
// Supported: \E (ESC), \n \r \t \b \f, \s (space), \\ (backslash), \^ (caret),
// \NNN (1-3 octal digits), ^X (control char), ^? (DEL/0x7F). Any other character
// is taken literally.
inline std::string decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
            case 'E': case 'e': out.push_back('\x1b'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 's': out.push_back(' '); break;
            case '\\': out.push_back('\\'); break;
            case '^': out.push_back('^'); break;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                int val = n - '0';
                for (int k = 0; k < 2 && i + 1 < s.size() &&
                                s[i + 1] >= '0' && s[i + 1] <= '7'; ++k) {
                    val = val * 8 + (s[++i] - '0');
                }
                out.push_back(static_cast<char>(val & 0xFF));
                break;
            }
            default: out.push_back(n); break;
            }
        } else if (c == '^' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == '?') out.push_back('\x7f');                 // ^? = DEL
            else out.push_back(static_cast<char>(std::toupper((unsigned char)n) & 0x1F));
        } else {
            out.push_back(c);
        }
    }
    return out;
}

// Modifier bits. A binding is identified by a virtual-key code plus whatever
// modifiers were held with it, so Ctrl+Left is a different binding from Left
// rather than the same one. Without this the two were indistinguishable: every
// modified press fell through to the unmodified sequence, and there was no way
// to say otherwise in the config.
enum : unsigned {
    KM_MOD_NONE  = 0,
    KM_MOD_CTRL  = 1,
    KM_MOD_SHIFT = 2,
    KM_MOD_ALT   = 4,
};

// Pack a virtual-key code and its modifiers into one lookup key.
inline unsigned keyId(int vk, unsigned mods) {
    return (static_cast<unsigned>(vk) & 0xFFFFu) | (mods << 16);
}

// Map a key name used in the config "keys" object to a Windows virtual-key code.
// Names are case-insensitive; a few common aliases are accepted. Returns -1 for
// names that are not bindable.
inline int vkForName(std::string name) {
    for (char& ch : name) ch = static_cast<char>(std::tolower((unsigned char)ch));
    if (name == "up")                                  return VK_UP;
    if (name == "down")                                return VK_DOWN;
    if (name == "left")                                return VK_LEFT;
    if (name == "right")                               return VK_RIGHT;
    if (name == "home")                                return VK_HOME;
    if (name == "end")                                 return VK_END;
    if (name == "insert" || name == "ins")             return VK_INSERT;
    if (name == "delete" || name == "del")             return VK_DELETE;
    if (name == "pageup"   || name == "pgup" || name == "prior") return VK_PRIOR;
    if (name == "pagedown" || name == "pgdn" || name == "next")  return VK_NEXT;
    // "f" followed by digits and nothing else. atoi stops at the first
    // non-digit and reports what it read, so it happily turned "F1x" - and
    // "F1 " - into F1; a name with a typo in it should be rejected, not
    // silently bound to something near it.
    if (name.size() >= 2 && name[0] == 'f' &&
        name.find_first_not_of("0123456789", 1) == std::string::npos) {
        int n = std::atoi(name.c_str() + 1);
        if (n >= 1 && n <= 12) return VK_F1 + (n - 1);  // VK_F1..VK_F12 are contiguous
    }
    return -1;
}

// Map a full config key name to a packed lookup key, accepting any number of
// "Ctrl+", "Shift+" and "Alt+" prefixes before the key name ("Ctrl+Left",
// "Ctrl+Shift+F3"). Case-insensitive, and "Control+" is accepted for "Ctrl+".
// Returns -1 for a name that is not bindable.
inline long keyIdForName(const std::string& name) {
    std::string rest = name;
    unsigned mods = KM_MOD_NONE;
    for (;;) {
        size_t plus = rest.find('+');
        if (plus == std::string::npos) break;
        std::string prefix = rest.substr(0, plus);
        for (char& ch : prefix) ch = static_cast<char>(std::tolower((unsigned char)ch));
        if (prefix == "ctrl" || prefix == "control") mods |= KM_MOD_CTRL;
        else if (prefix == "shift")                  mods |= KM_MOD_SHIFT;
        else if (prefix == "alt")                    mods |= KM_MOD_ALT;
        else break;   // not a modifier - leave it for vkForName to reject
        rest = rest.substr(plus + 1);
    }
    int vk = vkForName(rest);
    if (vk < 0) return -1;
    return static_cast<long>(keyId(vk, mods));
}

// One combination the application keeps for itself, and the words a Settings
// dialog should put beside it when it explains why the row cannot be bound.
struct ReservedKey {
    int vk;
    unsigned mods;          // modifiers that must ALL be held for this to bite
    const char* purpose;    // user-facing, e.g. "scroll back one page"
};

// The four scrollback combinations. TerminalView::handleKeyDown answers these
// before it consults the map, so no config file can put bytes on them. Every
// piece of code that needs to know WHICH combinations those are reads them from
// here rather than keeping a list of its own: reservedFor(), reservedPurpose(),
// isReservedForApp() and classifyName() below, and through classifyName() the
// config-file diagnostics in Config.cpp, which used to carry a second copy of
// this table and the purpose strings.
//
// The match is "at least these modifiers", not "exactly these" - see
// reservedFor().
//
// Adding a fifth row reserves the combination for all of those callers at once,
// but it is not sufficient on its own. Three things outside this file still
// have to be changed by hand, and none of them is generated from this table:
//
//   - TerminalView::handleKeyDown needs a case in the switch inside its
//     reservedFor() branch saying what the new combination DOES. Without one
//     the press is correctly withheld from the guest and then nothing happens -
//     the worst of both answers.
//   - tests/test_vt52.cpp walks this table and asserts it against a list of the
//     rows the suite expects, so a new row fails the suite until it is declared
//     there. That is deliberate: it is what stops this table drifting silently.
//   - docs/CONFIGURATION.md carries a hand-maintained sentence naming the same
//     four combinations. Nothing checks it against this table, so it is the one
//     copy that can go stale without anything complaining.
inline const ReservedKey* reservedKeys(size_t* count) {
    static const ReservedKey table[] = {
        { VK_PRIOR, KM_MOD_SHIFT, "scroll back one page" },
        { VK_NEXT,  KM_MOD_SHIFT, "scroll forward one page" },
        { VK_HOME,  KM_MOD_CTRL,  "jump to the oldest scrollback line" },
        { VK_END,   KM_MOD_CTRL,  "return to the live screen" },
    };
    if (count) *count = sizeof table / sizeof table[0];
    return table;
}

// What the app does with this key press, or nullptr if it is the guest's.
//
// The modifier test is a mask, not an equality: a press is reserved when it
// carries at least the listed modifiers. That is exactly what the hand-written
// if-statements in handleKeyDown did before this table existed - they read
// "shift held" and "ctrl held" independently - so Ctrl+Shift+PageUp has always
// scrolled back as well. It is preserved rather than tightened, because
// narrowing it to an exact match would be a behaviour change smuggled in under
// a refactor, and the wider mask is the friendlier of the two: a user who is
// still holding Ctrl from Ctrl+Home gets the scroll they asked for.
inline const char* reservedFor(int vk, unsigned mods) {
    size_t n = 0;
    const ReservedKey* table = reservedKeys(&n);
    for (size_t i = 0; i < n; ++i) {
        if (table[i].vk == vk && (mods & table[i].mods) == table[i].mods) {
            return table[i].purpose;
        }
    }
    return nullptr;
}

// The same question asked of a packed key id, for callers walking a map.
inline const char* reservedPurpose(unsigned id) {
    return reservedFor(static_cast<int>(id & 0xFFFFu), id >> 16);
}

inline bool isReservedForApp(unsigned id) {
    return reservedPurpose(id) != nullptr;
}

// How a config walker or a Settings dialog should treat one name from the
// "keys" object: Ok to bind, Unknown (a typo - vkForName rejected it), or
// Reserved (it resolves, but the app answers it first, so the bytes would
// never be sent). *id receives the packed id whenever the name resolves,
// Reserved included, so a dialog can point at the row it clashes with; *why
// receives the reservedPurpose text for Reserved and nullptr otherwise.
enum class NameStatus { Ok, Unknown, Reserved };

inline NameStatus classifyName(const std::string& name, unsigned* id = nullptr,
                               const char** why = nullptr) {
    if (why) *why = nullptr;
    long resolved = keyIdForName(name);
    if (resolved < 0) return NameStatus::Unknown;
    unsigned packed = static_cast<unsigned>(resolved);
    if (id) *id = packed;
    if (const char* purpose = reservedPurpose(packed)) {
        if (why) *why = purpose;
        return NameStatus::Reserved;
    }
    return NameStatus::Ok;
}

// The built-in default bindings (name -> termcap-style sequence). These follow
// the VT220/xterm convention and match the arrow-key sequences the terminal
// already produced before this module existed (Up=ESC[A, Home=ESC[H, etc.).
inline std::map<std::string, std::string> defaultBindings() {
    return {
        {"Up",       "\\E[A"},
        {"Down",     "\\E[B"},
        {"Right",    "\\E[C"},
        {"Left",     "\\E[D"},
        {"Home",     "\\E[H"},
        {"End",      "\\E[F"},
        {"Insert",   "\\E[2~"},
        {"Delete",   "^?"},        // 0x7F, preserves the previous Delete behavior
        {"PageUp",   "\\E[5~"},
        {"PageDown", "\\E[6~"},
        {"F1",  "\\EOP"}, {"F2",  "\\EOQ"}, {"F3",  "\\EOR"}, {"F4",  "\\EOS"},
        {"F5",  "\\E[15~"}, {"F6",  "\\E[17~"}, {"F7",  "\\E[18~"}, {"F8",  "\\E[19~"},
        {"F9",  "\\E[20~"}, {"F10", "\\E[21~"}, {"F11", "\\E[23~"}, {"F12", "\\E[24~"},
        // Ctrl+arrows, in the same xterm convention as the rest of this table
        // (CSI 1 ; 5 <final>, where the 5 is the ctrl modifier). Until the map
        // grew modifiers these were indistinguishable from the plain arrows.
        // A CP/M editor is more likely to want the WordStar word-left/word-right
        // pair, which is one line of config away:
        //     "keyboard": { "keys": { "Ctrl+Left": "^A", "Ctrl+Right": "^F" } }
        {"Ctrl+Up",    "\\E[1;5A"},
        {"Ctrl+Down",  "\\E[1;5B"},
        {"Ctrl+Right", "\\E[1;5C"},
        {"Ctrl+Left",  "\\E[1;5D"},
    };
}

// Runtime lookup table: virtual-key code -> decoded byte sequence.
class KeyMap {
public:
    KeyMap() { build({}); }

    // Rebuild from a set of name->termcap-string overrides, layered on top of the
    // built-in defaults so a partial (or empty) config still yields working keys.
    // An override with an empty value unbinds that key.
    //
    // Reserved combinations are NOT filtered out here, deliberately. Being
    // reserved is a fact about the user interface, not about the map: the app
    // wins the press in handleKeyDown, and the stored sequence simply never
    // fires. Dropping such entries at load would make a Settings dialog lossy,
    // because it does a read-modify-write of the whole map and would silently
    // delete a line the user typed by hand. classifyName() is how a caller
    // refuses one out loud instead.
    void build(const std::map<std::string, std::string>& overrides) {
        m_seqs.clear();
        // Resolve names to key ids BEFORE merging. Two spellings of the same
        // binding - "Ctrl+Left", "ctrl+left", "CTRL+Left" - are three different
        // strings and one key, so merging by name left both the default and the
        // override in the map and let ASCII ordering decide which was applied
        // last. "CTRL+Left" sorts before "Ctrl+Left", so that spelling of an
        // override lost to the default it was meant to replace.
        std::map<unsigned, std::string> merged;
        for (const auto& kv : defaultBindings()) {
            long id = keyIdForName(kv.first);
            if (id >= 0) merged[static_cast<unsigned>(id)] = kv.second;
        }
        for (const auto& kv : overrides) {
            long id = keyIdForName(kv.first);
            if (id >= 0) merged[static_cast<unsigned>(id)] = kv.second;
        }
        // An empty value unbinds, and is stored as an explicit empty entry
        // rather than left absent. The difference matters to find(): an absent
        // modified key falls back to the plain one, but a deliberately unbound
        // one must not - "Ctrl+Left": "" means send nothing, not send Left.
        for (const auto& kv : merged) {
            m_seqs[kv.first] = decode(kv.second);
        }
    }

    // Byte sequence for a key press, or nullptr if it is unbound.
    //
    // A modified press that has no binding of its own falls back to the
    // unmodified one, which is what this did for every press before modifiers
    // existed. That keeps Shift+Insert and the like working as they always did,
    // and means adding a modifier to the table is opt-in rather than a silent
    // removal of the plain binding.
    const std::string* find(UINT vk, unsigned mods = KM_MOD_NONE) const {
        auto it = m_seqs.find(keyId(static_cast<int>(vk), mods));
        if (it != m_seqs.end()) {
            // Present but empty means deliberately unbound. Report it as
            // unbound and stop - falling back here would answer a config that
            // said "Ctrl+Left sends nothing" with plain Left's sequence.
            return it->second.empty() ? nullptr : &it->second;
        }
        if (mods != KM_MOD_NONE) return findExact(vk, KM_MOD_NONE);
        return nullptr;
    }

    // Byte sequence bound to exactly this key and modifier combination, with no
    // fallback to the unmodified binding. The Alt handling needs this: an Alt
    // press is the menu key on Windows and must keep reaching the menu unless
    // the user has bound that specific combination, and the falling-back find()
    // would answer "bound" for every Alt press of an otherwise-bound key.
    const std::string* findExact(UINT vk, unsigned mods) const {
        auto it = m_seqs.find(keyId(static_cast<int>(vk), mods));
        if (it == m_seqs.end() || it->second.empty()) return nullptr;
        return &it->second;
    }

private:
    std::map<unsigned, std::string> m_seqs;
};

} // namespace keymap
