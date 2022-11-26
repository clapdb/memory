
#ifdef BOOST_STACKTRACE_USE_BACKTRACE
#include "hilbert/assert.hpp"
#define Assert(msg) STDB_ASSERT(msg)
#define AssertMsg(expr, msg) STDB_ASSERT_MSG(expr, msg)

#else

#include <cassert>
#define Assert(expr) assert(expr)
#define AssertMsg(expr, msg) assert((expr) && msg)
#endif
