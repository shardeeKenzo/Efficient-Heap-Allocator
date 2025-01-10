#include <iostream>
#include <cstddef>
#include <string>
#include <new>
#include <cassert>
#include <chrono>
#include <vector>

constexpr std::size_t MAX_BUCKETS = 9; // up to 1024 bytes (should be 9)
constexpr std::size_t POOL_SIZE = 1024 * 1024 * 4; // 4 mb fixed-size

char memory_pool[POOL_SIZE]; // 4 mb memory pool


class MemoryPool {
private:

    struct Bucket; 

    struct BlockHeader {
        std::size_t size;
        bool is_free;
        BlockHeader* next;
        Bucket* bucket_ptr;
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
                //std::cout << "Reached base case at level " << current_level << ". Setting next to nullptr.\n";
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
            curr_block->bucket_ptr = init_bucket;
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

    void fillFreeMemory(char* start_memory_pool, std::size_t max_level, std::size_t initial_total_blocks, std::size_t initial_size) {
        std::vector<int> number_of_blocks_in_each_bucket;

        auto* init_bucket = reinterpret_cast<Bucket*>(start_memory_pool);
        while (init_bucket->next != nullptr) {
            init_bucket = init_bucket->next;
            number_of_blocks_in_each_bucket.push_back(init_bucket->total_blocks);
        }

        auto* init_block = reinterpret_cast<BlockHeader*>(init_bucket + sizeof(Bucket));
        while (init_block->next != nullptr) {
            init_block = init_block->next;
        }

        std::size_t used_space = 0;

        for (int i = 0; i < number_of_blocks_in_each_bucket.size(); ++i) {
            used_space = number_of_blocks_in_each_bucket[i] * initial_size;
            initial_size *= 2;
        }

        auto* free_memory = reinterpret_cast<BlockHeader*>(init_block + sizeof(BlockHeader) + init_bucket->block_size);

        free_memory->size = POOL_SIZE - sizeof(BlockHeader) - used_space;
        free_memory->is_free = true;
        free_memory->next = nullptr;
        free_memory->bucket_ptr = nullptr;

        std::cout << free_memory->size << " bytes\n" << used_space << "\n" << used_space + free_memory->size << std::endl;
    }

    std::size_t calculate_offset(void* ptr) {
        return reinterpret_cast<char*>(ptr) - memory_pool;
    }



public:

    MemoryPool() {

    }

    void initialize_memory_pool() {
        std::size_t initial_size = sizeof(long long);
        std::size_t initial_total_blocks = 4;
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

        fillFreeMemory(memory_pool, max_levels, initial_total_blocks, initial_size);
    }

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

    template <typename T>
    T* alloc(const T& data) {
        auto* curr_bucket = reinterpret_cast<Bucket*>(memory_pool);

        while (curr_bucket != nullptr) {
            if (curr_bucket->block_size >= sizeof(T) && curr_bucket->free_blocks > 0) {
                BlockHeader* curr_block = curr_bucket->head;

                while (curr_block != nullptr) {
                    if (curr_block->is_free) {
                        /*std::cout << "Found a free block at " << curr_block
                            << " with size " << curr_block->size
                            << " for an allocation of size " << sizeof(T) << std::endl;*/

                        curr_block->is_free = false;
                        curr_block->bucket_ptr->free_blocks -= 1;

                        void* usable_memory = reinterpret_cast<void*>(curr_block + 1);

                        new (usable_memory) T(data);

                        /*if (sizeof(T) < curr_block->size) {
                            std::cout << "Internal fragmentation = "
                                << (curr_block->size - sizeof(T)) << " bytes" << std::endl;
                        }*/

                        return reinterpret_cast<T*>(usable_memory);
                    }
                    curr_block = curr_block->next;
                }
            }
            curr_bucket = curr_bucket->next;
        }

        //std::cout << "No suitable block found for the requested type." << std::endl;
        return nullptr;
    }

    template <typename T>
    void dealloc_(T* ptr) {
        if (!ptr) {
            //std::cout << "Attempted to deallocate a null pointer.\n";
            return;
        }

        auto* curr = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));

        if (curr->is_free) {
            //std::cout << "Block at " << curr << " is already free.\n";
            return;
        }

        curr->is_free = true;
        curr->bucket_ptr->free_blocks += 1;

        /*std::cout << "Deallocated block at " << curr
            << " (size " << curr->size << ") and marked it as free.\n";*/
    }
};

struct TestStruct {
    int a;
    double b;
    std::string c;

    TestStruct(int x, double y, const std::string& z) : a(x), b(y), c(z) {}
};

class Benchmark {
private:

    MemoryPool m_p{};

    struct AllocatorStats {
        std::size_t total_allocated = 0;
        std::size_t total_freed = 0;
        std::size_t current_usage = 0;
    } custom_stats, std_stats;

public:

    const AllocatorStats getCustomStat() {
        return custom_stats;
    }

    const AllocatorStats getStdStat() {
        return std_stats;
    }

    template <typename T>
    T* std_alloc(const T& data) {
        T* ptr = static_cast<T*>(::operator new(sizeof(T)));
        new (ptr) T(data);
        if (ptr) {
            std_stats.total_allocated += sizeof(T);
            std_stats.current_usage += sizeof(T);
        }
        return ptr;
    }

    template <typename T>
    T* custom_alloc(const T& data) {
        T* ptr = m_p.alloc(data);
        if (ptr) {
            custom_stats.total_allocated += sizeof(T);
            custom_stats.current_usage += sizeof(T);
        }
        return ptr;
    }

    template <typename T>
    void custom_dealloc(T* ptr) {
        m_p.dealloc_(ptr);
        if (ptr) {
            custom_stats.total_freed += sizeof(T);
            custom_stats.current_usage -= sizeof(T);
        }
    }

    template <typename T>
    void std_dealloc(T* ptr) {
        if (ptr) {
            ptr->~T();
            ::operator delete(ptr);
            std_stats.total_freed += sizeof(T);
            std_stats.current_usage -= sizeof(T);
        }
    }

    template <typename T>
    double benchmark_custom_alloc(int num_iterations) {
        auto start = std::chrono::high_resolution_clock::now();

        m_p.initialize_memory_pool();

        std::vector<T*> ptrs;
        ptrs.reserve(num_iterations);

        for (int i = 0; i < num_iterations; ++i) {
            ptrs.push_back(custom_alloc(TestStruct(i, i * 1.1, "Benchmark")));
        }

        for (int i = 0; i < num_iterations; ++i) {
            custom_dealloc(ptrs[i]);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        return elapsed.count();
    }

    template <typename T>
    double benchmark_std_alloc(int num_iterations) {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<T*> ptrs;
        ptrs.reserve(num_iterations);

        for (int i = 0; i < num_iterations; ++i) {
            ptrs.push_back(std_alloc(TestStruct(i, i * 1.1, "Benchmark")));
        }

        for (int i = 0; i < num_iterations; ++i) {
            std_dealloc(ptrs[i]);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        return elapsed.count();
    }

};


int main()
{   
    //initialize_memory_pool();

    //MemoryPool m_p {};
    Benchmark b_m {};

    const int num_iterations = 500;

    // perform benchmark for custom allocator
    double custom_time = b_m.benchmark_custom_alloc<TestStruct>(num_iterations);
    std::cout << "Custom Allocator: Allocated and deallocated " << num_iterations
        << " TestStruct objects in " << custom_time << " seconds.\n";

    // perform benchmark for standard allocator
    double std_time = b_m.benchmark_std_alloc<TestStruct>(num_iterations);
    std::cout << "Standard Allocator: Allocated and deallocated " << num_iterations
        << " TestStruct objects in " << std_time << " seconds.\n";

    // calculate and display speedup or slowdown
    if (custom_time > 0) {
        double speed_ratio = std_time / custom_time;
        std::cout << "Speed Ratio (Std / Custom): " << speed_ratio << "x\n";
    }

    std::vector<TestStruct*> custom_ptrs;
    std::vector<TestStruct*> std_ptrs;
    custom_ptrs.reserve(10);
    std_ptrs.reserve(10);

    // allocate using custom allocator
    for (int i = 0; i < 10; ++i) {
        custom_ptrs.push_back(b_m.custom_alloc(TestStruct(i, i * 1.1, "Benchmark")));
    }

    // allocate using standard allocator
    for (int i = 0; i < 10; ++i) {
        std_ptrs.push_back(b_m.std_alloc(TestStruct(i, i * 1.1, "Benchmark")));
    }

    // verify correctness for a few elements
    for (int i = 0; i < 10; ++i) {
        assert(custom_ptrs[i]->a == i && "Custom Allocator: TestStruct.a mismatch");
        assert(custom_ptrs[i]->b == i * 1.1 && "Custom Allocator: TestStruct.b mismatch");
        assert(custom_ptrs[i]->c == "Benchmark" && "Custom Allocator: TestStruct.c mismatch");

        assert(std_ptrs[i]->a == i && "Standard Allocator: TestStruct.a mismatch");
        assert(std_ptrs[i]->b == i * 1.1 && "Standard Allocator: TestStruct.b mismatch");
        assert(std_ptrs[i]->c == "Benchmark" && "Standard Allocator: TestStruct.c mismatch");
    }

    std::cout << "All correctness tests passed successfully.\n";

    // deallocate using custom allocator
    for (int i = 0; i < 10; ++i) {
        b_m.custom_dealloc(custom_ptrs[i]);
    }

    // deallocate using standard allocator
    for (int i = 0; i < 10; ++i) {
        b_m.std_dealloc(std_ptrs[i]);
    }

    // display memory usage statistics
    std::cout << "\n--- Memory Usage Statistics ---\n";
    std::cout << "Custom Allocator:\n";
    std::cout << "  Total Allocated: " << b_m.getCustomStat().total_allocated << " bytes\n";
    std::cout << "  Total Freed    : " << b_m.getCustomStat().total_freed << " bytes\n";
    std::cout << "  Current Usage  : " << b_m.getCustomStat().current_usage << " bytes\n";

    std::cout << "Standard Allocator:\n";
    std::cout << "  Total Allocated: " << b_m.getStdStat().total_allocated << " bytes\n";
    std::cout << "  Total Freed    : " << b_m.getStdStat().total_freed << " bytes\n";
    std::cout << "  Current Usage  : " << b_m.getStdStat().current_usage << " bytes\n";

    return 0;
}