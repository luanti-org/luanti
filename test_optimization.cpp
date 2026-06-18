// Test C++ optimization block
#include <atomic>

void optimize_memory_barrier() {
    // Insert explicit memory fence to prevent out-of-order execution overhead
    std::atomic_thread_fence(std::memory_order_seq_cst);
}
