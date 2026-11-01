/**
 * stl_parser.cpp
 * ASCII STL parser implementation.
 *
 * Parsing strategy:
 *   - Split content into lines.
 *   - Use a simple state machine:
 *       INIT → SOLID → FACET_NORMAL → OUTER_LOOP →
 *       VERTEX(0) → VERTEX(1) → VERTEX(2) → ENDLOOP → ENDFACET → ...
 *   - Reject non-finite float values (NaN, Inf).
 *   - Trim leading/trailing whitespace from each token.
 */

#include "stl_parser.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace viz {

// ─── Utilities ────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t lo = s.find_first_not_of(" \t\r\n");
    if (lo == std::string::npos) return "";
    size_t hi = s.find_last_not_of(" \t\r\n");
    return s.substr(lo, hi - lo + 1);
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

/// Parse three floats from a string like "1.0 2.5 -3.0".
/// Returns false on parse error or non-finite value.
static bool parse_vec3(const std::string& str, std::array<float, 3>& out,
                        std::string& err_out) {
    std::istringstream ss(str);
    float a, b, c;
    if (!(ss >> a >> b >> c)) {
        err_out = "Failed to parse three numeric values from: \"" + str + "\"";
        return false;
    }
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c)) {
        err_out = "Non-finite value in: \"" + str + "\"";
        return false;
    }
    out = {a, b, c};
    return true;
}

// ─── parse_stl ────────────────────────────────────────────────────────────────
STLResult parse_stl(const std::string& content) {
    STLResult result;
    result.valid           = false;
    result.file_size_bytes = content.size();

    if (content.empty()) {
        result.error_msg = "Empty STL content.";
        return result;
    }

    // Accept a UTF-8 BOM, which common Windows editors add to text files.
    std::string text = content;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) text.erase(0, 3);

    // Split into lines
    std::vector<std::string> lines;
    {
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) lines.push_back(trim(line));
    }

    if (lines.empty() || to_lower(lines[0]).rfind("solid", 0) != 0) {
        result.error_msg = "ASCII STL must begin with 'solid'.";
        return result;
    }

    // Extract solid name (everything after "solid ")
    {
        std::string first = trim(lines[0]);
        if (first.size() > 5 && first[5] == ' ') {
            result.solid_name = trim(first.substr(6));
        } else {
            result.solid_name = "";
        }
    }

    enum class State {
        START, FACET_NORMAL, OUTER_LOOP, VERTEX0, VERTEX1, VERTEX2, ENDLOOP
    };

    State      state = State::START;
    Triangle   current;
    int        line_num = 0;
    bool       saw_endsolid = false;

    for (const auto& raw_line : lines) {
        ++line_num;
        std::string ll = to_lower(raw_line);
        std::string err;

        if (ll.empty()) continue;

        if (state == State::START) {
            if (starts_with(ll, "facet normal")) {
                std::string nums = trim(raw_line.substr(12));
                if (!parse_vec3(nums, current.normal, err)) {
                    result.error_msg = "Line " + std::to_string(line_num) + ": " + err;
                    return result;
                }
                state = State::FACET_NORMAL;
            } else if (starts_with(ll, "endsolid")) {
                saw_endsolid = true;
                break;
            } else if (starts_with(ll, "solid")) {
                // Possible second solid — ignore header
            }

        } else if (state == State::FACET_NORMAL) {
            if (starts_with(ll, "outer loop")) {
                state = State::OUTER_LOOP;
            } else {
                result.error_msg = "Line " + std::to_string(line_num) +
                                   ": Expected 'outer loop', got: " + raw_line;
                return result;
            }

        } else if (state == State::OUTER_LOOP || state == State::VERTEX0 ||
                   state == State::VERTEX1) {
            if (starts_with(ll, "vertex")) {
                std::string nums = trim(raw_line.substr(6));
                std::array<float,3> v{};
                if (!parse_vec3(nums, v, err)) {
                    result.error_msg = "Line " + std::to_string(line_num) + ": " + err;
                    return result;
                }
                if (state == State::OUTER_LOOP) {
                    current.v0 = v; state = State::VERTEX0;
                } else if (state == State::VERTEX0) {
                    current.v1 = v; state = State::VERTEX1;
                } else {
                    current.v2 = v; state = State::VERTEX2;
                }
            } else {
                result.error_msg = "Line " + std::to_string(line_num) +
                                   ": Expected 'vertex', got: " + raw_line;
                return result;
            }

        } else if (state == State::VERTEX2) {
            if (starts_with(ll, "endloop")) {
                state = State::ENDLOOP;
            } else {
                result.error_msg = "Line " + std::to_string(line_num) +
                                   ": Expected 'endloop', got: " + raw_line;
                return result;
            }

        } else if (state == State::ENDLOOP) {
            if (starts_with(ll, "endfacet")) {
                result.triangles.push_back(current);
                state = State::START;
            } else {
                result.error_msg = "Line " + std::to_string(line_num) +
                                   ": Expected 'endfacet', got: " + raw_line;
                return result;
            }
        }
    }

    if (state != State::START) {
        result.error_msg = "Unexpected end of STL inside a facet.";
        return result;
    }
    if (!saw_endsolid) {
        result.error_msg = "ASCII STL is missing its 'endsolid' terminator.";
        return result;
    }
    if (result.triangles.empty()) {
        result.error_msg = "STL contains no facets.";
        return result;
    }

    result.valid = true;
    return result;
}

// ─── stl_to_json ─────────────────────────────────────────────────────────────
std::string stl_to_json(const STLResult& r) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(6);

    j << "{"
      << "\"valid\":" << (r.valid ? "true" : "false") << ","
      << "\"solid_name\":\"" << r.solid_name << "\","
      << "\"triangle_count\":" << r.triangles.size() << ","
      << "\"file_size_bytes\":" << r.file_size_bytes;

    if (!r.valid) {
        // Escape quotes in error message
        std::string em = r.error_msg;
        for (size_t pos = 0; (pos = em.find('"', pos)) != std::string::npos; ) {
            em.replace(pos, 1, "\\\""); pos += 2;
        }
        j << ",\"error_msg\":\"" << em << "\"";
    }

    if (r.valid) {
        j << ",\"triangles\":[";
        for (size_t i = 0; i < r.triangles.size(); ++i) {
            if (i > 0) j << ",";
            const auto& t = r.triangles[i];
            auto vec3 = [&](const std::array<float,3>& v) {
                return "[" + std::to_string(v[0]) + "," +
                              std::to_string(v[1]) + "," +
                              std::to_string(v[2]) + "]";
            };
            j << "{"
              << "\"normal\":" << vec3(t.normal) << ","
              << "\"v0\":"     << vec3(t.v0) << ","
              << "\"v1\":"     << vec3(t.v1) << ","
              << "\"v2\":"     << vec3(t.v2)
              << "}";
        }
        j << "]";
    }

    j << "}";
    return j.str();
}

} // namespace viz
