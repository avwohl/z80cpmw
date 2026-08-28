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

// Refuse a termcap-style string that decode() would silently turn into
// something other than what it says. Returns nullptr when the string is fine,
// and a one-line user-facing reason when it is not - the same nullptr-means-
// nothing-to-say shape as reservedFor() below.
//
// This exists because DECODE() CANNOT FAIL. It has no error return and every
// arm of its switch pushes a byte, so a mistake in a sequence is not diagnosed,
// it is sent: the bytes the guest receives are simply not the ones the author
// meant. In a config file read at startup that is survivable, because the
// configuration report names the line afterwards. Under a text field somebody
// is typing into it is not, which is what this is for.
//
// decode() itself is deliberately left alone. Every string already sitting in a
// user's z80cpmw.json goes on decoding exactly as it did, and a validator that
// only guards the new entry point cannot break an old file.
//
// The refusals are read out of decode()'s arms rather than invented:
//
//   - A TRAILING BACKSLASH. decode's `i + 1 < s.size()` guard fails, control
//     falls through to the final else, and the introducer is emitted as a
//     literal backslash - the one character it is least likely to have meant.
//   - AN UNKNOWN ESCAPE LETTER. decode's `default:` arm pushes the letter and
//     drops the backslash, so the C-style "\x1b" someone reaches for out of
//     habit sends the three bytes x, 1, b.
//   - AN OCTAL ESCAPE OVER \377. decode reads up to three octal digits into an
//     int and then stores `val & 0xFF`, so \400 - which reads as 256 - is sent
//     as NUL. That specific case is the one this function was written for.
//   - A TRAILING CARET, for the same reason as the trailing backslash: the
//     final else emits a literal '^'. A literal caret is spelled \^.
//   - A CARET ON SOMETHING THAT IS NOT A CONTROL CHARACTER'S NAME. decode does
//     `toupper(n) & 0x1F` unconditionally, so ^1 is not refused, it is 0x11 -
//     a control byte, just not the one caret notation names. The accepted set
//     is exactly the characters whose upper-case form lies in 0x40..0x5F (so
//     @ A-Z [ \ ] ^ _ and, folded, a-z), plus ^? for DEL. Those are the ones
//     for which 0x40 + the decoded byte names the character back again;
//     tests/test_vt52.cpp states the rule in that form, over decode's own
//     output, so it is not merely this predicate agreeing with itself.
//
// An EMPTY string is accepted. It is not an omission: KeyMap::build reads ""
// as "unbind this key" and find() answers nullptr for it, so it is a spelling
// with a meaning, and it is what a Settings dialog's Unbind button writes.
//
// The loop below walks the same arms in the same order as decode(), and nothing
// in the language makes the two move together - one is a scanner, the other a
// producer, and sharing a pass between them would mean rewriting the function
// whose output a hundred-odd checks already pin. What catches a drift is the
// suite. For all three arms that can refuse, it decides what this function OUGHT
// to say by running DECODE and looking at the bytes - over all 256 successors of
// a backslash, all 256 of a caret, and every octal value from \000 to \777 - so
// an escape added to decode() and forgotten here fails a check rather than
// quietly becoming unusable from the dialog.
//
// The messages are terser than they could be on purpose. The Settings dialog
// shows them on a single-line label, which was measured to hold about 78
// characters at the dialog's 900px width and to clip the rest without a mark,
// so every string below is written to fit inside that.
inline const char* validateSequence(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\') {
            if (i + 1 >= s.size()) {
                return "A backslash needs an escape letter or octal digits after it.";
            }
            char n = s[++i];
            switch (n) {
            case 'E': case 'e': case 'n': case 'r': case 't':
            case 'b': case 'f': case 's': case '\\': case '^':
                break;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                int val = n - '0';
                for (int k = 0; k < 2 && i + 1 < s.size() &&
                                s[i + 1] >= '0' && s[i + 1] <= '7'; ++k) {
                    val = val * 8 + (s[++i] - '0');
                }
                if (val > 0xFF) {
                    return "An octal escape stops at \\377; \\400 and up wrap round to NUL.";
                }
                break;
            }
            default:
                return "Unknown escape. Use \\E \\n \\r \\t \\b \\f \\s \\\\ \\^ or octal digits.";
            }
        } else if (c == '^') {
            if (i + 1 >= s.size()) {
                return "A caret needs a letter after it, or ^? for Delete. \\^ is literal.";
            }
            char n = s[++i];
            if (n == '?') continue;                       // ^? = DEL
            int up = std::toupper((unsigned char)n);
            if (up < 0x40 || up > 0x5F) {
                return "A caret takes a letter, or one of @ [ \\ ] ^ _ ? and nothing else.";
            }
        }
    }
    return nullptr;
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

// The other direction: the spelling the config file uses for a plain,
// unmodified key, or nullptr for a virtual-key code that is not bindable.
//
// vkForName() accepts several spellings per key - "ins", "Insert", "PgUp",
// "prior" - so there is no such thing as THE name of a key; there is only the
// one this returns. The one chosen is defaultBindings()'s, because that is what
// the app writes into a fresh z80cpmw.json and therefore what a user reading
// their own file sees. tests/test_vt52.cpp walks every code 0..255 and requires
// vkForName(baseNameForVk(vk)) == vk, so this table cannot drift away from
// vkForName's without failing the suite.
inline const char* baseNameForVk(int vk) {
    switch (vk) {
    case VK_UP:     return "Up";
    case VK_DOWN:   return "Down";
    case VK_LEFT:   return "Left";
    case VK_RIGHT:  return "Right";
    case VK_HOME:   return "Home";
    case VK_END:    return "End";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_PRIOR:  return "PageUp";
    case VK_NEXT:   return "PageDown";
    default: break;
    }
    // VK_F1..VK_F12 are contiguous, which is the same fact vkForName() uses to
    // turn a parsed number back into a code.
    if (vk >= VK_F1 && vk <= VK_F12) {
        static const char* const kFunctionKeys[] = {
            "F1", "F2", "F3", "F4",  "F5",  "F6",
            "F7", "F8", "F9", "F10", "F11", "F12",
        };
        return kFunctionKeys[vk - VK_F1];
    }
    return nullptr;
}

// Turn a packed key id back into the config file's own spelling, or an empty
// string if nothing in the file could spell it.
//
// THE PREFIX ORDER HAD TO BE FIXED, and Ctrl, Shift, Alt is the order. It is
// not a formatting preference: keyIdForName() accepts the prefixes in ANY
// order, so "Ctrl+Shift+F3" and "Shift+Ctrl+F3" are two spellings of one
// binding, and a caller that writes names back into "keyboard.keys" has to
// choose one spelling and then always choose the same one. If it did not, a
// read-modify-write of a file already containing "Shift+Ctrl+F3" would add
// "Ctrl+Shift+F3" beside it and leave two entries for one key - with the answer
// to which of them the app honours decided by KeyMap::build()'s merge order
// rather than by anything the user could see. ConfigManager::load()'s fill loop
// already refuses to create that situation, matching by resolved id rather than
// by name for exactly this reason; this is the same rule stated for the
// opposite direction.
//
// Ctrl, Shift, Alt is chosen over any other fixed order because three separate
// things already spell it that way: the KM_MOD_ bits are declared in that order
// (1, 2, 4), defaultBindings() writes "Ctrl+Up" and friends, and
// docs/CONFIGURATION.md lists them "Ctrl+, Shift+ and Alt+".
//
// An id carrying a modifier bit outside those three has no spelling at all -
// nothing would parse back to it - so it returns empty rather than a name that
// resolves to a DIFFERENT id. An unbindable virtual-key code does the same.
inline std::string nameForKeyId(unsigned id) {
    const char* base = baseNameForVk(static_cast<int>(id & 0xFFFFu));
    if (!base) return std::string();
    const unsigned mods = id >> 16;
    if (mods & ~(KM_MOD_CTRL | KM_MOD_SHIFT | KM_MOD_ALT)) return std::string();

    std::string out;
    if (mods & KM_MOD_CTRL)  out += "Ctrl+";
    if (mods & KM_MOD_SHIFT) out += "Shift+";
    if (mods & KM_MOD_ALT)   out += "Alt+";
    out += base;
    return out;
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
