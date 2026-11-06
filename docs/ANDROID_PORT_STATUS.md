# Darknes Android ARM64 port — status and engineering roadmap

**Status:** investigation / platform groundwork  
**Target:** Android on ARM64 (AArch64)  
**Upstream base:** Valve Source SDK 2013  
**Important:** this repository is Source SDK 2013, not the closed-source CS:GO codebase and not Source 2. This document does not claim that Android is already supported or that an APK can currently be built.

## Verified repository findings

The following findings were checked against the current `master` branch:

- The repository is a fork of `ValveSoftware/source-sdk-2013`.
- The upstream README documents desktop builds for Windows (Visual Studio 2022) and Linux (Podman / Steam Runtime). It does not document Android.
- `src/buildallprojects` invokes VPC with the `/linux64` target and builds the desktop Linux SDK; it is not an Android cross-compilation entry point.
- `src/public/tier0/platform.h` detects `__aarch64__` and defines `PLATFORM_ARM`, but its OS/platform selection is still based on Windows, Linux, and macOS. ARM64 CPU detection alone is not Android platform support.
- The source tree currently has Linux x86-64 libraries and binaries (for example, under `src/lib/public/linux64` and `src/lib/common/linux64`). These cannot be linked into an Android ARM64 application.
- A repository code search did not find an existing Android application/NDK build path or SDL2 Android integration. This is an initial search result, not proof that no platform-specific code exists anywhere.

## Main engineering blockers

1. **Platform layer:** introduce an explicit Android target without misidentifying Android as desktop Linux. Audit platform macros, filesystem paths, dynamic library loading, threading, timing, memory, signals, and logging.
2. **Toolchain and build graph:** add a reproducible Android NDK + CMake/Ninja cross-build for `arm64-v8a`. VPC currently generates desktop projects; determine whether to extend VPC or isolate Android-native targets behind a maintainable build layer.
3. **Native application lifecycle:** create an Android entry point and correctly handle activity pause/resume, surface recreation, orientation, safe shutdown, and asset/package paths.
4. **Rendering:** audit the current material system and OpenGL backend. Android needs a supported GLES path or a deliberate renderer port; desktop GL assumptions and extensions must not be treated as mobile-compatible.
5. **Input and UI:** add touch input, multi-touch gesture routing, virtual movement/look controls, mobile HUD scaling, and optional gamepad support. Keep mouse/keyboard behavior intact for desktop.
6. **Dependencies:** rebuild every required third-party library for Android ARM64. Existing Linux archives and Steam desktop binaries are not reusable Android libraries.
7. **Game content and licensing:** separate engine code from game content. Do not bundle proprietary Valve/Steam game assets or binaries without the required rights and distribution permissions.

## Implementation sequence and acceptance gates

### Phase 0 — repository audit (current)
- Record the actual build and platform boundaries.
- Identify the engine entry points, renderer backends, input subsystem, and third-party dependency graph.
- Do not add placeholder Android build scripts that cannot compile.

### Phase 1 — platform abstraction
- Add narrowly scoped Android platform definitions and compile-time guards.
- Keep desktop platform macros and build targets unchanged.
- Add compile-only checks for Android ARM64 platform headers where the NDK toolchain is available.

**Gate:** Android-specific platform code compiles for `arm64-v8a`; existing desktop targets remain unaffected.

### Phase 2 — native shell and build
- Establish a minimal Android NDK application target and native lifecycle bridge.
- Build a small native smoke-test library before attempting the full engine.
- Rebuild dependencies from source for Android.

**Gate:** installable test APK launches on an ARM64 device and handles pause/resume and surface recreation.

### Phase 3 — renderer and engine
- Port and validate the selected mobile graphics backend.
- Bring up filesystem, materials, audio, networking, and engine initialization incrementally.

**Gate:** a minimal test scene renders on a physical Android device with stable lifecycle handling.

### Phase 4 — mobile gameplay
- Add touch controls, mobile HUD, settings, performance profiles, and thermal/memory safeguards.
- Profile CPU/GPU and memory on real devices; optimize based on measurements.

**Gate:** repeatable gameplay session on target devices, with no desktop-only dependencies in the Android package.

## Rules for this port

- Do not claim Android support until the corresponding acceptance gate passes.
- Do not link desktop Linux libraries into Android.
- Do not silently change or break Windows/Linux build paths.
- Keep changes incremental and testable; document the exact NDK version, ABI, build command, and test device for each working milestone.
- Respect the Source 1 SDK license and all third-party notices. This project is not a redistribution of CS:GO.

## Immediate next audit targets

- VPC target generation and project configuration.
- `tier0` platform implementation and POSIX/Linux-specific source files.
- Engine window/surface creation and renderer initialization.
- Input backend and event dispatch.
- Third-party libraries that must be rebuilt for Android ARM64.

## Deeper source audit (2026-11-06)

A second pass inspected the VPC platform scripts, launcher, and input interface:

- `src/vpc_scripts/platform_dirs.vpc` defines Windows, Linux32/Linux64, and OSX32 output directories; it has no Android or ARM64 Android target.
- `src/vpc_scripts/source_posix_base.vpc` adds the host system SDL2 include directory (`/usr/include/SDL2`) and desktop POSIX definitions. It is not a portable NDK configuration.
- `src/vpc_scripts/source_posix_ninja.vpc` hard-codes desktop x86 tuning flags for Linux32/Linux64 (for example, `-march=nocona` / `-march=core2`) and desktop linker assumptions. Those flags and host libraries must not leak into an Android ARM64 build.
- `src/launcher_main/main.cpp` is a desktop launcher: it loads Steam's desktop shared library, expects the Source SDK 2013 Multiplayer Steam installation, and uses SDL message boxes on POSIX. It cannot serve unchanged as an Android app entry point.
- `src/public/inputsystem/iinputsystem.h` exposes a window attachment interface and desktop-oriented input polling/event APIs. The interface may be reusable, but Android touch events and lifecycle must be bridged into it deliberately.
- The tree contains `src/materialsystem/shaderapidx9` and the GLM/TOGL headers under `src/public/togl/linuxwin`. This is not evidence of a ready Android GLES renderer: the actual graphics device/context creation, shader translation path, and required GL extensions still need tracing and device validation.

### Updated priority

The first practical build milestone should be a standalone Android NDK native-shell/smoke-test target, independent of Steam desktop launcher code. In parallel, trace how the existing Linux game creates its window and GL context and how the GLM/TOGL layer is initialized. Do not add an Android target to the main VPC graph until its compiler flags, libraries, ABI output paths, and dependency builds have been separated from desktop POSIX.



## Phase 1 deliverable: isolated Android native shell

An initial Android application target has now been added under `android/`, separate from the desktop VPC projects:

- Gradle application module targeting `arm64-v8a` only.
- Android NDK `NativeActivity` entry point using `android_native_app_glue`.
- EGL / OpenGL ES 2 surface creation, clear-and-present loop, resize and surface teardown handling.
- Basic touch-event receipt and lifecycle logging.
- GitHub Actions workflow `.github/workflows/android-arm64-shell.yml` to build a debug APK and verify that the ARM64 native library is packaged.

This shell is intentionally a platform smoke test. It does **not** link Source engine libraries, initialize the Source renderer, or include game content. Its acceptance gate is a successful CI APK build followed by installation and lifecycle testing on a physical ARM64 Android device.
