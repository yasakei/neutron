#include "aot/dependency_graph.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace neutron {
namespace aot {

void DependencyGraph::add_module(const ModuleNode& node) {
    nodes_[node.name] = node;
}

void DependencyGraph::add_edge(const std::string& from, const std::string& to) {
    edges_[from].push_back(to);
}

bool DependencyGraph::detect_cycle(std::vector<std::string>& cycle_path) const {
    std::unordered_map<std::string, Color> colors;
    for (const auto& [name, _] : nodes_) {
        colors[name] = Color::WHITE;
    }

    for (const auto& [name, _] : nodes_) {
        if (colors[name] == Color::WHITE) {
            std::vector<std::string> path;
            if (dfs_cycle(name, colors, path)) {
                cycle_path = path;
                return true;
            }
        }
    }
    return false;
}

bool DependencyGraph::dfs_cycle(const std::string& node,
                                 std::unordered_map<std::string, Color>& colors,
                                 std::vector<std::string>& path) const {
    colors[node] = Color::GRAY;
    path.push_back(node);

    auto it = edges_.find(node);
    if (it != edges_.end()) {
        for (const auto& dep : it->second) {
            auto color_it = colors.find(dep);
            if (color_it == colors.end()) continue;  // unknown module
            if (color_it->second == Color::GRAY) {
                // Found cycle — compact the path to just the cycle
                auto cycle_start = std::find(path.begin(), path.end(), dep);
                if (cycle_start != path.end()) {
                    path.erase(path.begin(), cycle_start);
                    path.push_back(dep);
                }
                return true;
            }
            if (color_it->second == Color::WHITE) {
                if (dfs_cycle(dep, colors, path)) return true;
            }
        }
    }

    colors[node] = Color::BLACK;
    path.pop_back();
    return false;
}

std::vector<std::string> DependencyGraph::topological_sort() const {
    std::unordered_set<std::string> visited;
    std::vector<std::string> result;

    for (const auto& [name, _] : nodes_) {
        if (visited.find(name) == visited.end()) {
            dfs_topo(name, visited, result);
        }
    }
    // Result is in topological order (dependencies before dependents, leaves first)
    return result;
}

void DependencyGraph::dfs_topo(const std::string& node,
                                std::unordered_set<std::string>& visited,
                                std::vector<std::string>& result) const {
    visited.insert(node);
    auto it = edges_.find(node);
    if (it != edges_.end()) {
        for (const auto& dep : it->second) {
            if (visited.find(dep) == visited.end()) {
                dfs_topo(dep, visited, result);
            }
        }
    }
    result.push_back(node);
}

const ModuleNode* DependencyGraph::get_module(const std::string& name) const {
    auto it = nodes_.find(name);
    return it != nodes_.end() ? &it->second : nullptr;
}

bool DependencyGraph::is_import_line(const std::string& line, std::string& module_name, ModuleKind& kind) {
    std::string trimmed = line;
    // Trim whitespace
    auto not_space = [](char c) { return !std::isspace(static_cast<unsigned char>(c)); };
    auto first = std::find_if(trimmed.begin(), trimmed.end(), not_space);
    trimmed.erase(trimmed.begin(), first);
    auto last = std::find_if(trimmed.rbegin(), trimmed.rend(), not_space);
    trimmed.erase(last.base(), trimmed.end());

    // Skip comments and empty lines
    if (trimmed.empty() || trimmed[0] == '#') return false;

    // Check for: import c++ "header.h";
    if (trimmed.find("import c++") == 0 || trimmed.find("import cpp") == 0) {
        kind = ModuleKind::CPP;
        // Extract header name between quotes
        auto q1 = trimmed.find('"');
        auto q2 = trimmed.rfind('"');
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
            module_name = trimmed.substr(q1 + 1, q2 - q1 - 1);
            return true;
        }
        return false;
    }

    // Check for: using module_name;
    if (trimmed.find("using ") == 0) {
        kind = ModuleKind::NATIVE;
        // Extract module name (no quotes = built-in module)
        size_t start = 6;  // after "using "
        size_t end = trimmed.find(';', start);
        if (end == std::string::npos) end = trimmed.size();
        module_name = trimmed.substr(start, end - start);
        // Trim whitespace
        auto f = std::find_if(module_name.begin(), module_name.end(), not_space);
        auto l = std::find_if(module_name.rbegin(), module_name.rend(), not_space);
        if (f < l.base()) {
            module_name = std::string(f, l.base());
        }
        // Check for quoted path (using 'path/to/file.nt')
        if (module_name.size() >= 2 &&
            (module_name[0] == '"' || module_name[0] == '\'') &&
            module_name.front() == module_name.back()) {
            module_name = module_name.substr(1, module_name.size() - 2);
        }
        return !module_name.empty();
    }

    return false;
}

std::vector<std::string> DependencyGraph::scan_imports(const std::string& source) {
    std::vector<std::string> imports;
    std::istringstream stream(source);
    std::string line;
    std::string module_name;
    ModuleKind kind;

    while (std::getline(stream, line)) {
        if (is_import_line(line, module_name, kind)) {
            imports.push_back(module_name);
        }
    }

    return imports;
}

} // namespace aot
} // namespace neutron
