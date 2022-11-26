/*
 * Copyright (C) 2021 hurricane <l@stdb.io>. All rights reserved.
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

#include <boost/assert.hpp>
#include <boost/stacktrace.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "flags.hpp"

#ifndef __cplusplus
#error "STDB just support c++ compiler"
#else
#endif

#if STDB_ASSERT_LEVEL < STDB_ASSERT_NONE
namespace boost {
inline void assertion_failed_msg(char const* expr, char const* msg, char const* function, char const* file,
                                 int64_t line) {
    std::cerr << "Expression '" << expr << "' is false in function '" << function << "' of " << file
              << " in line : " << line << (msg != nullptr ? msg : "<...>") << ".\n"
              << "Backtrace:\n"
              << boost::stacktrace::stacktrace() << '\n';
    std::abort();
}

inline void assertion_failed(char const* expr, char const* function, char const* file, int64_t line) {
    ::boost::assertion_failed_msg(expr, nullptr, function, file, line);
}
}  // namespace boost

#include <fmt/core.h>

#include <atomic>
#include <boost/stacktrace/stacktrace_fwd.hpp>
#include <iostream>

template <std::size_t N>
constexpr auto get_function_name(const char (&funcName)[N]) {
    return std::string_view(funcName, N);
}

[[gnu::always_inline]] inline void stdb_print_assert(char const* expr, std::string_view msg, std::string_view function,
                                                     std::string_view file, int64_t line) {
    fmt::print(stderr, "Expression=[{}] is false in function=[{}] of location=[{}:{}]  msg=[{}].\n", expr, function,
               file, line, !msg.empty() ? msg : "<...>");
    auto traceInfo = boost::stacktrace::stacktrace();
    if (!traceInfo.empty()) {
        int index = 0;
        for (const auto& item : traceInfo) {
            if (!item.empty()) {
                fmt::print(stderr, "#[{}] Function:[{}] File:[{}:{}] \n", index, item.name(), item.source_file(),
                           item.source_line());
            } else {
                fmt::print(stderr, "Do not get traceInfo [{}] \n", __LINE__);
            }
            ++index;
        }
    } else {
        fmt::print(stderr, "Do not get traceInfo [{}] \n", __LINE__);
    }
    std::terminate();
}
#define STDB_RELEASE_ASSERT_MSG(expr, msg)                                                          \
    {                                                                                               \
        if (!(expr)) {                                                                              \
            stdb_print_assert(#expr, std::string_view(msg), get_function_name(__PRETTY_FUNCTION__), \
                              std::string(__FILE__), __LINE__);                                     \
        }                                                                                           \
    }

#define STDB_RELEASE_ASSERT(expr) STDB_RELEASE_ASSERT_MSG(expr, "")
#else
#define STDB_RELEASE_ASSERT(expr) (void(0))
#define STDB_RELEASE_ASSERT_MSG(expr, msg) (void(0))
#endif  // STDB_ASSERT_LEVEL < STDB_ASSERT_NONE

#if STDB_ASSERT_LEVEL < STDB_ASSERT_RELEASE

#define STDB_ASSERT_MSG(expr, msg) STDB_RELEASE_ASSERT_MSG(expr, msg)
#define STDB_ASSERT(expr) STDB_RELEASE_ASSERT(expr)

#else

#define STDB_ASSERT_MSG(expr, msg) (void(0))
#define STDB_ASSERT(expr) (void(0))

#endif  // STDB_ASSERT_LEVEL < STDB_ASSERT_RELEASE
