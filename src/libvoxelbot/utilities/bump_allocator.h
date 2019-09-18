#pragma once
#include <memory>

// A simple bump allocator
// Much faster than new or shared pointers

template <class T, int BlockSize = 1024>
struct BumpAllocator {
private:
    // const size_t BlockSize = 128;
    std::vector<T*> blocks;
    std::vector<T*> freeBlocks;
    size_t currentBlockCount = 0;
    T* currentBlock = nullptr;

    void grow() {
        if (currentBlock != nullptr) blocks.push_back(currentBlock);

        currentBlockCount = 0;
        if (!freeBlocks.empty()) {
            currentBlock = *freeBlocks.rbegin();
            freeBlocks.pop_back();
        } else {
            currentBlock = (T*)malloc(sizeof(T) * BlockSize);
            assert(currentBlock != nullptr);
        }
    }

public:
    BumpAllocator() {
        grow();
    }

    // Do not allow delete
    BumpAllocator(const BumpAllocator& x) = delete;
    
    // Allow move
    BumpAllocator(BumpAllocator&& x) = default;

    ~BumpAllocator() {
        for (auto block : blocks) {
            for (size_t i = 0; i < BlockSize; i++) (block + i)->~T();
            free(block);
        }
        for (auto block : freeBlocks) {
            free(block);
        }
        for (size_t i = 0; i < currentBlockCount; i++) (currentBlock + i)->~T();
        free(currentBlock);
        currentBlock = nullptr;
    }

    template <class... Args>
    T* allocate(Args&&... args) {
        if (currentBlockCount >= BlockSize) grow();
        T* ret = currentBlock + currentBlockCount;
        // *ret = T(std::forward<Args>(args)...);
        // Placement new. Constructs the new object at the ret pointer's location
        new(ret) T(std::forward<Args>(args)...);
        currentBlockCount++;
        return ret;
    }

    void clear() {
        for (auto block : blocks) {
            for (size_t i = 0; i < BlockSize; i++) (block + i)->~T();
        }

        for (auto block : blocks) freeBlocks.push_back(block);
        blocks.clear();

        for (size_t i = 0; i < currentBlockCount; i++) (currentBlock + i)->~T();
        currentBlockCount = 0;
    }

    size_t size() {
        return blocks.size() * BlockSize + currentBlockCount;
    }
};
