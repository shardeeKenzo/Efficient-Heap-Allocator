#include <iostream>
#include <cstddef>
#include <string>

constexpr std::size_t POOL_SIZE = 1024 * 1024 * 10; // 10 mb fixed-size

char memory_pool[POOL_SIZE]; // 10 mb memory pool

struct BlockHeader {
    std::size_t size;   
    bool is_free;
    BlockHeader* next;
};

//initialize memory pool
void init_memory_pool() {
    auto* init_block = reinterpret_cast<BlockHeader*>(memory_pool);
    init_block->size = POOL_SIZE - sizeof(BlockHeader);
    init_block->is_free = true;
    init_block->next = nullptr;

    std::cout << "Memory pool initialized: "
        << "Block size = " << init_block->size << " bytes\n";
}

//print data(blocks and info about them)
void print_memory_pool() {
    auto* curr = reinterpret_cast<BlockHeader*>(memory_pool);
    std::size_t block_index = 0;

    std::cout << "\n--- Memory Pool Blocks ---\n";
    while (curr) {
        // calculate offset within memory_pool
        std::size_t offset = reinterpret_cast<char*>(curr) - memory_pool;

        // calculate end address offset
        std::size_t end_offset = offset + sizeof(BlockHeader) + curr->size;

        std::cout
            << "Block #" << block_index << ": [Header at offset " << offset
            << "] | Size: " << curr->size
            << " | is_free: " << std::boolalpha << curr->is_free
            << " | next: " << curr->next
            << " | End offset: " << end_offset
            << "\n";

        curr = curr->next;
        ++block_index;
    }
    std::cout << "--------------------------\n\n";
}


//allocation for integer object(make template function to allocate any type)
template <typename T>
T* allocate_memory(const T& data) {
    auto* curr = reinterpret_cast<BlockHeader*>(memory_pool);

    //while curr is not nullptr
    while (curr) {
        //checks if current block is free + if it has enough space for allocation
        if (curr->is_free && curr->size >= sizeof(T)) {
            std::cout << "Found a free block at "
                << curr << " with size " << curr->size
                << " for an allocation of size " << sizeof(T) << "\n";
            
            //change state to not free
            curr->is_free = false;

            //points to right location of memory that will be used(right next to the block metadata)
            void* usable_memory = reinterpret_cast<void*>(curr + 1);
            //says to treat specific raw memory on this location like int* and change initial bytes to bytes that we provided(I understand it like that)
            *reinterpret_cast<T*>(usable_memory) = data;

            //calculate remaining memory
            std::size_t remaining_size = curr->size - sizeof(T) - sizeof(BlockHeader);

            //create new block if we have enough space so we can continue to use it
            if (remaining_size > sizeof(BlockHeader)) {
                auto* new_block = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(usable_memory) + sizeof(T));

                new_block->size = remaining_size;
                new_block->is_free = true;
                new_block->next = nullptr;

                curr->next = new_block;
                curr->size = sizeof(T);

                std::cout << "Splitting block. New block created at "
                    << new_block << " with size " << new_block->size << "\n";
            } else {
                // if not enough space to create a new block header, just don't split
                std::cout << "Not enough space to split block.\n";
            }
            
            std::cout << "Returning usable memory at " << usable_memory << "\n";
            return reinterpret_cast<T*>(usable_memory);
        }
        curr = curr->next;
    }

    std::cout << "No memory available" << std::endl;

    return nullptr;
}

template <typename T>
void deallocate_memory(T* ptr) {
    if (!ptr) {
        std::cout << "Attempted to deallocate a null pointer.\n";
        return;
    }

    auto* curr = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));

    if (curr->is_free) {
        std::cout << "Block at " << curr << " is already free.\n";
        return;
    }

    curr->is_free = true;
    std::cout << "Deallocated block at " << curr
        << " (size " << curr->size << ") and marked it as free.\n";

    if (curr->next && curr->next->is_free) {
        std::cout << "Coalescing block at " << curr
            << " with next free block at " << curr->next << "\n";

        curr->size += curr->next->size + sizeof(BlockHeader);
        curr->next = curr->next->next;
    }
}

int main()
{
    init_memory_pool();
    print_memory_pool();

    char letter = 'b';
    int number = 10;
    double decimal_number = 5.6;
    bool state = false;

    char* letter_ptr = allocate_memory(letter);
    print_memory_pool();
    int* number_ptr = allocate_memory(number);
    print_memory_pool();
    double* decimal_ptr = allocate_memory(decimal_number);
    print_memory_pool();
    bool* state_ptr = allocate_memory(state);
    print_memory_pool();
    int number2 = 999;
    int* number2_ptr;
    
    *state_ptr = true;
    
    if (*state_ptr == true) {
        number2_ptr = allocate_memory(number2);
    }
    else { number2_ptr = allocate_memory(200); }
    print_memory_pool();

    deallocate_memory(decimal_ptr);
    deallocate_memory(letter_ptr);
    print_memory_pool();
    deallocate_memory(number_ptr);
    print_memory_pool();
    deallocate_memory(state_ptr);
    print_memory_pool();
    deallocate_memory(number2_ptr);
    print_memory_pool();

    return 0;
}