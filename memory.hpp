/*
 * Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
 */
/* +------------------------------------------------------------------+
   |                                                                  |
   |                                                                  |
   |  #####                                                           |
   | #     # #####   ##   #####  #####  # #    # ######  ####   ####  |
   | #         #    #  #  #    # #    # # ##   # #      #      #      |
   |  #####    #   #    # #    # #    # # # #  # #####   ####   ####  |
   |       #   #   ###### #####  #####  # #  # # #           #      # |
   | #     #   #   #    # #   #  #   #  # #   ## #      #    # #    # |
   |  #####    #   #    # #    # #    # # #    # ######  ####   ####  |
   |                                                                  |
   |                                                                  |
   +------------------------------------------------------------------+
*/

#ifndef MEMORY_MEMORY_HPP_
#define MEMORY_MEMORY_HPP_

#include <unistd.h>

namespace stdb {
namespace memory {

class memblock {
 public:
  memblock() {}
  // prepare the data in the memroy.
  virtual bool load() = 0;

  // clean the memory
  virtual void unload() = 0;

  // return the size of file.
  virtual uint64_t size() const = 0;

  // return the size of allocated memory.
  virtual uint64_t allocated() const = 0;

  // return the fd's filename is the fs.
  virtual const char* name() const = 0;

  // start use the memblock: record the using to reference counting.
  // and return the use ref count.
  virtual int64_t begin_use() = 0;

  // end use of the memblock: record the dereference.
  // and return the use ref count.
  virtual int64_t end_use() = 0;

  virtual char* ptr() = 0;

  // get the count of the usage.
  virtual int64_t use_count() const = 0;

  virtual ~memblock() {}
};

}  // namespace memory
}  // namespace stdb

#endif  // MEMORY_MEMORY_HPP_
