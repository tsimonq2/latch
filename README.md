# latch

latch locks a Wayland session. It speaks `ext-session-lock-v1`, draws with Qt 6, and checks the password with PAM.

The build requires GCC 16.

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```
