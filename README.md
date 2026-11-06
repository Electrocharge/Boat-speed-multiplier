# Boat Speed (LeviLaunchroid native mod)
1. Create a GitHub repo from this folder (phone is fine).
2. Actions tab -> "Build boat-speed .levipack" -> Run workflow. Paste your MediaFire URL to also get `signature_report.txt`.
3. Download artifacts: `boat-speed.levipack` + signature report.
4. Pick the right function from the report, paste its `sig:` into `kBoatSpeedSignature` in `src/BoatSpeed.cpp`, rerun, install the .levipack in LeviLaunchroid.

## Layout
```
manifest.json            -> goes at the root of the .levipack
CMakeLists.txt
src/BoatSpeed.cpp
third_party/preloader-android/   (headers + lib, bundled so nothing else is needed)
tools/find_signatures.py         (string-xref signature finder)
tools/find_boat_vtable.py        (finds Boat RTTI/vtable in your libminecraftpe.so)
.github/workflows/build.yml
```
Local build (needs Android NDK + CMake):
`cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release && cmake --build build`
then `zip -j boat-speed.levipack manifest.json build/out/arm64-v8a/libboat_speed_mod.so`.
(The CMake output dir is `build/out/arm64-v8a/`; the workflow finds it with `find`.)

## Signature without recompiling
Leave `kBoatSpeedSignature` empty and put one signature line in `boat_signature.txt` inside the mod's config folder.

## Findings in the uploaded libminecraftpe.so (aarch64)
- Boat RTTI typeinfo 0x13d571c8, primary vtable entries start 0x13d56768; ChestBoat typeinfo 0x13d571e0.
- No Boat/Paddle dynamic symbols exist (stripped), so the movement function is NOT identified yet.
- Boat registration functions (not speed) have unique signatures, e.g. `minecraft:boat` at 0x88d24b4.
