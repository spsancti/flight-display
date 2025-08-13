# Repository Guidelines

This repository hosts Arduino/C++ code for an OLED flight display. Use the structure and conventions below to keep contributions consistent and easy to review.

## Project Structure & Module Organization
- Arduino IDE/CLI: place the main sketch at repo root as `flight-display.ino`; factor helpers into `src/` (`.cpp/.h`).
- PlatformIO (recommended for teams):
  - `platformio.ini`: board/env configuration.
  - `src/`: `main.cpp` or `.ino`, plus modules like `display.cpp`, `sensors.cpp`.
  - `lib/`: reusable libraries per folder.
  - `include/`: public headers (e.g., `config.h`).
  - `test/`: unit tests (AUnit/Unity).
  - `assets/` and `hardware/`: images, schematics, pinouts.

## Build, Test, and Development Commands
- Arduino CLI:
  - `arduino-cli board list`: detect serial ports.
  - `arduino-cli compile --fqbn <FQBN> .`: build sketch (example FQBN: `arduino:avr:uno`, `esp32:esp32:esp32s3`).
  - `arduino-cli upload -p <PORT> --fqbn <FQBN> .`: flash device.
- PlatformIO:
  - `pio run`: build; `pio run -t upload`: flash; `pio device monitor`: serial.
  - `pio test`: run tests in `test/` on host or target.

## Coding Style & Naming Conventions
- Indentation: 2 spaces; K&R braces; 100‑char line limit.
- Names: Classes `PascalCase`; functions `camelCase`; files `snake_case.*`; constants/macros `UPPER_CASE`.
- C++ tips: prefer `constexpr`, `enum class`, `String`-free APIs in hot paths; avoid dynamic allocation in loops/ISRs.
- Formatting: use repository `.clang-format` if present; otherwise run `clang-format -i src/**/*.cpp include/**/*.h` and follow Google style.

## Testing Guidelines
- Framework: AUnit or Unity via PlatformIO. Name tests `test_<feature>.cpp` under `test/`.
- Scope: cover rendering helpers, unit conversions, and sensor parsing; stub hardware I/O behind small interfaces for mocking.
- Run: `pio test` (choose environments in `platformio.ini`).

## Commit & Pull Request Guidelines
- Commits: follow Conventional Commits (e.g., `feat: add altitude tape`, `fix: correct I2C address`).
- Branches: `feature/<short-desc>` or `fix/<issue-id>`.
- PRs: include clear description, linked issues, board/FQBN used, test evidence (logs/screenshots), and any pinout/connection notes.

## Security & Configuration Tips
- Secrets: never commit credentials. Provide `include/config.example.h`; copy to `include/config.h` locally and add to `.gitignore`.
- Document hardware: record pin mappings and display model in `README.md` or `hardware/`.

