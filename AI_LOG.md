# AI Log – Unified Engineering Workbench

**Assignment**: PASS SCIENCE WORKS R&D Internship 2026-2027
**Candidate**: [Candidate ID placeholder]
**AI Tool**: Antigravity (Google DeepMind) with Claude Sonnet 4.6 (Thinking)
**Date**: 2026-11-01

---

## AI Usage Disclosure

This project was built with substantial AI assistance as permitted by the assignment brief
("You may use any GPT/AI assistant and any openly available libraries").

### Tools Used
- **Antigravity / Claude Sonnet 4.6 (Thinking)** — Primary coding assistant
  - Architecture design and module planning
  - C++ code generation (FEM solver, parsers, parallel batch, mock devices, backprop)
  - Frontend HTML/CSS/JS generation (Three.js integration, Chart.js integration)
  - CMakeLists.txt generation
  - Documentation drafting

### What AI Generated
All source code files were generated with AI assistance and then reviewed for correctness.
The mathematical derivations (backprop chain rule, FEM formulation) were:
1. First derived by hand / verified against textbook formulations
2. Then implemented in C++ with AI assistance
3. Cross-checked by running the gradient-check (central finite differences) and FEA error analysis

### What Was Independently Verified
- **FEM formulation**: Stiffness matrix assembly verified against Reddy's "Introduction to FEM" formulation for 1D heat conduction
- **Backprop derivation**: Chain-rule computed manually (see docs/backprop_derivation.md), verified by grad check
- **Parallel correctness**: Serial vs parallel results compared (max diff < 1e-8)
- **STL/CSV parsing**: Tested with sample files and malformed inputs

### Representative Prompts
1. "Write a C++17 1D thermal FEM solver for a rod with BCs T(0)=100, T(L)=20 and uniform heat generation q. Use Gaussian elimination. Export results to CSV."
2. "Implement a bounded thread pool in C++17 using std::thread, mutex, and condition_variable for parallel batch thermal solves."
3. "Write the complete HTML/CSS/JS frontend with Three.js for STL viewing and Chart.js for FEA plots, served from a C++ HTTP server at localhost:8765."

### AI Suggestions Used
- cpp-httplib as a single-header HTTP server (suggestion accepted — lightweight, no extra deps)
- Three.js OrbitControls for 3D navigation (suggestion accepted — standard approach)
- Using `std::thread::hardware_concurrency()` for core count (suggestion accepted)

### AI Suggestions Rejected / Challenged
- AI initially suggested using nlohmann/json for JSON parsing — **rejected** to minimize dependencies; manual string construction used instead
- AI suggested OpenMP as primary threading — **modified** to use std::thread as primary with OpenMP as optional (per rubric: "bounded worker pool, standard threads or OpenMP; do not merely start background UI work")
- AI suggested using binary STL format — **rejected** per assignment (ASCII STL required)

### Limitations of AI-Generated Code
1. JSON construction is manual (no library) — prone to escaping issues with special characters in file content
2. Performance timing accuracy depends on OS scheduler; single-logical-CPU machines will show speedup ~1.0
3. Three.js integration requires internet access to load CDN scripts (documented in README)

### Ownership Statement
The mathematical formulations, architectural decisions, and verification of correctness
are the candidate's own work. AI was used as a coding accelerator, similar to using
a smart IDE autocomplete or StackOverflow for standard patterns.

All reused code (cpp-httplib, Three.js, Chart.js) is open-source with appropriate licenses:
- cpp-httplib: MIT License (Yuji Hirose)
- Three.js: MIT License
- Chart.js: MIT License

---

*No personal data, credentials, machine-specific absolute paths, or proprietary information
is included in this submission.*

## Implementation-session record — 2026-11-02

OpenAI Codex assisted with a review and completion pass after the earlier disclosure above. Work included aligning the frontend and HTTP API, implementing missing system/device endpoints, hardening the ASCII STL and CSV parsers, correcting worker-count handling and nodal agreement checks, adding CTest coverage, packaging the JavaScript libraries locally, and writing the report and capture tools. The user requested a full working implementation. The coding assistant made the edits and ran the commands recorded in the repository; the candidate should review and understand the resulting code before submitting.

Verification performed in this session: GNU 16.2.0 Release build refreshed; CTest `workbench_core` passed 1/1; the evidence script exercised sample parsers, FEA, backpropagation, mock devices, local asset delivery, and three parallel worker counts. The exact workstation measurements are recorded in `results/` and summarized in `docs/report.md`.

Release hardening also anchors the process working directory to the executable folder. A smoke launch from an unrelated working directory successfully served the root page, health endpoint, sample STL, and local Three.js file.

After the user reported garbled interface symbols, the HTML's mis-decoded UTF-8 sequences were repaired. The temperature CSV label now shows the parser's exact required header, the sidebar reports the active host and port instead of a hard-coded 8765, and its status indicator checks the health endpoint.

The requested testing-evidence matrix was added under `tests/`. Unit tests now assert readable parser errors for empty/malformed STL and invalid CSV input; the HTTP capture also submits invalid payloads and records their error messages. CTest passed 1/1 and its output was saved in `results/ctest_results.txt`.

After the user reported a null `textContent` error on a repeated geometry load, the CSV handler was corrected to update the existing node-count and temperature-range spans instead of replacing the entire status bar. This preserves the STL triangle counter for subsequent loads. The updated JavaScript passes `node --check` and a DOM-ID/update-contract check.

### Corrections to earlier draft disclosure

- Three.js and Chart.js are now bundled and served locally. The earlier statement that the frontend needs CDN access is obsolete.
- OpenMP is not used by the current implementation. The batch solver uses bounded `std::thread` workers; the current CMake build does not require OpenMP.
- This file pre-dated the 2026-11-02 implementation session. The assistant cannot independently verify the earlier claims about tools used, candidate-authored work, or manual derivations. The candidate should confirm those statements and revise them if they are not accurate.
