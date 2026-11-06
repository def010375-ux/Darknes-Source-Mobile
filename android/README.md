# Darknes Android native shell

This is the first isolated Android ARM64 application target for the Source SDK port. It is deliberately separate from the desktop VPC build graph.

## What it proves

- Android Gradle Plugin + Android NDK cross-compilation for `arm64-v8a`.
- NativeActivity lifecycle handling through the NDK native-app-glue.
- EGL + OpenGL ES 2 context creation and frame presentation.
- Basic touch event delivery and pause/resume/window-destruction handling.

The current shell only clears the screen to a subtle animated dark background. **It does not run Source engine code, load CS:GO assets, or provide gameplay yet.** It is a platform bring-up test, not the completed game port.

## Build

Requirements: JDK 17, Android SDK platform 35, Android NDK 27.2.12479018, CMake 3.22.1, and Gradle 8.9.

From this directory:

```sh
gradle --no-daemon assembleDebug
```

APK output:

```
app/build/outputs/apk/debug/app-debug.apk
```

The APK targets ARM64 only. The GitHub Actions workflow builds and uploads it as `Darknes-Android-ARM64-Shell`.

## Next integration gates

1. Confirm this APK installs and runs on a physical ARM64 device.
2. Trace the Source Linux GLM/TOGL context and window initialization path.
3. Replace the shell clear-screen loop with an engine-owned render loop only after the platform interfaces are understood.
4. Port filesystem, input, audio, and dependencies incrementally.
