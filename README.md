# Unified Engineering Workbench

C++17 engineering prototype for ASCII STL inspection, thermal finite elements, independent batch solves, simulated embedded devices, and scalar neural-network backpropagation.

## Windows 11 one-click launch

1. Extract the release ZIP to a writable folder.
2. Double-click `Launch.exe`.
3. The local server binds to `127.0.0.1:8765` and opens the default browser. Keep the process running while using the interface; close its console window to stop the server.

The release bundles the Three.js and Chart.js files locally and works offline. The GNU release configuration statically links GCC/C++ runtime libraries. Windows system libraries remain OS dependencies. If port 8765 is occupied, close the conflicting local service or set `WORKBENCH_PORT` before launch.

## Run from VS Code

The repository includes **Run Workbench** under **Terminal → Run Task**. From the source repository, first prepare the staging folder with `scripts/package_release.ps1 -StageOnly`; the task then starts that executable. If you open an extracted release folder in VS Code, the same task starts its root `Launch.exe`. The C/C++ extension and CMake Tools extension are recommended for source editing, kit selection, and debugging.

## Build from source

Prerequisites: Windows 11 x64, CMake 3.20+, Ninja, and a C++17 compiler with working `std::thread` support (GNU 8+ or a supported MSVC toolset). Select that compiler in CMake Tools or pass its path explicitly; the machine's default PATH may point at an older compiler.

```powershell
cmake -S . -B build-source -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="C:/path/to/C++17/compiler.exe"
cmake --build build-source --config Release
ctest --test-dir build-source --output-on-failure
```

The build produces `build-source/Launch.exe` and copies `frontend/` and `data/` beside it. Run with that build folder as the working directory.

To prepare a reviewable release folder, run `scripts/package_release.ps1 -StageOnly`. After entering the real candidate ID, run `scripts/package_release.ps1 -CandidateId <your-id>` to create the assignment's `CandidateID_Workbench.zip`. Add the candidate's genuine handwritten derivation scan separately. To recapture workstation-specific evidence, run `scripts/capture_evidence.ps1 -Executable .\build-release16\Launch.exe`, followed by `python scripts/generate_plots.py`.

## Interface and API

- **Geometry:** load an ASCII `.stl`, nodal temperature `.csv`, or both. STL facets render as a mesh; CSV rows render as a temperature-colored 3D point cloud with node inspection and a legend. Drag to rotate, right-drag to pan, wheel to zoom, and Fit View to frame the scene.
- **Thermal:** solve the 1 m rod for N=10/20/40/80. The chart overlays FE nodal values and the analytical solution. The table reports maximum absolute midpoint error, and Export CSV saves nodal temperatures.
- **Performance:** seed-42 cases use the same thermal solve in serial and a bounded standard-thread pool. The UI reports system/compiler metadata, warm-up/repetition protocol, median wall times, speedup, efficiency, and nodal agreement.
- **Hardware:** C++ mocks expose an MCU, FPGA, and JTAG adapter, capabilities, connection lifecycle, simulated telemetry, and a recoverable timeout. No physical device is accessed.
- **Backprop:** x=1, y=2, w=0.5, b=0, v=1, c=0, η=0.1. C++ reports the forward pass, four analytic gradients, central-difference checks (ε=1e−6), simultaneous first update, and losses at steps 0 through 100.

REST endpoints run on loopback: `GET /api/health`, `GET /api/system/info`, `GET /api/embedded/devices`, `POST /api/fea/solve`, `POST /api/parallel/run`, `POST /api/stl/parse`, `POST /api/csv/parse`, `POST /api/embedded/{discover,connect,disconnect,telemetry,capabilities}`, and `POST /api/backprop/run`.

## Thermal formulation

The assembled equation is `−d/dx(kA dT/dx)=qA`, with uniform material and source, insulated lateral surfaces, and prescribed endpoint temperatures. The exact temperature is `T(x)=100−80x + [q/(2k)]x(1−x)` for L=1 m. Linear two-node interpolation is exact at nodes but not at element midpoints for this quadratic solution. The midpoint error is `q h²/(8k)=125/N² °C`; it decreases by a factor of four whenever N doubles. Raw midpoint CSVs and labeled SVG plots are in `results/`.

## Embedded integration and safety

`docs/embedded_integration.md` contains the block diagram, interface/pin-voltage table, JTAG ownership, voltage-domain notes, and safe integration plan. Connector shape does not establish pin compatibility; verify board revision and vendor manuals before any physical wiring. The application labels all device behavior as simulated.

## Verification evidence

`tests/test_workbench.cpp` checks FEA midpoint error and endpoint boundary conditions, empty/malformed/non-finite parser cases, serial/parallel result order and nodal agreement, all four finite-difference gradients and decreasing training loss, and mock device recovery. The latest CTest run passed 1/1 core target. `results/performance_raw.json` and `results/performance_summary.csv` preserve measured data; `results/system_info.json` records the test workstation and compiler. The capture script also checks the HTTP routes for the page, samples, and local JavaScript libraries, then records launch-to-ready time.

Current measurements are specific to this machine: AMD64 Family 25 Model 116, 8 physical and 16 logical cores, 15,654 MB RAM, GNU 16.2.0, `-Wall -Wextra -O2 -pthread`. Re-run capture on the final submission machine before replacing these evidence files.

## Formats and limitations

- ASCII STL only; binary STL and native CAD solids are out of scope. Empty or facet-free STL files are rejected. UTF-8 BOM is accepted.
- CSV requires exactly `node_id,x_m,y_m,z_m,temperature_C`, with positive unique integer node IDs and finite values. Quoted CSV fields are not supported. Empty and header-only files are rejected. UTF-8 BOM is accepted.
- Bundled examples are self-created synthetic data. The CSV is explicitly not an actual ANSYS export; see `data/PROVENANCE.md`.
- The parser was exercised with the 1.4 KB STL and 0.6 KB CSV samples. There is no separate application-level byte cap; stay within practical memory limits.
- Thermal properties are constant. Transient/3D models and mesh-to-CSV registration are not implemented.
- Hardware is simulated; firmware, board access, and FPGA programming are not performed.
- Performance data is one workstation's evidence and is not a speedup guarantee on other machines.

## Submission notes

`docs/report.md` is the concise report. `docs/handwritten_backprop_draft.md` is a calculation draft, not a handwritten scan. The candidate must independently copy and check the derivation and attach their own scan. `AI_LOG.md` contains the earlier project disclosure and a truthful entry for this implementation session. The release script needs the actual candidate ID, and a clean-machine Windows 11 launch should be checked before final submission.
