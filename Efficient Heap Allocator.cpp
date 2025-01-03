#include <iostream>
#include <cstddef>

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
    
    std::cout << "Memory pool blocks:\n";

    while (curr) {
        std::cout << "Block at " << curr
            << " | Size: " << curr->size
            << " | Free?: " << curr->is_free << std::endl;
        curr = curr->next;
    }
}


//allocation for integer object(make template function to allocate any type)
void* allocate_memory_for_int(int data) {
    auto* curr = reinterpret_cast<BlockHeader*>(memory_pool);

    //while curr is not nullptr
    while (curr) {
        //checks if current block is free + if it has enough space for allocation
        if (curr->is_free && curr->size >= sizeof(int)) {
            
            //change state to not free
            curr->is_free = false;

            //points to right location of memory that will be used(right next to the block metadata)
            void* usable_memory = reinterpret_cast<void*>(curr + 1);
            //says to treat specific raw memory on this location like int* and change initial bytes to bytes that we provided(I understand it like that)
            *reinterpret_cast<int*>(usable_memory) = data;

            //calculate remaining memory
            std::size_t remaining_size = curr->size - sizeof(int) - sizeof(BlockHeader);

            //create new block if we have enough space so we can continue to use it
            if (remaining_size > 0) {
                auto* new_block = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(usable_memory) + sizeof(int));

                new_block->size = remaining_size;
                new_block->is_free = true;
                new_block->next = nullptr;
                
                curr->next = new_block;
                curr->size = sizeof(int);
            }
            
            return usable_memory;
        }
        curr = curr->next;
    }

    std::cout << "No memory available" << std::endl;

    return nullptr;
}

void deallocate_memory_for_int(void* int_ptr) {
    auto* curr = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(int_ptr) - sizeof(BlockHeader));

    if (curr->is_free == true) {
        std::cout << "Memory is already free" << std::endl;
        return;
    }

    curr->is_free = true;

    std::cout << "Block at " << curr << " deallocated" << std::endl;

    if (curr->next->is_free && curr->next) {
        curr->size += curr->next->size + sizeof(BlockHeader);
        curr->next = curr->next->next;
    }
}

int main()
{
    init_memory_pool();
    print_memory_pool();

    int* number = (int*)allocate_memory_for_int(3);

    print_memory_pool();
    
    int* number2 = (int*)allocate_memory_for_int(5);
    print_memory_pool();

    int* number3 = (int*)allocate_memory_for_int(7);

    deallocate_memory_for_int(number2);
    deallocate_memory_for_int(number);
    print_memory_pool();

    return 0;
}