#include "backprop/backprop.hpp"
#include "embedded/device_adapter.hpp"
#include "fea/fem_solver.hpp"
#include "parallel/batch_solver.hpp"
#include "viz/csv_parser.hpp"
#include "viz/stl_parser.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#ifndef WORKBENCH_SOURCE_DIR
#define WORKBENCH_SOURCE_DIR "."
#endif

static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

static std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot open fixture: " + path);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

int main() {
    try {
        for (int n : {10, 20, 40, 80}) {
            const auto r = fea::solve_thermal_1d(n);
            const double expected = 10000.0 / (8.0 * 10.0 * n * n);
            require(std::abs(r.max_error - expected) < 1e-8,
                    "FEA midpoint error mismatch at N=" + std::to_string(n));
            require(std::abs(r.temperatures.front() - 100.0) < 1e-10, "left Dirichlet BC mismatch");
            require(std::abs(r.temperatures.back() - 20.0) < 1e-10, "right Dirichlet BC mismatch");
        }
        const auto invalid_fea = [&]() { fea::solve_thermal_1d(0); };
        bool fea_rejected = false;
        try { invalid_fea(); } catch (const std::invalid_argument&) { fea_rejected = true; }
        require(fea_rejected, "FEA must reject a zero-element mesh");

        const auto stl = viz::parse_stl(read_file(std::string(WORKBENCH_SOURCE_DIR) + "/data/sample.stl"));
        require(stl.valid && stl.triangles.size() == 12, "sample ASCII STL did not parse as 12 facets");
        const auto empty_stl = viz::parse_stl("");
        require(!empty_stl.valid && !empty_stl.error_msg.empty(),
                "empty STL must be rejected with an understandable error");
        const auto malformed_stl = viz::parse_stl("not an STL\n");
        require(!malformed_stl.valid && malformed_stl.error_msg.find("solid") != std::string::npos,
                "malformed STL must be rejected with a format hint");
        const auto facet_free_stl = viz::parse_stl("solid empty\nendsolid empty\n");
        require(!facet_free_stl.valid && !facet_free_stl.error_msg.empty(),
                "facet-free STL must be rejected with an understandable error");

        const auto csv = viz::parse_temperature_csv(read_file(std::string(WORKBENCH_SOURCE_DIR) + "/data/sample_temperatures.csv"));
        require(csv.valid && csv.nodes.size() == 30, "sample temperature CSV did not parse as 30 nodes");
        const auto empty_csv = viz::parse_temperature_csv("");
        require(!empty_csv.valid && !empty_csv.error_msg.empty(),
                "empty CSV must be rejected with an understandable error");
        const auto nonfinite_csv = viz::parse_temperature_csv(
            "node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,NaN\n");
        require(!nonfinite_csv.valid && nonfinite_csv.error_msg.find("non-finite") != std::string::npos,
                "non-finite CSV values must be rejected");
        const auto malformed_csv = viz::parse_temperature_csv(
            "node_id,x_m,y_m,z_m,temperature_C\n1,0x,0,0,20\n");
        require(!malformed_csv.valid && !malformed_csv.error_msg.empty(),
                "trailing characters in numeric CSV values must be rejected");

        const backprop::BackpropParams p{0.5, 0.0, 1.0, 0.0};
        const auto checks = backprop::check_gradients(p, 1.0, 2.0, 1e-6);
        require(checks.size() == 4, "expected four backprop gradient checks");
        for (const auto& check : checks) require(check.abs_diff <= 1e-5, "backprop gradient check failed: " + check.param_name);
        const auto history = backprop::train(p, 1.0, 2.0, 0.1, 100);
        require(history.size() == 101 && history.front().first == 0 && history.back().first == 100,
                "backprop history must include the initial state and all 100 updates");
        require(history.back().second < history.front().second, "backprop loss did not decrease");

        const auto cases = parallel_solver::generate_cases(8, 8);
        const auto serial = parallel_solver::run_batch_serial(cases);
        const auto parallel = parallel_solver::run_batch_parallel(cases, 2);
        require(serial.case_temperatures.size() == parallel.case_temperatures.size(), "batch size mismatch");
        for (size_t i = 0; i < serial.case_temperatures.size(); ++i) {
            require(serial.case_max_errors[i].first == parallel.case_max_errors[i].first,
                    "parallel batch result order mismatch");
            for (size_t j = 0; j < serial.case_temperatures[i].size(); ++j)
                require(std::abs(serial.case_temperatures[i][j] - parallel.case_temperatures[i][j]) <= 1e-8,
                        "serial/parallel nodal temperature mismatch");
        }

        embedded::DeviceAdapter devices;
        require(devices.discover().find("\"count\":3") != std::string::npos, "device discovery mismatch");
        require(devices.connect(0).find("\"success\":true") != std::string::npos, "mock MCU connect failed");
        require(devices.get_telemetry(0).find("\"success\":true") != std::string::npos, "mock telemetry failed");
        require(devices.connect(2).find("\"success\":false") != std::string::npos, "JTAG timeout recovery scenario missing");
        require(devices.connect(2).find("\"success\":true") != std::string::npos, "JTAG retry did not recover");

        std::cout << "All Workbench core checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Workbench check failed: " << error.what() << "\n";
        return 1;
    }
}
