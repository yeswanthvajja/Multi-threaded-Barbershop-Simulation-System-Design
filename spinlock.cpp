#include <atomic>
#include <thread>

extern "C" {
    struct Spinlock {
        std::atomic_flag flag = ATOMIC_FLAG_INIT;

        void lock() {
            while (flag.test_and_set(std::memory_order_acquire)) {
            }
        }

        void unlock() {
            flag.clear(std::memory_order_release);
        }
    };

    Spinlock* create_spinlock() {
        return new Spinlock();
    }

    void destroy_spinlock(Spinlock* lock) {
        delete lock;
    }

    void spinlock_lock(Spinlock* lock) {
        lock->lock();
    }

    void spinlock_unlock(Spinlock* lock) {
        lock->unlock();
    }
}
