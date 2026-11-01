# Timestamped Workbench Walkthrough

**Recorded:** 2026-11-02 02:06:28 +05:30 (Asia/Kolkata)
**Build:** GNU 16.2.0, Release, C++17
**Mode:** local HTTP capture on `127.0.0.1:18765`; no browser auto-launch
**Raw machine evidence:** `results/performance_raw.json`

## Run the walkthrough

1. Extract `Launch.exe`, `frontend/`, and `data/` together into a writable directory.
2. Double-click `Launch.exe`. The app serves on port 8765 and opens the default browser. Keep the launcher running.
3. Select each workbench tab and follow the checks below. The server exposes `/api/health` and `/api/system/info` for quick health/metadata checks.

## Observed checks

| Workbench | Action | Recorded result |
|---|---|---|
| Geometry | Load the bundled ASCII STL and temperature CSV | 12 STL triangles and 30 CSV nodes parsed; both sample endpoints returned HTTP 200 |
| Thermal FEM | Solve N=10, 20, 40, and 80 | Midpoint errors 1.25, 0.3125, 0.078125, and 0.01953125 °C |
| Performance | Run seed-42 batch with 1, 2, and 4 workers | 100 cases, one warm-up, five timed repetitions; max nodal difference 0 °C for each worker count |
| Hardware mocks | Discover, connect MCU, request telemetry, connect JTAG twice | Three devices; MCU and telemetry succeeded; first JTAG connect timed out and retry succeeded |
| Backpropagation | Run 100 training updates | Four central-difference checks passed; initial loss 1.18254182, final loss rounds to 0.00000000 |
| Browser assets | Request the app, examples, and local Three.js/Chart.js scripts | All tested routes returned HTTP 200; scripts are served from the extracted app folder |

## Captured workstation results

The machine reported AMD64 Family 25 Model 116, 8 physical and 16 logical cores, 15,654 MB RAM, and GNU 16.2.0. The 1/2/4-worker speedups were 0.8868, 1.3734, and 1.9008. This record is evidence from the development workstation, not a guarantee for another machine.

## Candidate completion

Before submission, open the staged release on a clean Windows 11 machine, confirm browser launch and local assets, and capture screenshots if required by the evaluator. Add the candidate's real ID to the archive name. The backpropagation handwritten page must be written and scanned by the candidate; this walkthrough does not substitute for it.
