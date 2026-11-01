/**
 * main.cpp
 * Unified Engineering Workbench â€” HTTP server entry point.
 *
 * Uses cpp-httplib (single-header) to serve:
 *   â€¢ Static files from ./frontend/
 *   â€¢ REST API endpoints under /api/
 *
 * All responses include CORS headers.
 * JSON request bodies are parsed with simple substring extraction
 * (no external JSON library required).
 *
 * Endpoints:
 *   POST /api/fea/solve         { "n_elements": 20 }
 *   POST /api/parallel/run      { "case_count": 20, "mesh_size": 20, "max_workers": 4 }
 *   POST /api/embedded/discover (no body)
 *   POST /api/embedded/connect  { "device_id": 0 }
 *   POST /api/embedded/telemetry { "device_id": 0 }
 *   POST /api/backprop/run      { "steps": 100, "lr": 0.1 }
 *   POST /api/stl/parse         { "content": "<ASCII STL>" }
 *   POST /api/csv/parse         { "content": "<CSV text>" }
 */

#define CPPHTTPLIB_OPENSSL_SUPPORT 0
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#include "../server/httplib.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "fea/fem_solver.hpp"
#include "parallel/batch_solver.hpp"
#include "embedded/device_adapter.hpp"
#include "backprop/backprop.hpp"
#include "viz/stl_parser.hpp"
#include "viz/csv_parser.hpp"

// â”€â”€â”€ JSON extraction helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

/// Extract the integer value for a given JSON key.
/// Returns default_val if the key is not found.
static int extract_int(const std::string& body, const std::string& key,
                        int default_val = 0) {
    // Search for "key": <digits>
    std::string search = "\"" + key + "\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return default_val;

    // Skip past key, whitespace, colon, whitespace
    pos += search.size();
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' ||
                                  body[pos] == '\n' || body[pos] == ':')) ++pos;
    // Read integer (possibly negative)
    size_t start = pos;
    if (pos < body.size() && (body[pos] == '-' || body[pos] == '+')) ++pos;
    while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos]))) ++pos;
    if (start == pos) return default_val;
    try { return std::stoi(body.substr(start, pos - start)); }
    catch (...) { return default_val; }
}

/// Extract a double value for a given JSON key.
static double extract_double(const std::string& body, const std::string& key,
                              double default_val = 0.0) {
    std::string search = "\"" + key + "\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return default_val;

    pos += search.size();
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' ||
                                  body[pos] == '\n' || body[pos] == ':')) ++pos;
    size_t start = pos;
    // Accept: -, digits, '.', 'e', 'E', '+', '-'
    if (pos < body.size() && body[pos] == '-') ++pos;
    while (pos < body.size() && (std::isdigit(static_cast<unsigned char>(body[pos])) ||
                                  body[pos] == '.' || body[pos] == 'e' ||
                                  body[pos] == 'E' || body[pos] == '+' ||
                                  body[pos] == '-')) ++pos;
    if (start == pos) return default_val;
    try { return std::stod(body.substr(start, pos - start)); }
    catch (...) { return default_val; }
}

/**
 * Extract a JSON string value for a given key, unescaping basic sequences.
 * Handles multi-line / embedded escaped characters in the value.
 */
static std::string extract_string(const std::string& body,
                                   const std::string& key,
                                   const std::string& default_val = "") {
    std::string search = "\"" + key + "\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return default_val;

    // Find the opening quote of the value
    pos = body.find('"', pos + search.size());
    if (pos == std::string::npos) return default_val;
    ++pos; // skip the opening quote

    std::string result;
    result.reserve(512);
    while (pos < body.size()) {
        char c = body[pos++];
        if (c == '\\' && pos < body.size()) {
            char esc = body[pos++];
            switch (esc) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += esc;  break;
            }
        } else if (c == '"') {
            break; // closing quote
        } else {
            result += c;
        }
    }
    return result;
}

// â”€â”€â”€ CORS helper â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static void add_cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// â”€â”€â”€ JSON serialisation helpers for API responses â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static std::string fea_result_to_json(const fea::FEAResult& r) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(8);

    auto vec_to_arr = [&](const std::vector<double>& v) {
        std::ostringstream a;
        a << std::fixed << std::setprecision(8);
        a << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) a << ",";
            a << v[i];
        }
        a << "]";
        return a.str();
    };

    j << "{"
      << "\"n_elements\":"          << r.n_elements          << ","
      << "\"max_error\":"           << r.max_error           << ","
      << "\"x_coords\":"            << vec_to_arr(r.x_coords) << ","
      << "\"temperatures\":"        << vec_to_arr(r.temperatures) << ","
      << "\"x_midpoints\":"         << vec_to_arr(r.x_midpoints) << ","
      << "\"midpoint_temps_fe\":"   << vec_to_arr(r.midpoint_temps_fe) << ","
      << "\"midpoint_temps_exact\":" << vec_to_arr(r.midpoint_temps_exact) << ","
      << "\"errors\":"              << vec_to_arr(r.errors)
      << "}";
    return j.str();
}

static std::string batch_result_to_json(
        const parallel_solver::BatchResult& serial,
        const parallel_solver::BatchResult& parallel_r,
        double speedup, double efficiency) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(4);

    j << "{"
      << "\"serial_wall_ms\":"   << serial.wall_time_ms   << ","
      << "\"parallel_wall_ms\":" << parallel_r.wall_time_ms << ","
      << "\"worker_count\":"     << parallel_r.worker_count << ","
      << "\"speedup\":"          << speedup                << ","
      << "\"efficiency\":"       << efficiency             << ","
      << "\"hw_concurrency\":"   << std::thread::hardware_concurrency() << ","
      << "\"case_count\":"       << serial.case_max_errors.size() << ","
      << "\"serial_errors\":[";
    for (size_t i = 0; i < serial.case_max_errors.size(); ++i) {
        if (i) j << ",";
        j << "{\"case_id\":" << serial.case_max_errors[i].first
          << ",\"max_error\":" << std::setprecision(8)
          << serial.case_max_errors[i].second << "}";
    }
    j << "],\"parallel_errors\":[";
    for (size_t i = 0; i < parallel_r.case_max_errors.size(); ++i) {
        if (i) j << ",";
        j << "{\"case_id\":" << parallel_r.case_max_errors[i].first
          << ",\"max_error\":" << std::setprecision(8)
          << parallel_r.case_max_errors[i].second << "}";
    }
    j << "]}";
    return j.str();
}

static double median_of(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

static int physical_core_count() {
#ifdef _WIN32
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bytes > 0) {
        std::vector<unsigned char> buffer(bytes);
        auto* item = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, item, &bytes)) {
            int count = 0;
            for (DWORD offset = 0; offset < bytes;) {
                ++count;
                const auto* current = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
                if (current->Size == 0) break;
                offset += current->Size;
            }
            return count;
        }
    }
#endif
    return 0;
}

static std::string backprop_result_to_json(
        const backprop::ForwardResult& fr,
        const backprop::GradResult& gr,
        const std::vector<backprop::GradCheckResult>& gc,
        const std::vector<std::pair<int,double>>& history,
        const backprop::BackpropParams& init_p) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(8);

    j << "{"
      << "\"initial_params\":{"
      <<   "\"w\":" << init_p.w << ","
      <<   "\"b\":" << init_p.b << ","
      <<   "\"v\":" << init_p.v << ","
      <<   "\"c\":" << init_p.c
      << "},"
      << "\"forward\":{"
      <<   "\"z\":"     << fr.z     << ","
      <<   "\"h\":"     << fr.h     << ","
      <<   "\"y_hat\":" << fr.y_hat << ","
      <<   "\"loss\":"  << fr.loss
      << "},"
      << "\"gradients\":{"
      <<   "\"dJ_dw\":" << gr.dJ_dw << ","
      <<   "\"dJ_db\":" << gr.dJ_db << ","
      <<   "\"dJ_dv\":" << gr.dJ_dv << ","
      <<   "\"dJ_dc\":" << gr.dJ_dc
      << "},"
      << "\"grad_check\":[";
    for (size_t i = 0; i < gc.size(); ++i) {
        if (i) j << ",";
        j << "{"
          << "\"param\":\"" << gc[i].param_name << "\","
          << "\"analytic\":"  << gc[i].analytic_grad << ","
          << "\"numeric\":"   << gc[i].numeric_grad  << ","
          << "\"abs_diff\":"  << gc[i].abs_diff
          << "}";
    }
    j << "],"
      << "\"loss_history\":[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i) j << ",";
        j << "{\"step\":" << history[i].first
          << ",\"loss\":"  << history[i].second << "}";
    }
    j << "]}";
    return j.str();
}

// â”€â”€â”€ main â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int main() {
    httplib::Server svr;

    // Register the pinned browser bundles before the catch-all frontend mount.
    const auto mount_asset = [&svr](const std::string& url, const std::string& name) {
        svr.Get(url, [name](const httplib::Request&, httplib::Response& res) {
            add_cors(res);
            std::ifstream input("./frontend/" + name, std::ios::binary);
            if (!input) { res.status = 404; return; }
            std::string contents((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
            res.set_content(std::move(contents), "application/javascript; charset=utf-8");
        });
    };
    mount_asset("/assets/three-r134.min.js", "three-r134.min.js");
    mount_asset("/assets/OrbitControls-r134.js", "OrbitControls-r134.js");
    mount_asset("/assets/chart-4.4.0.umd.min.js", "chart-4.4.0.umd.min.js");

    if (!svr.set_mount_point("/data", "./data")) {
        std::cerr << "[WARN] Could not mount ./data; bundled examples will not load.\n";
    }
    if (!svr.set_mount_point("/", "./frontend")) {
        std::cerr << "[WARN] Could not mount ./frontend; static files will not be served.\n";
    }

    // Persistent adapter (across requests, per process lifetime)
    embedded::DeviceAdapter device_adapter;

    // â”€â”€ CORS pre-flight â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Options("/api/.*", [](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.status = 204;
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/fea/solve
    // Body: { "n_elements": 20 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/fea/solve", [](const httplib::Request& req,
                                   httplib::Response& res) {
        add_cors(res);
        try {
            int n = extract_int(req.body, "n_elements", 10);
            if (n < 1)  n = 1;
            if (n > 500) n = 500; // safety cap

            fea::FEAResult r = fea::solve_thermal_1d(n);
            std::string json = fea_result_to_json(r);

            res.set_content(json, "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/parallel/run
    // Body: { "case_count": 20, "mesh_size": 20, "max_workers": 4 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/parallel/run", [](const httplib::Request& req,
                                      httplib::Response& res) {
        add_cors(res);
        try {
            int case_count  = extract_int(req.body, "case_count",  20);
            int mesh_size   = extract_int(req.body, "mesh_size",   20);
            int max_workers = extract_int(req.body, "num_workers",
                                           extract_int(req.body, "max_workers", 4));

            if (case_count < 1)  case_count = 1;
            if (case_count > 200) case_count = 200;
            if (mesh_size  < 4)  mesh_size  = 4;
            if (mesh_size  > 100) mesh_size = 100;
            if (max_workers < 1) max_workers = 1;

            auto cases = parallel_solver::generate_cases(case_count, mesh_size);

            // Warm each execution path once, then report medians of five paired runs.
            (void)parallel_solver::run_batch_serial(cases);
            (void)parallel_solver::run_batch_parallel(cases, max_workers);
            std::vector<double> serial_times, parallel_times;
            parallel_solver::BatchResult serial, par;
            for (int repetition = 0; repetition < 5; ++repetition) {
                serial = parallel_solver::run_batch_serial(cases);
                par = parallel_solver::run_batch_parallel(cases, max_workers);
                serial_times.push_back(serial.wall_time_ms);
                parallel_times.push_back(par.wall_time_ms);
            }
            serial.wall_time_ms = median_of(serial_times);
            par.wall_time_ms = median_of(parallel_times);

            double speedup    = (par.wall_time_ms > 0.0)
                                    ? serial.wall_time_ms / par.wall_time_ms
                                    : 1.0;
            double efficiency = speedup / static_cast<double>(par.worker_count);

            // Verify numerical agreement
            double max_diff = 0.0;
            for (size_t i = 0; i < serial.case_temperatures.size(); ++i) {
                for (size_t node = 0; node < serial.case_temperatures[i].size(); ++node) {
                    max_diff = std::max(max_diff, std::abs(
                        serial.case_temperatures[i][node] - par.case_temperatures[i][node]));
                }
            }

            std::ostringstream j;
            j << std::fixed << std::setprecision(8);
            j << batch_result_to_json(serial, par, speedup, efficiency);
            // Inject agreement field
            std::string out = j.str();
            // Insert before closing brace
            out.pop_back(); // remove '}'
            std::ostringstream tail;
            tail << std::fixed << std::setprecision(2);
            tail << ",\"max_serial_parallel_diff\":" << std::setprecision(10) << max_diff
                 << ",\"agreement_ok\":" << (max_diff <= 1e-8 ? "true" : "false")
                 << ",\"warmup_runs\":1,\"measured_repetitions\":5"
                 << "}";
            out += tail.str();

            res.set_content(out, "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/embedded/discover
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/embedded/discover", [&device_adapter](const httplib::Request&,
                                                          httplib::Response& res) {
        add_cors(res);
        try {
            res.set_content(device_adapter.discover(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/embedded/connect
    // Body: { "device_id": 0 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/embedded/connect", [&device_adapter](const httplib::Request& req,
                                                         httplib::Response& res) {
        add_cors(res);
        try {
            int id = extract_int(req.body, "device_id", 0);
            res.set_content(device_adapter.connect(id), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/embedded/disconnect
    // Body: { "device_id": 0 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/embedded/disconnect", [&device_adapter](const httplib::Request& req,
                                                            httplib::Response& res) {
        add_cors(res);
        try {
            int id = extract_int(req.body, "device_id", 0);
            res.set_content(device_adapter.disconnect(id), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/embedded/telemetry
    // Body: { "device_id": 0 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/embedded/telemetry", [&device_adapter](const httplib::Request& req,
                                                           httplib::Response& res) {
        add_cors(res);
        try {
            int id = extract_int(req.body, "device_id", 0);
            res.set_content(device_adapter.get_telemetry(id), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/embedded/capabilities
    // Body: { "device_id": 0 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/embedded/capabilities", [&device_adapter](const httplib::Request& req,
                                                              httplib::Response& res) {
        add_cors(res);
        try {
            int id = extract_int(req.body, "device_id", 0);
            res.set_content(device_adapter.get_capabilities(id), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/backprop/run
    // Body: { "steps": 100, "lr": 0.1 }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/backprop/run", [](const httplib::Request& req,
                                      httplib::Response& res) {
        add_cors(res);
        try {
            int    steps = extract_int(req.body, "steps", 100);
            double lr    = extract_double(req.body, "lr",   0.1);

            if (steps < 1)   steps = 1;
            if (steps > 10000) steps = 10000;
            if (lr <= 0.0)   lr = 0.01;
            if (lr > 10.0)   lr = 10.0;

            // Fixed single training sample
            const double x        = 1.0;
            const double y_target = 2.0;

            // Initial parameters
            backprop::BackpropParams init_p = {
                extract_double(req.body, "w", 0.5),
                extract_double(req.body, "b", 0.0),
                extract_double(req.body, "v", 1.0),
                extract_double(req.body, "c", 0.0)
            };

            // One forward + backward at initial params (for reporting)
            backprop::ForwardResult fr_init = backprop::forward_pass(init_p, x, y_target);
            backprop::GradResult    gr_init = backprop::backward_pass(init_p, fr_init, x, y_target);

            // Gradient check at initial params
            auto gc = backprop::check_gradients(init_p, x, y_target);

            // Training
            auto history = backprop::train(init_p, x, y_target, lr, steps);

            std::string json = backprop_result_to_json(fr_init, gr_init, gc, history, init_p);
            res.set_content(json, "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/stl/parse
    // Body: { "content": "<ASCII STL text>" }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/stl/parse", [](const httplib::Request& req,
                                   httplib::Response& res) {
        add_cors(res);
        try {
            std::string content = extract_string(req.body, "content");
            viz::STLResult r    = viz::parse_stl(content);
            res.set_content(viz::stl_to_json(r), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // POST /api/csv/parse
    // Body: { "content": "<CSV text>" }
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Post("/api/csv/parse", [](const httplib::Request& req,
                                   httplib::Response& res) {
        add_cors(res);
        try {
            std::string content = extract_string(req.body, "content");
            viz::CSVResult r    = viz::parse_temperature_csv(content);
            res.set_content(viz::csv_to_json(r), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + ex.what() + "\"}", "application/json");
        }
    });

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Health check
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.set_content("{\"status\":\"ok\",\"server\":\"Unified Engineering Workbench\"}", "application/json");
    });

    svr.Get("/api/embedded/devices", [&device_adapter](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.set_content(device_adapter.discover(), "application/json");
    });

    svr.Get("/api/system/info", [](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        unsigned long long ram_mb = 0;
#ifdef _WIN32
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory)) ram_mb = memory.ullTotalPhys / (1024ULL * 1024ULL);
#endif
        const char* cpu = std::getenv("PROCESSOR_IDENTIFIER");
        std::ostringstream json;
        json << "{\"cpu\":\"" << (cpu ? cpu : "Unavailable")
             << "\",\"logical_cores\":" << std::thread::hardware_concurrency()
             << ",\"physical_cores\":" << physical_core_count()
             << ",\"ram_mb\":" << ram_mb
             << ",\"compiler\":\"" << WORKBENCH_COMPILER_INFO
             << "\",\"build_flags\":\"" << WORKBENCH_BUILD_FLAGS << "\"}";
        res.set_content(json.str(), "application/json");
    });

    int port = 8765;
    if (const char* configured_port = std::getenv("WORKBENCH_PORT")) {
        try {
            const int candidate = std::stoi(configured_port);
            if (candidate >= 1024 && candidate <= 65535) port = candidate;
        } catch (...) { /* Use the documented default port. */ }
    }
    if (std::getenv("WORKBENCH_NO_BROWSER") == nullptr) {
        std::thread browser_thread([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            const std::string command = "start http://127.0.0.1:" + std::to_string(port);
            std::system(command.c_str());
        });
        browser_thread.detach();
    }

    std::cout << "==========================================================\n"
              << " Unified Engineering Workbench\n"
              << " Running at http://127.0.0.1:" << port << "\n"
              << " Press Ctrl+C to stop.\n"
              << "==========================================================\n";

    if (!svr.listen("127.0.0.1", port)) {
        std::cerr << "Could not listen on 127.0.0.1:" << port
                  << ". Choose another WORKBENCH_PORT if it is already in use.\n";
        return 1;
    }
    return 0;
}
