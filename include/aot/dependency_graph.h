#ifndef NEUTRON_AOT_DEPENDENCY_GRAPH_H
#define NEUTRON_AOT_DEPENDENCY_GRAPH_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace neutron {
namespace aot {

enum class ModuleKind {
    NATIVE,  // .nt Neutron module
    CPP      // import c++ "header.h"
};

struct ModuleNode {
    std::string name;
    ModuleKind kind;
    std::string source_path;       // Path to .nt file
    std::string header_path;       // For C++ FFI: path to .h file
    std::vector<std::string> imports;  // Direct dependency names
};

class DependencyGraph {
public:
    void add_module(const ModuleNode& node);
    void add_edge(const std::string& from, const std::string& to);

    // Returns true if a cycle is detected (outputs cycle path)
    bool detect_cycle(std::vector<std::string>& cycle_path) const;

    // Returns modules in topological order (leaves first)
    std::vector<std::string> topological_sort() const;

    // Shallow-scan a .nt file for `using` / `import c++` directives (lexer only, no full parse)
    static std::vector<std::string> scan_imports(const std::string& source);

    // Check if a string starts with 'using' or 'import c++'
    static bool is_import_line(const std::string& line, std::string& module_name, ModuleKind& kind);

    const std::unordered_map<std::string, ModuleNode>& nodes() const { return nodes_; }
    const ModuleNode* get_module(const std::string& name) const;

private:
    std::unordered_map<std::string, ModuleNode> nodes_;
    // adjacency: from → [to, ...]
    std::unordered_map<std::string, std::vector<std::string>> edges_;

    enum class Color { WHITE, GRAY, BLACK };
    bool dfs_cycle(const std::string& node, std::unordered_map<std::string, Color>& colors,
                   std::vector<std::string>& path) const;
    void dfs_topo(const std::string& node, std::unordered_set<std::string>& visited,
                  std::vector<std::string>& result) const;
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_DEPENDENCY_GRAPH_H
