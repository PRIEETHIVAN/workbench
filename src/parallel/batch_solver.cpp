/**
 * batch_solver.cpp
 * Parallel batch thermal FEM solver — C++17 std::thread implementation.
 *
 * Thread pool design:
 *   - A fixed-size pool of `num_workers` threads is created once.
 *   - Tasks (indices into the cases vector) are pushed to a shared queue.
 *   - Workers pop tasks under a mutex, run the solver, and write the result
 *     into a pre-allocated output array (no result-side locking needed).
 *   - A condition_variable wakes idle workers; a `done` flag drains the pool.
 *
 * Reproducibility:
 *   - generate_cases() uses a seeded LCG so both serial and parallel paths
 *     receive identical SolveCase structs — guaranteed identical results.
 */

#include "batch_solver.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

// ─── Local solver (parameterised) ────────────────────────────────────────────
// Re-implement a lightweight version of fem_solver that accepts custom k/q/T0/TL
// so we don't impose a global state change on the fea:: module.

namespace {

static void gauss_solve(std::vector<std::vector<double>>& A,
                        std::vector<double>& b, int n) {
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int r = col + 1; r < n; ++r)
            if (std::abs(A[r][col]) > std::abs(A[pivot][col])) pivot = r;
        std::swap(A[col], A[pivot]);
        std::swap(b[col], b[pivot]);
        if (std::abs(A[col][col]) < 1e-14) throw std::runtime_error("Singular matrix");
        for (int r = col + 1; r < n; ++r) {
            double f = A[r][col] / A[col][col];
            for (int j = col; j < n; ++j) A[r][j] -= f * A[col][j];
            b[r] -= f * b[col];
        }
    }
    for (int r = n - 1; r >= 0; --r) {
        b[r] /= A[r][r];
        for (int j = r - 1; j >= 0; --j) {
            b[j] -= A[j][r] * b[r];
            A[j][r] = 0.0;
        }
    }
}

/// Returns max |T_FE - T_exact| for a parameterised 1D thermal problem.
std::pair<double, std::vector<double>> solve_case_result(
        int n_elements, double k, double q, double T0, double TL,
        double L = 1.0, double A_cs = 1e-4) {
    const int    nn = n_elements + 1;
    const double Le = L / n_elements;

    std::vector<std::vector<double>> K(nn, std::vector<double>(nn, 0.0));
    std::vector<double> F(nn, 0.0);

    for (int e = 0; e < n_elements; ++e) {
        int i = e, j = e + 1;
        double ke = k * A_cs / Le;
        double fe = q * A_cs * Le / 2.0;
        K[i][i] += ke; K[i][j] -= ke;
        K[j][i] -= ke; K[j][j] += ke;
        F[i] += fe;    F[j] += fe;
    }

    auto apply_bc = [&](int node, double val) {
        for (int r = 0; r < nn; ++r) {
            if (r != node) {
                F[r] -= K[r][node] * val;
                K[r][node] = 0.0; K[node][r] = 0.0;
            }
        }
        K[node][node] = 1.0; F[node] = val;
    };
    apply_bc(0, T0);
    apply_bc(nn - 1, TL);

    gauss_solve(K, F, nn);
    const auto& T = F;

    // Exact solution: T_exact(x) = T0 - (T0-TL)*x/L + q/(2k)*x*(L-x)
    double max_err = 0.0;
    for (int e = 0; e < n_elements; ++e) {
        double xm   = (e + 0.5) * Le;
        double t_fe = 0.5 * (T[e] + T[e + 1]);
        double t_ex = T0 - (T0 - TL) * xm / L + (q / (2.0 * k)) * xm * (L - xm);
        max_err = std::max(max_err, std::abs(t_fe - t_ex));
    }
    return {max_err, T};
}

} // anonymous namespace

// ─── namespace parallel_solver ───────────────────────────────────────────────
namespace parallel_solver {

// ── Simple LCG PRNG (seed 42) ─────────────────────────────────────────────
struct LCG {
    uint64_t state;
    explicit LCG(uint64_t seed = 42) : state(seed) {}
    uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
    /// Returns a double in [lo, hi)
    double uniform(double lo, double hi) {
        return lo + (static_cast<double>(next() >> 11) /
                     static_cast<double>(1ULL << 53)) * (hi - lo);
    }
    /// Returns an int in [lo, hi]
    int uniform_int(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<uint64_t>(hi - lo + 1));
    }
};

std::vector<SolveCase> generate_cases(int count, int mesh_size) {
    LCG rng(42);
    std::vector<SolveCase> cases;
    cases.reserve(count);
    for (int i = 0; i < count; ++i) {
        SolveCase sc;
        sc.case_id    = i;
        // n_elements in [max(10, mesh_size/2), mesh_size*2] clamped to [10,80]
        int lo = std::max(10, mesh_size / 2);
        int hi = std::min(80, mesh_size * 2);
        if (lo > hi) lo = hi;
        sc.n_elements = rng.uniform_int(lo, hi);
        sc.k          = rng.uniform(5.0,   50.0);
        sc.q          = rng.uniform(5000.0, 50000.0);
        sc.T0         = 100.0;
        sc.TL         = 20.0;
        cases.push_back(sc);
    }
    return cases;
}

// ── Serial run ────────────────────────────────────────────────────────────
BatchResult run_batch_serial(const std::vector<SolveCase>& cases) {
    BatchResult br;
    br.worker_count = 1;
    br.case_max_errors.resize(cases.size());
    br.case_temperatures.resize(cases.size());

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < cases.size(); ++i) {
        const auto& c = cases[i];
        auto solved = solve_case_result(c.n_elements, c.k, c.q, c.T0, c.TL);
        br.case_max_errors[i] = {c.case_id, solved.first};
        br.case_temperatures[i] = std::move(solved.second);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    br.wall_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    br.speedup    = 1.0;
    br.efficiency = 1.0;
    return br;
}

// ── Parallel run (thread pool) ────────────────────────────────────────────
BatchResult run_batch_parallel(const std::vector<SolveCase>& cases, int num_workers) {
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    num_workers = std::max(1, std::min(num_workers, hw > 0 ? hw : 16));

    const int n = static_cast<int>(cases.size());
    BatchResult br;
    br.worker_count = num_workers;
    br.case_max_errors.resize(n);
    br.case_temperatures.resize(n);

    // Task queue
    std::queue<int>        task_queue;
    std::mutex             mtx;
    std::condition_variable cv;
    bool done = false;

    for (int i = 0; i < n; ++i) task_queue.push(i);

    // Worker lambda
    auto worker = [&]() {
        while (true) {
            int idx = -1;
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&]{ return !task_queue.empty() || done; });
                if (task_queue.empty()) return;
                idx = task_queue.front();
                task_queue.pop();
            }
            const auto& c = cases[idx];
            auto solved = solve_case_result(c.n_elements, c.k, c.q, c.T0, c.TL);
            // Each index is unique — no result-side lock needed
            br.case_max_errors[idx] = {c.case_id, solved.first};
            br.case_temperatures[idx] = std::move(solved.second);
        }
    };

    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(num_workers);
    for (int t = 0; t < num_workers; ++t) threads.emplace_back(worker);

    // Notify all workers to start draining the queue
    cv.notify_all();

    // Signal done after all tasks are consumed
    // Workers stop when queue is empty AND done is true
    {
        std::unique_lock<std::mutex> lk(mtx);
        done = true;
    }
    cv.notify_all();

    for (auto& th : threads) th.join();

    auto t1 = std::chrono::high_resolution_clock::now();
    br.wall_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Speedup and efficiency are computed externally (caller compares with serial).
    // We set placeholder values; main.cpp will fill speedup after timing both.
    br.speedup    = 1.0;
    br.efficiency = 1.0 / num_workers;
    return br;
}

} // namespace parallel_solver
