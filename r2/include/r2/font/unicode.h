#pragma once
#include <r2/renderer_definitions.h>


r2_begin_

namespace unicode {
    inline constexpr int codepoint_max = 0x10FFFF;
    inline constexpr int codepoint_invalid = 0xFFFD;

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

    template <typename CharT>
        requires char_compatible<CharT>
    v_always_inline unicode_type impl_get_char(const CharT* c, std::uint32_t length, std::uint32_t& pos)
    {
        if (pos >= length)
            return static_cast<unicode_type>(codepoint_invalid);

        if constexpr (sizeof(CharT) == 1) {
            // UTF-8
            const std::uint8_t lead = static_cast<std::uint8_t>(c[0]);

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
                s[1] = (pos + 1u < length) ? static_cast<std::uint8_t>(c[1]) : 0u;
                s[2] = (pos + 2u < length) ? static_cast<std::uint8_t>(c[2]) : 0u;
                s[3] = (pos + 3u < length) ? static_cast<std::uint8_t>(c[3]) : 0u;

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
        else if constexpr (sizeof(CharT) == 2) {
            // UTF-16
            char16_t w1 = static_cast<char16_t>(c[0]);

            if (w1 < 0xD800 || w1 > 0xDFFF) {
                pos += 1u;
                return static_cast<unicode_type>(w1);
            }

            if (w1 >= 0xD800u && w1 <= 0xDBFFu) {
                if (pos + 1u < length) {
                    char16_t w2 = static_cast<char16_t>(c[1]);
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
        else if constexpr (sizeof(CharT) == 4) {
            // UTF-32
            unicode_type ch = static_cast<unicode_type>(c[0]);
            if (static_cast<std::uint32_t>(ch) > codepoint_max) {
                ch = static_cast<unicode_type>(codepoint_invalid);
            }

            pos += 1u;
            return ch;
        }
        else
            throw 0;
    }

    // [] operator returns reference
    template <typename S>
    inline constexpr bool index_returns_ref_v =
        std::is_lvalue_reference_v<decltype(std::declval<S&>()[0])>;

    template <string_like String>
    v_always_inline unicode_type get_char_auto(const String& str, std::uint32_t length, std::uint32_t& pos) {
        using char_t = get_char_t<String>;

        assert(pos < length);

        unicode_type cp;

        if constexpr (index_returns_ref_v<String>) {
            cp = impl_get_char<char_t>(&str[pos], length, pos);
        }
        else {
            std::uint32_t remaining = length - pos;

            constexpr std::uint32_t max_units = sizeof(unicode_type) / sizeof(char_t);

            std::uint32_t to_read = (std::min)(remaining, max_units);

            char_t buf[max_units] = {};
            for (std::uint32_t i = 0u; i < to_read; ++i) {
                buf[i] = static_cast<char_t>(str[static_cast<std::size_t>(pos + i)]);
            }

            std::uint32_t local_pos = 0;
            cp = impl_get_char<char_t>(buf, to_read, local_pos);
            pos += local_pos;
        }

        return cp;
    }
}

r2_end_