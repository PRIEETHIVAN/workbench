# Dependency and version record

| Component | Version | Source / license | Runtime use |
|---|---:|---|---|
| C++ standard | C++17 | ISO C++ standard | Backend and application |
| cpp-httplib | 0.14.3 | Vendored `src/server/httplib.h`; MIT, Yuji Hirose | HTTP server |
| Three.js | r134 | Vendored `src/frontend/three-r134.min.js`; MIT | STL scene rendering |
| Three.js OrbitControls | r134 | Vendored `src/frontend/OrbitControls-r134.js`; MIT | STL rotate, pan, and zoom |
| Chart.js | 4.4.0 | Vendored `src/frontend/chart-4.4.0.umd.min.js`; MIT | FEA, performance, and training charts |
| CMake | 3.20 minimum | https://cmake.org/; BSD-3-Clause | Source build only |
| C++ runtime | Toolchain dependent | MSVC redistributable or MinGW UCRT runtime | Source-build deployment |

There is no JavaScript package manager manifest: the frontend uses pinned local vendor files, so it runs offline. SHA-256 checksums for the vendored files are recorded in `src/frontend/vendor/LICENSES.md`. MinGW release builds statically link the GCC/C++ runtime; Windows system libraries remain OS dependencies. A clean-machine Windows 11 launch check is still required before claiming the delivery gate is verified.
