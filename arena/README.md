# libArena

Arena is a memory allocation library inspired by google chrome project. It use many codes borrowed from google's opensource projects.

## Purpose
Arena's purpose is that making heap memory layout alligned to OS/hardware memory-paging function, then the user could get below benefits:

- More gathered heap layout, better data locality.

- All memory block is aligned to CPU's wordsize, cacheline will never be polluted.

- No need to call delete/free functions, because Arena destroy a memblock/memblock-list in a single call.

- Object lifecycle management, associated objects will be destroy in one single point.

- With power of C++ 20, STL's standard contains could use Arena to optimize memory layout.

  > C++ program's standard contain's internal memory layout hasn't best locality because STL's default Allocator make memory layout be scattered.
## Overhead
Arena use some padding or to make cpu cacheline more clearly, but memory overhead will be higher.

## Working with Mallocs

Arena could be use with tcmalloc, jemalloc, or other malloc implementations.

with [Seastar](https://github.com/scylladb/seastar), Arena can also work, now you get a shared-nothing version Arena.

## Internal of Arena

An arena has a chain of blocks memory pool,  block is a linear continuous memory area.

### the memory layout
	                           +-------+                         +-------+
	          +------+         |Cleanup|      +------+           |Cleanup|
	          |Header|         | Area  |      |Header|           | Area  |
	          +------+         +-------+      +------+           +-------+
	          +------+-----------+---+        +-----+------------+-------+
	          |      |           |   |        |     |            |       |
	          |      |           |   |        |     |            |       |
	          |      |           |   |        <-----+ limit_ ---->       |
	       +->|      |           |   |        |     |            |       |
	       |  |      |           |   |        |     |            |       |
	       |  |      |           |   |        <-----+---- size_ -+------->
	       |  |      |           |   |        |     |            |       |
	       |  +---+--+-----------+---+        +--+--+------------+-------+
	       |      |                              |
	       |      |                              |  +------+
	<------+------+                              |  | Prev |
	       |  +----------------+                 |  +------+
	       |  | Prev = nullptr |                 |
	       |  +----------------+                 |
	       |                                     |
	       |                                     |
	       +-------------------------------------+

### Align for what?
Aligned memory will make CPU working in the best situation. Modern cpus have complicated memory and cache mechanism, and it was designed for aligned memory.
### Cleanup Area and Cleanup functions.
### Tags
Arena 约定了两个tag
1. ArenaFullManagedTag
2. ArenaManagedCreateOnlyTag

如果 class public member 里有其中任何一个 Tag，就能保证可以被 Create 构建。
如果 class public member 里有 ArenaManagedCreateOnlyTag, 那么 Arena 会跳过这个 class 的 Destructor.

** 仅仅需要一个 Tag 就 ok **
如果没有任何一个Tag，就必须是类似简单的 c struct，否则 arena.Create会失败。

## Usage Examples
### pure C like structs
c++ struct is a simple class.
它要满足以下任意一个条件就可以直接用 Create 来构建, 这里的限制是：
1. is_trivial && is_standard_layout
2. mark by ArenaFullManagedTag or ArenaManagedCreateOnlyTag

is_trivial 要求默认构造和拷贝构造必须是编译器生成的。
is_standard_layout 是要求内存布局简单, 要求所有非static member 的 access control level，没有 virtual 函数，没有非 static 的reference member，没有多重继承。

简而言之，需要一个简单的内存布局，并且可以直接算出来占用多大空间，因为arena是需要将数据分配在连续内存上的。
ArenaFullManagedTag 是程序员自己控制的，自己手动告诉Arena，可以这么Create。
```c++
/*
 * simplest struct
 * not Tags was needed, just Create it.
 * it also can be CreateArray.
 */
struct trivial {
    int b;
    char x;
};
```

```c++
struct base {
};

/*
 * derived class but still with standard_layout
 * just Create it.
 * it also can be CreateArray.
 */
class derived_standard_layout : public base {
   public:
    int x;
};
```

```c++
/*
 * it is not a trivial class,
 * can not be Create in Arena by now.
 */
class not_trival_class {
   public:
    not_trival_class(Arena& a);
   private:
};
```
```c++
/*
 * has ref member, it is not standard loyout class.
 * can not be Create in Arena By now.
 */
class not_standard_layout {
   Arena& arena;
};
```
### C++ class
class 意味着，你要首先 cstr 和 dstr了，显然不满足于 is_trivial 了，你肯定要 ArenaFullManagedTag 了，这个时候因为 class 相对复杂, 需要考虑是否要执行析构，因为有可能也被Arena管理了。
ArenaManagedCreateOnlyTag 要酌情使用。
理论上 arena 上构造的 class 不应该被copy 和 move，但是实际中我们有的时候也需要。注意几个原则就好:
1. 一个 instance 要么只能被 arena 管理，要么绝对不能，最好不要同时支持。
2. 一个 instance 如果被 copy, 一定要跨 arena，否则的话，用 clone 语义或者重新构建。
3. 一个 instance 如果被 move, 一定在同一个 arena。 TODO: 最好通过 Arena 的设计避免，但是目前看很难。
```c++
/*
 * is not_standard_layout, but has ArenaFullManagedTag,
 * can be Create in Arena, but can not be CreateArray.
 */
class not_standard_layout {
   public: 
    Arena& arena;
    ArenaFullManagedTag;
};

```
### containers
container 因为内部管理了指针并且, 并且有的时候需要 rehash 或者 double size, 所以只有两种情况可以这么做。
1. pmr 的 container，通过 memory_resource 来实现。
2. 自定义的 container, 内部持有一个 arena ref, 调用 arena allocate 和 Create 来解决。
```c++
Arena a;
std::pmr::vector(a.get_memory_resource());

```
注意上面的容器要和 arena instance 保持声明周期的一致，否则会 crash
### class with internal ptr
注意要保证 internal ptr 指向的内存生命周期要不短于 arena 本身的。
```c++
/*
 * make p point to 
 */
class with_ptr {
   public: 
    with_ptr(B* p);
    ArenaFullManagedTag;
   private: 
    B* ptr;
};

Arena arena;
// make sure bb is in arena, or long life-term than arena.
B* bb;
arena.Create<with_ptr>(bb);

// failed, because have to be trivial and standard_layout.
auto failed_ptr = arena.CreateArray<with_ptr>(100);

```
### class with internal container
class 要持有一个 arena ref 或者 memory resource
```c++
class with_container {
    with_container(Arena& arena): a(arena), mapper(a.get_memory_resource()) {};
    // for make Arena Create work
    ArenaFullManagedTag;
    // to skip dstr, otherwise the memory will be double freed.
    ArenaManagedCreateOnlyTag;
   private:
    Arena& a;
    std::pmr::vector<uint32_t, uint64_t> mapper;
    
};

Arena a;
with_contaner* p = a.Create<with_container>();

```
### class just owning arena
class 可以只是持有一个 arena ref，但是不打 ArenaFullManagedTag，这样就不会被arena构造。(因为 ref member 让这个class 不满足于 standard_layout 约束)
```cpp
/*
 * own_arena can own an arena ref, 
 * but can not be construct in arena because without ArenaFullManagedTag
 */
class own_arena {
    own_arena(Arena& arena): a(arean) { }
   private: 
    Arena& a;
};
```

### how to pass Arena ref to a managed class' constructor.
当一个 class 的 cstr 需要传入 Arena， 我们要求cstr 的第一个参数为 non-const ref of Arena, Create 会自己传 *this, 无需手动传入。
如果一个 class 的 cstr 无需传入 Arena 的引用的时候，正常写 Create 即可，类似于 make_unique。

```cpp
class arena_only_class {
    arena_only_class(Arena& arena): a(arena), ptrs(a.get_memory_resource()) {}
    ArenaManagedCreateOnlyTag;
   private:    
    Arena& a;
    B* ptr;
    std::pmr::vector<B*> ptrs;
};

Arena a;
// success, and the Create will auto passed the arena ref.
auto* x = a.Create<arena_only_class>();

// failed, will passed twice the ref.
auto* failed = a.Create<arena_only_class>(a);
```

### arena create an instance own a different arena ref.
```cpp
/*
 * can create in arena, and hold a different arena ptr;
 */
class arena_with_arena {
   public:
    arena_with_arena(Arena* a):diff_arena(a){}
    ArenaFullManagedTag;
   private:
    Arena* diff_arena;
};

```
## TODO
1. 修改一些 Arena 来自 c++ 11 时代的一些语法。
2. 给 Arena 的函数提供更清晰的注释，并自动生成文档。