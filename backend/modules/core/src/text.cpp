// fusion core: numbers, times and paths as text (section 09, Determinism; 04.C2, 11.C5; cross-section
// items 1.20 and 3A.19).
#include <array>
#include <charconv>
#include <cmath>
#include <format>

#include <fast_float/fast_float.h>

#include "fusion/core/core.hpp"

namespace fusion::core {

// ---------------------------------------------------------------- format_number
// FuSION 1 set std::cout to fixed notation with two decimals for the whole run
// (Linux/src/main.cc:1142-1143, PRECISION in Linux/include/macros.h:84), so 0.004 printed as 0.00.
// S1.27 (fixed) and cross-section item 1.20: the shortest digits that read back exactly, laid out
// as Python's repr lays them out, so 2.0's tables read like FuSION 1's Python-written ones.
// std::to_chars gives the shortest round-trip digits without the locale (11's C++20 subset).

std::string format_number(double value) {
    if (std::isnan(value)) return "nan";
    if (std::isinf(value)) return value < 0 ? "-inf" : "inf";

    std::array<char, 64> buffer{};
    const auto converted =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
    std::string_view text(buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data()));

    std::string out;
    if (text.front() == '-') {
        out.push_back('-');
        text.remove_prefix(1);
    }
    // text is now "D[.DDD]e±XX": the digits, and the exponent of the first digit.
    const std::size_t e = text.find('e');
    std::string digits;
    for (const char c : text.substr(0, e))
        if (c != '.') digits.push_back(c);
    std::string_view exponent_text = text.substr(e + 1);
    if (exponent_text.front() == '+') exponent_text.remove_prefix(1);
    int exponent = 0;
    std::from_chars(exponent_text.data(), exponent_text.data() + exponent_text.size(), exponent);

    // Python's float_repr_style 'short' (CPython Python/pystrtod.c, format_float_short, mode 'r'):
    // with the value 0.DDD x 10^decpt, an exponent is used when decpt <= -4 or decpt > 16; it has
    // a sign and at least two digits; a fixed-notation integer keeps ".0".
    const int decpt = exponent + 1;
    const auto ndigits = static_cast<int>(digits.size());
    if (decpt <= -4 || decpt > 16) {
        out.push_back(digits[0]);
        if (ndigits > 1) out.append(".").append(digits, 1, std::string::npos);
        const int shown = decpt - 1;
        out.append(shown < 0 ? "e-" : "e+");
        const int magnitude = shown < 0 ? -shown : shown;
        if (magnitude < 10) out.push_back('0');
        out.append(std::to_string(magnitude));
    } else if (decpt <= 0) {
        out.append("0.").append(static_cast<std::size_t>(-decpt), '0').append(digits);
    } else if (decpt < ndigits) {
        out.append(digits, 0, static_cast<std::size_t>(decpt)).append(".").append(digits, static_cast<std::size_t>(decpt));
    } else {
        out.append(digits).append(static_cast<std::size_t>(decpt - ndigits), '0').append(".0");
    }
    return out;
}

// ---------------------------------------------------------------- parse_decimal
// FuSION 1 read thresholds with std::stof (Linux/src/main.cc:2644-2645) and values with
// operator>>(float&) (main.cc:2679): the longest numeric prefix, so "1,5" read as 1, "0x1p0" as 1,
// "nan" and "inf" were accepted, and a value with no numeric prefix read as 0 (S3.02 to S3.04).
// Fixed by 04.C2 and 11.C5: a token counts only when the whole of it matches
// [+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?, and fast_float converts it to the correctly
// rounded double, without the locale, the same on every platform.

namespace {

constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

bool matches_decimal_grammar(std::string_view t) noexcept {
    std::size_t i = 0;
    const std::size_t n = t.size();
    const auto digits = [&] {
        const std::size_t start = i;
        while (i < n && is_digit(t[i])) ++i;
        return i - start;
    };
    if (i < n && (t[i] == '+' || t[i] == '-')) ++i;
    const std::size_t integer_digits = digits();
    if (i < n && t[i] == '.') {
        ++i;
        const std::size_t fraction_digits = digits();
        if (integer_digits == 0 && fraction_digits == 0) return false;   // "." or "-."
    } else if (integer_digits == 0) {
        return false;
    }
    if (i < n && (t[i] == 'e' || t[i] == 'E')) {
        ++i;
        if (i < n && (t[i] == '+' || t[i] == '-')) ++i;
        if (digits() == 0) return false;
    }
    return i == n;
}

}  // namespace

Decimal parse_decimal(std::string_view token) noexcept {
    if (!matches_decimal_grammar(token)) return {DecimalStatus::not_a_number, 0.0};
    double value = 0;
    const fast_float::parse_options options(fast_float::chars_format::general |
                                            fast_float::chars_format::allow_leading_plus);
    const auto result = fast_float::from_chars_advanced(token.data(), token.data() + token.size(), value, options);
    if (result.ptr != token.data() + token.size()) return {DecimalStatus::not_a_number, 0.0};
    // fast_float reports both overflow and underflow as result_out_of_range, with infinity or zero
    // as the value; the value tells them apart, which also covers tokens with no exponent at all.
    if (std::isinf(value)) return {DecimalStatus::not_finite, 0.0};
    // Section 04, line rule 6: a nonzero value below the smallest normal double reads as 0,
    // whatever the library returns, so no subnormal ever enters a computation.
    if (std::fpclassify(value) == FP_SUBNORMAL || result.ec == std::errc::result_out_of_range)
        value = std::copysign(0.0, value);
    return {DecimalStatus::ok, value};
}

// ---------------------------------------------------------------- times and paths

std::string utc_timestamp(std::chrono::system_clock::time_point time) {
    using namespace std::chrono;
    const auto ms = floor<milliseconds>(time);
    const auto day = floor<days>(ms);
    const year_month_day date{day};
    const hh_mm_ss<milliseconds> clock{ms - day};
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", static_cast<int>(date.year()),
                       static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                       clock.hours().count(), clock.minutes().count(), clock.seconds().count(),
                       clock.subseconds().count());
}

std::string to_utf8(const std::filesystem::path& path) {
    // The generic form, with '/' on every platform (09: run.json stores paths that way). On
    // Windows a path built from a std::string would use the ANSI code page, hence char8_t here.
    const std::u8string text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path path_from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

}  // namespace fusion::core
