# AI PC Guardian

Minimal C++ project foundation for AI PC Guardian.

## Toolchain

- CMake 3.25 or newer
- C++17 without compiler-specific language extensions
- Windows: Visual Studio 2022 (MSVC 19.30 or newer)
- Ubuntu/Linux: GCC 11 or newer

No third-party runtime dependencies are introduced by `ARCH-001`.

## Build on Windows

Run from a PowerShell prompt with Visual Studio 2022 installed:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/agent/Debug/guardian-agent.exe
```

Expected output:

```text
AI PC Guardian Agent v0.1
```

## Build on Ubuntu

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
./build/server/guardian-server
```

Expected output:

```text
AI PC Guardian Server v0.1
```

Set `-DBUILD_TESTING=OFF` when test targets are not needed.

## Repository layout

```text
agent/    Windows background agent entry point
common/   Shared C++ targets and generated version API
server/   Ubuntu server entry point
tests/    Cross-platform foundation tests
cmake/    CMake templates and helpers
```
