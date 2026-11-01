/**
 * csv_parser.cpp
 * ANSYS nodal temperature CSV parser implementation.
 */

#include "csv_parser.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_set>

namespace viz {

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t lo = s.find_first_not_of(" \t\r\n");
    if (lo == std::string::npos) return "";
    size_t hi = s.find_last_not_of(" \t\r\n");
    return s.substr(lo, hi - lo + 1);
}

/// Split a line on commas, trimming each field.
static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(trim(field));
    }
    return fields;
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// ─── Expected header columns ─────────────────────────────────────────────────
static const std::vector<std::string> EXPECTED_COLS = {
    "node_id", "x_m", "y_m", "z_m", "temperature_c"
};

// ─── parse_temperature_csv ────────────────────────────────────────────────────
CSVResult parse_temperature_csv(const std::string& content) {
    CSVResult result;
    result.valid           = false;
    result.file_size_bytes = content.size();

    if (content.empty()) {
        result.error_msg = "Empty CSV content.";
        return result;
    }

    std::vector<std::string> lines;
    {
        std::string text = content;
        if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF) text.erase(0, 3);
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            std::string t = trim(line);
            if (!t.empty()) lines.push_back(t);
        }
    }

    if (lines.empty()) {
        result.error_msg = "No non-empty lines in CSV.";
        return result;
    }

    // ── Validate header ───────────────────────────────────────────────────────
    std::vector<std::string> header = split_csv_line(lines[0]);
    if (header.size() != 5) {
        result.error_msg = "Header must contain exactly 5 columns. "
                           "Expected: node_id,x_m,y_m,z_m,temperature_C";
        return result;
    }
    for (size_t c = 0; c < EXPECTED_COLS.size(); ++c) {
        if (to_lower(header[c]) != EXPECTED_COLS[c]) {
            result.error_msg = "Column " + std::to_string(c) +
                               " expected \"" + EXPECTED_COLS[c] +
                               "\" but got \"" + header[c] + "\".";
            return result;
        }
    }

    // ── Parse data rows ───────────────────────────────────────────────────────
    std::unordered_set<int> seen_ids;
    for (size_t row = 1; row < lines.size(); ++row) {
        std::vector<std::string> fields = split_csv_line(lines[row]);
        if (fields.size() != 5) {
            result.error_msg = "Row " + std::to_string(row) +
                               " must contain exactly 5 fields.";
            return result;
        }

        Node node;
        try {
            size_t consumed = 0;
            node.id = std::stoi(fields[0], &consumed);
            if (consumed != fields[0].size() || node.id <= 0) {
                result.error_msg = "Row " + std::to_string(row) +
                                   ": node_id must be a positive integer.";
                return result;
            }
        } catch (...) {
            result.error_msg = "Row " + std::to_string(row) +
                               ": could not parse node_id \"" + fields[0] + "\".";
            return result;
        }

        if (!seen_ids.insert(node.id).second) {
            result.error_msg = "Row " + std::to_string(row) +
                               ": duplicate node_id " + std::to_string(node.id) + ".";
            return result;
        }

        auto parse_double = [&](const std::string& tok, double& out,
                                 const std::string& col_name) -> bool {
            try {
                size_t consumed = 0;
                out = std::stod(tok, &consumed);
                if (consumed != tok.size()) throw std::invalid_argument("trailing characters");
            } catch (...) {
                result.error_msg = "Row " + std::to_string(row) +
                                   ": could not parse " + col_name +
                                   " \"" + tok + "\".";
                return false;
            }
            if (!std::isfinite(out)) {
                result.error_msg = "Row " + std::to_string(row) +
                                   ": non-finite value in " + col_name + ".";
                return false;
            }
            return true;
        };

        if (!parse_double(fields[1], node.x,    "x_m"))    return result;
        if (!parse_double(fields[2], node.y,    "y_m"))    return result;
        if (!parse_double(fields[3], node.z,    "z_m"))    return result;
        if (!parse_double(fields[4], node.temp, "temperature_C")) return result;

        result.nodes.push_back(node);
    }

    if (result.nodes.empty()) {
        result.error_msg = "CSV contains a header but no node rows.";
        return result;
    }
    result.valid = true;
    return result;
}

// ─── csv_to_json ──────────────────────────────────────────────────────────────
std::string csv_to_json(const CSVResult& r) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(6);

    j << "{"
      << "\"valid\":" << (r.valid ? "true" : "false") << ","
      << "\"node_count\":" << r.nodes.size() << ","
      << "\"file_size_bytes\":" << r.file_size_bytes;

    if (!r.valid) {
        std::string em = r.error_msg;
        for (size_t pos = 0; (pos = em.find('"', pos)) != std::string::npos; ) {
            em.replace(pos, 1, "\\\""); pos += 2;
        }
        j << ",\"error_msg\":\"" << em << "\"";
    }

    if (r.valid) {
        j << ",\"nodes\":[";
        for (size_t i = 0; i < r.nodes.size(); ++i) {
            if (i > 0) j << ",";
            const auto& n = r.nodes[i];
            j << "{"
              << "\"id\":"   << n.id   << ","
              << "\"x\":"    << n.x    << ","
              << "\"y\":"    << n.y    << ","
              << "\"z\":"    << n.z    << ","
              << "\"temp\":" << n.temp
              << "}";
        }
        j << "]";
    }

    j << "}";
    return j.str();
}

} // namespace viz
