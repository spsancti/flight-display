# Repository Guidelines

This repository hosts Arduino/C++ code for an OLED flight display. The guidance below is optimized for Arduino IDE usage.

## Project Structure & Module Organization
- Sketch name must match folder: `flight-display/flight-display.ino` at repo root.
- Place helper `.h/.cpp` next to the sketch (repo root). Include with `#include "display.h"`.
- Use `assets/` for images/fonts and `hardware/` for pinouts, wiring, and schematics.
- Optional: create a reusable library in a separate repo or ZIP and install via Arduino IDE → Sketch → Include Library → Add .ZIP Library.

## Build, Upload, and Monitor
- Arduino IDE: select Tools → Board (e.g., ESP32, AVR) and Tools → Port, then click Upload.
- Serial Monitor: Tools → Serial Monitor (start at `115200` baud unless specified). Use it for logs and simple test output.
- Optional CLI: `arduino-cli compile --fqbn <FQBN> .` and `arduino-cli upload -p <PORT> --fqbn <FQBN> .` for headless builds.

## Coding Style & Naming Conventions
- Indentation: 2 spaces; K&R braces; 100‑char line limit.
- Names: Classes `PascalCase`; functions `camelCase`; files `snake_case.*`; constants/macros `UPPER_CASE`.
- C++ tips: prefer `constexpr`, `enum class`, avoid `String` in hot paths; no dynamic allocation in loops/ISRs.
- Formatting: use `.clang-format` if present; otherwise run `clang-format -i *.ino *.cpp *.h` before opening a PR.

## Testing Guidelines
- Framework: AUnit recommended. Create a test sketch under `test/` (e.g., `test/test_display/test_display.ino`). Open it in Arduino IDE, upload to the target, and read pass/fail in Serial Monitor.
- Focus: rendering utilities, unit conversions, sensor parsing. Isolate hardware I/O behind small interfaces to enable mocking.

## Commit & Pull Request Guidelines
- Commits: use Conventional Commits (e.g., `feat: add altitude tape`, `fix: correct I2C address`).
- Branches: `feature/<short-desc>` or `fix/<issue-id>`.
- PRs: include description, linked issues, board model + FQBN, serial logs/screenshots, and any pinout notes.

## Security & Configuration Tips
- Do not commit secrets. Provide `config.example.h`; contributors copy to `config.h` locally and add to `.gitignore`.
- Record display model and pin mappings in `README.md` or `hardware/` so others can reproduce.
