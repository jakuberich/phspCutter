# PHSP Cutter

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)

## Overview

**PHSP Cutter** is a C++ utility that reads an IAEA phase space (PHSP) file, applies user-defined filtering criteria (such as cutting based on particle position), and writes the accepted records into a new PHSP file with an updated header. This tool allows you to, for example, select only those particles whose projected positions (calculated at a given Z-plane) fall within a specified region.

## Features

- **Selective Filtering:**  
  Filter records based on custom criteria. In the default example, the cutter projects the particle's position to a specified Z-plane and accepts the particle only if its projected (X, Y) coordinates lie within a defined rectangle.

- **Header Update:**  
  The tool copies the header from the input file, removes extra data if desired, and then updates key statistical fields (such as total histories and particle counts) based on the filtered data.

- **Error Handling:**  
  Robust error handling ensures that read errors are logged and, if too many occur, processing for that file is aborted.

## Requirements

- **C++ Compiler** (e.g., GCC)
- **CMake** and **Make** (if using the provided CMakeLists.txt build system)
- The IAEA libraries:
  - `iaea_phsp.h` / `iaea_phsp.cpp`
  - `iaea_header.h` / `iaea_header.cpp`
  - `iaea_record.h` / `iaea_record.cpp`
  - `utilities.h` / `utilities.cpp`

## Building

### With CMake/Make

1. Create and enter a build directory:
   ```bash
   mkdir build && cd build
   ```
2. Run CMake:
   ```bash
   cmake ..
   ```
3. Build the project:
   ```bash
   make
   ```

### Direct Compilation with GCC

Alternatively, compile directly with:
```bash
g++ -o PHSPcutter PHSPcutter.cc iaea_phsp.cpp iaea_header.cpp iaea_record.cpp utilities.cpp -lm -lstdc++
```
Make sure that all source files are in the correct directories.

## Usage

The PHSP Cutter is a command-line tool that takes two arguments:
- **Input File Base Name:** The base name of the input PHSP file (without extension). The tool expects to find files such as `yourInput.IAEAheader` and `yourInput.IAEAphsp`.
- **Output File Base Name:** The base name for the output file. The tool will create `yourOutput.IAEAheader` and `yourOutput.IAEAphsp`.

Example:
```bash
./PHSPcutter inputFileBase outputFileBase
```

### Filtering Details

In the default configuration, the cutter applies the following filter:
- **Z-Plane Cut:**  
  When a particle is moving in the positive z-direction (w > 0) and its z-position is below a defined Z_PLANE (e.g., 100 cm), the tool calculates the projected (x, y) position at Z_PLANE.
- **Region Check:**  
  The particle is accepted only if the projected x and y values fall within a predefined range (e.g., x between -10 cm and 10 cm, y between -10 cm and 10 cm).

If the conditions are met, the particle record is written to the output file; otherwise, it is skipped.

**Note:**  
Paths containing spaces should be enclosed in quotes:
```bash
./PHSPcutter "path/to/inputFileBase" "path/to/outputFileBase"
```

## How It Works

1. **Input and Header Copy:**  
   The tool opens the input PHSP file (using its base name) in read mode, copies the header to the output file, and then modifies the header (e.g., disabling extra long/float storage) to match the desired output format.

2. **Record Processing:**  
   The tool reads the expected number of records (usually one record less than indicated in the header to avoid a read error) and applies the filtering criteria. Only the records that meet the criteria are written to the output file.

3. **Header Update:**  
   After processing, the output header is updated (via `iaea_update_header`) so that fields such as checksum, total histories, and particle counts correctly reflect the filtered data.

4. **Error Handling:**  
   The tool logs individual record read errors. If errors exceed a defined threshold, processing is aborted for the file.

## Troubleshooting

- **File Size/Checksum Mismatch:**  
  If errors related to file size or byte order occur, ensure that the input file is in the expected IAEA PHSP format and that it has a consistent byte order.

- **Last Record Read Error:**  
  The tool reads one record less than the header's expected count to avoid read errors at the end of the file. This behavior is normal.

## Customization

- **Filter Criteria:**  
  Modify the constants (e.g., `Z_PLANE`, `X_MIN`, `X_MAX`, `Y_MIN`, `Y_MAX`) in the source code to adjust the filter conditions.

- **Extra Data Handling:**  
  If needed, modify the header handling section to either preserve or remove extra long/float data.

- **Error Threshold:**  
  Adjust the `ERROR_THRESHOLD` constant to control how many errors are tolerated during processing.

## Contributing

Contributions, suggestions, and bug reports are welcome! Please open an issue or submit a pull request.

## License

This project is licensed under the [MIT License](LICENSE).

---

Happy cutting and filtering!
