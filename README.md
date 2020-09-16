# libArena 

Arena is a memory allocation library inspired by google chrome project. It use many codes from google's opensource projects.

Arena's purpose is that making heap memory layout alligned to OS memory page, then it get some benefits:

- More gathered heap layout, better data locality.  

- No need to call memory-free functions, because Arena destroy page in single call.

- Object lifecycle management, associate objects will be destroy in one single point.

- With power of C++ 20, STL's standard contains could use Arena to optimize memory layout.

  > C++ program's standard contain's internal memory layout hasn't best locality because STL's default Allocator make memory layout be scattered.

## Working with Mallocs

Arena could be use with tcmalloc, jemalloc, or other malloc implementations.

with [Seastar](https://github.com/scylladb/seastar), Arena can alow works well, now you get a share-nothing Arena.

