# libArena

Arena is a memory allocation library inspired by google chrome project. It use many codes borrowed from google's opensource projects.

### Purpose
Arena's purpose is that making heap memory layout alligned to OS/hardware memory-paging function, then the user could get below benefits:

- More gathered heap layout, better data locality.

- All memory block is aligned to CPU's wordsize, cacheline will never be polluted.

- No need to call delete/free functions, because Arena destroy a memblock/memblock-list in a single call.

- Object lifecycle management, associated objects will be destroy in one single point.

- With power of C++ 20, STL's standard contains could use Arena to optimize memory layout.

  > C++ program's standard contain's internal memory layout hasn't best locality because STL's default Allocator make memory layout be scattered.
### Overhead
Arena use some padding or to make cpu cacheline more clearly, but memory overhead will be higher.

### Working with Mallocs

Arena could be use with tcmalloc, jemalloc, or other malloc implementations.

with [Seastar](https://github.com/scylladb/seastar), Arena can also work, now you get a shared-nothing version Arena.
