# Test Evidence

This matrix summarizes the automated unit and HTTP integration checks. It records evidence collected on 2 November 2026 with GNU 16.2.0 on Windows. The C++ checks run through CTest; HTTP checks exercise the same routes used by the browser interface. Device behavior is simulated.

## Test matrix

| Module | Test | Pass condition | Result and evidence |
|---|---|---|---|
| Geometry | Parse the bundled ASCII STL | Valid parse with 12 facets | Passed: `results/performance_raw.json` records `stl_triangles: 12`; CTest checks the bundled fixture. |
| Geometry | Submit empty and malformed STL content | Reject input, return a readable error, and keep the server alive | Passed: empty input returns `Empty STL content.`; malformed input returns `ASCII STL must begin with 'solid'.`; both are recorded in `results/performance_raw.json`. |
| Geometry UI | Preserve metric fields across repeated file loads | Update the node count/range spans without replacing the status bar or deleting the triangle counter | Passed: JavaScript syntax and DOM-update contract checks confirm the parent status bar is no longer overwritten. Refresh the browser to load the fix. |
| Temperature CSV | Parse bundled temperature data | Valid parse with 30 finite, uniquely identified nodes | Passed: `csv_nodes: 30` in the capture; CTest parses the bundled file. |
| Temperature CSV | Submit non-finite and malformed values | Reject input with a row/field error | Passed: `NaN` is rejected with `Row 1: non-finite value in temperature_C.`; CTest also checks invalid numeric text. |
| Thermal FEA | Solve N=10, 20, 40, 80 and check endpoint temperatures | Endpoints equal 100 °C and 20 °C; midpoint error equals `125/N²` °C | Passed in CTest and HTTP capture. Errors: 1.25, 0.3125, 0.078125, and 0.01953125 °C. See `results/fea_N*.csv` and `results/fea_convergence.csv`. |
| Performance | Compare deterministic serial and parallel batches | Same ordered nodal results within `1e-8` °C | Passed in CTest. HTTP capture used 100 cases, seed 42, one warm-up and five measured repetitions for 1/2/4 workers; maximum nodal difference was 0 °C in all three rows. See `results/performance_summary.csv`. |
| Hardware | Connect mock MCU and request telemetry; trigger JTAG timeout and retry | MCU/telemetry succeed; first JTAG attempt fails cleanly; retry recovers | Passed in CTest and HTTP capture. `results/performance_raw.json` records the first JTAG failure and successful retry. No physical device is contacted. |
| Backpropagation | Compare analytic gradients to central differences; train 100 updates | Four gradient errors ≤ `1e-5`; loss decreases; history includes steps 0–100 | Passed in CTest and HTTP capture. `ε=1e-6`; captured maximum difference is 0 at reported precision; history has 101 entries and step-100 loss rounds to 0.00000000. |
| Launcher and browser assets | Start the app and request page, sample files, and local libraries | Health and static routes return HTTP 200 | Passed in the capture script. It also verifies sample file delivery, API parsing, system metadata, and no-browser startup. |

## Reproduce the checks

From the project root in PowerShell:

```powershell
cmake --build build-release16 --config Release
ctest --test-dir build-release16 --output-on-failure
.\scripts\capture_evidence.ps1 -Executable .\dist\UnifiedEngineeringWorkbench\Launch.exe -Port 18769
```

The CTest output is saved in `results/ctest_results.txt`. The capture script writes timestamped raw responses to `results/performance_raw.json`, the benchmark table to `results/performance_summary.csv`, and workstation metadata to `results/system_info.json`. It chooses a configurable loopback port so an existing workbench process does not have to be stopped.

The browser's Geometry tab displays parser failures in its alert area using the backend's `error_msg`; the empty-STL case therefore shows the explicit message rather than terminating the app. The API and parser failure paths are automated above. A separate clean-machine Windows 11 run remains a final deployment check.
