#pragma once
/**
 * device_adapter.hpp
 * Mock embedded device adapters for the Unified Engineering Workbench.
 *
 * Simulates three hardware devices:
 *   ID 0 — STM32F4 Microcontroller  (UART, 3.3V)
 *   ID 1 — Xilinx Artix-7 FPGA      (SPI, 1.8V)
 *   ID 2 — Segger J-Link JTAG Adapter (JTAG, varies)
 *
 * All public methods return JSON-formatted std::string (manual construction,
 * no external library).  Simulated delays mimic real hardware latency.
 *
 * Recoverable failure:
 *   The JTAG adapter (ID 2) times out on the FIRST connect attempt;
 *   a retry succeeds.  This tests error-recovery paths in the UI.
 */

#include <string>
#include <vector>
#include <mutex>

namespace embedded {

enum class DeviceType { Microcontroller, FPGA, JTAGAdapter };

struct DeviceInfo {
    int         id;
    std::string name;
    DeviceType  type;
    std::string port;
    std::string voltage;
    std::string protocol;
    bool        connected;
    std::string status;
};

struct TelemetryMsg {
    std::string device_name;
    std::string timestamp;
    double      temperature_C;
    double      voltage_V;
    int         status_code;
    std::string message;
};

/**
 * DeviceAdapter — facade over the three mock devices.
 *
 * Thread-safety: not thread-safe; intended for single-threaded HTTP handlers.
 */
class DeviceAdapter {
public:
    DeviceAdapter();

    /// Scan for available devices; returns JSON array of DeviceInfo.
    std::string discover();

    /// Connect to device by id; returns JSON with success/failure.
    /// JTAG device (id=2) fails on first attempt with "timeout".
    std::string connect(int id);

    /// Disconnect device; returns JSON confirmation.
    std::string disconnect(int id);

    /// Fetch simulated telemetry; returns JSON TelemetryMsg.
    std::string get_telemetry(int id);

    /// Return capability descriptor JSON for the device.
    std::string get_capabilities(int id);

private:
    std::vector<DeviceInfo> devices_;
    mutable std::mutex mutex_;
    bool jtag_first_connect_attempted_;  ///< tracks recoverable-failure state

    static std::string device_type_str(DeviceType t);
    static std::string device_info_to_json(const DeviceInfo& d);
    static std::string telemetry_to_json(const TelemetryMsg& msg);
    std::string error_json(const std::string& msg) const;
};

} // namespace embedded
