# Repository Guidelines

## Project Structure & Module Organization
Congregation follows KDE's ECM layout. The root `CMakeLists.txt` wires in ECM modules and delegates to `App/`. All C++ sources live in `App/`, with the application entry point at `src/main.cpp`. QML assets stay under `src/qml/`. Keep every CMake build output in the repository-local `build/` directory; alternate build trees are not supported. Never commit generated binaries.

## Build, Test, and Development Commands
Configure with ECM and KDE install dirs via `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`. The script will emit helpful dependency summaries. Build with `cmake --build build` to produce the `Congregation` bundle in `build/`. Launch the macOS binary from `build/Congregation.app/Contents/MacOS/Congregation` (adjust per platform). Clean artifacts using `cmake --build build --target clean` or by pruning the `build/` directory.

## Coding Style & Naming Conventions
Follow Qt and KDE coding guidelines: four-space indentation, Allman braces, and camelCase names, matching the repo `.clang-format`. Favor explicit Qt/KF types (e.g., `QString`, `KLocalizedString`) and modern signal-slot patterns (`QObject::connect` lambdas). Keep QML imports grouped at the top, one root component per file, and format with `qmlformat` before review.

## Testing Guidelines
There are currently no automated tests; plan new work with Qt Test or Catch2 under a `tests/` directory. Name test targets `tests_<feature>` and wire them into CMake so they run with `ctest --output-on-failure` from the `build/` tree. When adding UI scenarios, include reproducible steps or recordings because visual regressions are hard to cover automatically.

## Commit & Pull Request Guidelines
Write concise, imperative commit subjects (`Add QML brush toolbar`) and include context in the body when behavior changes. Group related edits into logical commits to simplify review. Pull requests should describe the motivation, summarize user-facing changes, and link any issue tracker IDs. Attach screenshots or screen captures for UI updates and mention manual test coverage performed.

## Qt Configuration Tips
Export `CMAKE_PREFIX_PATH` so CMake locates Qt 6.8+ and KDE Frameworks 6 (Kirigami, KI18n). After adding new dependencies, re-run `cmake -S . -B build` to refresh the ECM feature summary and ensure `qt_add_qml_module` tracks added QML files.
