// Boat Speed: Mod Menu slider (1-100) that multiplies how fast boats accelerate.
// Slider 1 = vanilla, 100 = boats paddle 100x harder (top speed scales the same).
//
// How it works (Bedrock's boats are ECS-driven): BoatMoveControlSystem::boatControl<bool>
// runs once per boat per tick. Its arguments are
//   x0 StrictEntityContext&   x1 BoatMovementComponent&   x2 BoatPaddleComponent&
//   x3 StateVectorComponent&  x4 ActorRotationComponent&  x5 view/optional
// It computes   posDelta = (posDelta + paddleAccel) * friction   where
//   paddleAccel ~ 0.0138 * BoatPaddleComponent[+0x10]  (paddle force)
// so scaling the paddle force by N scales acceleration, and top speed, by N
// without destabilising the friction term. [+0x0C] is the paddle animation phase,
// which that function also advances by the force; we undo the extra advance.
//
// Offsets/signatures below were taken from libminecraftpe.so (arm64) of the game
// version you uploaded. After a Minecraft update the signatures may need redoing
// with tools/find_boat_vtable.py + the notes in README.md.
#include <algorithm>
#include <atomic>
#include <charconv>
#include <string>
#include <string_view>

#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

namespace {

// boatControl<false> and boatControl<true>: unique byte patterns ('??' = wildcard).
constexpr const char *kSigControlA =
    "28 04 40 B9 1F 61 00 71 ?? ?? ?? ?? EC 0F 1B FC EB 2B 01 6D E9 23 02 6D "
    "FD 7B 03 A9 F4 4F 04 A9 FD C3 00 91 A9 00 40 F9";
constexpr const char *kSigControlB =
    "FF 83 01 D1 EC 0B 00 FD EB AB 01 6D E9 A3 02 6D FD FB 03 A9 F5 27 00 F9 "
    "F4 4F 05 A9 FD E3 00 91 54 D0 3B D5 88 16 40 F9";

constexpr const char *kModuleId = "boat_speed.module";
constexpr const char *kSpeedKey = "boatSpeed";

constexpr std::size_t kPaddlePhaseOffset = 0x0C;
constexpr std::size_t kPaddleForceOffset = 0x10;

std::atomic<int> gSpeed{1};

using ControlFn = void (*)(void *, void *, void *, void *, void *, void *, void *, void *);
ControlFn gOrigA = nullptr;
ControlFn gOrigB = nullptr;

inline void callScaled(ControlFn orig, void *a0, void *a1, void *a2, void *a3,
                       void *a4, void *a5, void *a6, void *a7) {
  const int mult = gSpeed.load(std::memory_order_relaxed);
  if (mult <= 1 || !a2) {
    orig(a0, a1, a2, a3, a4, a5, a6, a7);
    return;
  }
  auto *base = static_cast<char *>(a2);
  auto *force = reinterpret_cast<float *>(base + kPaddleForceOffset);
  auto *phase = reinterpret_cast<float *>(base + kPaddlePhaseOffset);
  const float oldForce = *force;
  const float oldPhase = *phase;
  const float m = static_cast<float>(mult);

  *force = oldForce * m;
  orig(a0, a1, a2, a3, a4, a5, a6, a7);
  *force = oldForce;

  // Keep the paddle animation at its normal speed (skip if it wrapped around).
  const float newPhase = *phase;
  if (newPhase >= oldPhase) *phase = oldPhase + (newPhase - oldPhase) / m;
}

void detourA(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6, void *a7) {
  callScaled(gOrigA, a0, a1, a2, a3, a4, a5, a6, a7);
}
void detourB(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6, void *a7) {
  callScaled(gOrigB, a0, a1, a2, a3, a4, a5, a6, a7);
}

class BoatSpeedMod {
public:
  static BoatSpeedMod &instance() {
    static BoatSpeedMod mod;
    return mod;
  }

  BoatSpeedMod() : mSelf(*ll::mod::NativeMod::current()) {}

  bool load() { return true; }

  bool enable() {
    const bool ok =
        pl::modmenu::ModuleBuilder(kModuleId, "Boat Speed")
            .modId(mSelf.getId())
            .description("Multiplies boat speed. 1 = vanilla, 100 = x100.")
            .defaultEnabled(true)
            .config(kSpeedKey, "Boat speed multiplier",
                    pl::modmenu::ConfigType::SliderInt, "1", "1", "100")
            .onConfigChanged(&BoatSpeedMod::onConfigChanged)
            .registerModule();
    if (!ok) {
      mSelf.getLogger().error("Failed to register Mod Menu module");
      return false;
    }

    mTargetA = install(kSigControlA, reinterpret_cast<void *>(&detourA),
                       reinterpret_cast<void **>(&gOrigA), "A");
    mTargetB = install(kSigControlB, reinterpret_cast<void *>(&detourB),
                       reinterpret_cast<void **>(&gOrigB), "B");
    if (!mTargetA && !mTargetB)
      mSelf.getLogger().warn("No boat hook installed (game version changed?)");
    return true;
  }

  bool disable() {
    if (mTargetA) pl::memory::unhook(mTargetA, reinterpret_cast<void *>(&detourA));
    if (mTargetB) pl::memory::unhook(mTargetB, reinterpret_cast<void *>(&detourB));
    mTargetA = mTargetB = nullptr;
    pl::modmenu::unregisterModule(kModuleId);
    return true;
  }

  bool unload() { return true; }

private:
  void *install(const char *sig, void *detour, void **orig, const char *tag) {
    void *target = reinterpret_cast<void *>(
        pl::memory::resolveSignature(sig, "libminecraftpe.so"));
    if (!target) {
      mSelf.getLogger().warn("Boat signature {} not found", tag);
      return nullptr;
    }
    if (pl::memory::hook(target, detour, orig) != 0) {
      mSelf.getLogger().error("Hook {} failed", tag);
      return nullptr;
    }
    mSelf.getLogger().info("Boat hook {} installed", tag);
    return target;
  }

  static void onConfigChanged(std::string_view moduleId, std::string_view key,
                              std::string_view value) {
    if (moduleId != kModuleId || key != kSpeedKey) return;
    int v = 1;
    const auto r = std::from_chars(value.data(), value.data() + value.size(), v);
    if (r.ec != std::errc{}) return;
    gSpeed.store(std::clamp(v, 1, 100), std::memory_order_relaxed);
  }

  ll::mod::NativeMod &mSelf;
  void *mTargetA = nullptr;
  void *mTargetB = nullptr;
};

} // namespace

PL_REGISTER_MOD(BoatSpeedMod, BoatSpeedMod::instance())
