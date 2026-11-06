// Boat Speed: Mod Menu slider (1-100) that multiplies boat speed.
// Slider value 1 = vanilla (x1), 100 = x100.
//
// STATUS: the menu + hook plumbing is complete. kBoatSpeedSignature is EMPTY
// until you run tools/find_signatures.py against YOUR libminecraftpe.so and
// confirm which function returns the boat speed. Empty = mod loads but does
// not hook anything (safe, no crash).
#include <algorithm>
#include <atomic>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

namespace {

// Byte pattern, e.g. "FD 7B ?? A9 ..." ('?' = wildcard). Fill from the tool.
constexpr const char *kBoatSpeedSignature = "";

constexpr const char *kModuleId = "boat_speed.module";
constexpr const char *kSpeedKey = "boatSpeed";

std::atomic<int> gSpeed{1};

// ASSUMPTION: the target is a float-returning speed getter taking `this`.
// Confirm in a disassembler; change this type/detour if the real function differs.
using SpeedFn = float (*)(void *);
SpeedFn gOriginal = nullptr;

float boatSpeedDetour(void *self) {
  const float base = gOriginal ? gOriginal(self) : 0.0f;
  return base * static_cast<float>(gSpeed.load(std::memory_order_relaxed));
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

    // Signature: compiled-in value wins; otherwise read <configDir>/boat_signature.txt
    // so you can try candidates on the phone without recompiling.
    std::string sig = kBoatSpeedSignature;
    if (sig.empty()) {
      std::ifstream in(mSelf.getConfigDir() / "boat_signature.txt");
      std::stringstream ss;
      if (in) ss << in.rdbuf();
      sig = ss.str();
      while (!sig.empty() && (sig.back() == '\n' || sig.back() == '\r' || sig.back() == ' '))
        sig.pop_back();
    }
    if (sig.empty()) {
      mSelf.getLogger().warn("No boat signature set; slider has no effect yet");
      return true;
    }
    mTarget = reinterpret_cast<void *>(
        pl::memory::resolveSignature(sig, "libminecraftpe.so"));
    if (!mTarget) {
      mSelf.getLogger().warn("Boat signature not found in this game version");
      return true;
    }
    if (pl::memory::hook(mTarget, reinterpret_cast<void *>(&boatSpeedDetour),
                         reinterpret_cast<void **>(&gOriginal)) != 0) {
      mSelf.getLogger().error("Hook install failed");
      mTarget = nullptr;
    } else {
      mSelf.getLogger().info("Boat speed hook installed");
    }
    return true;
  }

  bool disable() {
    if (mTarget) {
      pl::memory::unhook(mTarget, reinterpret_cast<void *>(&boatSpeedDetour));
      mTarget = nullptr;
    }
    pl::modmenu::unregisterModule(kModuleId);
    return true;
  }

  bool unload() { return true; }

private:
  static void onConfigChanged(std::string_view moduleId, std::string_view key,
                              std::string_view value) {
    if (moduleId != kModuleId || key != kSpeedKey) return;
    int v = 1;
    const auto r = std::from_chars(value.data(), value.data() + value.size(), v);
    if (r.ec != std::errc{}) return;
    gSpeed.store(std::clamp(v, 1, 100), std::memory_order_relaxed);
  }

  ll::mod::NativeMod &mSelf;
  void *mTarget = nullptr;
};

} // namespace

PL_REGISTER_MOD(BoatSpeedMod, BoatSpeedMod::instance())
