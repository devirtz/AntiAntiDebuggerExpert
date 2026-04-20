# antiantidebugger-x64dbg-expert

x64dbg plugin that uses Unicorn Engine to emulate the target process and bypass anti-debug tricks (rdtsc, syscall detection, out-of-module detection).

**x64 only.**

## How it works

1. Click `sys_trace` in the Plugins menu
2. Plugin snapshots current registers + all committed memory into a Unicorn emulator
3. Emulator runs until it hits rdtsc, syscall, or an out-of-module jump
4. Plugin sets a hardware breakpoint at the stop address, resumes the real process
5. On breakpoint hit, loops back to step 2

## Requirements

- x64dbg (64-bit)
- [Unicorn Engine](https://github.com/unicorn-engine/unicorn) 
- CMake 3.15+

## Build

```cmd
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

Output: `build/Release/antidebugger.dp64`

Copy to `x64dbg/release/x64/plugins/`.
