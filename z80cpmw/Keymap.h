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
    if (name.size() >= 2 && name[0] == 'f') {
        int n = std::atoi(name.c_str() + 1);
        if (n >= 1 && n <= 12) return VK_F1 + (n - 1);  // VK_F1..VK_F12 are contiguous
    }
    return -1;
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
    };
}

// Runtime lookup table: virtual-key code -> decoded byte sequence.
class KeyMap {
public:
    KeyMap() { build({}); }

    // Rebuild from a set of name->termcap-string overrides, layered on top of the
    // built-in defaults so a partial (or empty) config still yields working keys.
    // An override with an empty value unbinds that key.
    void build(const std::map<std::string, std::string>& overrides) {
        m_seqs.clear();
        std::map<std::string, std::string> merged = defaultBindings();
        for (const auto& kv : overrides) merged[kv.first] = kv.second;
        for (const auto& kv : merged) {
            int vk = vkForName(kv.first);
            if (vk < 0) continue;
            std::string seq = decode(kv.second);
            if (!seq.empty()) m_seqs[static_cast<UINT>(vk)] = seq;
        }
    }

    // Byte sequence for a virtual key, or nullptr if the key is unbound.
    const std::string* find(UINT vk) const {
        auto it = m_seqs.find(vk);
        return it == m_seqs.end() ? nullptr : &it->second;
    }

private:
    std::map<UINT, std::string> m_seqs;
};

} // namespace keymap
