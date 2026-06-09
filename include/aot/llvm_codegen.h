#ifndef NEUTRON_LLVM_CODEGEN_H
#define NEUTRON_LLVM_CODEGEN_H

#include "aot/aot_compiler.h"
#include <memory>

namespace neutron {
namespace aot {

struct LlvmCodegenImpl;

class LlvmCodegen {
public:
    LlvmCodegen(const Chunk* chunk);
    ~LlvmCodegen();

    void setDebugMode(bool enabled) { generateDebugSymbols = enabled; }
    void setTargetPlatform(TargetPlatform platform) { targetPlatform = platform; }
    TargetPlatform getTargetPlatform() const { return targetPlatform; }

    bool generateModule(const std::string& functionName, const std::string& outputPath);

    const std::unordered_map<size_t, size_t>& getSourceMap() const { return sourceMap; }

    // Convenience: generate to default path
    bool generateModule(const std::string& functionName = "neutron_main");

private:
    const Chunk* chunk;
    bool generateDebugSymbols;
    TargetPlatform targetPlatform;
    std::unordered_map<size_t, size_t> sourceMap;
    std::unique_ptr<LlvmCodegenImpl> impl;
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_LLVM_CODEGEN_H
