---
description: Run tests for focus-zones-native
---

# Run Tests

Tests use function-pointer injection for mocking Win32 calls.

// turbo
1. Build tests:
```
cmake --build build --config Debug --target focus_zones_tests
```

// turbo
2. Run tests:
```
.\build\Debug\focus_zones_tests.exe
```
