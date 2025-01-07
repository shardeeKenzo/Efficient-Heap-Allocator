#include <iostream>
#include <cstddef>
#include <string>

constexpr std::size_t MAX_BUCKETS = 9; // up to 1024 bytes
constexpr std::size_t POOL_SIZE = 1024 * 1024 * 10; // 10 mb fixed-size

char memory_pool[POOL_SIZE]; // 10 mb memory pool

struct BlockHeader {
    std::size_t size;   
    bool is_free;
    BlockHeader* next;
};

struct Bucket {
    std::size_t block_size;
    std::size_t total_blocks;
    std::size_t free_blocks;
    BlockHeader* head;
    Bucket* next;
};


//initialize memory pool using segregated lists with recursion
void init_memory_pool_recursively(
    std::size_t size,
    std::size_t total_blocks,
    std::size_t current_level,
    std::size_t max_levels,
    char* current_memory_pool,
    Bucket*& current_bucket_ptr,
    char* pool_start
) {

    if (current_level >= max_levels || (current_memory_pool + sizeof(Bucket)) >= (pool_start + POOL_SIZE)) {
        if (current_bucket_ptr != nullptr) {
            current_bucket_ptr->next = nullptr;
            std::cout << "Reached base case at level " << current_level << ". Setting next to nullptr.\n";
        }
        return;
    }

    Bucket* init_bucket = reinterpret_cast<Bucket*>(current_memory_pool);
    BlockHeader* init_block = reinterpret_cast<BlockHeader*>(init_bucket + 1);

    init_bucket->block_size = size;
    init_bucket->total_blocks = total_blocks;
    init_bucket->free_blocks = total_blocks;
    init_bucket->head = init_block;
    init_bucket->next = nullptr;

    BlockHeader* curr_block = init_block;
    for (std::size_t i = 0; i < init_bucket->total_blocks - 1; ++i) {
        char* next_block_address = reinterpret_cast<char*>(curr_block) + sizeof(BlockHeader) + init_bucket->block_size;
        BlockHeader* next_block = reinterpret_cast<BlockHeader*>(next_block_address);
        curr_block->next = next_block;
        curr_block = next_block;
    }
    curr_block->next = nullptr;

    curr_block = init_block;
    for (std::size_t i = 0; i < init_bucket->total_blocks; ++i) {
        curr_block->size = init_bucket->block_size;
        curr_block->is_free = true;
        if (curr_block->next != nullptr) {
            curr_block = curr_block->next;
        }
    }

    std::size_t current_bucket_size = sizeof(Bucket) + ((sizeof(BlockHeader) + init_bucket->block_size) * total_blocks);
    char* next_memory_pool = current_memory_pool + current_bucket_size;

    std::size_t required_memory_for_next = sizeof(Bucket) + (size * 2 + sizeof(BlockHeader)) * (total_blocks * 2);
    if (next_memory_pool + required_memory_for_next > pool_start + POOL_SIZE) {
        init_bucket->next = nullptr;
        return;
    }

    init_memory_pool_recursively(
        size * 2,
        total_blocks * 2,
        current_level + 1,
        max_levels,
        next_memory_pool,
        init_bucket->next,
        pool_start
    );

    current_bucket_ptr = init_bucket;
}

void initialize_memory_pool() {
    std::size_t initial_size = sizeof(int);     
    std::size_t initial_total_blocks = 2;   
    std::size_t current_level = 0;
    std::size_t max_levels = MAX_BUCKETS;      
    char* starting_memory_pool = memory_pool;  

    Bucket* head_bucket = nullptr;

    init_memory_pool_recursively(
        initial_size,
        initial_total_blocks,
        current_level,
        max_levels,
        starting_memory_pool,
        head_bucket,   
        memory_pool    
    );

}

std::size_t calculate_offset(void* ptr) {
    return reinterpret_cast<char*>(ptr) - memory_pool;
}

// enhanced print_memory_pool function
void print_memory_pool() {
    Bucket* current_bucket = reinterpret_cast<Bucket*>(memory_pool);
    std::size_t bucket_index = 0;

    std::cout << "\n=== Memory Pool Buckets ===\n";

    while (current_bucket) {
        std::size_t bucket_offset = calculate_offset(current_bucket);

        std::cout << "Bucket #" << bucket_index << ":\n";
        std::cout << "  [Header at offset " << bucket_offset << "]\n";
        std::cout << "  Block Size     : " << current_bucket->block_size << " bytes\n";
        std::cout << "  Total Blocks   : " << current_bucket->total_blocks << "\n";
        std::cout << "  Free Blocks    : " << current_bucket->free_blocks << "\n";
        std::cout << "  Head of Free List: " << current_bucket->head << "\n";
        std::cout << "  Next Bucket    : " << current_bucket->next << "\n";

        BlockHeader* current_block = current_bucket->head;
        std::size_t block_index = 0;

        std::cout << "  --- Free List Blocks ---\n";
        while (current_block) {
            std::size_t block_offset = calculate_offset(current_block);
            std::size_t end_offset = block_offset + sizeof(BlockHeader) + current_bucket->block_size;

            std::cout << "    Block #" << block_index << ":\n";
            std::cout << "      [Header at offset " << block_offset << "]\n";
            std::cout << "      Size     : " << current_block->size << " bytes\n";
            std::cout << "      Is Free  : " << std::boolalpha << current_block->is_free << "\n";
            std::cout << "      Next Block: " << current_block->next << "\n";
            std::cout << "      End Offset: " << end_offset << "\n";

            current_block = current_block->next;
            ++block_index;
        }
        std::cout << "  -------------------------\n\n";

        current_bucket = current_bucket->next;
        ++bucket_index;
    }

    std::cout << "===========================\n\n";
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
    initialize_memory_pool();
    print_memory_pool();
    /*init_memory_pool();
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
    */



    return 0;
}