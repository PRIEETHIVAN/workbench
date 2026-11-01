#pragma once
/**
 * fem_solver.hpp
 * 1D Thermal FEM Solver (two-node linear elements)
 *
 * PDE: -d/dx(k*A * dT/dx) = q*A
 * Parameters: L=1.0m, A=1e-4 m^2, k=10 W/(m·K), q=10000 W/m^3
 * BCs: T(0)=100°C, T(L)=20°C
 * Exact: T_exact(x) = 100 - 80*x/L + (q/(2k))*x*(L-x)
 */

#include <vector>
#include <string>

namespace fea {

/// Results from a 1D thermal FEM solve
struct FEAResult {
    std::vector<double> x_coords;           ///< Node x-coordinates [m]
    std::vector<double> temperatures;       ///< FE nodal temperatures [°C]
    std::vector<double> x_midpoints;        ///< Element midpoint x-coords [m]
    std::vector<double> midpoint_temps_fe;  ///< FE temperature at midpoints [°C]
    std::vector<double> midpoint_temps_exact; ///< Exact temperature at midpoints [°C]
    std::vector<double> errors;             ///< |T_FE - T_exact| at midpoints [°C]
    double max_error;                       ///< Maximum absolute error [°C]
    int n_elements;                         ///< Number of elements used
};

/**
 * Solve the 1D steady-state thermal problem using linear FEM.
 *
 * @param n_elements  Number of equal-length finite elements (>= 1)
 * @return            FEAResult with nodal + midpoint data, errors, and max error
 *
 * Also exports a CSV to tests/results/fea_N{n_elements}.csv with columns:
 *   x_m, T_FE_C, T_exact_C, error_C
 */
FEAResult solve_thermal_1d(int n_elements);

} // namespace fea
