#include <mutex>
#include <shared_mutex>
#include <utility>
#include <type_traits>

class upgrade_mutex {
  template <bool shared>
  class lock {
    template<bool> friend class lock;
    friend class upgrade_mutex;
  public:
    lock() noexcept = default;
    ~lock() {
      reset();
    }
    lock(const lock&) = delete;
    lock& operator=(const lock&) = delete;
    lock(lock&& other) noexcept
      : pmutex(std::exchange(other.pmutex, nullptr)) {}
    lock& operator=(lock&& other) {
      if (this != &other) {
        reset();
        pmutex = std::exchange(other.pmutex, nullptr);
      }
      return *this;
    }

    void reset() {
      if (pmutex) {
        shared ? pmutex->core.unlock_shared() : pmutex->core.unlock();
        pmutex = nullptr;
      }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return pmutex;
    }

    template <typename T = void, typename = std::enable_if_t<shared && std::is_same_v<T, void>>>
    [[nodiscard]] lock<false> upgrade() {
      if (!pmutex) return {};
      std::unique_lock<std::mutex> try_lock_barrier(pmutex->barrier, std::try_to_lock);
      if (!try_lock_barrier) return {};
      pmutex->core.unlock_shared();
      pmutex->core.lock();
      return std::exchange(pmutex, nullptr);
    }
  
  private:
    lock(upgrade_mutex* pmutex) noexcept
      : pmutex(pmutex) {}

    upgrade_mutex* pmutex{};
  };

public:
  using shared_lock = lock<true>;
  using unique_lock = lock<false>;

  [[nodiscard]] shared_lock lock_shared() {
    core.lock_shared();
    return this;
  }

  [[nodiscard]] unique_lock lock_unique() {
    std::unique_lock<std::mutex> lock_barrier(barrier);
    core.lock();
    return this;
  }

private:
  std::mutex barrier;
  std::shared_mutex core;
};
