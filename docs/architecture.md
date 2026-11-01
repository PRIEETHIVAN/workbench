# Architecture Overview

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  Unified Engineering Workbench              │
├───────────────────────┬─────────────────────────────────────┤
│   FRONTEND (Browser)  │        BACKEND (C++17)              │
│                       │                                     │
│  ┌─────────────────┐  │  ┌────────────────────────────────┐ │
│  │  Tab: Geometry  │◄─┼──┤  /api/stl/parse  STL Parser   │ │
│  │  (Three.js 3D)  │  │  │  /api/csv/parse  CSV Parser   │ │
│  └─────────────────┘  │  └────────────────────────────────┘ │
│  ┌─────────────────┐  │  ┌────────────────────────────────┐ │
│  │  Tab: Thermal   │◄─┼──┤  /api/fea/solve  FEM Solver   │ │
│  │  (Chart.js)     │  │  │  Gaussian elimination, 1D rod │ │
│  └─────────────────┘  │  └────────────────────────────────┘ │
│  ┌─────────────────┐  │  ┌────────────────────────────────┐ │
│  │  Tab: Perf      │◄─┼──┤  /api/parallel/run            │ │
│  │  (Charts)       │  │  │  Thread pool, batch solver    │ │
│  └─────────────────┘  │  └────────────────────────────────┘ │
│  ┌─────────────────┐  │  ┌────────────────────────────────┐ │
│  │  Tab: Hardware  │◄─┼──┤  /api/embedded/*              │ │
│  │  (Block Diag)   │  │  │  Mock device adapters (MCU,   │ │
│  └─────────────────┘  │  │  FPGA, JTAG)                  │ │
│  ┌─────────────────┐  │  └────────────────────────────────┘ │
│  │  Tab: Backprop  │◄─┼──┤  /api/backprop/*              │ │
│  │  (Math + Chart) │  │  │  Forward/backward pass,       │ │
│  └─────────────────┘  │  │  grad check, training loop    │ │
└───────────────────────┘  └────────────────────────────────┘ │
                              └────────────────────────────────┘
```

## C++ Module Responsibilities

| Module | File | Responsibility |
|--------|------|----------------|
| FEM Solver | fea/fem_solver.cpp | 1D thermal FEA, Gaussian elim, error vs exact |
| STL Parser | viz/stl_parser.cpp | ASCII STL parsing, validation |
| CSV Parser | viz/csv_parser.cpp | ANSYS temperature CSV, validation |
| Batch Solver | parallel/batch_solver.cpp | Thread pool, serial+parallel batch |
| Device Adapter | embedded/device_adapter.cpp | Mock MCU/FPGA/JTAG adapters |
| Backprop | backprop/backprop.cpp | Forward/backward pass, grad check |
| HTTP Server | main.cpp | Route API endpoints, serve frontend |

## Frontend Module Responsibilities

| Component | Technology | Purpose |
|-----------|-----------|---------|
| 3D Viewer | Three.js r134 | STL mesh + temperature point cloud |
| FEA Charts | Chart.js 4.4 | T(x) plots, error tables |
| Perf Charts | Chart.js 4.4 | Speedup, efficiency, timing bars |
| Hardware UI | Vanilla JS | Device management, telemetry |
| Backprop UI | Vanilla JS | Chain-rule display, training chart |

## Separation of Concerns
- **All** numerical computation is in C++ backend
- **All** presentation logic is in JavaScript frontend
- Communication via REST JSON API over localhost:8765
- No shared code between frontend and backend

## Thread Model
The HTTP server (cpp-httplib) handles each request in a thread pool.
The batch solver uses a separate bounded worker pool (std::thread + mutex + condition_variable).
Browser UI remains responsive during all computations via async fetch().

## Build System
CMake 3.20+ with find_package(Threads). No external dependencies beyond
cpp-httplib (single header, vendored in src/server/httplib.h).

## Known Limitations
- HTTP server uses plain HTTP (no TLS) on localhost only
- Single-machine: browser and server must be on same host
- Parallel speedup assessment requires ≥2 logical CPUs
- STL parser handles ASCII format only (not binary STL)
- CSV parser expects exact column order: node_id,x_m,y_m,z_m,temperature_C
