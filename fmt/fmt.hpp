#pragma once

#include <gmp.h>
#include <gmpxx.h>
#include <fmt/format.h>
#include <fmt/args.h>

struct custom_type {
    void *ptr;
};

template<typename Char>
using lookup_func = std::function<std::string(void *value, fmt::detail::dynamic_format_specs<Char>)>;

template<typename Char>
using lookup_type = std::unordered_map<std::string, lookup_func<Char>>;

template<typename Char>
static lookup_type<Char> lookup_map{
    {
        "mpz_t", [](void *value, fmt::detail::dynamic_format_specs<Char> spec) {
            int base = 10;
            if (spec.type == fmt::presentation_type::hex_lower or spec.type == fmt::presentation_type::hex_upper) {
                base = 16;
            }

            mpz_t *n = static_cast<mpz_t*>(value);
            std::string str{mpz_get_str(NULL, base, *n)};
            return str;
        }
    },
    {
        "d", [](void *value, fmt::detail::dynamic_format_specs<Char> spec) {
            return std::to_string(reinterpret_cast<int64_t>(value));
        }
    },
    {
        "u", [](void *value, fmt::detail::dynamic_format_specs<Char> spec) {
            return std::to_string(reinterpret_cast<uint64_t>(value));
        }
    },
    {
        "s", [](void *value, fmt::detail::dynamic_format_specs<Char> spec) {
            return std::string{static_cast<char*>(value)};
        }
    },
};

template<typename Char>
struct fmt::formatter<custom_type, Char> : fmt::formatter<std::string_view> {
    mutable std::string m_type;
    fmt::detail::dynamic_format_specs<Char> spec;

    template<typename Context>
    constexpr auto parse(Context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin(), end = ctx.end();
        m_type = "";

        // Scan until we either reach end of string, or we find ':' or '}'.
        while (it != ctx.end() and *it != '}' and *it != ':') {
            m_type += *it;
            ++it;
        }

        // If we found ':', advance iterator. The rest of the string is now
        // passed as a format specifier.
        if (it != ctx.end() and *it == ':') {
            fmt::detail::dynamic_specs_handler<Context> setter(this->spec, ctx);
            it = fmt::detail::parse_format_specs(++it, end, setter);
        }

        return it;
    }

    template<typename FormatCtx>
    auto format(const custom_type& value, FormatCtx& ctx) {
        if (lookup_map<Char>.count(m_type) == 0) {
            std::string fmt_out = "{:"+m_type+"}";
            std::cout << fmt_out << std::endl;
            return fmt::format_to(ctx.out(), fmt_out, value.ptr);
        }
        else {
            auto f = lookup_map<Char>[m_type];
            return fmt::format_to(ctx.out(), "{}", f(value.ptr, this->spec));
        }
    }
};
