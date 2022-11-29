/*
 * Copyright (C) 2022 hurricane <l@stdb.io>. All rights reserved.
 */
/**
 +------------------------------------------------------------------------------+
|                                                                              | 
|                                                                              | 
|                    ..######..########.########..########.                    | 
|                    .##....##....##....##.....##.##.....##                    | 
|                    .##..........##....##.....##.##.....##                    | 
|                    ..######.....##....##.....##.########.                    | 
|                    .......##....##....##.....##.##.....##                    | 
|                    .##....##....##....##.....##.##.....##                    | 
|                    ..######.....##....########..########.                    | 
|                                                                              | 
|                                                                              | 
|                                                                              | 
+------------------------------------------------------------------------------+ 
*/

#pragma once

#ifdef BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/stacktrace.hpp>
#include <fmt/core.h>

template <std::size_t N>
constexpr auto function_name(const char (&funcName)[N]) {
    return std::string_view(funcName, N);
}

[[gnu::always_inline]] inline void print_assert(char const* expr, std::string_view msg, std::string_view function,
                                                     std::string_view file, int64_t line) {
    auto traceInfo = boost::stacktrace::stacktrace{};
    fmt::print(stderr, "Expression=[{}] is false in function=[{}] of location=[{}:{}]  msg=[{}].\n", expr, function,
               file, line, !msg.empty() ? msg : "<...>");
    if (!traceInfo.empty()) {
        int index = 0;
        for (const auto& item : traceInfo) {
            if (!item.empty()) {
                fmt::print(stderr, "#[{}] Function:[{}] File:[{}:{}] \n", index, item.name(), item.source_file(),
                           item.source_line());
            } else {
                fmt::print(stderr, "Do not get traceInfo [{}] \n", line);
            }
            ++index;
        }
    } else {
        fmt::print(stderr, "Do not get traceInfo [{}] \n", line);
    }
    std::terminate();
}
#define AssertMsg(expr, msg)                                                          \
    {                                                                                               \
        if (!(expr)) {                                                                              \
            print_assert(#expr, std::string_view(msg), function_name(__PRETTY_FUNCTION__),          \
                              std::string(__FILE__), __LINE__);                                     \
        }                                                                                           \
    }

#define Assert(expr) AssertMsg(expr, "")

#else
#include <cassert>
#define Assert(expr) assert(expr)
#endif
