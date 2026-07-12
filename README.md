# Absand

Absand is a cross-platform archive creation and transfer tool built with Qt. It was designed as a fast way to package files into ZIP or 7z archives and then send them through different destination plugins such as local folders, FTP, SFTP, network shares, or clipboard-based handoff.

The project currently includes:

- archive creation for ZIP and 7z
- archive browser support
- optional archive encryption where the format supports it
- destination plugins for local folder, FTP, SFTP, network share, and clipboard transfer
- bundled 7-Zip runtime support through bit7z
- plugin-based architecture for archive and destination handling
- a desktop Qt Widgets UI

## Requirements

You will need:

- CMake 3.20 or newer
- a C++17-capable compiler
- Qt 6.11.1
- Qt Widgets development libraries
- libzip
- the bundled `libs/bit7z` and `libs/7zip` sources that are already included in the repository

If you use a different Qt installation, make sure CMake points to the same Qt toolchain and runtime you plan to run with.

## Build

Recommended build using the Qt 6.11.1 SDK installed in `~/Qt/6.11.1/gcc_64`:

```bash
export PATH=$HOME/Qt/6.11.1/gcc_64/bin:$PATH
export LD_LIBRARY_PATH=$HOME/Qt/6.11.1/gcc_64/lib:$LD_LIBRARY_PATH

cmake -S . -B build-qt611 \
  -DQt6_DIR=$HOME/Qt/6.11.1/gcc_64/lib/cmake/Qt6 \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.1/gcc_64

cmake --build build-qt611 -j4
```

If you want to use your system Qt instead, configure CMake with the matching Qt 6 package paths on your machine.

## Run

After building:

```bash
./build-qt611/Absand
```

You can also pass files as arguments:

```bash
./build-qt611/Absand /path/to/file1 /path/to/file2
```

## Tests

Run the test suite with:

```bash
ctest --test-dir build-qt611 --output-on-failure
```

## Project layout

- `src/` — application code, UI, models, and core helpers
- `plugins/` — archive, destination, and send plugins
- `libs/bit7z/` — bundled bit7z library source
- `libs/7zip/` — bundled 7-Zip source tree used for runtime support
- `tests/` — integration tests

## Notes

- The app is plugin-driven, so most archive and destination features are loaded dynamically at runtime.
- Destination configuration is saved per user under the platform-specific application config folder.
- The bundled 7-Zip runtime is copied next to the executable during build.

## License

This repository contains third-party code in `libs/bit7z` and `libs/7zip`, each under their own upstream licenses. Check those directories for details.
