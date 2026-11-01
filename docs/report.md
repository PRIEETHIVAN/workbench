# Unified Engineering Workbench — Engineering Report

**Capture date:** 2 November 2026 (Asia/Kolkata)
**Application:** C++17 loopback service with a browser interface
**Evidence:** workstation capture in `results/performance_raw.json`; detailed run sequence in `docs/walkthrough.md`.

## 1. Scope and design

The workbench combines the assignment's geometry, thermal finite-element, batch-performance, embedded-interface, and backpropagation demonstrations in one local application. The C++ service owns numerical calculations and parsers. The browser interface uses locally bundled Three.js and Chart.js, so the application does not need a CDN or internet connection after extraction. Device operations are simulated.

The Windows launcher serves the interface and REST API on loopback (`127.0.0.1`, default port 8765) and opens the default browser. It resolves bundled resources relative to the executable, independent of the process's starting directory. `WORKBENCH_PORT` selects another port; `WORKBENCH_NO_BROWSER=1` suppresses browser startup for scripted evidence capture. Source and tests build separately with CMake and a C++17 compiler.

## 2. Geometry input

The STL parser accepts ASCII facets and rejects empty, facet-free, truncated, and malformed geometry. The CSV parser requires the columns `node_id,x_m,y_m,z_m,temperature_C`, positive unique integer IDs, and finite numeric values. Both accept a UTF-8 BOM. The sample mesh contains 12 triangles and the sample temperature CSV contains 30 nodes. The examples are synthetic and are identified as such in `data/PROVENANCE.md`; the CSV is not represented as an ANSYS export.

The interface renders STL facets and temperature-colored CSV points in an interactive 3D view. The two input types can be loaded independently; the workbench does not infer mesh-to-node correspondence.

## 3. One-dimensional thermal FEM

The model is a 1 m rod with `A=1e-4 m²`, `k=10 W/(m·°C)`, `q=10000 W/m³`, `T(0)=100 °C`, and `T(1)=20 °C`. It solves

`−d/dx(kA dT/dx)=qA`

using linear two-node elements and prescribed end temperatures. The analytical solution is

`T(x)=100−80x+[q/(2k)]x(1−x)`.

Linear interpolation reproduces nodal values for this quadratic field, while the maximum element-midpoint error is `q h²/(8k)=125/N² °C`. The measured results are:

| Elements N | Maximum midpoint error (°C) |
|---:|---:|
| 10 | 1.25000000 |
| 20 | 0.31250000 |
| 40 | 0.07812500 |
| 80 | 0.01953125 |

Each doubling reduces this error by four, matching the expected second-order behavior. The nodal CSVs and SVG plots are included in `results/`.

## 4. Deterministic batch performance

The captured run used 100 cases, mesh baseline 48, seed 42, one warm-up, and five measured repetitions. Serial and bounded-thread runs use the same case inputs and solver. Timings are medians; agreement compares actual nodal temperatures.

| Workers | Serial median (ms) | Parallel median (ms) | Speedup | Efficiency | Max nodal difference (°C) |
|---:|---:|---:|---:|---:|---:|
| 1 | 4.8065 | 5.3692 | 0.8952 | 0.8952 | 0 |
| 2 | 5.4078 | 2.7570 | 1.9615 | 0.9807 | 0 |
| 4 | 4.8135 | 1.9533 | 2.4643 | 0.6161 | 0 |

All rows passed the `1e-8 °C` agreement limit. One worker is slower than serial on this capture, which is expected from thread-pool overhead. These measurements describe this workstation only: AMD64 Family 25 Model 116, 8 physical/16 logical cores, 15,654 MB reported RAM, GNU 16.2.0, and `-Wall -Wextra -O2 -pthread`.

## 5. Embedded integration model

The MCU, FPGA, and JTAG entries are mock adapters; they make no physical connection. The interface exposes discovery, capabilities, connection state, simulated telemetry, and disconnect. The capture verified three devices, a successful MCU connection and telemetry request, and a JTAG timeout on the first attempt followed by successful recovery. See `docs/embedded_integration.md` for the block diagram, signal table, voltage notes, and safe connection plan.

## 6. Backpropagation demonstration

The scalar network uses `z=wx+b`, `h=tanh(z)`, `ŷ=vh+c`, squared loss `J=½(ŷ−y)²`, and one sample `(x,y)=(1,2)`. Initial parameters are `(w,b,v,c)=(0.5,0,1,0)` with learning rate `η=0.1`. The initial prediction is `0.46211716` and initial loss is `1.18254182`.

The API reports analytic gradients for all four parameters, central-difference checks with `ε=1e-6`, and loss history from step 0 through step 100. The interface calculates and displays a simultaneous first update. The captured absolute discrepancy for each gradient rounds to 0 at the output's eight-decimal precision, which is below the `1e-5` requirement. The reported step-100 loss also rounds to `0.00000000`. `docs/handwritten_backprop_draft.md` is only a calculation draft; the candidate must independently verify and attach their own handwritten scan.

## 7. Verification and limits

The CTest suite passed its single `workbench_core` target. It exercises FEM values and boundary conditions, malformed/empty/non-finite parser input, parallel ordering and numerical agreement, all four gradient checks and loss reduction, and mock-device timeout recovery. The evidence script additionally exercised HTTP health, frontend and local asset delivery, sample file delivery/parsing, FEA runs, backprop, mock devices, and 1/2/4-worker benchmarks. A 2 November capture completed with the latest source build.

The implementation is a prototype: it supports ASCII STL rather than binary STL or native CAD, uses constant thermal properties and a one-dimensional steady model, and does not map CSV nodes onto STL topology. Hardware behavior is simulated. Input sizes are constrained by practical memory, not a configurable application-level byte limit. The runtime has Windows system-library dependencies; the project has not been validated on a separate clean Windows 11 machine.

## 8. Submission items requiring candidate input

The actual candidate ID is not present in the project context, so the submission archive filename must be supplied by the candidate. The project includes the calculation draft but no genuine handwritten scan. The candidate should provide their actual ID, create and attach their own handwritten derivation, and perform a clean-machine launch check before submission.
