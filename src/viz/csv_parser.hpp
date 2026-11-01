#pragma once
/**
 * csv_parser.hpp
 * ANSYS nodal temperature CSV parser for the Unified Engineering Workbench.
 *
 * Expected CSV format (first row is a header, exactly these column names):
 *   node_id,x_m,y_m,z_m,temperature_C
 *
 * Each subsequent row provides one finite-element node's position and
 * temperature reading.  Non-finite values (NaN, Inf) are rejected.
 *
 * The parser is permissive about whitespace around commas and tolerates
 * Windows (\r\n) and Unix (\n) line endings.
 */

#include <string>
#include <vector>

namespace viz {

/// A single finite-element node with spatial coordinates and temperature
struct Node {
    int    id;    ///< Node identifier (integer)
    double x;     ///< x coordinate [m]
    double y;     ///< y coordinate [m]
    double z;     ///< z coordinate [m]
    double temp;  ///< Temperature [°C]
};

/// Result of parsing a nodal temperature CSV
struct CSVResult {
    std::vector<Node> nodes;           ///< Parsed nodes (empty on error)
    bool              valid;           ///< true if parsing succeeded
    std::string       error_msg;       ///< Non-empty when valid == false
    size_t            file_size_bytes; ///< Byte length of input content
};

/**
 * Parse the CSV content and return nodes.
 *
 * @param content  Full text content of the CSV (header + data rows)
 * @return         CSVResult with nodes list (or error info if invalid)
 */
CSVResult parse_temperature_csv(const std::string& content);

/**
 * Serialise CSVResult to a JSON string.
 *
 * Schema:
 * {
 *   "valid": bool,
 *   "node_count": int,
 *   "file_size_bytes": int,
 *   "error_msg": string,       // present only when !valid
 *   "nodes": [                 // present only when valid
 *     { "id": int, "x": float, "y": float, "z": float, "temp": float },
 *     ...
 *   ]
 * }
 */
std::string csv_to_json(const CSVResult& result);

} // namespace viz
