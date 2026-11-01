/**
 * fem_solver.cpp
 * Implementation of the 1D thermal FEM solver.
 *
 * Approach:
 *   - Assemble global (n+1)×(n+1) stiffness matrix K and load vector F
 *     using two-node linear elements.
 *   - Each element contributes:
 *       K_e = (k*A/Le) * [[1,-1],[-1,1]]
 *       F_e = q*A*Le/2 * [1, 1]
 *   - Apply Dirichlet BCs by elimination (modify rows/cols for pinned nodes).
 *   - Solve K*T = F with Gaussian elimination (partial pivoting).
 *   - Interpolate midpoint temperatures; compare with exact solution.
 *   - Export CSV to tests/results/fea_N{n}.csv.
 */

#include "fem_solver.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

namespace fea {

// ─── Problem constants ───────────────────────────────────────────────────────
static constexpr double L  = 1.0;      ///< Rod length [m]
static constexpr double A  = 1e-4;     ///< Cross-section area [m^2]
static constexpr double k  = 10.0;     ///< Thermal conductivity [W/(m·K)]
static constexpr double q  = 10000.0;  ///< Heat generation rate [W/m^3]
static constexpr double T0 = 100.0;    ///< Left BC temperature [°C]
static constexpr double TL = 20.0;     ///< Right BC temperature [°C]

// ─── Exact solution ──────────────────────────────────────────────────────────
static double exact_temperature(double x) {
    // T(x) = T0 - (T0-TL)*x/L + (q/(2k))*x*(L-x)
    return T0 - (T0 - TL) * x / L + (q / (2.0 * k)) * x * (L - x);
}

// ─── Gaussian elimination with partial pivoting ──────────────────────────────
/**
 * Solves A*x = b in place (modifies A and b).
 * @param A  n×n matrix stored row-major (vector of rows)
 * @param b  n-element right-hand side; holds solution on exit
 * @param n  system size
 */
static void gaussian_elimination(std::vector<std::vector<double>>& A,
                                  std::vector<double>& b, int n) {
    for (int col = 0; col < n; ++col) {
        // Partial pivot
        int pivot = col;
        for (int row = col + 1; row < n; ++row) {
            if (std::abs(A[row][col]) > std::abs(A[pivot][col])) pivot = row;
        }
        std::swap(A[col], A[pivot]);
        std::swap(b[col], b[pivot]);

        if (std::abs(A[col][col]) < 1e-14) {
            throw std::runtime_error("Singular stiffness matrix during Gaussian elimination.");
        }

        // Eliminate below
        for (int row = col + 1; row < n; ++row) {
            double factor = A[row][col] / A[col][col];
            for (int j = col; j < n; ++j) A[row][j] -= factor * A[col][j];
            b[row] -= factor * b[col];
        }
    }

    // Back substitution
    for (int row = n - 1; row >= 0; --row) {
        b[row] /= A[row][row];
        for (int j = row - 1; j >= 0; --j) {
            b[j] -= A[j][row] * b[row];
            A[j][row] = 0.0;
        }
    }
}

// ─── Main solver ─────────────────────────────────────────────────────────────
FEAResult solve_thermal_1d(int n_elements) {
    if (n_elements < 1) throw std::invalid_argument("n_elements must be >= 1");

    const int n_nodes = n_elements + 1;
    const double Le   = L / static_cast<double>(n_elements); // element length

    // Global stiffness matrix and load vector
    std::vector<std::vector<double>> K(n_nodes, std::vector<double>(n_nodes, 0.0));
    std::vector<double> F(n_nodes, 0.0);

    // ── Assemble ─────────────────────────────────────────────────────────────
    for (int e = 0; e < n_elements; ++e) {
        int i = e;      // local node 0 → global node i
        int j = e + 1;  // local node 1 → global node j

        double ke = k * A / Le; // element stiffness scale
        double fe = q * A * Le / 2.0; // element load contribution per node

        // Add element stiffness
        K[i][i] += ke;
        K[i][j] -= ke;
        K[j][i] -= ke;
        K[j][j] += ke;

        // Add element load
        F[i] += fe;
        F[j] += fe;
    }

    // ── Apply Dirichlet BCs by elimination ────────────────────────────────────
    // Node 0: T(0) = T0
    // Node n: T(L) = TL
    auto apply_bc = [&](int node, double value) {
        // Modify RHS for all other rows
        for (int r = 0; r < n_nodes; ++r) {
            if (r != node) {
                F[r] -= K[r][node] * value;
                K[r][node] = 0.0;
                K[node][r] = 0.0;
            }
        }
        // Set diagonal to 1, RHS to value
        K[node][node] = 1.0;
        F[node] = value;
    };

    apply_bc(0, T0);
    apply_bc(n_nodes - 1, TL);

    // ── Solve K*T = F ─────────────────────────────────────────────────────────
    gaussian_elimination(K, F, n_nodes);
    // F now holds the solution vector T
    const std::vector<double>& T = F;

    // ── Build result ──────────────────────────────────────────────────────────
    FEAResult res;
    res.n_elements = n_elements;
    res.x_coords.resize(n_nodes);
    res.temperatures.resize(n_nodes);

    for (int i = 0; i < n_nodes; ++i) {
        res.x_coords[i]    = i * Le;
        res.temperatures[i] = T[i];
    }

    // Midpoint values (element interiors)
    res.x_midpoints.resize(n_elements);
    res.midpoint_temps_fe.resize(n_elements);
    res.midpoint_temps_exact.resize(n_elements);
    res.errors.resize(n_elements);
    res.max_error = 0.0;

    for (int e = 0; e < n_elements; ++e) {
        double xm = (e + 0.5) * Le;
        double t_fe = 0.5 * (T[e] + T[e + 1]); // linear interpolation
        double t_ex = exact_temperature(xm);

        res.x_midpoints[e]        = xm;
        res.midpoint_temps_fe[e]  = t_fe;
        res.midpoint_temps_exact[e] = t_ex;
        res.errors[e]             = std::abs(t_fe - t_ex);
        res.max_error             = std::max(res.max_error, res.errors[e]);
    }

    // ── Export CSV ────────────────────────────────────────────────────────────
    try {
        std::filesystem::create_directories("tests/results");
        std::ostringstream fname;
        fname << "tests/results/fea_N" << n_elements << ".csv";
        std::ofstream csv(fname.str());
        if (csv.is_open()) {
            csv << "x_m,T_FE_C,T_exact_C,error_C\n";
            csv << std::scientific;
            for (int e = 0; e < n_elements; ++e) {
                csv << res.x_midpoints[e] << ","
                    << res.midpoint_temps_fe[e] << ","
                    << res.midpoint_temps_exact[e] << ","
                    << res.errors[e] << "\n";
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[FEA] Warning: could not write CSV: " << ex.what() << "\n";
    }

    return res;
}

} // namespace fea
