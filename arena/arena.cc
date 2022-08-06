/*
 * Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
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

#include "arena.hpp"

#include <algorithm>
#include <limits>

using stdb::memory::Arena;

Arena::Block::Block(uint64_t size, Block* prev) : _prev(prev), _pos(kBlockHeaderSize), _size(size), _limit(size) {}

// generate a new memory Block.
auto Arena::newBlock(uint64_t min_bytes, Block* prev_block) noexcept -> Arena::Block* {
    uint64_t required_bytes = min_bytes + kBlockHeaderSize;
    uint64_t size = 0;

    // verify not overflow, with glog
    // STDB_CHECK_LE(min_bytes, std::numeric_limits<uint64_t>::max() - kBlockHeaderSize);
    if (min_bytes > std::numeric_limits<uint64_t>::max() - kBlockHeaderSize) {
        fmt::print(stderr, "newBlock need too many min_bytes : {}, it add kBlockHeaderSize more than uint64_t max.",
                   min_bytes);
    }

    if (prev_block != nullptr) {
        [[likely]] {
            // not the first block "New" action.
            if (required_bytes <= _options.normal_block_size) {
                size = _options.normal_block_size;
            } else if (required_bytes <= _options.huge_block_size / kThresholdHuge) {
                size = align::AlignUp(min_bytes, _options.normal_block_size);
            } else if ((required_bytes > _options.huge_block_size / kThresholdHuge) &&
                       (required_bytes <= _options.huge_block_size)) {
                size = _options.huge_block_size;
            }
            // for the more than huge_block_size size
            // will be handle out of the code scope
            // by now, the size remains to be 0.
        }
    } else {
        // the size may be insuffcient than the required.
        size = _options.suggested_initblock_size;
    }

    // NOTICE: when the size will be change to required_bytes?
    // #1. larger than huge on second or older block.
    // #2. larger than suggested_initblock_size on first block.
    // on both of them, the block will be monoplolized.
    //
    // if size is insuffcient, make it suffcient
    size = std::max(size, required_bytes);

    // allocate the memory by the block_alloc function.
    // no AlignUpTo8 need, because
    // normal_block_size and huge_block_size should be power of 2.
    // if the size over the huge_block_size, the block will be monoplolized.
    void* mem = _options.block_alloc(size);
    // if mem == nullptr, means no memory available for current os status.
    // if mem == nullptr, placement new will trigger a segmentfault
    if (mem == nullptr) {
        [[unlikely]] return nullptr;
    }

    // call the on_arena_newblock callback
    // if on_arena_newblock is nullptr, block num counting is a useless process, so avoid it.
    if (_options.on_arena_newblock != nullptr) {
        // count the blk num
        uint64_t blk_num = 0;
        for (Block* prev = prev_block; prev != nullptr; prev = prev->prev(), ++blk_num) {
        }

        _options.on_arena_newblock(blk_num, size, _cookie);
    }

    auto* b = new (mem) Block(size, prev_block);
    _space_allocated += size;
    return b;
}

void Arena::Block::Reset() noexcept {
    // run all cleanups first
    run_cleanups();
    _pos = kBlockHeaderSize;
    _limit = _size;
}

// if return nullptr means failure
auto Arena::allocateAligned(uint64_t bytes) noexcept -> char* {
    uint64_t needed = align_size(bytes);
    if (need_create_new_block(needed)) {
        [[unlikely]] {
            Block* curr = newBlock(needed, _last_block);
            if (curr != nullptr) {
                [[likely]] _last_block = curr;
            } else {
                return nullptr;
            }
        }
    }
    char* result = _last_block->alloc(needed);
    // make sure aligned
    STDB_ASSERT((reinterpret_cast<uint64_t>(result) & 7UL) == 0);
    return result;
}
