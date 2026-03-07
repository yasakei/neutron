#ifndef NEUTRON_MODULE_LOADER_H
#define NEUTRON_MODULE_LOADER_H

/*
 * Code Documentation: Module Loader (module_loader.h)
 * ===================================================
 * 
 * This header defines the module loading utilities for Neutron.
 * It provides functions for discovering and loading modules.
 * 
 * What This File Includes:
 * ------------------------
 * - getUsedModules(): Extract import statements from source
 * - Module discovery: Find modules in search paths
 * 
 * How It Works:
 * -------------
 * The module loader:
 * 1. Scans source code for "use" statements
 * 2. Resolves module paths in search directories
 * 3. Loads modules (source or native)
 * 4. Caches loaded modules to avoid reloading
 * 
 * Module Resolution Order:
 * 1. Built-in modules (sys, json, http, etc.)
 * 2. Native modules (.so/.dll in module paths)
 * 3. Source modules (.nt files in module paths)
 * 
 * Adding Features:
 * ----------------
 * - New module formats: Extend resolution logic
 * - Module aliases: Add alias mapping to loader
 * - Lazy loading: Defer module loading until first use
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT load modules from untrusted sources without sandboxing
 * - Do NOT modify module search paths during module loading
 * - Do NOT bypass the module cache (causes duplicate loading)
 */

#include <string>
#include <vector>

namespace neutron {

/**
 * @brief Extract module names used in source code.
 * @param source The Neutron source code to scan.
 * @return Vector of module names from "use" statements.
 * 
 * Parses source code to find all "use module_name" statements.
 * Used for dependency analysis and preloading modules.
 * 
 * Example:
 * @code
 * Source: 
 *   use sys
 *   use json
 *   use http
 * Result: ["sys", "json", "http"]
 * @endcode
 */
std::vector<std::string> getUsedModules(const std::string& source);

} // namespace neutron

#endif // NEUTRON_MODULE_LOADER_H
