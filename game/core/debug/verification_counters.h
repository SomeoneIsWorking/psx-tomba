#pragma once

#include "cfg.h"
#include "core.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>
#include <type_traits>

namespace tomba {

struct VerificationCounter {
  long nMatch = 0;
  long nMismatch = 0;
};

struct VerificationCounters {
  VerificationCounter bit;
  VerificationCounter bit868;
  VerificationCounter backgroundScene;
  VerificationCounter inventory;
  VerificationCounter sceneTransition;
  VerificationCounter substateSwap;
  VerificationCounter animation;
  VerificationCounter objectTable;
  VerificationCounter despawn;
  VerificationCounter placement;
  VerificationCounter spawnParent;

  void bind(Core &core) {
    core_ = &core;
  }

  bool on(const char *channel) const {
    return cfg_dbg(channel);
  }

  unsigned char *ram0() {
    return ram0_.data();
  }

  unsigned char *ramN() {
    return ramN_.data();
  }

  template <typename Function> void run(Function function, std::uint32_t, const char *channel, bool requested) {
    if (!core_) {
      lucent::error("verification", "verification workspace was not bound to a Core");
      std::abort();
    }
    if (requested) {
      lucent::error("verification",
                    "'{}' used the retired inline comparison harness; use the external dynarec differential harness",
                    channel);
      std::abort();
    }
    if constexpr (std::is_void_v<std::invoke_result_t<Function, Core *>>) {
      function(core_);
    } else {
      core_->r[2] = static_cast<std::uint32_t>(function(core_));
    }
  }

private:
  Core *core_ = nullptr;
  std::array<unsigned char, 0x200000> ram0_{};
  std::array<unsigned char, 0x200000> ramN_{};
};

} // namespace tomba
