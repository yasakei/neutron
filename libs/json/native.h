#ifndef NEUTRON_JSON_NATIVE_H
#define NEUTRON_JSON_NATIVE_H

/*
 * Code Documentation: JSON Module (libs/json/native.h)
 * ====================================================
 * 
 * This header defines the JSON module - Neutron's JSON parsing and
 * serialization support. It provides seamless conversion between
 * Neutron objects/arrays and JSON format.
 * 
 * What This File Includes:
 * ------------------------
 * - register_json_functions(): Register all JSON module functions
 * - neutron_init_json_module(): C API for module initialization
 * 
 * Available Functions (implemented in native.cpp):
 * -----------------------------------------------
 * - json.parse(string) → object|array: Parse JSON string
 * - json.stringify(value) → string: Convert to JSON string
 * - json.parseFile(path) → object|array: Parse JSON from file
 * - json.writeFile(path, value) → void: Write JSON to file
 * - json.isJson(string) → bool: Check if string is valid JSON
 * 
 * Type Mapping:
 * -------------
 * | Neutron Type | JSON Type    | Notes                    |
 * |--------------|--------------|------------------------- |
 * | Number       | number       | Double precision         |
 * | String       | string       | UTF-8 encoded            |
 * | true/false   | boolean      | Direct mapping           |
 * | nil          | null         | Null representation      |
 * | Object       | object       | Key-value pairs          |
 * | Array        | array        | Ordered list             |
 * 
 * Adding Features:
 * ----------------
 * - New parsers: Add support for JSON5, JSONL, etc.
 * - Custom serializers: Add pretty-print options, compact mode
 * - Streaming: Add incremental parsing for large files
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT parse untrusted JSON without size limits (DoS risk)
 * - Do NOT assume parsed objects have expected structure (validate!)
 * - Do NOT use for binary data (use base64 encoding)
 * - Do NOT parse JSON in tight loops without caching results
 * 
 * Example Usage:
 * --------------
 * @code
 * use json
 * 
 * // Parse JSON string
 * var data = json.parse('{"name": "Alice", "age": 30}')
 * print(data.name)  // "Alice"
 * 
 * // Create and stringify
 * var person = {"name": "Bob", "city": "NYC"}
 * var jsonStr = json.stringify(person)
 * 
 * // File operations
 * var config = json.parseFile("config.json")
 * json.writeFile("output.json", data)
 * @endcode
 */

#include "vm.h"
#include "expr.h"

namespace neutron {

/**
 * @brief Register all JSON module functions with the VM.
 * @param vm The VM instance.
 * @param env The environment to register functions in.
 * 
 * Creates a "json" module and registers all JSON functions.
 * Called automatically when "use json" is encountered.
 */
void register_json_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    /**
     * @brief C API entry point for JSON module initialization.
     * @param vm The VM instance (as opaque pointer).
     */
    void neutron_init_json_module(VM* vm);
}

} // namespace neutron

#endif // NEUTRON_JSON_NATIVE_H