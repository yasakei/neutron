#include "aot/llvm_codegen.h"
#include "types/obj_string.h"
#include "types/function.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>

#include <iostream>
#include <cstdlib>
#include <cmath>

namespace neutron {
namespace aot {

// Value tag constants (must match C++ codegen)
enum ValueTag : uint8_t {
    TAG_NIL = 0,
    TAG_BOOL = 1,
    TAG_NUMBER = 2,
    TAG_STRING = 3
};

struct LlvmCodegenImpl {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // LLVM type handles
    llvm::StructType* valueTy = nullptr;
    llvm::Type* i8Ty = nullptr;
    llvm::Type* i32Ty = nullptr;
    llvm::Type* doubleTy = nullptr;
    llvm::PointerType* i8PtrTy = nullptr;

    // Active function state
    llvm::Function* func = nullptr;
    llvm::Value* stackAlloca = nullptr;
    llvm::Value* localsAlloca = nullptr;
    llvm::Value* spAlloca = nullptr;

    // Global constants array reference
    llvm::GlobalVariable* constantsGlobal = nullptr;

    // External function declarations
    llvm::Function* putsFunc = nullptr;

    const Chunk* chunk = nullptr;

    // Control flow state
    std::vector<bool> isJumpTarget;
    std::unordered_map<size_t, llvm::BasicBlock*> bbMap;

    LlvmCodegenImpl() : builder(std::make_unique<llvm::IRBuilder<>>(context)) {}

    // First pass: find all jump target bytecode offsets
    void findJumpTargets() {
        isJumpTarget.assign(chunk->code.size() + 1, false);
        size_t scanIp = 0;
        while (scanIp < chunk->code.size()) {
            size_t instrStart = scanIp;
            (void)instrStart;
            uint8_t b = chunk->code[scanIp++];
            OpCode op = static_cast<OpCode>(b);

            auto scanSkip = [&](size_t n) { scanIp += n; };
            bool handled = false;

            switch (op) {
                case OpCode::OP_JUMP:
                case OpCode::OP_JUMP_IF_FALSE:
                case OpCode::OP_LESS_JUMP:
                case OpCode::OP_GREATER_JUMP:
                case OpCode::OP_EQUAL_JUMP:
                    if (scanIp + 1 < chunk->code.size()) {
                        uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                        scanIp += 2;
                        size_t target = scanIp + offset;
                        if (target < chunk->code.size()) isJumpTarget[target] = true;
                    }
                    handled = true;
                    break;

                case OpCode::OP_LOOP:
                    if (scanIp + 1 < chunk->code.size()) {
                        uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                        scanIp += 2;
                        size_t target = (scanIp > offset) ? (scanIp - offset) : 0;
                        if (target < chunk->code.size()) isJumpTarget[target] = true;
                    }
                    handled = true;
                    break;

                case OpCode::OP_LOOP_IF_LESS_LOCAL:
                    if (scanIp + 3 < chunk->code.size()) {
                        scanIp += 3; // slot + const index
                        uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                        scanIp += 2;
                        size_t target = scanIp + offset;
                        if (target < chunk->code.size()) isJumpTarget[target] = true;
                    }
                    handled = true;
                    break;

                default: break;
            }

            if (handled) continue;

            // Skip operands for opcodes that have them
            bool skip1 = false, skip2 = false;
            switch (op) {
                case OpCode::OP_CONSTANT:
                case OpCode::OP_GET_LOCAL:
                case OpCode::OP_SET_LOCAL:
                case OpCode::OP_GET_GLOBAL:
                case OpCode::OP_SET_GLOBAL:
                case OpCode::OP_DEFINE_GLOBAL:
                case OpCode::OP_GET_UPVALUE:
                case OpCode::OP_SET_UPVALUE:
                case OpCode::OP_GET_PROPERTY:
                case OpCode::OP_SET_PROPERTY:
                case OpCode::OP_GET_GLOBAL_FAST:
                case OpCode::OP_SET_GLOBAL_FAST:
                case OpCode::OP_INCREMENT_LOCAL:
                case OpCode::OP_DECREMENT_LOCAL:
                case OpCode::OP_INCREMENT_GLOBAL:
                case OpCode::OP_INC_LOCAL_INT:
                case OpCode::OP_DEC_LOCAL_INT:
                case OpCode::OP_CONST_INT8:
                case OpCode::OP_CALL:
                case OpCode::OP_CALL_FAST:
                case OpCode::OP_TAIL_CALL:
                case OpCode::OP_ARRAY:
                case OpCode::OP_OBJECT:
                case OpCode::OP_OPTIONAL_CHAIN:
                case OpCode::OP_TYPE_GUARD:
                case OpCode::OP_VALIDATE_SAFE_VARIABLE:
                case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
                case OpCode::OP_SET_GLOBAL_TYPED:
                case OpCode::OP_SET_LOCAL_TYPED:
                    skip1 = true;
                    break;
                case OpCode::OP_CONSTANT_LONG:
                case OpCode::OP_ADD_LOCAL_CONST:
                    skip2 = true;
                    break;
                case OpCode::OP_INVOKE:
                    scanIp += 2; // constIdx + argCount
                    break;
                case OpCode::OP_LOGICAL_AND:
                case OpCode::OP_LOGICAL_OR:
                    scanIp += 2; // short-circuit jump offset
                    break;
                case OpCode::OP_TRY:
                    scanIp += 6; // tryEnd(2) + catchStart(2) + finallyStart(2)
                    break;
                case OpCode::OP_FOR_IN_NEXT:
                    scanIp += 4; // baseSlot + varSlot + offsetHi + offsetLo
                    break;
                case OpCode::OP_CLOSURE:
                    scanIp++;
                    if (scanIp + 1 < chunk->code.size()) {
                        uint16_t n = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                        scanIp += 2 + n * 2;
                    }
                    break;
                default:
                    break;
            }
            if (skip1) scanIp++;
            if (skip2) scanIp += 2;
        }
        isJumpTarget[0] = true;
    }

    // Get or create a basic block for a given bytecode offset
    llvm::BasicBlock* getOrCreateBB(size_t offset, llvm::Function* f) {
        auto it = bbMap.find(offset);
        if (it != bbMap.end()) return it->second;
        auto* bb = llvm::BasicBlock::Create(context, "bb_" + std::to_string(offset), f);
        bbMap[offset] = bb;
        return bb;
    }

    // Ensure we're in the right block for the current IP
    void ensureBlock(size_t currentIp) {
        auto* curBlock = builder->GetInsertBlock();
        bool terminated = curBlock->getTerminator() != nullptr;

        if (terminated || (isJumpTarget[currentIp] && currentIp != 0)) {
            auto* targetBB = getOrCreateBB(currentIp, func);
            if (!terminated) {
                builder->CreateBr(targetBB);
            }
            builder->SetInsertPoint(targetBB);
        }
    }

    // Initialize LLVM types for Value struct
    void initTypes() {
        i8Ty = llvm::Type::getInt8Ty(context);
        i32Ty = llvm::Type::getInt32Ty(context);
        doubleTy = llvm::Type::getDoubleTy(context);
        i8PtrTy = llvm::PointerType::get(i8Ty, 0);

        valueTy = llvm::StructType::create(context, "Value");
        valueTy->setBody({i8Ty, doubleTy});
    }

    // Create a constant value struct
    llvm::Constant* constValue(ValueTag tag, double data) {
        auto* tagConst = llvm::ConstantInt::get(i8Ty, tag);
        auto* dataConst = llvm::ConstantFP::get(doubleTy, data);
        return llvm::ConstantStruct::get(valueTy, {tagConst, dataConst});
    }

    // Allocate and initialize stack + locals
    void setupStackLocals(llvm::Function* f) {
        func = f;
        auto* entry = &f->getEntryBlock();
        auto savedIP = builder->saveIP();

        // Insert allocas at the beginning of the entry block
        llvm::IRBuilder<> allocBuilder(entry, entry->begin());

        auto* stackTy = llvm::ArrayType::get(valueTy, 256);
        auto* localsTy = llvm::ArrayType::get(valueTy, 256);

        stackAlloca = allocBuilder.CreateAlloca(stackTy, nullptr, "stack");
        localsAlloca = allocBuilder.CreateAlloca(localsTy, nullptr, "locals");

        // Initialize all locals to nil
        for (int i = 0; i < 256; i++) {
            auto* gep = allocBuilder.CreateGEP(localsTy, localsAlloca,
                                                {llvm::ConstantInt::get(i32Ty, 0),
                                                 llvm::ConstantInt::get(i32Ty, i)});
            allocBuilder.CreateStore(constValue(TAG_NIL, 0.0), gep);
        }

        spAlloca = allocBuilder.CreateAlloca(i32Ty, nullptr, "sp");
        allocBuilder.CreateStore(llvm::ConstantInt::get(i32Ty, 0), spAlloca);

        builder->restoreIP(savedIP);
    }

    // Declare external functions
    void declareExternals() {
        auto* putsTy = llvm::FunctionType::get(i32Ty, {i8PtrTy}, false);
        putsFunc = llvm::Function::Create(putsTy, llvm::Function::ExternalLinkage, "puts", module.get());
    }

    // Create the constants global array from chunk
    void createConstants() {
        size_t n = chunk->constants.size();
        auto* arrTy = llvm::ArrayType::get(valueTy, n);

        std::vector<llvm::Constant*> elems;
        for (size_t i = 0; i < n; i++) {
            const Value& v = chunk->constants[i];
            switch (v.type) {
                case ValueType::NIL:
                    elems.push_back(constValue(TAG_NIL, 0.0));
                    break;
                case ValueType::BOOLEAN:
                    elems.push_back(constValue(TAG_BOOL, v.as.boolean ? 1.0 : 0.0));
                    break;
                case ValueType::NUMBER:
                    elems.push_back(constValue(TAG_NUMBER, v.as.number));
                    break;
                default:
                    elems.push_back(constValue(TAG_NIL, 0.0));
                    break;
            }
        }

        auto* init = llvm::ConstantArray::get(arrTy, elems);
        constantsGlobal = new llvm::GlobalVariable(*module, arrTy, true,
                                                     llvm::GlobalValue::InternalLinkage, init, "constants");
    }

    // --- Stack access helpers ---

    llvm::Value* stackGEP(llvm::Value* idx) {
        return builder->CreateGEP(llvm::ArrayType::get(valueTy, 256), stackAlloca,
                                   {llvm::ConstantInt::get(i32Ty, 0), idx});
    }

    llvm::Value* localsGEP(llvm::Value* idx) {
        return builder->CreateGEP(llvm::ArrayType::get(valueTy, 256), localsAlloca,
                                   {llvm::ConstantInt::get(i32Ty, 0), idx});
    }

    llvm::Value* constantsGEP(llvm::Value* idx) {
        auto* arrTy = llvm::ArrayType::get(valueTy, chunk->constants.size());
        return builder->CreateGEP(arrTy, constantsGlobal,
                                   {llvm::ConstantInt::get(i32Ty, 0), idx});
    }

    // Read sp, write sp
    llvm::Value* readSP() {
        return builder->CreateLoad(i32Ty, spAlloca, "sp");
    }

    void writeSP(llvm::Value* val) {
        builder->CreateStore(val, spAlloca);
    }

    // Push a value onto the stack, returns the stack slot pointer
    llvm::Value* pushValue() {
        auto* sp = readSP();
        auto* ptr = stackGEP(sp);
        writeSP(builder->CreateAdd(sp, llvm::ConstantInt::get(i32Ty, 1)));
        return ptr;
    }

    // Pop top of stack, returns value pointer
    llvm::Value* popValue() {
        auto* sp = readSP();
        auto* newSp = builder->CreateSub(sp, llvm::ConstantInt::get(i32Ty, 1));
        writeSP(newSp);
        return stackGEP(newSp);
    }

    // Peek at top of stack
    llvm::Value* peekValue() {
        auto* sp = readSP();
        auto* idx = builder->CreateSub(sp, llvm::ConstantInt::get(i32Ty, 1));
        return stackGEP(idx);
    }

    // --- Value access helpers ---
    // Extract tag from a value pointer
    llvm::Value* loadTag(llvm::Value* ptr) {
        return builder->CreateLoad(i8Ty,
                                    builder->CreateStructGEP(valueTy, ptr, 0), "tag");
    }

    // Extract data from a value pointer
    llvm::Value* loadData(llvm::Value* ptr) {
        return builder->CreateLoad(doubleTy,
                                    builder->CreateStructGEP(valueTy, ptr, 1), "data");
    }

    // Store tag + data into a value pointer
    void storeValue(llvm::Value* ptr, llvm::Value* tag, llvm::Value* data) {
        builder->CreateStore(tag, builder->CreateStructGEP(valueTy, ptr, 0));
        builder->CreateStore(data, builder->CreateStructGEP(valueTy, ptr, 1));
    }

    // Create a constant value and store it
    void emitConstStore(llvm::Value* ptr, ValueTag tag, double data) {
        storeValue(ptr,
                    llvm::ConstantInt::get(i8Ty, tag),
                    llvm::ConstantFP::get(doubleTy, data));
    }

    // --- Global variable tracking ---
    std::unordered_map<std::string, llvm::GlobalVariable*> globalVars;

    // First pass: find all global variable declarations
    void collectGlobals() {
        size_t scanIp = 0;
        while (scanIp < chunk->code.size()) {
            uint8_t b = chunk->code[scanIp++];
            OpCode op = static_cast<OpCode>(b);

            if (op == OpCode::OP_DEFINE_GLOBAL || op == OpCode::OP_DEFINE_TYPED_GLOBAL) {
                if (scanIp < chunk->code.size()) {
                    uint8_t idx = chunk->code[scanIp];
                    if (idx < chunk->constants.size()) {
                        auto* gv = getOrCreateGlobal(idx);
                        (void)gv;
                    }
                    scanIp++;
                    if (op == OpCode::OP_DEFINE_TYPED_GLOBAL) scanIp++;
                }
            } else {
                // Skip operands for other opcodes
                bool skip1 = false, skip2 = false;
                switch (op) {
                    case OpCode::OP_CONSTANT:
                    case OpCode::OP_GET_LOCAL:
                    case OpCode::OP_SET_LOCAL:
                    case OpCode::OP_GET_GLOBAL:
                    case OpCode::OP_SET_GLOBAL:
                    case OpCode::OP_SET_GLOBAL_TYPED:
                    case OpCode::OP_SET_LOCAL_TYPED:
                    case OpCode::OP_GET_GLOBAL_FAST:
                    case OpCode::OP_SET_GLOBAL_FAST:
                    case OpCode::OP_INCREMENT_GLOBAL:
                    case OpCode::OP_GET_UPVALUE:
                    case OpCode::OP_SET_UPVALUE:
                    case OpCode::OP_GET_PROPERTY:
                    case OpCode::OP_SET_PROPERTY:
                    case OpCode::OP_INCREMENT_LOCAL:
                    case OpCode::OP_DECREMENT_LOCAL:
                    case OpCode::OP_INC_LOCAL_INT:
                    case OpCode::OP_DEC_LOCAL_INT:
                    case OpCode::OP_CONST_INT8:
                    case OpCode::OP_CALL:
                    case OpCode::OP_CALL_FAST:
                    case OpCode::OP_TAIL_CALL:
                    case OpCode::OP_ARRAY:
                    case OpCode::OP_OBJECT:
                    case OpCode::OP_OPTIONAL_CHAIN:
                    case OpCode::OP_TYPE_GUARD:
                    case OpCode::OP_VALIDATE_SAFE_VARIABLE:
                    case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
                        skip1 = true;
                        break;
                    case OpCode::OP_CONSTANT_LONG:
                    case OpCode::OP_ADD_LOCAL_CONST:
                        skip2 = true;
                        break;
                    case OpCode::OP_INVOKE:
                        scanIp += 2; // constIdx + argCount
                        break;
                    case OpCode::OP_LOGICAL_AND:
                    case OpCode::OP_LOGICAL_OR:
                        scanIp += 2; // short-circuit jump offset
                        break;
                    case OpCode::OP_TRY:
                        scanIp += 6; // tryEnd(2) + catchStart(2) + finallyStart(2)
                        break;
                    case OpCode::OP_FOR_IN_NEXT:
                        scanIp += 4; // baseSlot + varSlot + offsetHi + offsetLo
                        break;
                    case OpCode::OP_JUMP:
                    case OpCode::OP_JUMP_IF_FALSE:
                    case OpCode::OP_LOOP:
                    case OpCode::OP_LESS_JUMP:
                    case OpCode::OP_GREATER_JUMP:
                    case OpCode::OP_EQUAL_JUMP:
                        scanIp += 2;
                        break;
                    case OpCode::OP_LOOP_IF_LESS_LOCAL:
                        scanIp += 5;
                        break;
                    case OpCode::OP_CLOSURE:
                        scanIp++;
                        if (scanIp + 1 < chunk->code.size()) {
                            uint16_t n = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                            scanIp += 2 + n * 2;
                        }
                        break;
                    default:
                        break;
                }
                if (skip1) scanIp++;
                if (skip2) scanIp += 2;
            }
        }
    }

    // Get or create an LLVM global variable for a given constant pool index
    llvm::GlobalVariable* getOrCreateGlobal(size_t constIdx) {
        if (constIdx >= chunk->constants.size()) return nullptr;
        const Value& nameVal = chunk->constants[constIdx];
        if (nameVal.type != ValueType::OBJ_STRING) return nullptr;

        std::string rawName = nameVal.as.obj_string->chars;
        // Sanitize name for LLVM identifier
        std::string safe;
        safe.reserve(rawName.size());
        for (char c : rawName) {
            if (isalnum(c) || c == '_') safe += c;
            else safe += '_';
        }

        // Key by name, not constIdx — the same variable may be referenced
        // from different constant-pool indices
        auto it = globalVars.find(safe);
        if (it != globalVars.end()) return it->second;

        auto* gv = new llvm::GlobalVariable(*module, valueTy, false,
                                             llvm::GlobalValue::InternalLinkage,
                                             constValue(TAG_NIL, 0.0), "global_" + safe);
        globalVars[safe] = gv;
        return gv;
    }

    // --- Bytecode reading ---
    size_t ip = 0;

    uint8_t readByte() {
        return chunk->code[ip++];
    }

    uint16_t readShort() {
        uint8_t b1 = readByte();
        uint8_t b2 = readByte();
        return (static_cast<uint16_t>(b1) << 8) | b2;
    }
};

LlvmCodegen::LlvmCodegen(const Chunk* c)
    : chunk(c), generateDebugSymbols(false),
      targetPlatform(TargetPlatform::NATIVE),
      impl(std::make_unique<LlvmCodegenImpl>())
{
    static bool targetsInitialized = false;
    if (!targetsInitialized) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        targetsInitialized = true;
    }
}

LlvmCodegen::~LlvmCodegen() = default;

bool LlvmCodegen::generateModule(const std::string& functionName, const std::string& outputPath) {
    auto& ctx = impl->context;
    impl->chunk = chunk;

    impl->module = std::make_unique<llvm::Module>(functionName, ctx);
    impl->initTypes();
    impl->declareExternals();
    impl->createConstants();

    // Create main function: i32 @neutron_main()
    auto* funcType = llvm::FunctionType::get(impl->i32Ty, false);
    auto* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, functionName, impl->module.get());

    auto* entry = llvm::BasicBlock::Create(ctx, "entry", func);
    impl->builder->SetInsertPoint(entry);
    impl->setupStackLocals(func);

    // Find all jump targets and branch from entry to first bytecode block
    impl->findJumpTargets();
    // Find all global variables
    impl->collectGlobals();
    auto* startBB = impl->getOrCreateBB(0, func);
    impl->builder->CreateBr(startBB);
    impl->builder->SetInsertPoint(startBB);

    // Walk bytecode and emit LLVM IR for each instruction
    impl->ip = 0;
    while (impl->ip < chunk->code.size()) {
        // Ensure we're in the right basic block for this IP
        impl->ensureBlock(impl->ip);

        uint8_t instr = impl->readByte();
        OpCode op = static_cast<OpCode>(instr);

        // After a terminator, the switchToBlock at next iteration will create
        // a new block for the fallthrough code
        switch (op) {
            case OpCode::OP_RETURN: {
                // Pop return value (unused for now), return 0
                impl->popValue();
                impl->builder->CreateRet(llvm::ConstantInt::get(impl->i32Ty, 0));
                break;
            }

            case OpCode::OP_CONSTANT: {
                uint8_t index = impl->readByte();
                auto* dest = impl->pushValue();
                auto* src = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, index));
                impl->builder->CreateLoad(impl->valueTy, src, "const_val");
                impl->builder->CreateStore(
                    impl->builder->CreateLoad(impl->valueTy, src),
                    dest);
                break;
            }

            case OpCode::OP_CONSTANT_LONG: {
                uint16_t index = impl->readShort();
                auto* dest = impl->pushValue();
                auto* src = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, index));
                impl->builder->CreateStore(
                    impl->builder->CreateLoad(impl->valueTy, src),
                    dest);
                break;
            }

            case OpCode::OP_NIL:
                impl->emitConstStore(impl->pushValue(), TAG_NIL, 0.0);
                break;

            case OpCode::OP_TRUE:
                impl->emitConstStore(impl->pushValue(), TAG_BOOL, 1.0);
                break;

            case OpCode::OP_FALSE:
                impl->emitConstStore(impl->pushValue(), TAG_BOOL, 0.0);
                break;

            case OpCode::OP_POP:
                impl->popValue();
                break;

            case OpCode::OP_DUP: {
                auto* src = impl->peekValue();
                auto* dest = impl->pushValue();
                auto* val = impl->builder->CreateLoad(impl->valueTy, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_GET_LOCAL: {
                uint8_t slot = impl->readByte();
                auto* src = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* dest = impl->pushValue();
                auto* val = impl->builder->CreateLoad(impl->valueTy, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = impl->readByte();
                auto* src = impl->popValue();
                auto* dest = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* val = impl->builder->CreateLoad(impl->valueTy, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_ADD: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* bData = impl->loadData(b);
                auto* aData = impl->loadData(a);
                auto* sum = impl->builder->CreateFAdd(aData, bData, "add");
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), sum);
                break;
            }

            case OpCode::OP_SUBTRACT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFSub(impl->loadData(a), impl->loadData(b), "sub");
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }

            case OpCode::OP_MULTIPLY: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFMul(impl->loadData(a), impl->loadData(b), "mul");
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }

            case OpCode::OP_DIVIDE: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFDiv(impl->loadData(a), impl->loadData(b), "div");
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }

            case OpCode::OP_MODULO: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aData = impl->loadData(a);
                auto* bData = impl->loadData(b);
                auto* result = impl->builder->CreateFRem(aData, bData, "mod");
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }

            case OpCode::OP_NEGATE: {
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* neg = impl->builder->CreateFNeg(impl->loadData(a), "neg");
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), neg);
                break;
            }

            case OpCode::OP_EQUAL: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpOEQ(impl->loadData(a), impl->loadData(b), "eq");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_BOOL), ext);
                break;
            }

            case OpCode::OP_NOT_EQUAL: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpONE(impl->loadData(a), impl->loadData(b), "neq");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_BOOL), ext);
                break;
            }

            case OpCode::OP_GREATER: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpOGT(impl->loadData(a), impl->loadData(b), "gt");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_BOOL), ext);
                break;
            }

            case OpCode::OP_LESS: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpOLT(impl->loadData(a), impl->loadData(b), "lt");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_BOOL), ext);
                break;
            }

            case OpCode::OP_NOT: {
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* data = impl->loadData(a);
                auto* zero = llvm::ConstantFP::get(impl->doubleTy, 0.0);
                auto* cmp = impl->builder->CreateFCmpOEQ(data, zero, "not");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_BOOL), ext);
                break;
            }

            case OpCode::OP_ADD_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFAdd(impl->loadData(a), impl->loadData(b), "add_int");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_SUB_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFSub(impl->loadData(a), impl->loadData(b), "sub_int");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_MUL_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFMul(impl->loadData(a), impl->loadData(b), "mul_int");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_DIV_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFDiv(impl->loadData(a), impl->loadData(b), "div_int");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_MOD_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* result = impl->builder->CreateFRem(impl->loadData(a), impl->loadData(b), "mod_int");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_NEGATE_INT: {
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* neg = impl->builder->CreateFNeg(impl->loadData(a), "neg_int");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), neg);
                break;
            }
            case OpCode::OP_EQUAL_INT:
            case OpCode::OP_LESS_INT:
            case OpCode::OP_GREATER_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aData = impl->loadData(a);
                auto* bData = impl->loadData(b);
                llvm::CmpInst::Predicate pred;
                if (op == OpCode::OP_EQUAL_INT) pred = llvm::CmpInst::FCMP_OEQ;
                else if (op == OpCode::OP_LESS_INT) pred = llvm::CmpInst::FCMP_OLT;
                else pred = llvm::CmpInst::FCMP_OGT;
                auto* cmp = impl->builder->CreateFCmp(pred, aData, bData, "int_cmp");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_BOOL), ext);
                break;
            }

            // Bitwise operations
            case OpCode::OP_BITWISE_AND:
            case OpCode::OP_BITWISE_OR:
            case OpCode::OP_BITWISE_XOR:
            case OpCode::OP_LEFT_SHIFT:
            case OpCode::OP_RIGHT_SHIFT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i32Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i32Ty, "b_int");
                llvm::Value* result;
                switch (op) {
                    case OpCode::OP_BITWISE_AND: result = impl->builder->CreateAnd(aInt, bInt, "and"); break;
                    case OpCode::OP_BITWISE_OR:  result = impl->builder->CreateOr(aInt, bInt, "or"); break;
                    case OpCode::OP_BITWISE_XOR: result = impl->builder->CreateXor(aInt, bInt, "xor"); break;
                    case OpCode::OP_LEFT_SHIFT:  result = impl->builder->CreateShl(aInt, bInt, "shl"); break;
                    case OpCode::OP_RIGHT_SHIFT: result = impl->builder->CreateAShr(aInt, bInt, "shr"); break;
                    default: result = llvm::ConstantInt::get(impl->i32Ty, 0); break;
                }
                auto* asDouble = impl->builder->CreateSIToFP(result, impl->doubleTy);
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), asDouble);
                break;
            }
            case OpCode::OP_BITWISE_NOT: {
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i32Ty, "a_int");
                auto* result = impl->builder->CreateNot(aInt, "not_bit");
                auto* asDouble = impl->builder->CreateSIToFP(result, impl->doubleTy);
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), asDouble);
                break;
            }

            // Extended constants
            case OpCode::OP_CONST_ZERO:
                impl->emitConstStore(impl->pushValue(), TAG_NUMBER, 0.0);
                break;
            case OpCode::OP_CONST_ONE:
                impl->emitConstStore(impl->pushValue(), TAG_NUMBER, 1.0);
                break;
            case OpCode::OP_CONST_INT8: {
                int8_t val = static_cast<int8_t>(impl->readByte());
                impl->emitConstStore(impl->pushValue(), TAG_NUMBER, static_cast<double>(val));
                break;
            }

            // Load/Store shortcuts
            case OpCode::OP_LOAD_LOCAL_0:
            case OpCode::OP_LOAD_LOCAL_1:
            case OpCode::OP_LOAD_LOCAL_2:
            case OpCode::OP_LOAD_LOCAL_3: {
                uint8_t slot = static_cast<uint8_t>(op) - static_cast<uint8_t>(OpCode::OP_LOAD_LOCAL_0);
                auto* src = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* dest = impl->pushValue();
                auto* val = impl->builder->CreateLoad(impl->valueTy, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            // Fused add local+const
            case OpCode::OP_ADD_LOCAL_CONST: {
                uint8_t slot = impl->readByte();
                uint8_t constIdx = impl->readByte();
                auto* local = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* constPtr = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, constIdx));
                auto* localData = impl->loadData(local);
                auto* constData = impl->loadData(constPtr);
                auto* sum = impl->builder->CreateFAdd(localData, constData, "add_lc");
                auto* dest = impl->pushValue();
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), sum);
                break;
            }

            // Increment/decrement locals
            case OpCode::OP_INCREMENT_LOCAL:
            case OpCode::OP_INC_LOCAL_INT: {
                uint8_t slot = impl->readByte();
                auto* localPtr = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* data = impl->loadData(localPtr);
                auto* one = llvm::ConstantFP::get(impl->doubleTy, 1.0);
                auto* inc = impl->builder->CreateFAdd(data, one, "inc");
                impl->storeValue(localPtr, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), inc);
                break;
            }
            case OpCode::OP_DECREMENT_LOCAL:
            case OpCode::OP_DEC_LOCAL_INT: {
                uint8_t slot = impl->readByte();
                auto* localPtr = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* data = impl->loadData(localPtr);
                auto* one = llvm::ConstantFP::get(impl->doubleTy, 1.0);
                auto* dec = impl->builder->CreateFSub(data, one, "dec");
                impl->storeValue(localPtr, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), dec);
                break;
            }

            // === Control flow ===
            case OpCode::OP_JUMP: {
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;
                auto* targetBB = impl->getOrCreateBB(target, func);
                impl->builder->CreateBr(targetBB);
                break;
            }

            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;

                // Check truthiness: pop the condition value
                auto* val = impl->popValue();
                auto* tag = impl->loadTag(val);
                auto* data = impl->loadData(val);
                auto* zero = llvm::ConstantFP::get(impl->doubleTy, 0.0);
                auto* isTrue = impl->builder->CreateFCmpONE(data, zero, "is_truthy");
                // Nil is always false, bool false is 0.0
                auto* notNil = impl->builder->CreateICmpNE(tag,
                    llvm::ConstantInt::get(impl->i8Ty, TAG_NIL), "not_nil");
                auto* truthy = impl->builder->CreateAnd(notNil, isTrue, "truthy");

                auto* targetBB = impl->getOrCreateBB(target, func);
                auto* contBB = llvm::BasicBlock::Create(ctx, "cont_jf", func);
                impl->builder->CreateCondBr(truthy, contBB, targetBB);
                impl->builder->SetInsertPoint(contBB);
                break;
            }

            case OpCode::OP_LOOP: {
                uint16_t offset = impl->readShort();
                size_t target = (impl->ip > offset) ? (impl->ip - offset) : 0;
                auto* targetBB = impl->getOrCreateBB(target, func);
                impl->builder->CreateBr(targetBB);
                break;
            }

            // Fused comparison+jump
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP: {
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;

                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* aData = impl->loadData(a);
                auto* bData = impl->loadData(b);

                llvm::CmpInst::Predicate pred;
                if (op == OpCode::OP_LESS_JUMP) pred = llvm::CmpInst::FCMP_OLT;
                else if (op == OpCode::OP_GREATER_JUMP) pred = llvm::CmpInst::FCMP_OGT;
                else pred = llvm::CmpInst::FCMP_OEQ;
                auto* cmp = impl->builder->CreateFCmp(pred, aData, bData, "fused_cmp");

                auto* targetBB = impl->getOrCreateBB(target, func);
                auto* contBB = llvm::BasicBlock::Create(ctx, "cont_fj", func);
                impl->builder->CreateCondBr(cmp, contBB, targetBB);
                impl->builder->SetInsertPoint(contBB);
                break;
            }

            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                uint8_t slot = impl->readByte();
                uint8_t constIdx = impl->readByte();
                uint16_t offset = impl->readShort();
                size_t exitTarget = impl->ip + offset;

                auto* localPtr = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* constPtr = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, constIdx));
                auto* localData = impl->loadData(localPtr);
                auto* constData = impl->loadData(constPtr);
                auto* cond = impl->builder->CreateFCmpOLT(localData, constData, "loop_cond");

                // Loop back to start of this instruction (before operands were read)
                size_t loopTarget = impl->ip - 5; // 1(op) + 1(slot) + 1(const) + 2(offset)
                auto* loopBB = impl->getOrCreateBB(loopTarget, func);
                auto* exitBB = impl->getOrCreateBB(exitTarget, func);
                impl->builder->CreateCondBr(cond, loopBB, exitBB);
                break;
            }

            // === Globals ===
            case OpCode::OP_GET_GLOBAL:
            case OpCode::OP_GET_GLOBAL_FAST: {
                uint8_t constIdx = impl->readByte();
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* dest = impl->pushValue();
                    auto* val = impl->builder->CreateLoad(impl->valueTy, gv, "global");
                    impl->builder->CreateStore(val, dest);
                } else {
                    impl->pushValue();
                    impl->emitConstStore(impl->peekValue(), TAG_NIL, 0.0);
                }
                break;
            }

            case OpCode::OP_SET_GLOBAL:
            case OpCode::OP_SET_GLOBAL_FAST:
            case OpCode::OP_SET_GLOBAL_TYPED:
            case OpCode::OP_DEFINE_GLOBAL: {
                uint8_t constIdx = impl->readByte();
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* src = impl->popValue();
                    auto* val = impl->builder->CreateLoad(impl->valueTy, src);
                    impl->builder->CreateStore(val, gv);
                } else {
                    impl->popValue();
                }
                break;
            }

            case OpCode::OP_DEFINE_TYPED_GLOBAL: {
                uint8_t constIdx = impl->readByte();
                uint8_t typeByte = impl->readByte(); // skip type annotation
                (void)typeByte;
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* src = impl->popValue();
                    auto* val = impl->builder->CreateLoad(impl->valueTy, src);
                    impl->builder->CreateStore(val, gv);
                } else {
                    impl->popValue();
                }
                break;
            }

            case OpCode::OP_INCREMENT_GLOBAL: {
                uint8_t constIdx = impl->readByte();
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* dataPtr = impl->builder->CreateStructGEP(impl->valueTy, gv, 1, "g_inc_data");
                    auto* data = impl->builder->CreateLoad(impl->doubleTy, dataPtr, "g_inc_val");
                    auto* inc = impl->builder->CreateFAdd(data,
                        llvm::ConstantFP::get(impl->doubleTy, 1.0), "inc_global");
                    auto* tagPtr = impl->builder->CreateStructGEP(impl->valueTy, gv, 0);
                    impl->builder->CreateStore(llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), tagPtr);
                    impl->builder->CreateStore(inc, dataPtr);
                }
                break;
            }

            // === Functions & Calls ===
            case OpCode::OP_CALL:
            case OpCode::OP_CALL_FAST: {
                uint8_t argCount = impl->readByte();
                // Pop callee + args, push nil (interpreter fallback)
                for (int i = 0; i <= argCount; i++) {
                    impl->popValue();
                }
                auto* dest = impl->pushValue();
                auto* nilVal = impl->constValue(TAG_NIL, 0.0);
                impl->builder->CreateStore(nilVal, dest);
                break;
            }

            case OpCode::OP_CLOSURE: {
                uint8_t funcIdx = impl->readByte();
                // Push function from constants (upvalues not supported yet)
                auto* dest = impl->pushValue();
                auto* src = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, funcIdx));
                auto* val = impl->builder->CreateLoad(impl->valueTy, src, "closure");
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_GET_UPVALUE: {
                uint8_t slot = impl->readByte();
                (void)slot;
                // Upvalues not supported — push nil
                auto* dest = impl->pushValue();
                auto* nilVal = impl->constValue(TAG_NIL, 0.0);
                impl->builder->CreateStore(nilVal, dest);
                break;
            }

            case OpCode::OP_SET_UPVALUE: {
                uint8_t slot = impl->readByte();
                (void)slot;
                // Upvalues not supported — discard
                impl->popValue();
                break;
            }

            case OpCode::OP_CLOSE_UPVALUE: {
                // Upvalues not supported — discard top of stack
                impl->popValue();
                break;
            }

            // === Arrays & Objects ===
            case OpCode::OP_ARRAY: {
                uint8_t count = impl->readByte();
                for (int i = 0; i < count; i++) impl->popValue();
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_OBJECT: {
                uint8_t count = impl->readByte();
                for (int i = 0; i < count * 2; i++) impl->popValue();
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_INDEX_GET: {
                impl->popValue(); // index
                impl->popValue(); // object/array/string/buffer
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_INDEX_SET: {
                auto* val = impl->popValue();
                impl->popValue(); // index
                impl->popValue(); // object/array
                // Push value back for chaining
                auto* d = impl->pushValue();
                auto* v = impl->builder->CreateLoad(impl->valueTy, val);
                impl->builder->CreateStore(v, d);
                break;
            }

            case OpCode::OP_GET_PROPERTY: {
                uint8_t propIdx = impl->readByte();
                (void)propIdx;
                impl->popValue(); // object
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_SET_PROPERTY: {
                uint8_t propIdx = impl->readByte();
                (void)propIdx;
                auto* val = impl->popValue(); // value to set
                impl->popValue(); // object
                // Push value back for chaining
                auto* d = impl->pushValue();
                auto* v = impl->builder->CreateLoad(impl->valueTy, val);
                impl->builder->CreateStore(v, d);
                break;
            }

            case OpCode::OP_THIS: {
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_INVOKE: {
                uint8_t propIdx = impl->readByte();
                uint8_t argCount = impl->readByte();
                (void)propIdx;
                for (int i = 0; i < argCount; i++) impl->popValue();
                impl->popValue(); // receiver
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_FOR_IN_INIT: {
                impl->popValue(); // iterable
                // Push 3 nils (iterable, index, keys)
                for (int i = 0; i < 3; i++) {
                    auto* d = impl->pushValue();
                    impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                }
                break;
            }

            case OpCode::OP_FOR_IN_NEXT: {
                uint8_t baseSlot = impl->readByte();
                uint8_t varSlot = impl->readByte();
                (void)baseSlot; (void)varSlot;
                uint8_t offsetHi = impl->readByte();
                uint8_t offsetLo = impl->readByte();
                // Simulate "no more items" — jump to exit
                uint16_t offset = (static_cast<uint16_t>(offsetHi) << 8) | offsetLo;
                impl->ip += offset;
                break;
            }

            case OpCode::OP_OPTIONAL_CHAIN: {
                uint8_t propIdx = impl->readByte();
                (void)propIdx;
                impl->popValue(); // object (or nil)
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_SPREAD: {
                impl->popValue(); // array
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            // === Extended opcodes (fallback handlers) ===
            case OpCode::OP_TAIL_CALL: {
                uint8_t argCount = impl->readByte();
                for (int i = 0; i <= argCount; i++) impl->popValue();
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_THROW: {
                impl->popValue(); // exception value
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_BREAK:
            case OpCode::OP_CONTINUE:
            case OpCode::OP_END_TRY:
            case OpCode::OP_LOOP_HINT:
            case OpCode::OP_VALIDATE_SAFE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_FILE_FUNCTION:
                // No operands, no stack effect
                break;

            case OpCode::OP_TRY: {
                // tryEnd(2) + catchStart(2) + finallyStart(2) — skip all operands
                impl->readShort(); impl->readShort(); impl->readShort();
                break;
            }

            case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE: {
                // 1 byte constant index for variable name
                impl->readByte();
                break;
            }

            case OpCode::OP_TYPE_GUARD: {
                // 1 byte type info operand
                impl->readByte();
                break;
            }

            case OpCode::OP_LOGICAL_AND:
            case OpCode::OP_LOGICAL_OR: {
                // 2-byte short-circuit jump offset: pop 2, push 1 result
                uint16_t offset = impl->readShort();
                (void)offset;
                impl->popValue(); // b
                impl->popValue(); // a
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_SAY: {
                auto* a = impl->popValue();
                auto* data = impl->loadData(a);
                auto* tag = impl->loadTag(a);

                // For now: print the number. Full toString requires more work.
                // Declare printf for output
                auto* printfTy = llvm::FunctionType::get(impl->i32Ty, {impl->i8PtrTy}, true);
                auto* printfFunc = impl->module->getFunction("printf");
                if (!printfFunc) {
                    printfFunc = llvm::Function::Create(printfTy,
                                                         llvm::Function::ExternalLinkage, "printf",
                                                         impl->module.get());
                }

                // Check if tag == NUMBER → print number
                // Otherwise print "<non-number>"
                auto* numTag = llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER);
                auto* isNum = impl->builder->CreateICmpEQ(tag, numTag, "is_num");

                auto* curBlock = impl->builder->GetInsertBlock();
                auto* printNumBB = llvm::BasicBlock::Create(ctx, "print_num", func);
                auto* printOtherBB = llvm::BasicBlock::Create(ctx, "print_other", func);
                auto* afterBB = llvm::BasicBlock::Create(ctx, "after_print", func);

                impl->builder->CreateCondBr(isNum, printNumBB, printOtherBB);

                // Print number
                impl->builder->SetInsertPoint(printNumBB);
                auto* fmtNum = impl->builder->CreateGlobalStringPtr("%g\n", "fmt_num");
                auto* numAsDouble = impl->loadData(a);
                // Promote to i64 for integer check
                auto* truncated = impl->builder->CreateFPToSI(numAsDouble, impl->i32Ty, "trunc");
                auto* backToDouble = impl->builder->CreateSIToFP(truncated, impl->doubleTy);
                auto* isInt = impl->builder->CreateFCmpOEQ(numAsDouble, backToDouble, "is_int");
                auto* intBlock = llvm::BasicBlock::Create(ctx, "print_int", func);
                auto* floatBlock = llvm::BasicBlock::Create(ctx, "print_float", func);

                impl->builder->CreateCondBr(isInt, intBlock, floatBlock);

                impl->builder->SetInsertPoint(intBlock);
                auto* fmtInt = impl->builder->CreateGlobalStringPtr("%d\n", "fmt_int");
                impl->builder->CreateCall(printfFunc, {fmtInt, truncated});
                impl->builder->CreateBr(afterBB);

                impl->builder->SetInsertPoint(floatBlock);
                impl->builder->CreateCall(printfFunc, {fmtNum, numAsDouble});
                impl->builder->CreateBr(afterBB);

                // Print other
                impl->builder->SetInsertPoint(printOtherBB);
                auto* tagCheck = impl->builder->CreateICmpEQ(tag,
                                                               llvm::ConstantInt::get(impl->i8Ty, TAG_NIL), "is_nil");
                auto* nilBB = llvm::BasicBlock::Create(ctx, "print_nil", func);
                auto* boolBB = llvm::BasicBlock::Create(ctx, "print_bool", func);

                impl->builder->CreateCondBr(tagCheck, nilBB, boolBB);

                impl->builder->SetInsertPoint(nilBB);
                auto* fmtNil = impl->builder->CreateGlobalStringPtr("nil\n", "fmt_nil");
                impl->builder->CreateCall(printfFunc, {fmtNil});
                impl->builder->CreateBr(afterBB);

                impl->builder->SetInsertPoint(boolBB);
                auto* isTrue = impl->builder->CreateFCmpONE(data,
                                                              llvm::ConstantFP::get(impl->doubleTy, 0.0), "is_true");
                auto* trueBB = llvm::BasicBlock::Create(ctx, "print_true", func);
                auto* falseBB = llvm::BasicBlock::Create(ctx, "print_false", func);
                impl->builder->CreateCondBr(isTrue, trueBB, falseBB);

                impl->builder->SetInsertPoint(trueBB);
                auto* fmtTrue = impl->builder->CreateGlobalStringPtr("true\n", "fmt_true");
                impl->builder->CreateCall(printfFunc, {fmtTrue});
                impl->builder->CreateBr(afterBB);

                impl->builder->SetInsertPoint(falseBB);
                auto* fmtFalse = impl->builder->CreateGlobalStringPtr("false\n", "fmt_false");
                impl->builder->CreateCall(printfFunc, {fmtFalse});
                impl->builder->CreateBr(afterBB);

                impl->builder->SetInsertPoint(afterBB);
                break;
            }

            default: {
                // Unhandled opcodes — skip their operands
                bool skip1 = false, skip2 = false, skipShort = false;
                switch (op) {
                    case OpCode::OP_CONSTANT:
                    case OpCode::OP_GET_LOCAL:
                    case OpCode::OP_SET_LOCAL:
                    case OpCode::OP_INCREMENT_LOCAL:
                    case OpCode::OP_DECREMENT_LOCAL:
                    case OpCode::OP_INC_LOCAL_INT:
                    case OpCode::OP_DEC_LOCAL_INT:
                    case OpCode::OP_CONST_INT8:
                    case OpCode::OP_SET_LOCAL_TYPED:
                        skip1 = true;
                        break;
                    case OpCode::OP_CONSTANT_LONG:
                    case OpCode::OP_ADD_LOCAL_CONST:
                        skip2 = true;
                        break;
                    case OpCode::OP_JUMP:
                    case OpCode::OP_JUMP_IF_FALSE:
                    case OpCode::OP_LOOP:
                    case OpCode::OP_LESS_JUMP:
                    case OpCode::OP_GREATER_JUMP:
                    case OpCode::OP_EQUAL_JUMP:
                        skipShort = true;
                        break;
                    case OpCode::OP_LOOP_IF_LESS_LOCAL:
                        impl->ip += 3;
                        break;
                    default:
                        break;
                }
                if (skip1) impl->ip++;
                if (skip2) impl->ip += 2;
                if (skipShort) impl->ip += 2;
                break;
            }
        }
    }

    // Verify
    if (llvm::verifyFunction(*func, &llvm::outs())) {
        std::cerr << "LLVM function verification failed" << std::endl;
        return false;
    }

    // Select target
    std::string targetTriple;
    switch (targetPlatform) {
        case TargetPlatform::LINUX_X64:   targetTriple = "x86_64-linux-gnu"; break;
        case TargetPlatform::LINUX_ARM64: targetTriple = "aarch64-linux-gnu"; break;
        case TargetPlatform::MACOS_X64:   targetTriple = "x86_64-apple-macosx"; break;
        case TargetPlatform::MACOS_ARM64: targetTriple = "arm64-apple-macosx"; break;
        case TargetPlatform::WINDOWS_X64: targetTriple = "x86_64-pc-windows-msvc"; break;
        case TargetPlatform::WINDOWS_X86: targetTriple = "i686-pc-windows-msvc"; break;
        default:                          targetTriple = llvm::sys::getDefaultTargetTriple(); break;
    }
    impl->module->setTargetTriple(llvm::Triple(targetTriple));

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        std::cerr << "LLVM target lookup failed: " << error << std::endl;
        return false;
    }

    auto* tm = target->createTargetMachine(llvm::Triple(targetTriple), "generic", "", {}, llvm::Reloc::PIC_);
    impl->module->setDataLayout(tm->createDataLayout());

    // Run LLVM optimization pipeline (O2)
    {
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        llvm::PassBuilder PB(tm);
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        MPM.run(*impl->module, MAM);
    }

    // Emit object file
    std::error_code ec;
    llvm::raw_fd_ostream dest(outputPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Could not open output file: " << ec.message() << std::endl;
        return false;
    }

    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "LLVM pass manager setup failed" << std::endl;
        return false;
    }
    pass.run(*impl->module);
    dest.flush();

    return true;
}

bool LlvmCodegen::generateModule(const std::string& functionName) {
    return generateModule(functionName, "output.o");
}

} // namespace aot
} // namespace neutron
