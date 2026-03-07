/*
 * Neutron Programming Language - Project Manager
 * Copyright (c) 2026 yasakei
 *
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * For full license text, see LICENSE file in the root directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Code Documentation: Project Manager (project_manager.h)
 * ======================================================
 * 
 * This header defines the ProjectManager class - the project scaffolding
 * and management system for Neutron applications.
 * 
 * What This File Includes:
 * ------------------------
 * - ProjectManager class: Project initialization, building, running
 * - Project utilities: Root detection, config loading, command execution
 * - Box integration: Package manager installation
 * 
 * How It Works:
 * -------------
 * The ProjectManager handles Neutron project lifecycle:
 * 
 * 1. Project Initialization (neutron init):
 *    - Create project directory structure
 *    - Generate neutron-module.toml config
 *    - Create main.nt entry point
 *    - Initialize .quark dependency file
 * 
 * 2. Project Building (neutron build):
 *    - Load project configuration
 *    - Compile source to bytecode
 *    - AOT compile to native executable
 *    - Bundle dependencies
 * 
 * 3. Project Running (neutron run):
 *    - Load configuration
 *    - Execute entry point with VM
 *    - Handle errors and output
 * 
 * 4. Package Management (neutron install):
 *    - Parse .quark dependencies
 *    - Invoke Box package manager
 *    - Install packages to project
 * 
 * Project Structure:
 * ------------------
 * @code
 * my-project/
 * ├── neutron-module.toml  # Project configuration
 * ├── .quark               # Dependencies file
 * ├── src/
 * │   └── main.nt          # Entry point
 * ├── libs/                # Local libraries
 * ├── build/               # Build output (generated)
 * └── .neutron/            # Project metadata (generated)
 * @endcode
 * 
 * Adding Features:
 * ----------------
 * - New commands: Add methods to ProjectManager
 * - Custom templates: Extend initProject() with templates
 * - Build hooks: Add pre/post-build script execution
 * - IDE integration: Generate IDE configuration files
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT modify project structure during build
 * - Do NOT run builds from multiple processes simultaneously
 * - Do NOT skip config validation before building
 * - Do NOT execute untrusted commands without sanitization
 */

#ifndef NEUTRON_PROJECT_MANAGER_H
#define NEUTRON_PROJECT_MANAGER_H

#include <string>
#include <memory>
#include "project/project_config.h"

namespace neutron {

/**
 * @brief ProjectManager - Neutron Project Scaffolding and Management.
 * 
 * Provides commands for:
 * - Creating new Neutron projects
 * - Building projects to native executables
 * - Running projects in development mode
 * - Managing dependencies via Box package manager
 * 
 * Usage from CLI:
 * @code
 * neutron init my-app          # Create new project
 * cd my-app
 * neutron run                  # Run in development mode
 * neutron build                # Build native executable
 * neutron install              # Install dependencies from .quark
 * @endcode
 */
class ProjectManager {
public:
    /**
     * @brief Initialize a new Neutron project in the current directory.
     * @param projectName Optional project name (defaults to directory name).
     * @return true if project was created successfully.
     * 
     * Creates:
     * - Project directory structure
     * - neutron-module.toml configuration
     * - src/main.nt entry point
     * - .quark dependency file
     * - README.md with getting started instructions
     */
    static bool initProject(const std::string& projectName = "");

    /**
     * @brief Build the project to a native executable.
     * @param projectRoot Path to project root (default: current directory).
     * @return true if build succeeded.
     * 
     * Build process:
     * 1. Load project configuration
     * 2. Compile source files to bytecode
     * 3. AOT compile to native C++
     * 4. Compile C++ with system compiler
     * 5. Output executable to build/ directory
     */
    static bool buildProject(const std::string& projectRoot = ".");

    /**
     * @brief Run the project in development mode.
     * @param projectRoot Path to project root (default: current directory).
     * @return true if execution succeeded (no runtime errors).
     * 
     * Development mode:
     * - Uses interpreter (faster startup)
     * - Hot reload on file changes (future feature)
     * - Full error reporting with source maps
     */
    static bool runProject(const std::string& projectRoot = ".");

    /**
     * @brief Install Box package manager if not present.
     * @param projectRoot Path to project root.
     * @return true if Box is available (installed or already present).
     */
    static bool installBox(const std::string& projectRoot = ".");

    /**
     * @brief Check if a directory is a Neutron project.
     * @param path Directory to check (default: current directory).
     * @return true if the directory contains neutron-module.toml.
     */
    static bool isNeutronProject(const std::string& path = ".");

    /**
     * @brief Find the project root from a starting path.
     * @param startPath Starting directory (default: current directory).
     * @return Path to project root, or empty string if not found.
     * 
     * Searches upward through parent directories for neutron-module.toml.
     */
    static std::string findProjectRoot(const std::string& startPath = ".");

    /**
     * @brief Load project configuration.
     * @param projectRoot Path to project root.
     * @return ProjectConfig pointer, or nullptr if loading failed.
     * 
     * Reads neutron-module.toml and parses:
     * - Project name and version
     * - Entry point file
     * - Dependencies
     * - Build options
     */
    static std::unique_ptr<ProjectConfig> loadConfig(const std::string& projectRoot = ".");

private:
    // Filesystem utilities
    static std::string getCurrentDirectory();     ///< Get current working directory
    static bool createDirectory(const std::string& path);  ///< Create directory (and parents)
    static bool fileExists(const std::string& path);       ///< Check if file exists
    static bool directoryExists(const std::string& path);  ///< Check if directory exists
    
    // Command execution
    static int executeCommand(const std::string& command);  ///< Execute shell command
    
    // Box package manager
    static std::string getBoxPath(const std::string& projectRoot);  ///< Get Box executable path
};

} // namespace neutron

#endif // NEUTRON_PROJECT_MANAGER_H
