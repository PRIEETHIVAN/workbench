#pragma once
/**
 * stl_parser.hpp
 * ASCII STL file parser for the Unified Engineering Workbench.
 *
 * Supports the standard ASCII STL format:
 *   solid <name>
 *     facet normal <nx> <ny> <nz>
 *       outer loop
 *         vertex <x> <y> <z>
 *         vertex <x> <y> <z>
 *         vertex <x> <y> <z>
 *       endloop
 *     endfacet
 *     ...
 *   endsolid [<name>]
 *
 * Validation:
 *   - Empty input → error
 *   - Missing "solid" header → error
 *   - Non-finite vertex/normal components → rejected
 *   - Malformed numeric tokens → error with line info
 */

#include <array>
#include <string>
#include <vector>

namespace viz {

/// One triangular facet with normal vector and three vertices
struct Triangle {
    std::array<float, 3> normal; ///< Normal vector {nx, ny, nz}
    std::array<float, 3> v0;     ///< First vertex  {x, y, z}
    std::array<float, 3> v1;     ///< Second vertex {x, y, z}
    std::array<float, 3> v2;     ///< Third vertex  {x, y, z}
};

/// Result of parsing an ASCII STL file/string
struct STLResult {
    std::vector<Triangle> triangles;    ///< Parsed triangles (empty on error)
    std::string           solid_name;   ///< Name from "solid <name>" line
    bool                  valid;        ///< true if parsing succeeded
    std::string           error_msg;    ///< Non-empty when valid == false
    size_t                file_size_bytes; ///< Byte length of input content
};

/**
 * Parse ASCII STL content from a std::string.
 *
 * @param content  Full text content of the STL file
 * @return         STLResult with triangles (or error info if invalid)
 */
STLResult parse_stl(const std::string& content);

/**
 * Serialise STLResult to a JSON string.
 *
 * Schema:
 * {
 *   "valid": bool,
 *   "solid_name": string,
 *   "triangle_count": int,
 *   "file_size_bytes": int,
 *   "error_msg": string,            // present only when !valid
 *   "triangles": [                  // present only when valid
 *     {
 *       "normal": [nx, ny, nz],
 *       "v0": [x, y, z],
 *       "v1": [x, y, z],
 *       "v2": [x, y, z]
 *     }, ...
 *   ]
 * }
 */
std::string stl_to_json(const STLResult& result);

} // namespace viz
