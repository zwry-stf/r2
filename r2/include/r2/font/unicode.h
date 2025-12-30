#pragma once
#include <r2/renderer_definitions.h>


r2_begin_

namespace unicode {
    inline constexpr std::uint32_t codepoint_max = 0x10FFFF;
    inline constexpr std::uint32_t codepoint_invalid = 0xFFFD;

    using unicode_type = char32_t;

    template <typename C>
    concept char_compatible =
        std::same_as<std::remove_cv_t<C>, char>    ||
        std::same_as<std::remove_cv_t<C>, char8_t> ||
        std::same_as<std::remove_cv_t<C>, char16_t>||
        std::same_as<std::remove_cv_t<C>, char32_t>||
        std::same_as<std::remove_cv_t<C>, wchar_t>;

    template <typename S>
    using get_char_t = std::remove_cv_t<
        std::remove_reference_t<decltype(std::declval<const S&>()[0])>
    >;

    template <typename S>
    concept string_like =
        requires(const S& s, std::uint32_t i) {
            { s.length() } -> std::convertible_to<std::size_t>;
            { s.empty() } -> std::convertible_to<bool>;
            { s[i] };
        } &&
        char_compatible<get_char_t<S>>;

    template <string_like String>
    [[nodiscard]] v_always_inline unicode_type impl_get_char(const String& str, std::uint32_t length, std::uint32_t& pos)
    {
        using char_t = get_char_t<String>;
        if (pos >= length)
            return static_cast<unicode_type>(codepoint_invalid);

        if constexpr (sizeof(char_t) == 1) {
            // UTF-8
            const std::uint8_t lead = static_cast<std::uint8_t>(str[pos]);

            if (lead < 0x80u) {
                unicode_type un = static_cast<unicode_type>(lead);
                pos += 1u;
                return un;
            }
            else {
                constexpr std::uint8_t lengths[32] = {
                    1u,1u,1u,1u,1u,1u,1u,1u,
                    1u,1u,1u,1u,1u,1u,1u,1u,
                    0u,0u,0u,0u,0u,0u,0u,0u,
                    2u,2u,2u,2u,3u,3u,4u,0u
                };
                constexpr std::uint32_t masks[]  = { 0x00u, 0x7fu, 0x1fu, 0x0fu, 0x07u };
                constexpr std::uint32_t mins[]   = { 0x400000u, 0x0u, 0x80u, 0x800u, 0x10000u };
                constexpr std::uint32_t shiftc[] = { 0u, 18u, 12u, 6u, 0u };
                constexpr std::uint32_t shifte[] = { 0u, 6u, 4u, 2u, 0u };

                std::uint32_t len    = lengths[lead >> 3];
                std::uint32_t wanted = len + (len != 0u ? 0u : 1u);

                std::uint8_t s[4] = { 0u, 0u, 0u, 0u };
                s[0] = lead;
                s[1] = (pos + 1u < length) ? static_cast<std::uint8_t>(str[pos + 1u]) : 0u;
                s[2] = (pos + 2u < length) ? static_cast<std::uint8_t>(str[pos + 2u]) : 0u;
                s[3] = (pos + 3u < length) ? static_cast<std::uint8_t>(str[pos + 3u]) : 0u;

                std::uint32_t code = (std::uint32_t)(s[0] & masks[len]) << 18u;
                code |= (std::uint32_t)(s[1] & 0x3f) << 12;
                code |= (std::uint32_t)(s[2] & 0x3f) << 6;
                code |= (std::uint32_t)(s[3] & 0x3f);
                code >>= shiftc[len];

                std::uint32_t e = 0;
                e  = (code < mins[len]) << 6;
                e |= ((code >> 11) == 0x1b) << 7;
                e |= (code > codepoint_max) << 8;
                e |= (s[1] & 0xc0) >> 2;
                e |= (s[2] & 0xc0) >> 4;
                e |= (s[3]) >> 6;
                e ^= 0x2a;
                e >>= shifte[len];

                if (e != 0u) {
                    wanted = 1u;
                    code = codepoint_invalid;
                }

                pos += wanted;
                return static_cast<unicode_type>(code);
            }
        }
        else if constexpr (sizeof(char_t) == 2) {
            // UTF-16
            char16_t w1 = static_cast<char16_t>(str[pos]);

            if (w1 < 0xD800 || w1 > 0xDFFF) {
                pos += 1u;
                return static_cast<unicode_type>(w1);
            }

            if (w1 >= 0xD800u && w1 <= 0xDBFFu) {
                if (pos + 1u < length) {
                    char16_t w2 = static_cast<char16_t>(str[pos + 1u]);
                    if (w2 >= 0xDC00u && w2 <= 0xDFFFu)
                    {
                        std::uint32_t code = 0x10000u
                            + (((std::uint32_t)(w1 - 0xD800u) << 10u)
                            |  (std::uint32_t)(w2 - 0xDC00u));

                        pos += 2u;
                        return static_cast<unicode_type>(code);
                    }
                }
                pos += 1u;
                return static_cast<unicode_type>(codepoint_invalid);
            }

            pos += 1u;
            return static_cast<unicode_type>(codepoint_invalid);
        }
        else if constexpr (sizeof(char_t) == 4) {
            // UTF-32
            unicode_type ch = static_cast<unicode_type>(str[pos]);
            if (static_cast<std::uint32_t>(ch) > codepoint_max) {
                ch = static_cast<unicode_type>(codepoint_invalid);
            }

            pos += 1u;
            return ch;
        }
        else
            throw 0;
    }

    template <string_like String>
    [[nodiscard]] v_always_inline unicode_type get_char_auto(const String& str, std::uint32_t length, std::uint32_t& pos) {
        using char_t = get_char_t<String>;

        assert(pos < length);

        return impl_get_char<String>(str, length, pos);
    }

    template <char_compatible OutT, char_compatible CharT, std::size_t N>
    [[nodiscard]] v_always_inline void to_lower_bytes(CharT in, OutT(&out)[N]) {
        static_assert(sizeof(CharT) > sizeof(OutT));
        static_assert(N >= sizeof(CharT) / sizeof(OutT));
    }
    
    [[nodiscard]] v_always_inline bool is_valid_codepoint(std::uint32_t cp) {
        if (cp > codepoint_max) return false;
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false;
        return true;
    }

    template <char_compatible OutCharT>
    [[nodiscard]] v_always_inline std::uint32_t encoded_units_needed(unicode_type cp) {
        std::uint32_t u = static_cast<std::uint32_t>(cp);
        if (!is_valid_codepoint(u)) u = codepoint_invalid;

        if constexpr (sizeof(OutCharT) == 1) {
            // UTF-8
            if (u <= 0x7Fu) return 1;
            if (u <= 0x7FFu) return 2;
            if (u <= 0xFFFFu) return 3;
            return 4;
        }
        else if constexpr (sizeof(OutCharT) == 2) {
            // UTF-16
            if (u <= 0xFFFFu) return 1;
            return 2;
        }
        else if constexpr (sizeof(OutCharT) == 4) {
            // UTF-32
            return 1;
        }
        else {
            return 0;
        }
    }

    template <char_compatible OutCharT>
    [[nodiscard]] v_always_inline std::uint32_t impl_put_char(unicode_type cp, OutCharT* out) {
        std::uint32_t u = static_cast<std::uint32_t>(cp);
        if (!is_valid_codepoint(u)) u = codepoint_invalid;

        if constexpr (sizeof(OutCharT) == 1) {
            // UTF-8
            if (u <= 0x7Fu) {
                out[0] = static_cast<OutCharT>(u);
                return 1;
            }
            if (u <= 0x7FFu) {
                out[0] = static_cast<OutCharT>(0xC0u | (u >> 6));
                out[1] = static_cast<OutCharT>(0x80u | (u & 0x3Fu));
                return 2;
            }
            if (u <= 0xFFFFu) {
                out[0] = static_cast<OutCharT>(0xE0u | (u >> 12));
                out[1] = static_cast<OutCharT>(0x80u | ((u >> 6) & 0x3Fu));
                out[2] = static_cast<OutCharT>(0x80u | (u & 0x3Fu));
                return 3;
            }
            out[0] = static_cast<OutCharT>(0xF0u | (u >> 18));
            out[1] = static_cast<OutCharT>(0x80u | ((u >> 12) & 0x3Fu));
            out[2] = static_cast<OutCharT>(0x80u | ((u >> 6) & 0x3Fu));
            out[3] = static_cast<OutCharT>(0x80u | (u & 0x3Fu));
            return 4;
        }
        else if constexpr (sizeof(OutCharT) == 2) {
            // UTF-16
            if (u <= 0xFFFFu) {
                out[0] = static_cast<OutCharT>(u);
                return 1;
            }
            u -= 0x10000u;
            out[0] = static_cast<OutCharT>(0xD800u + (u >> 10));
            out[1] = static_cast<OutCharT>(0xDC00u + (u & 0x3FFu));
            return 2;
        }
        else if constexpr (sizeof(OutCharT) == 4) {
            // UTF-32
            out[0] = static_cast<OutCharT>(u);
            return 1;
        }
        else {
            return 0;
        }
    }

    template <char_compatible OutCharT, std::size_t N>
    [[nodiscard]] v_always_inline std::uint32_t put_char_to_array(unicode_type cp, OutCharT(&out)[N]) {
        const std::uint32_t need = encoded_units_needed<OutCharT>(cp);
        static_assert(N >= 4);
        if (need > N) return 0;
        return impl_put_char<OutCharT>(cp, out);
    }
}

r2_end_