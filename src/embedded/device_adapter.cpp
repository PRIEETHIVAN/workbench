/**
 * device_adapter.cpp
 * Mock embedded device adapter implementation.
 *
 * Simulated timing:
 *   MCU  (ID 0) — 50 ms connect / 20 ms telemetry
 *   FPGA (ID 1) — 100 ms connect / 30 ms telemetry
 *   JTAG (ID 2) — first connect: 200 ms then "timeout" error
 *                 retry connect: 150 ms then success
 *
 * All JSON is hand-constructed; no external library is required.
 */

#include "device_adapter.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace embedded {

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static std::string iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val{};
#if defined(_WIN32)
    gmtime_s(&tm_val, &tt);
#else
    gmtime_r(&tt, &tm_val);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static std::string escape_json_str(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        if      (c == '"')  out << "\\\"";
        else if (c == '\\') out << "\\\\";
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else                out << c;
    }
    return out.str();
}

std::string DeviceAdapter::device_type_str(DeviceType t) {
    switch (t) {
        case DeviceType::Microcontroller: return "Microcontroller";
        case DeviceType::FPGA:            return "FPGA";
        case DeviceType::JTAGAdapter:     return "JTAGAdapter";
    }
    return "Unknown";
}

std::string DeviceAdapter::device_info_to_json(const DeviceInfo& d) {
    std::ostringstream j;
    j << "{"
      << "\"id\":" << d.id << ","
      << "\"name\":\"" << escape_json_str(d.name) << "\","
      << "\"type\":\"" << device_type_str(d.type) << "\","
      << "\"port\":\"" << escape_json_str(d.port) << "\","
      << "\"voltage\":\"" << escape_json_str(d.voltage) << "\","
      << "\"protocol\":\"" << escape_json_str(d.protocol) << "\","
      << "\"connected\":" << (d.connected ? "true" : "false") << ","
      << "\"status\":\"" << escape_json_str(d.status) << "\""
      << "}";
    return j.str();
}

std::string DeviceAdapter::telemetry_to_json(const TelemetryMsg& m) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(3);
    j << "{"
      << "\"device_name\":\"" << escape_json_str(m.device_name) << "\","
      << "\"timestamp\":\"" << escape_json_str(m.timestamp) << "\","
      << "\"temperature_C\":" << m.temperature_C << ","
      << "\"voltage_V\":" << m.voltage_V << ","
      << "\"status_code\":" << m.status_code << ","
      << "\"message\":\"" << escape_json_str(m.message) << "\""
      << "}";
    return j.str();
}

std::string DeviceAdapter::error_json(const std::string& msg) const {
    return "{\"success\":false,\"error\":\"" + escape_json_str(msg) + "\"}";
}

// ─── Constructor ──────────────────────────────────────────────────────────────
DeviceAdapter::DeviceAdapter() : jtag_first_connect_attempted_(false) {
    devices_ = {
        { 0, "STM32F4 Discovery",  DeviceType::Microcontroller, "COM3",   "3.3V",   "UART", false, "idle" },
        { 1, "Xilinx Artix-7",    DeviceType::FPGA,            "SPI0",   "1.8V",   "SPI",  false, "idle" },
        { 2, "Segger J-Link",      DeviceType::JTAGAdapter,     "USB",    "varies", "JTAG", false, "idle" },
    };
}

// ─── discover() ───────────────────────────────────────────────────────────────
std::string DeviceAdapter::discover() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Simulate a short scan delay
    sleep_ms(30);

    std::ostringstream j;
    j << "{\"success\":true,\"devices\":[";
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (i > 0) j << ",";
        j << device_info_to_json(devices_[i]);
    }
    j << "],\"count\":" << devices_.size() << "}";
    return j.str();
}

// ─── connect(id) ──────────────────────────────────────────────────────────────
std::string DeviceAdapter::connect(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id < 0 || id >= static_cast<int>(devices_.size())) {
        return error_json("Invalid device id: " + std::to_string(id));
    }

    DeviceInfo& dev = devices_[id];

    switch (dev.type) {
        case DeviceType::Microcontroller:
            sleep_ms(50);
            dev.connected = true;
            dev.status    = "connected";
            break;

        case DeviceType::FPGA:
            sleep_ms(100);
            dev.connected = true;
            dev.status    = "connected";
            break;

        case DeviceType::JTAGAdapter:
            if (!jtag_first_connect_attempted_) {
                // Simulate first-attempt timeout
                jtag_first_connect_attempted_ = true;
                sleep_ms(200);
                dev.status = "timeout";
                return "{\"success\":false,"
                       "\"error\":\"JTAG adapter timed out. Please retry.\","
                       "\"device\":" + device_info_to_json(dev) + "}";
            }
            // Retry succeeds
            sleep_ms(150);
            dev.connected = true;
            dev.status    = "connected";
            break;
    }

    std::ostringstream j;
    j << "{\"success\":true,"
      << "\"message\":\"Connected to " << escape_json_str(dev.name) << "\","
      << "\"device\":" << device_info_to_json(dev)
      << "}";
    return j.str();
}

// ─── disconnect(id) ──────────────────────────────────────────────────────────
std::string DeviceAdapter::disconnect(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id < 0 || id >= static_cast<int>(devices_.size())) {
        return error_json("Invalid device id: " + std::to_string(id));
    }
    DeviceInfo& dev = devices_[id];
    dev.connected = false;
    dev.status    = "idle";

    std::ostringstream j;
    j << "{\"success\":true,"
      << "\"message\":\"Disconnected " << escape_json_str(dev.name) << "\","
      << "\"device\":" << device_info_to_json(dev)
      << "}";
    return j.str();
}

// ─── get_telemetry(id) ────────────────────────────────────────────────────────
std::string DeviceAdapter::get_telemetry(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id < 0 || id >= static_cast<int>(devices_.size())) {
        return error_json("Invalid device id: " + std::to_string(id));
    }
    const DeviceInfo& dev = devices_[id];
    if (!dev.connected) {
        return error_json("Device " + std::to_string(id) + " is not connected.");
    }

    TelemetryMsg msg;
    msg.device_name = dev.name;
    msg.timestamp   = iso_timestamp();
    msg.status_code = 0;

    // Simulated readings per device type
    switch (dev.type) {
        case DeviceType::Microcontroller:
            sleep_ms(20);
            msg.temperature_C = 42.7;
            msg.voltage_V     = 3.29;
            msg.message       = "MCU nominal. Clock 168 MHz. Flash: 512 KB.";
            break;

        case DeviceType::FPGA:
            sleep_ms(30);
            msg.temperature_C = 58.3;
            msg.voltage_V     = 1.79;
            msg.message       = "FPGA nominal. LUT utilisation 34%. PLL locked.";
            break;

        case DeviceType::JTAGAdapter:
            sleep_ms(25);
            msg.temperature_C = 35.1;
            msg.voltage_V     = 3.28;
            msg.message       = "J-Link firmware v7.94. Target supply OK.";
            break;
    }

    std::ostringstream j;
    j << "{\"success\":true,\"telemetry\":" << telemetry_to_json(msg) << "}";
    return j.str();
}

// ─── get_capabilities(id) ────────────────────────────────────────────────────
std::string DeviceAdapter::get_capabilities(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id < 0 || id >= static_cast<int>(devices_.size())) {
        return error_json("Invalid device id: " + std::to_string(id));
    }
    const DeviceInfo& dev = devices_[id];

    std::ostringstream j;
    j << "{\"success\":true,\"device_id\":" << id << ",\"capabilities\":{";

    switch (dev.type) {
        case DeviceType::Microcontroller:
            j << "\"core\":\"ARM Cortex-M4 @ 168 MHz\","
              << "\"flash_kb\":512,"
              << "\"ram_kb\":192,"
              << "\"peripherals\":[\"UART\",\"SPI\",\"I2C\",\"ADC\",\"DAC\",\"TIM\"],"
              << "\"debug_interface\":\"SWD/JTAG\","
              << "\"voltage_range\":\"1.8–3.6 V\"";
            break;

        case DeviceType::FPGA:
            j << "\"family\":\"Artix-7\","
              << "\"luts\":33280,"
              << "\"block_ram_kb\":1800,"
              << "\"dsp_slices\":90,"
              << "\"io_pins\":285,"
              << "\"interfaces\":[\"SPI\",\"I2C\",\"LVDS\",\"PCIe x1\"],"
              << "\"config_mode\":\"JTAG / SPI Flash\"";
            break;

        case DeviceType::JTAGAdapter:
            j << "\"max_speed_khz\":50000,"
              << "\"supported_targets\":[\"ARM\",\"RISC-V\",\"FPGA\"],"
              << "\"protocols\":[\"SWD\",\"JTAG\",\"cJTAG\"],"
              << "\"usb_version\":\"USB 2.0 Hi-Speed\","
              << "\"rtt_support\":true,"
              << "\"trace_support\":\"ETM/ETB\"";
            break;
    }

    j << "}}";
    return j.str();
}

} // namespace embedded
