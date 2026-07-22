#include <signal.h>

#include <any>
#include <csignal>
#include <utility>
#include <vector>

namespace sgmask {

template <typename... Args>
struct [[nodiscard]] SignalMaskGuard {
   private:
    sigset_t old_mask_;
    sigset_t current_mask_;

   public:
    SignalMaskGuard(sigset_t new_mask)
        : current_mask_(std::move(new_mask)) {
        sigprocmask(SIG_BLOCK, &current_mask_, &old_mask_);
    }

    SignalMaskGuard(const SignalMaskGuard& other) = delete;

    SignalMaskGuard& operator=(const SignalMaskGuard& other) = delete;

    ~SignalMaskGuard() {
        sigprocmask(SIG_SETMASK, &old_mask_, nullptr);
    }
};
};  // namespace sgmask