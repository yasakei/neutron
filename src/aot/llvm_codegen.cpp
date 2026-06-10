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
#include <string>
#include <cstdlib>
#include <cmath>
#include <cstring>

namespace neutron {
namespace aot {

// NaN-boxing v2: 3-bit tag at bits 49:47, 47-bit payload at bits 46:0.
// Numbers are raw double bits (not NaN-boxed).
// NAN_BASE = 0x7FFC000000000000 (quiet NaN with bit 50 set).
// Tagged value: (val & NAN_MASK) == NAN_BASE → tag = (val >> 47) & 0x7
// Payload: val & PAYLOAD_MASK
static const uint64_t NAN_BASE = 0x7FFC000000000000ULL;
static const uint64_t NAN_MASK = 0x7FFC000000000000ULL;
static const uint64_t PAYLOAD_MASK = 0x7FFFFFFFFFFFULL;
static const int TAG_SHIFT = 47;

// Sentinel for "cache miss" in focused property/array helpers.
// Matches AOT_SENTINEL in aot_runtime.h
static const uint64_t AOT_SENTINEL = 0x7FFE380000000000ULL;

enum ValueTag : uint8_t {
    TAG_NIL = 0,
    TAG_BOOL = 1,
    TAG_NUMBER = 2,
    TAG_STRING = 3,
    TAG_ARRAY = 4,
    TAG_INSTANCE = 5,
    TAG_CALLABLE = 6,
    TAG_OBJECT = 7
};

static inline uint64_t f64bits(double d) {
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    return bits;
}

struct LlvmCodegenImpl {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // LLVM type handles
    llvm::Type* i64Ty = nullptr;
    llvm::Type* i8Ty = nullptr;
    llvm::Type* i16Ty = nullptr;
    llvm::Type* i32Ty = nullptr;
    llvm::Type* doubleTy = nullptr;
    llvm::PointerType* i8PtrTy = nullptr;

    // Active function state
    llvm::Function* func = nullptr;
    llvm::Value* vmCtx = nullptr;
    llvm::Value* slotsAlloca = nullptr;
    llvm::Value* spAlloca = nullptr;

    // Global constants array reference
    llvm::GlobalVariable* constantsGlobal = nullptr;

    // Per-chunk cache of pre-interned strings (populated lazily on first access)
    llvm::GlobalVariable* internCacheGlobal = nullptr;

    // Function table for runtime-resolved function closures
    // Populated at startup by the C wrapper from the chunk's function-typed constants
    llvm::GlobalVariable* funcTableGlobal = nullptr;

    // External function declarations
    llvm::Function* printfFunc = nullptr;
    llvm::Function* putsFunc = nullptr;
    llvm::Function* aotGetPropFunc = nullptr;
    llvm::Function* aotSetPropFunc = nullptr;
    llvm::Function* aotGetPropCachedFunc = nullptr;
    llvm::Function* aotSetPropCachedFunc = nullptr;
    llvm::Function* aotIndexGetFunc = nullptr;
    llvm::Function* aotIndexSetFunc = nullptr;
    llvm::Function* aotCreateArrayFunc = nullptr;
    llvm::Function* aotCreateObjectFunc = nullptr;
    llvm::Function* aotCallFunc = nullptr;
    llvm::Function* aotInvokeFunc = nullptr;
    llvm::Function* aotPrintFunc = nullptr;
    llvm::Function* aotInternFunc = nullptr;
    llvm::Function* aotForInInitFunc = nullptr;
    llvm::Function* aotAddFunc = nullptr;
    llvm::Function* aotThrowErrorFunc = nullptr;
    llvm::Function* aotRuntimeErrorFunc = nullptr;
    llvm::Function* aotTryPushFunc = nullptr;
    llvm::Function* aotTryPopFunc = nullptr;
    llvm::Function* aotValidateSafeFuncFunc = nullptr;
    llvm::Function* aotValidateSafeFileFuncFunc = nullptr;
    llvm::Function* aotReportTypeErrorFunc = nullptr;
    llvm::Function* aotSetGlobalTypedFunc = nullptr;
    llvm::Function* aotDefineTypedGlobalFunc = nullptr;

    llvm::Function* aotStringCharAtFunc = nullptr;

    // Phase 6: Direct native call support
    llvm::Function* aotTryDirectCallFunc = nullptr;
    llvm::Function* aotRegisterLlvmFuncFunc = nullptr;

    // Whether we're compiling the main entry function (affects OP_RETURN behavior)
    bool isMainFunc = true;

    const Chunk* chunk = nullptr;

    // Per-callsite property cache counter (for unique global names)
    uint32_t propCacheId = 0;

    // Create a per-callsite property cache global variable
    // The cache is a 16-byte struct: {void* klass, uint8_t inlineIndex, uint8_t pad[7]}
    llvm::GlobalVariable* createPropCache() {
        auto* cacheTy = llvm::ArrayType::get(i64Ty, 2); // 16 bytes = two i64s
        std::string name = "prop_cache_" + std::to_string(propCacheId++);
        auto* gv = new llvm::GlobalVariable(*module, cacheTy, false,
                                              llvm::GlobalValue::InternalLinkage,
                                              llvm::Constant::getNullValue(cacheTy), name);
        return gv;
    }

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

                case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                    size_t instrStart = scanIp;
                    if (scanIp + 3 < chunk->code.size()) {
                        scanIp += 3; // slot + const index
                        uint16_t offset = (chunk->code[scanIp] << 8) | chunk->code[scanIp + 1];
                        scanIp += 2;
                        size_t exitTarget = scanIp + offset;
                        if (exitTarget < chunk->code.size()) isJumpTarget[exitTarget] = true;
                        size_t loopTarget = scanIp - 5;
                        if (loopTarget < chunk->code.size()) isJumpTarget[loopTarget] = true;
                    }
                    handled = true;
                    break;
                }

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
                    skip1 = true;
                    break;
                case OpCode::OP_SET_LOCAL_TYPED:
                    scanIp += 2; // slot + typeByte
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
                    scanIp++; // skip funcIdx byte only (compiler doesn't emit upvalue operands)
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

    // Initialize LLVM types
    void initTypes() {
        i8Ty = llvm::Type::getInt8Ty(context);
        i16Ty = llvm::Type::getInt16Ty(context);
        i32Ty = llvm::Type::getInt32Ty(context);
        i64Ty = llvm::Type::getInt64Ty(context);
        doubleTy = llvm::Type::getDoubleTy(context);
        i8PtrTy = llvm::PointerType::get(i8Ty, 0);
    }

    // Create a NaN-boxed constant value as i64
    llvm::Constant* constValue(ValueTag tag, double data) {
        if (tag == TAG_NUMBER) {
            return llvm::ConstantInt::get(i64Ty, f64bits(data));
        }
        uint64_t bits = NAN_BASE | (static_cast<uint64_t>(tag) << TAG_SHIFT);
        if (tag == TAG_BOOL && data != 0.0) {
            bits |= 1ULL; // bool payload at bit 0
        }
        return llvm::ConstantInt::get(i64Ty, bits);
    }

    // Allocate and initialize combined locals+stack buffer
    void setupStackLocals(llvm::Function* f, llvm::Value* ctx) {
        func = f;
        vmCtx = ctx;
        auto* entry = &f->getEntryBlock();
        auto savedIP = builder->saveIP();

        // Insert allocas at the beginning of the entry block
        llvm::IRBuilder<> allocBuilder(entry, entry->begin());

        // Single shared array for both locals and stack (matching interpreter behavior)
        auto* slotsTy = llvm::ArrayType::get(i64Ty, 512);
        slotsAlloca = allocBuilder.CreateAlloca(slotsTy, nullptr, "slots");

        // Initialize all local slots to nil
        for (int i = 0; i < 256; i++) {
            auto* gep = allocBuilder.CreateGEP(slotsTy, slotsAlloca,
                                                {llvm::ConstantInt::get(i32Ty, 0),
                                                 llvm::ConstantInt::get(i32Ty, i)});
            allocBuilder.CreateStore(constValue(TAG_NIL, 0.0), gep);
        }

        // sp starts at 0. Local slots (0..) and stack temps share the same array,
        // matching the interpreter where slot_offset aligns with the first push.
        spAlloca = allocBuilder.CreateAlloca(i32Ty, nullptr, "sp");
        allocBuilder.CreateStore(llvm::ConstantInt::get(i32Ty, 0), spAlloca);

        builder->restoreIP(savedIP);
    }

    // Declare external functions
    void declareExternals() {
        // i32 @printf(i8*, ...) — inlined for all OP_SAY value printing
        auto* printfTy = llvm::FunctionType::get(i32Ty, {i8PtrTy}, true);
        printfFunc = llvm::Function::Create(printfTy, llvm::Function::ExternalLinkage, "printf", module.get());

        auto* putsTy = llvm::FunctionType::get(i32Ty, {i8PtrTy}, false);
        putsFunc = llvm::Function::Create(putsTy, llvm::Function::ExternalLinkage, "puts", module.get());

        // AOT runtime helpers
        // i64 @aot_getProperty(i8* vm_ctx, i64 obj_val, i8* prop_name)
        auto* getPropTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i8PtrTy}, false);
        aotGetPropFunc = llvm::Function::Create(getPropTy, llvm::Function::ExternalLinkage,
                                                  "aot_getProperty", module.get());

        // void @aot_setProperty(i8* vm_ctx, i64 obj_val, i8* prop_name, i64 val)
        auto* setPropTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty, i8PtrTy, i64Ty}, false);
        aotSetPropFunc = llvm::Function::Create(setPropTy, llvm::Function::ExternalLinkage,
                                                  "aot_setProperty", module.get());

        // i64 @aot_getPropertyCached(i8* vm_ctx, i64 obj_val, i8* prop_name, i8* cache_ptr)
        auto* getPropCachedTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i8PtrTy, i8PtrTy}, false);
        aotGetPropCachedFunc = llvm::Function::Create(getPropCachedTy, llvm::Function::ExternalLinkage,
                                                        "aot_getPropertyCached", module.get());

        // void @aot_setPropertyCached(i8* vm_ctx, i64 obj_val, i8* prop_name, i64 val, i8* cache_ptr)
        auto* setPropCachedTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty, i8PtrTy, i64Ty, i8PtrTy}, false);
        aotSetPropCachedFunc = llvm::Function::Create(setPropCachedTy, llvm::Function::ExternalLinkage,
                                                        "aot_setPropertyCached", module.get());

        // i64 @aot_indexGet(i8* vm_ctx, i64 obj_val, i64 index_val)
        auto* indexGetTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i64Ty}, false);
        aotIndexGetFunc = llvm::Function::Create(indexGetTy, llvm::Function::ExternalLinkage,
                                                   "aot_indexGet", module.get());

        // void @aot_indexSet(i8* vm_ctx, i64 obj_val, i64 index_val, i64 val)
        auto* indexSetTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty, i64Ty, i64Ty}, false);
        aotIndexSetFunc = llvm::Function::Create(indexSetTy, llvm::Function::ExternalLinkage,
                                                   "aot_indexSet", module.get());

        // i64 @aot_createArray(i8* vm_ctx, i8* elements, i8 count)
        auto* createArrTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy, i8Ty}, false);
        aotCreateArrayFunc = llvm::Function::Create(createArrTy, llvm::Function::ExternalLinkage,
                                                      "aot_createArray", module.get());

        // i64 @aot_createObject(i8* vm_ctx, i8* keys, i8* values, i8 count)
        auto* createObjTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy, i8PtrTy, i8Ty}, false);
        aotCreateObjectFunc = llvm::Function::Create(createObjTy, llvm::Function::ExternalLinkage,
                                                       "aot_createObject", module.get());

        // i64 @aot_call(i8* vm_ctx, i64 callee, i8* args, i8 arg_count)
        auto* callTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i8PtrTy, i8Ty}, false);
        aotCallFunc = llvm::Function::Create(callTy, llvm::Function::ExternalLinkage,
                                               "aot_call", module.get());

        // i64 @aot_invoke(i8* vm_ctx, i64 receiver, i8* method_name, i8* args, i8 arg_count)
        auto* invokeTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i8PtrTy, i8PtrTy, i8Ty}, false);
        aotInvokeFunc = llvm::Function::Create(invokeTy, llvm::Function::ExternalLinkage,
                                                  "aot_invoke", module.get());

        // void @aot_printValue(i8* vm_ctx, i64 val)
        auto* printTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty}, false);
        aotPrintFunc = llvm::Function::Create(printTy, llvm::Function::ExternalLinkage,
                                                "aot_printValue", module.get());

        // i64 @aot_internString(i8* vm_ctx, i8* str)
        auto* internTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy}, false);
        aotInternFunc = llvm::Function::Create(internTy, llvm::Function::ExternalLinkage,
                                                  "aot_internString", module.get());

        // i64 @aot_add(i8* vm_ctx, i64 a, i64 b)
        auto* addTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i64Ty}, false);
        aotAddFunc = llvm::Function::Create(addTy, llvm::Function::ExternalLinkage,
                                               "aot_add", module.get());

        // void @aot_runtimeError(i8* vm_ctx, i8* message)
        auto* runtimeErrTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i8PtrTy}, false);
        aotRuntimeErrorFunc = llvm::Function::Create(runtimeErrTy, llvm::Function::ExternalLinkage,
                                                      "aot_runtimeError", module.get());

        // void @aot_tryPush(i8* vm_ctx, i16 tryEnd, i16 catchStart, i16 finallyStart)
        auto* tryPushTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context),
            {i8PtrTy, i16Ty, i16Ty, i16Ty}, false);
        aotTryPushFunc = llvm::Function::Create(tryPushTy, llvm::Function::ExternalLinkage,
                                                  "aot_tryPush", module.get());

        // void @aot_tryPop(i8* vm_ctx)
        auto* tryPopTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy}, false);
        aotTryPopFunc = llvm::Function::Create(tryPopTy, llvm::Function::ExternalLinkage,
                                                 "aot_tryPop", module.get());

        // i64 @aot_forInInit(i8* vm_ctx, i64 iterable)
        auto* forInInitTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty}, false);
        aotForInInitFunc = llvm::Function::Create(forInInitTy, llvm::Function::ExternalLinkage,
                                                    "aot_forInInit", module.get());

        // void @aot_throwError(i8* vm_ctx, i64 exception)
        auto* throwTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty}, false);
        aotThrowErrorFunc = llvm::Function::Create(throwTy, llvm::Function::ExternalLinkage,
                                                     "aot_throwError", module.get());

        // void @aot_validateSafeFunction(i8* vm_ctx, i64 funcVal, i32 isSafeFile)
        auto* validateFnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty, i32Ty}, false);
        aotValidateSafeFuncFunc = llvm::Function::Create(validateFnTy, llvm::Function::ExternalLinkage,
                                                           "aot_validateSafeFunction", module.get());

        // void @aot_validateSafeFileFunction(i8* vm_ctx, i64 funcVal)
        auto* validateFileFnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i64Ty}, false);
        aotValidateSafeFileFuncFunc = llvm::Function::Create(validateFileFnTy, llvm::Function::ExternalLinkage,
                                                               "aot_validateSafeFileFunction", module.get());

        // void @aot_reportTypeError(i8* vm_ctx, i8 expectedType, i64 val)
        auto* reportTypeErrTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i8Ty, i64Ty}, false);
        aotReportTypeErrorFunc = llvm::Function::Create(reportTypeErrTy, llvm::Function::ExternalLinkage,
                                                          "aot_reportTypeError", module.get());

        // void @aot_setGlobalTyped(i8* vm_ctx, i8* name, i64 val)
        auto* setGlobalTypedTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i8PtrTy, i64Ty}, false);
        aotSetGlobalTypedFunc = llvm::Function::Create(setGlobalTypedTy, llvm::Function::ExternalLinkage,
                                                         "aot_setGlobalTyped", module.get());

        // void @aot_defineTypedGlobal(i8* vm_ctx, i8* name, i64 val, i8 typeByte)
        auto* defineTypedGlobalTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8PtrTy, i8PtrTy, i64Ty, i8Ty}, false);
        aotDefineTypedGlobalFunc = llvm::Function::Create(defineTypedGlobalTy, llvm::Function::ExternalLinkage,
                                                            "aot_defineTypedGlobal", module.get());

        // i64 @aot_stringCharAt(i8* vm_ctx, i64 str_val, i64 idxVal)
        auto* stringCharAtTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i64Ty}, false);
        aotStringCharAtFunc = llvm::Function::Create(stringCharAtTy, llvm::Function::ExternalLinkage,
                                                        "aot_stringCharAt", module.get());
        // Phase 6: Direct native call support
        // i64 @aot_tryDirectCall(i8* vm_ctx, i64 callee, i8* args, i8 argCount)
        auto* tryDirectTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty, i8PtrTy, i8Ty}, false);
        aotTryDirectCallFunc = llvm::Function::Create(tryDirectTy, llvm::Function::ExternalLinkage,
                                                        "aot_tryDirectCall", module.get());

        // void @aot_registerLlvmFunc(i32 idx, i8* funcPtr)
        auto* regFuncTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i32Ty, i8PtrTy}, false);
        aotRegisterLlvmFuncFunc = llvm::Function::Create(regFuncTy, llvm::Function::ExternalLinkage,
                                                          "aot_registerLlvmFunc", module.get());
    }

    // Create the constants global array from a chunk (as NaN-boxed i64 values)
    llvm::GlobalVariable* createConstantsForChunk(const Chunk* c, const std::string& name) {
        size_t n = c->constants.size();
        auto* arrTy = llvm::ArrayType::get(i64Ty, n);

        std::vector<llvm::Constant*> elems;
        for (size_t i = 0; i < n; i++) {
            const Value& v = c->constants[i];
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
        return new llvm::GlobalVariable(*module, arrTy, true,
                                         llvm::GlobalValue::InternalLinkage, init, name);
    }

    void createConstants() {
        constantsGlobal = createConstantsForChunk(chunk, "constants");
    }

    // Create a per-chunk cache array for lazily-interned string constants (all zeros = not interned)
    void createInternCache() {
        size_t n = chunk->constants.size();
        auto* arrTy = llvm::ArrayType::get(i64Ty, n);
        internCacheGlobal = new llvm::GlobalVariable(*module, arrTy, false,
            llvm::GlobalValue::InternalLinkage,
            llvm::Constant::getNullValue(arrTy), "intern_cache");
    }

    // --- Stack access helpers ---

    llvm::Value* stackGEP(llvm::Value* idx) {
        return builder->CreateGEP(llvm::ArrayType::get(i64Ty, 512), slotsAlloca,
                                   {llvm::ConstantInt::get(i32Ty, 0), idx});
    }

    llvm::Value* localsGEP(llvm::Value* idx) {
        return builder->CreateGEP(llvm::ArrayType::get(i64Ty, 512), slotsAlloca,
                                   {llvm::ConstantInt::get(i32Ty, 0), idx});
    }

    llvm::Value* constantsGEP(llvm::Value* idx) {
        auto* arrTy = llvm::ArrayType::get(i64Ty, chunk->constants.size());
        return builder->CreateGEP(arrTy, constantsGlobal,
                                   {llvm::ConstantInt::get(i32Ty, 0), idx});
    }

    llvm::Value* constantsGEPForChunk(llvm::Value* idx, const Chunk* c, llvm::GlobalVariable* cg) {
        auto* arrTy = llvm::ArrayType::get(i64Ty, c->constants.size());
        return builder->CreateGEP(arrTy, cg,
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

    // --- NaN-boxed value access helpers ---
    // All values are stored as i64. Numbers (TAG_NUMBER) use raw double bits.
    // Tagged values (nil, bool, string) use NAN_BASE | tag [| bool_payload].

    // Extract tag from a value pointer (returns i8)
    llvm::Value* loadTag(llvm::Value* ptr) {
        auto* val = builder->CreateLoad(i64Ty, ptr, "val");
        auto* masked = builder->CreateAnd(val,
            llvm::ConstantInt::get(i64Ty, NAN_MASK));
        auto* isTagged = builder->CreateICmpEQ(masked,
            llvm::ConstantInt::get(i64Ty, NAN_BASE), "is_tagged");
        auto* shifted = builder->CreateLShr(val,
            llvm::ConstantInt::get(i64Ty, TAG_SHIFT), "tag_shifted");
        auto* tagBits = builder->CreateTrunc(
            builder->CreateAnd(shifted, llvm::ConstantInt::get(i64Ty, 0x7)),
            i8Ty, "tag_bits");
        auto* numTag = llvm::ConstantInt::get(i8Ty, TAG_NUMBER);
        return builder->CreateSelect(isTagged, tagBits, numTag, "tag");
    }

    // Extract double data from a value pointer (for numbers; bitcast i64 to double)
    llvm::Value* loadData(llvm::Value* ptr) {
        auto* val = builder->CreateLoad(i64Ty, ptr, "val");
        return builder->CreateBitCast(val, doubleTy, "data");
    }

    // Compute truthiness (returns i1). True if the value is truthy.
    // Numbers: non-zero is truthy. Nil: always falsy. Bool: payload bit 0.
    // Other tagged types (string, array, object, etc.): truthy.
    llvm::Value* computeTruthy(llvm::Value* ptr) {
        auto* val = builder->CreateLoad(i64Ty, ptr, "val");
        auto* masked = builder->CreateAnd(val,
            llvm::ConstantInt::get(i64Ty, NAN_MASK));
        auto* isTagged = builder->CreateICmpEQ(masked,
            llvm::ConstantInt::get(i64Ty, NAN_BASE), "is_tagged");

        auto* shifted = builder->CreateLShr(val,
            llvm::ConstantInt::get(i64Ty, TAG_SHIFT), "t_shifted");
        auto* tagBits = builder->CreateTrunc(
            builder->CreateAnd(shifted, llvm::ConstantInt::get(i64Ty, 0x7)),
            i8Ty, "tag_bits");

        // Number truthy: double != 0.0
        auto* numData = builder->CreateBitCast(val, doubleTy);
        auto* numTruthy = builder->CreateFCmpONE(numData,
            llvm::ConstantFP::get(doubleTy, 0.0), "num_t");
        auto* falseVal = llvm::ConstantInt::getFalse(context);
        auto* trueVal = llvm::ConstantInt::getTrue(context);
        auto* nilTag = llvm::ConstantInt::get(i8Ty, TAG_NIL);
        auto* boolTag = llvm::ConstantInt::get(i8Ty, TAG_BOOL);
        auto* isNil = builder->CreateICmpEQ(tagBits, nilTag);
        auto* boolPayload = builder->CreateAnd(val,
            llvm::ConstantInt::get(i64Ty, 1));
        auto* boolTruthy = builder->CreateICmpNE(boolPayload,
            llvm::ConstantInt::get(i64Ty, 0), "bool_t");
        auto* isBool = builder->CreateICmpEQ(tagBits, boolTag);
        // tagged truthy = not nil and (not bool or bool truthy)
        auto* notNil = builder->CreateNot(isNil);
        auto* boolResolved = builder->CreateSelect(isBool, boolTruthy, trueVal);
        auto* taggedTruthy = builder->CreateAnd(notNil, boolResolved, "tagged_t");
        return builder->CreateSelect(isTagged, taggedTruthy, numTruthy, "truthy");
    }

    // Store a NaN-boxed value (tagged or number) into a pointer
    void storeValue(llvm::Value* ptr, llvm::Value* tag, llvm::Value* data) {
        auto* isNum = builder->CreateICmpEQ(tag,
            llvm::ConstantInt::get(i8Ty, TAG_NUMBER), "is_num");
        auto* numVal = builder->CreateBitCast(data, i64Ty, "num_i64");
        auto* tag64 = builder->CreateZExt(tag, i64Ty, "tag64");
        auto* shiftedTag = builder->CreateShl(tag64,
            llvm::ConstantInt::get(i64Ty, TAG_SHIFT), "tag_shifted");
        auto* taggedVal = builder->CreateOr(
            llvm::ConstantInt::get(i64Ty, NAN_BASE), shiftedTag, "tagged");

        // Bool payload: bit 0 = (data != 0.0)
        auto* isBool = builder->CreateICmpEQ(tag,
            llvm::ConstantInt::get(i8Ty, TAG_BOOL), "is_bool");
        auto* boolBit = builder->CreateFCmpONE(data,
            llvm::ConstantFP::get(doubleTy, 0.0));
        auto* boolPayload = builder->CreateSelect(boolBit,
            llvm::ConstantInt::get(i64Ty, 1),
            llvm::ConstantInt::get(i64Ty, 0), "bool_payload");
        auto* taggedWithPayload = builder->CreateOr(taggedVal, boolPayload, "tagged_wp");

        auto* finalVal = builder->CreateSelect(isNum, numVal,
            builder->CreateSelect(isBool, taggedWithPayload, taggedVal), "store_val");
        builder->CreateStore(finalVal, ptr);
    }

    // Create a constant NaN-boxed value and store it
    void emitConstStore(llvm::Value* ptr, ValueTag tag, double data) {
        builder->CreateStore(constValue(tag, data), ptr);
    }

    // Convert a C++ Value struct at valPtr (i8*) to NaN-boxed i64
    // Value layout: [0] type (i32), [8] as (i64)
    llvm::Value* emitValueToNan(llvm::Value* valPtr) {
        auto* vType = builder->CreateLoad(i32Ty,
            builder->CreateConstGEP1_32(i8Ty, valPtr, 0, "vt_addr"), "vt");
        auto* vAs = builder->CreateLoad(i64Ty,
            builder->CreateConstGEP1_32(i8Ty, valPtr, 8, "va_addr"), "va");
        auto* nilBB = llvm::BasicBlock::Create(context, "v2n_nil", func);
        auto* notNilBB = llvm::BasicBlock::Create(context, "v2n_nnil", func);
        auto* boolBB = llvm::BasicBlock::Create(context, "v2n_bool", func);
        auto* notBoolBB = llvm::BasicBlock::Create(context, "v2n_nbool", func);
        auto* numBB = llvm::BasicBlock::Create(context, "v2n_num", func);
        auto* notNumBB = llvm::BasicBlock::Create(context, "v2n_nnum", func);
        auto* strBB = llvm::BasicBlock::Create(context, "v2n_str", func);
        auto* notStrBB = llvm::BasicBlock::Create(context, "v2n_nstr", func);
        auto* restBB = llvm::BasicBlock::Create(context, "v2n_rest", func);
        auto* mgBB = llvm::BasicBlock::Create(context, "v2n_mg", func);
        auto* isNil = builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 0), "vt_nil");
        builder->CreateCondBr(isNil, nilBB, notNilBB);
        // nil
        builder->SetInsertPoint(nilBB);
        auto* nilRes = llvm::ConstantInt::get(i64Ty, NAN_BASE);
        builder->CreateBr(mgBB);
        // not nil: bool?
        builder->SetInsertPoint(notNilBB);
        auto* isBool = builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 1), "vt_bool");
        builder->CreateCondBr(isBool, boolBB, notBoolBB);
        // bool
        builder->SetInsertPoint(boolBB);
        auto* boolPay = builder->CreateAnd(vAs, llvm::ConstantInt::get(i64Ty, 1), "bp");
        auto* boolRes = builder->CreateOr(llvm::ConstantInt::get(i64Ty, NAN_BASE | (1ULL << TAG_SHIFT)), boolPay, "b_res");
        builder->CreateBr(mgBB);
        // not bool: number?
        builder->SetInsertPoint(notBoolBB);
        auto* isNum = builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 2), "vt_num");
        builder->CreateCondBr(isNum, numBB, notNumBB);
        // number: raw double bits
        builder->SetInsertPoint(numBB);
        builder->CreateBr(mgBB); // vAs already holds the raw bits
        // not number: string?
        builder->SetInsertPoint(notNumBB);
        auto* isStr = builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 3), "vt_str");
        builder->CreateCondBr(isStr, strBB, notStrBB);
        // string
        builder->SetInsertPoint(strBB);
        auto* strPay = builder->CreateAnd(vAs, llvm::ConstantInt::get(i64Ty, PAYLOAD_MASK), "sp");
        auto* strRes = builder->CreateOr(llvm::ConstantInt::get(i64Ty, NAN_BASE | (2ULL << TAG_SHIFT)), strPay, "s_res");
        builder->CreateBr(mgBB);
        // rest (array=4, object=5, callable=6, class=8, instance=9, ...)
        builder->SetInsertPoint(notStrBB);
        // Map ValueType to runtime tag:
        // ARRAY(4)→3, OBJECT(5)→6, CALLABLE(6)→5, INSTANCE(9)→4, rest→NIL
        auto* tagSel = builder->CreateSelect(
            builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 4)),
            llvm::ConstantInt::get(i64Ty, 3),
            builder->CreateSelect(
                builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 5)),
                llvm::ConstantInt::get(i64Ty, 6),
                builder->CreateSelect(
                    builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 6)),
                    llvm::ConstantInt::get(i64Ty, 5),
                    builder->CreateSelect(
                        builder->CreateICmpEQ(vType, llvm::ConstantInt::get(i32Ty, 9)),
                        llvm::ConstantInt::get(i64Ty, 4),
                        llvm::ConstantInt::get(i64Ty, 0)))), "rt_tag");
        auto* restPay = builder->CreateAnd(vAs, llvm::ConstantInt::get(i64Ty, PAYLOAD_MASK), "rp");
        auto* tagged = builder->CreateOr(llvm::ConstantInt::get(i64Ty, NAN_BASE),
            builder->CreateOr(builder->CreateShl(tagSel, llvm::ConstantInt::get(i64Ty, TAG_SHIFT), "ts"),
                              restPay), "rest_res");
        builder->CreateBr(mgBB);
        // merge
        builder->SetInsertPoint(mgBB);
        auto* phi = builder->CreatePHI(i64Ty, 5, "v2n");
        phi->addIncoming(nilRes, nilBB);
        phi->addIncoming(boolRes, boolBB);
        phi->addIncoming(vAs, numBB);    // raw double for number
        phi->addIncoming(strRes, strBB);
        phi->addIncoming(tagged, notStrBB);
        return phi;
    }

    // Convert NaN-boxed i64 to a C++ Value struct at destPtr (i8*)
    // Value layout: [0] type (i32), [8] as (i64)
    void emitNanToValue(llvm::Value* nanVal, llvm::Value* destPtr) {
        auto* nanMask = llvm::ConstantInt::get(i64Ty, NAN_MASK);
        auto* nanBase = llvm::ConstantInt::get(i64Ty, NAN_BASE);
        auto* masked = builder->CreateAnd(nanVal, nanMask, "n2v_msk");
        auto* isTagged = builder->CreateICmpEQ(masked, nanBase, "n2v_tgd");
        auto* numBB = llvm::BasicBlock::Create(context, "n2v_num", func);
        auto* tagBB = llvm::BasicBlock::Create(context, "n2v_tag", func);
        auto* mgBB = llvm::BasicBlock::Create(context, "n2v_mg", func);
        builder->CreateCondBr(isTagged, tagBB, numBB);
        // Untagged = number
        builder->SetInsertPoint(numBB);
        auto* numAddr = builder->CreateConstGEP1_32(i8Ty, destPtr, 8, "n2v_num_ua");
        builder->CreateStore(llvm::ConstantInt::get(i32Ty, 2), // NUMBER = 2
            builder->CreateConstGEP1_32(i8Ty, destPtr, 0, "n2v_num_ta"));
        builder->CreateStore(nanVal, numAddr); // raw double bits
        builder->CreateBr(mgBB);
        // Tagged
        builder->SetInsertPoint(tagBB);
        auto* shifted = builder->CreateLShr(nanVal, llvm::ConstantInt::get(i64Ty, TAG_SHIFT), "n2v_sh");
        auto* tagBits = builder->CreateAnd(shifted, llvm::ConstantInt::get(i64Ty, 0x7), "n2v_tag");
        auto* payload = builder->CreateAnd(nanVal, llvm::ConstantInt::get(i64Ty, PAYLOAD_MASK), "n2v_pay");
        // Build tag→ValueType mapping: 0→0(NIL),1→1(BOOL),2→3(OBJ_STRING),3→4(ARRAY),4→9(INSTANCE),5→6(CALLABLE),6→5(OBJECT)
        // Using selects for each tag
        auto* nilCmp = builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(i64Ty, 0));
        auto* boolCmp = builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(i64Ty, 1));
        auto* strCmp = builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(i64Ty, 2));
        auto* arrCmp = builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(i64Ty, 3));
        auto* instCmp = builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(i64Ty, 4));
        auto* callCmp = builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(i64Ty, 5));
        // store type (as i32)
        auto* vt = builder->CreateSelect(nilCmp, llvm::ConstantInt::get(i32Ty, 0),
            builder->CreateSelect(boolCmp, llvm::ConstantInt::get(i32Ty, 1),
            builder->CreateSelect(strCmp, llvm::ConstantInt::get(i32Ty, 3),
            builder->CreateSelect(arrCmp, llvm::ConstantInt::get(i32Ty, 4),
            builder->CreateSelect(instCmp, llvm::ConstantInt::get(i32Ty, 9),
            builder->CreateSelect(callCmp, llvm::ConstantInt::get(i32Ty, 6),
                                  llvm::ConstantInt::get(i32Ty, 5)))))), "n2v_vt");
        builder->CreateStore(vt, builder->CreateConstGEP1_32(i8Ty, destPtr, 0, "n2v_ta"));
        // For bool: store payload & 1; for others: store payload as pointer bits
        auto* boolPay = builder->CreateAnd(payload, llvm::ConstantInt::get(i64Ty, 1), "n2v_bp");
        auto* storedVal = builder->CreateSelect(boolCmp, boolPay, payload, "n2v_val");
        builder->CreateStore(storedVal, builder->CreateConstGEP1_32(i8Ty, destPtr, 8, "n2v_ua"));
        builder->CreateBr(mgBB);
        // merge (nothing to do)
        builder->SetInsertPoint(mgBB);
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

        auto* gv = new llvm::GlobalVariable(*module, i64Ty, false,
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
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
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
    impl->createInternCache();

    // Create externally-visible func table (populated at runtime by C wrapper)
    {
        auto* arrTy = llvm::ArrayType::get(impl->i64Ty, 256);
        impl->funcTableGlobal = new llvm::GlobalVariable(*impl->module, arrTy, false,
            llvm::GlobalValue::ExternalLinkage,
            llvm::Constant::getNullValue(arrTy), "neutron_func_table");
    }

    // Create main function: i32 @neutron_main(i8* %vm_ctx)
    auto* vmCtxTy = llvm::PointerType::get(impl->i8Ty, 0);
    auto* funcType = llvm::FunctionType::get(impl->i32Ty, {vmCtxTy}, false);
    auto* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, functionName, impl->module.get());
    auto* vmCtxArg = func->getArg(0);
    vmCtxArg->setName("vm_ctx");

    auto* entry = llvm::BasicBlock::Create(ctx, "entry", func);
    impl->builder->SetInsertPoint(entry);
    impl->setupStackLocals(func, vmCtxArg);

    // Lambda for compiling current function
    auto emitCurrentFunc = [&]() -> bool {
        impl->bbMap.clear();
        impl->findJumpTargets();
        if (impl->isMainFunc) impl->collectGlobals();
        auto* startBB = impl->getOrCreateBB(0, impl->func);
        impl->builder->CreateBr(startBB);
        impl->builder->SetInsertPoint(startBB);

        impl->ip = 0;
        while (impl->ip < impl->chunk->code.size()) {
            impl->ensureBlock(impl->ip);

            uint8_t instr = impl->readByte();
            OpCode op = static_cast<OpCode>(instr);

            switch (op) {
                case OpCode::OP_RETURN: {
                auto* retVal = impl->popValue();
                if (impl->isMainFunc) {
                    impl->builder->CreateRet(llvm::ConstantInt::get(impl->i32Ty, 0));
                } else {
                    impl->builder->CreateRet(retVal);
                }
                break;
            }

            case OpCode::OP_CONSTANT: {
                uint8_t index = impl->readByte();
                if (index < impl->chunk->constants.size() &&
                    impl->chunk->constants[index].type == ValueType::OBJ_STRING) {
                    std::string strContent = impl->chunk->constants[index].as.obj_string->chars;
                    auto* strPtr = impl->builder->CreateGlobalStringPtr(strContent, "const_str");
                    auto* arrTy = llvm::ArrayType::get(impl->i64Ty, impl->chunk->constants.size());
                    auto* cacheSlot = impl->builder->CreateGEP(arrTy, impl->internCacheGlobal,
                        {llvm::ConstantInt::get(impl->i32Ty, 0),
                         llvm::ConstantInt::get(impl->i32Ty, index)}, "cs");
                    auto* cached = impl->builder->CreateLoad(impl->i64Ty, cacheSlot, "cached_s");
                    auto* isCached = impl->builder->CreateICmpNE(cached,
                        llvm::ConstantInt::get(impl->i64Ty, 0), "is_cached");
                    auto* internBB = llvm::BasicBlock::Create(ctx, "intern_s", impl->func);
                    auto* doneIBB = llvm::BasicBlock::Create(ctx, "intern_done", impl->func);
                    auto* curIBB = impl->builder->GetInsertBlock();
                    impl->builder->CreateCondBr(isCached, doneIBB, internBB);
                    impl->builder->SetInsertPoint(internBB);
                    auto* interned = impl->builder->CreateCall(impl->aotInternFunc,
                        {impl->vmCtx, strPtr}, "interned");
                    impl->builder->CreateStore(interned, cacheSlot);
                    impl->builder->CreateBr(doneIBB);
                    impl->builder->SetInsertPoint(doneIBB);
                    auto* sResult = impl->builder->CreatePHI(impl->i64Ty, 2, "s_val");
                    sResult->addIncoming(cached, curIBB);
                    sResult->addIncoming(interned, internBB);
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(sResult, dest);
                } else {
                    auto* dest = impl->pushValue();
                    auto* src = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, index));
                    auto* val = impl->builder->CreateLoad(impl->i64Ty, src, "const_val");
                    impl->builder->CreateStore(val, dest);
                }
                break;
            }

            case OpCode::OP_CONSTANT_LONG: {
                uint16_t index = impl->readShort();
                if (index < impl->chunk->constants.size() &&
                    impl->chunk->constants[index].type == ValueType::OBJ_STRING) {
                    std::string strContent = impl->chunk->constants[index].as.obj_string->chars;
                    auto* strPtr = impl->builder->CreateGlobalStringPtr(strContent, "const_str");
                    auto* arrTy = llvm::ArrayType::get(impl->i64Ty, impl->chunk->constants.size());
                    auto* cacheSlot = impl->builder->CreateGEP(arrTy, impl->internCacheGlobal,
                        {llvm::ConstantInt::get(impl->i32Ty, 0),
                         llvm::ConstantInt::get(impl->i32Ty, index)}, "cs_l");
                    auto* cached = impl->builder->CreateLoad(impl->i64Ty, cacheSlot, "cached_s");
                    auto* isCached = impl->builder->CreateICmpNE(cached,
                        llvm::ConstantInt::get(impl->i64Ty, 0), "is_cached");
                    auto* internBB = llvm::BasicBlock::Create(ctx, "intern_s_l", impl->func);
                    auto* doneIBB = llvm::BasicBlock::Create(ctx, "intern_done_l", impl->func);
                    auto* curIBB = impl->builder->GetInsertBlock();
                    impl->builder->CreateCondBr(isCached, doneIBB, internBB);
                    impl->builder->SetInsertPoint(internBB);
                    auto* interned = impl->builder->CreateCall(impl->aotInternFunc,
                        {impl->vmCtx, strPtr}, "interned");
                    impl->builder->CreateStore(interned, cacheSlot);
                    impl->builder->CreateBr(doneIBB);
                    impl->builder->SetInsertPoint(doneIBB);
                    auto* sResult = impl->builder->CreatePHI(impl->i64Ty, 2, "s_val");
                    sResult->addIncoming(cached, curIBB);
                    sResult->addIncoming(interned, internBB);
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(sResult, dest);
                } else {
                    auto* dest = impl->pushValue();
                    auto* src = impl->constantsGEP(llvm::ConstantInt::get(impl->i32Ty, index));
                    auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                    impl->builder->CreateStore(val, dest);
                }
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
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_GET_LOCAL: {
                uint8_t slot = impl->readByte();
                auto* src = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* dest = impl->pushValue();
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = impl->readByte();
                auto* src = impl->popValue();
                auto* dest = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_SET_LOCAL_TYPED: {
                uint8_t slot = impl->readByte();
                uint8_t typeByte = impl->readByte();
                auto* src = impl->popValue();
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                // Inline type validation: compare tag bits against expected type
                auto* tlNanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* tlNanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* tlMasked = impl->builder->CreateAnd(val, tlNanMask, "tl_m");
                auto* tlTagged = impl->builder->CreateICmpEQ(tlMasked, tlNanBase, "tl_tg");
                llvm::Value* tlValid = nullptr;
                if (typeByte == 83) { // TYPE_ANY
                    tlValid = llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), 1);
                } else if (typeByte == 77 || typeByte == 78) { // TYPE_INT, TYPE_FLOAT → untagged number
                    tlValid = impl->builder->CreateNot(tlTagged, "tl_num");
                } else {
                    // String(79)→tag2, Bool(80)→tag1, Array(81)→tag3, Object(82)→tag6
                    uint64_t expectedTag = (typeByte == 79) ? 2 :
                                           (typeByte == 80) ? 1 :
                                           (typeByte == 81) ? 3 :
                                           (typeByte == 82) ? 6 : 0;
                    auto* tlShifted = impl->builder->CreateLShr(val,
                        llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "tl_sh");
                    auto* tlTagBits = impl->builder->CreateAnd(tlShifted,
                        llvm::ConstantInt::get(impl->i64Ty, 0x7), "tl_tb");
                    auto* tlTagMatch = impl->builder->CreateICmpEQ(tlTagBits,
                        llvm::ConstantInt::get(impl->i64Ty, expectedTag), "tl_tm");
                    tlValid = impl->builder->CreateAnd(tlTagged, tlTagMatch, "tl_v");
                }
                auto* tlPassBB = llvm::BasicBlock::Create(ctx, "tl_pass", impl->func);
                auto* tlFailBB = llvm::BasicBlock::Create(ctx, "tl_fail", impl->func);
                impl->builder->CreateCondBr(tlValid, tlPassBB, tlFailBB);
                // Fail: report type error (helper generates message + calls runtimeError)
                impl->builder->SetInsertPoint(tlFailBB);
                impl->builder->CreateCall(impl->aotReportTypeErrorFunc,
                    {impl->vmCtx, llvm::ConstantInt::get(impl->i8Ty, typeByte), val});
                impl->builder->CreateUnreachable();
                // Pass: store value to local slot
                impl->builder->SetInsertPoint(tlPassBB);
                auto* dest = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_ADD: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aVal = impl->builder->CreateLoad(impl->i64Ty, a, "add_a");
                auto* bVal = impl->builder->CreateLoad(impl->i64Ty, b, "add_b");

                // Fast path: both values are numbers (untagged) → fadd
                auto* nanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* nanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* aMasked = impl->builder->CreateAnd(aVal, nanMask, "add_a_masked");
                auto* bMasked = impl->builder->CreateAnd(bVal, nanMask, "add_b_masked");
                auto* aIsNum = impl->builder->CreateICmpNE(aMasked, nanBase, "add_a_is_num");
                auto* bIsNum = impl->builder->CreateICmpNE(bMasked, nanBase, "add_b_is_num");
                auto* bothNum = impl->builder->CreateAnd(aIsNum, bIsNum, "both_num");

                auto* addNumBB = llvm::BasicBlock::Create(ctx, "add_num", impl->func);
                auto* addHelperBB = llvm::BasicBlock::Create(ctx, "add_helper", impl->func);
                auto* addMergeBB = llvm::BasicBlock::Create(ctx, "add_merge", impl->func);
                impl->builder->CreateCondBr(bothNum, addNumBB, addHelperBB);

                // Number path: fadd
                impl->builder->SetInsertPoint(addNumBB);
                auto* aDouble = impl->builder->CreateBitCast(aVal, impl->doubleTy, "add_a_d");
                auto* bDouble = impl->builder->CreateBitCast(bVal, impl->doubleTy, "add_b_d");
                auto* numSum = impl->builder->CreateFAdd(aDouble, bDouble, "add_num");
                auto* numVal = impl->builder->CreateBitCast(numSum, impl->i64Ty, "add_num_i64");
                impl->builder->CreateBr(addMergeBB);

                // Slow path: call aot_add (handles string concatenation)
                impl->builder->SetInsertPoint(addHelperBB);
                auto* helperResult = impl->builder->CreateCall(impl->aotAddFunc,
                    {impl->vmCtx, aVal, bVal}, "add_slow");
                impl->builder->CreateBr(addMergeBB);

                // Merge
                impl->builder->SetInsertPoint(addMergeBB);
                auto* phi = impl->builder->CreatePHI(impl->i64Ty, 2, "add_result");
                phi->addIncoming(numVal, addNumBB);
                phi->addIncoming(helperResult, addHelperBB);
                impl->builder->CreateStore(phi, dest);
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
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), ext);
                break;
            }

            case OpCode::OP_NOT_EQUAL: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpONE(impl->loadData(a), impl->loadData(b), "neq");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), ext);
                break;
            }

            case OpCode::OP_GREATER: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpOGT(impl->loadData(a), impl->loadData(b), "gt");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), ext);
                break;
            }

            case OpCode::OP_LESS: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* cmp = impl->builder->CreateFCmpOLT(impl->loadData(a), impl->loadData(b), "lt");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), ext);
                break;
            }

            case OpCode::OP_NOT: {
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* truthy = impl->computeTruthy(a);
                auto* notTruthy = impl->builder->CreateNot(truthy, "not");
                auto* ext = impl->builder->CreateUIToFP(notTruthy, impl->doubleTy);
                impl->storeValue(dest,
                                  llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), ext);
                break;
            }

            case OpCode::OP_ADD_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i64Ty, "b_int");
                auto* intResult = impl->builder->CreateAdd(aInt, bInt, "add_int");
                auto* result = impl->builder->CreateSIToFP(intResult, impl->doubleTy, "add_int_f");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_SUB_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i64Ty, "b_int");
                auto* intResult = impl->builder->CreateSub(aInt, bInt, "sub_int");
                auto* result = impl->builder->CreateSIToFP(intResult, impl->doubleTy, "sub_int_f");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_MUL_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i64Ty, "b_int");
                auto* intResult = impl->builder->CreateMul(aInt, bInt, "mul_int");
                auto* result = impl->builder->CreateSIToFP(intResult, impl->doubleTy, "mul_int_f");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_DIV_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i64Ty, "b_int");
                auto* intResult = impl->builder->CreateSDiv(aInt, bInt, "div_int");
                auto* result = impl->builder->CreateSIToFP(intResult, impl->doubleTy, "div_int_f");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_MOD_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i64Ty, "b_int");
                auto* intResult = impl->builder->CreateSRem(aInt, bInt, "mod_int");
                auto* result = impl->builder->CreateSIToFP(intResult, impl->doubleTy, "mod_int_f");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_NEGATE_INT: {
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* intResult = impl->builder->CreateNeg(aInt, "neg_int");
                auto* result = impl->builder->CreateSIToFP(intResult, impl->doubleTy, "neg_int_f");
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), result);
                break;
            }
            case OpCode::OP_EQUAL_INT:
            case OpCode::OP_LESS_INT:
            case OpCode::OP_GREATER_INT: {
                auto* b = impl->popValue();
                auto* a = impl->popValue();
                auto* dest = impl->pushValue();
                auto* aInt = impl->builder->CreateFPToSI(impl->loadData(a), impl->i64Ty, "a_int");
                auto* bInt = impl->builder->CreateFPToSI(impl->loadData(b), impl->i64Ty, "b_int");
                llvm::CmpInst::Predicate pred;
                if (op == OpCode::OP_EQUAL_INT) pred = llvm::CmpInst::ICMP_EQ;
                else if (op == OpCode::OP_LESS_INT) pred = llvm::CmpInst::ICMP_SLT;
                else pred = llvm::CmpInst::ICMP_SGT;
                auto* cmp = impl->builder->CreateICmp(pred, aInt, bInt, "int_cmp");
                auto* ext = impl->builder->CreateUIToFP(cmp, impl->doubleTy);
                impl->storeValue(dest, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), ext);
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
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
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
                if (op == OpCode::OP_INC_LOCAL_INT) {
                    auto* dataInt = impl->builder->CreateFPToSI(impl->loadData(localPtr), impl->i64Ty, "data_int");
                    auto* one = llvm::ConstantInt::get(impl->i64Ty, 1);
                    auto* incInt = impl->builder->CreateAdd(dataInt, one, "inc_int");
                    auto* inc = impl->builder->CreateSIToFP(incInt, impl->doubleTy, "inc_int_f");
                    impl->storeValue(localPtr, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), inc);
                } else {
                    auto* data = impl->loadData(localPtr);
                    auto* one = llvm::ConstantFP::get(impl->doubleTy, 1.0);
                    auto* inc = impl->builder->CreateFAdd(data, one, "inc");
                    impl->storeValue(localPtr, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), inc);
                }
                break;
            }
            case OpCode::OP_DECREMENT_LOCAL:
            case OpCode::OP_DEC_LOCAL_INT: {
                uint8_t slot = impl->readByte();
                auto* localPtr = impl->localsGEP(llvm::ConstantInt::get(impl->i32Ty, slot));
                if (op == OpCode::OP_DEC_LOCAL_INT) {
                    auto* dataInt = impl->builder->CreateFPToSI(impl->loadData(localPtr), impl->i64Ty, "data_int");
                    auto* one = llvm::ConstantInt::get(impl->i64Ty, 1);
                    auto* decInt = impl->builder->CreateSub(dataInt, one, "dec_int");
                    auto* dec = impl->builder->CreateSIToFP(decInt, impl->doubleTy, "dec_int_f");
                    impl->storeValue(localPtr, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), dec);
                } else {
                    auto* data = impl->loadData(localPtr);
                    auto* one = llvm::ConstantFP::get(impl->doubleTy, 1.0);
                    auto* dec = impl->builder->CreateFSub(data, one, "dec");
                    impl->storeValue(localPtr, llvm::ConstantInt::get(impl->i8Ty, TAG_NUMBER), dec);
                }
                break;
            }

            // === Control flow ===
            case OpCode::OP_JUMP: {
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;
                auto* targetBB = impl->getOrCreateBB(target, impl->func);
                impl->builder->CreateBr(targetBB);
                break;
            }

            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;

                auto* val = impl->popValue();
                auto* truthy = impl->computeTruthy(val);

                auto* targetBB = impl->getOrCreateBB(target, impl->func);
                auto* contBB = llvm::BasicBlock::Create(ctx, "cont_jf", impl->func);
                impl->builder->CreateCondBr(truthy, contBB, targetBB);
                impl->builder->SetInsertPoint(contBB);
                break;
            }

            case OpCode::OP_LOOP: {
                uint16_t offset = impl->readShort();
                size_t target = (impl->ip > offset) ? (impl->ip - offset) : 0;
                auto* targetBB = impl->getOrCreateBB(target, impl->func);
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

                auto* targetBB = impl->getOrCreateBB(target, impl->func);
                auto* contBB = llvm::BasicBlock::Create(ctx, "cont_fj", impl->func);
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
                auto* localInt = impl->builder->CreateFPToSI(impl->loadData(localPtr), impl->i64Ty, "local_int");
                auto* constInt = impl->builder->CreateFPToSI(impl->loadData(constPtr), impl->i64Ty, "const_int");
                auto* cond = impl->builder->CreateICmpSLT(localInt, constInt, "loop_cond");

                auto* exitBB = impl->getOrCreateBB(exitTarget, impl->func);
                auto* contBB = llvm::BasicBlock::Create(ctx, "cont_less", impl->func);
                impl->builder->CreateCondBr(cond, contBB, exitBB);
                impl->builder->SetInsertPoint(contBB);
                break;
            }

            // === Globals ===
            case OpCode::OP_GET_GLOBAL:
            case OpCode::OP_GET_GLOBAL_FAST: {
                uint8_t constIdx = impl->readByte();
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* dest = impl->pushValue();
                    auto* val = impl->builder->CreateLoad(impl->i64Ty, gv, "global");
                    impl->builder->CreateStore(val, dest);
                } else {
                    impl->pushValue();
                    impl->emitConstStore(impl->peekValue(), TAG_NIL, 0.0);
                }
                break;
            }

            case OpCode::OP_SET_GLOBAL:
            case OpCode::OP_SET_GLOBAL_FAST:
            case OpCode::OP_SET_GLOBAL_TYPED: {
                uint8_t constIdx = impl->readByte();
                const Value& nameV = impl->chunk->constants[constIdx];
                std::string gName = nameV.type == ValueType::OBJ_STRING ? nameV.as.obj_string->chars : "?";
                auto* gNameStr = impl->builder->CreateGlobalStringPtr(gName, "g_name");
                auto* src = impl->popValue();
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                impl->builder->CreateCall(impl->aotSetGlobalTypedFunc,
                    {impl->vmCtx, gNameStr, val});
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    impl->builder->CreateStore(val, gv);
                }
                break;
            }

            case OpCode::OP_DEFINE_GLOBAL: {
                uint8_t constIdx = impl->readByte();
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* src = impl->popValue();
                    auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                    impl->builder->CreateStore(val, gv);
                } else {
                    impl->popValue();
                }
                break;
            }

            case OpCode::OP_DEFINE_TYPED_GLOBAL: {
                uint8_t constIdx = impl->readByte();
                uint8_t typeByte = impl->readByte();
                const Value& nameV = impl->chunk->constants[constIdx];
                std::string gName = nameV.type == ValueType::OBJ_STRING ? nameV.as.obj_string->chars : "?";
                auto* gNameStr = impl->builder->CreateGlobalStringPtr(gName, "g_name");
                auto* src = impl->popValue();
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src);
                impl->builder->CreateCall(impl->aotDefineTypedGlobalFunc,
                    {impl->vmCtx, gNameStr, val, llvm::ConstantInt::get(impl->i8Ty, typeByte)});
                break;
            }

            case OpCode::OP_INCREMENT_GLOBAL: {
                uint8_t constIdx = impl->readByte();
                auto* gv = impl->getOrCreateGlobal(constIdx);
                if (gv) {
                    auto* cur = impl->builder->CreateLoad(impl->i64Ty, gv, "g_cur");
                    auto* dbl = impl->builder->CreateBitCast(cur, impl->doubleTy, "g_dbl");
                    auto* inc = impl->builder->CreateFAdd(dbl,
                        llvm::ConstantFP::get(impl->doubleTy, 1.0), "g_inc");
                    auto* stored = impl->builder->CreateBitCast(inc, impl->i64Ty, "g_i64");
                    impl->builder->CreateStore(stored, gv);
                }
                break;
            }

            // === Functions & Calls ===
            case OpCode::OP_CALL:
            case OpCode::OP_CALL_FAST: {
                uint8_t argCount = impl->readByte();
                llvm::Value* argsPtr = llvm::ConstantPointerNull::get(impl->i8PtrTy);
                if (argCount > 0) {
                    auto* argsTy = llvm::ArrayType::get(impl->i64Ty, argCount);
                    auto* argsAlloca = impl->builder->CreateAlloca(argsTy, nullptr, "call_args");
                    for (int i = argCount - 1; i >= 0; i--) {
                        auto* argPtr = impl->popValue();
                        auto* arg = impl->builder->CreateLoad(impl->i64Ty, argPtr, "call_arg");
                        auto* gep = impl->builder->CreateGEP(argsTy, argsAlloca,
                            {llvm::ConstantInt::get(impl->i32Ty, 0),
                             llvm::ConstantInt::get(impl->i32Ty, i)});
                        impl->builder->CreateStore(arg, gep);
                    }
                    argsPtr = impl->builder->CreatePointerCast(argsAlloca, impl->i8PtrTy);
                }
                auto* calleePtr = impl->popValue();
                auto* callee = impl->builder->CreateLoad(impl->i64Ty, calleePtr, "callee");
                auto* sentinel = llvm::ConstantInt::get(impl->i64Ty, AOT_SENTINEL);
                auto* directResult = impl->builder->CreateCall(impl->aotTryDirectCallFunc,
                    {impl->vmCtx, callee, argsPtr,
                     llvm::ConstantInt::get(impl->i8Ty, argCount)}, "direct_call");
                auto* isMiss = impl->builder->CreateICmpEQ(directResult, sentinel, "is_miss");
                auto* tryDirectBB = impl->builder->GetInsertBlock();
                auto* callFallbackBB = llvm::BasicBlock::Create(ctx, "call_fb", impl->func);
                auto* callMergeBB = llvm::BasicBlock::Create(ctx, "call_mg", impl->func);
                impl->builder->CreateCondBr(isMiss, callFallbackBB, callMergeBB);

                impl->builder->SetInsertPoint(callFallbackBB);
                auto* fallbackResult = impl->builder->CreateCall(impl->aotCallFunc,
                    {impl->vmCtx, callee, argsPtr,
                     llvm::ConstantInt::get(impl->i8Ty, argCount)}, "call_fb_r");
                impl->builder->CreateBr(callMergeBB);

                impl->builder->SetInsertPoint(callMergeBB);
                auto* phi = impl->builder->CreatePHI(impl->i64Ty, 2, "call_r");
                phi->addIncoming(directResult, tryDirectBB);
                phi->addIncoming(fallbackResult, callFallbackBB);
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(phi, dest);
                break;
            }

            case OpCode::OP_CLOSURE: {
                uint8_t funcIdx = impl->readByte();
                auto* dest = impl->pushValue();
                auto* gep = impl->builder->CreateGEP(
                    llvm::ArrayType::get(impl->i64Ty, 256),
                    impl->funcTableGlobal,
                    {llvm::ConstantInt::get(impl->i32Ty, 0),
                     llvm::ConstantInt::get(impl->i32Ty, funcIdx)});
                auto* val = impl->builder->CreateLoad(impl->i64Ty, gep, "func");
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_GET_UPVALUE: {
                uint8_t slot = impl->readByte();
                (void)slot;
                auto* dest = impl->pushValue();
                auto* nilVal = impl->constValue(TAG_NIL, 0.0);
                impl->builder->CreateStore(nilVal, dest);
                break;
            }

            case OpCode::OP_SET_UPVALUE: {
                uint8_t slot = impl->readByte();
                (void)slot;
                impl->popValue();
                break;
            }

            case OpCode::OP_CLOSE_UPVALUE: {
                impl->popValue();
                break;
            }

            // === Arrays & Objects ===
            case OpCode::OP_ARRAY: {
                uint8_t count = impl->readByte();
                if (count > 0) {
                    // Allocate temporary array on stack to hold elements
                    auto* arrTy = llvm::ArrayType::get(impl->i64Ty, count);
                    auto* arrAlloca = impl->builder->CreateAlloca(arrTy, nullptr, "arr_elems");
                    for (int i = count - 1; i >= 0; i--) {
                        auto* elemPtr = impl->popValue();
                        auto* elem = impl->builder->CreateLoad(impl->i64Ty, elemPtr);
                        auto* gep = impl->builder->CreateGEP(arrTy, arrAlloca,
                            {llvm::ConstantInt::get(impl->i32Ty, 0),
                             llvm::ConstantInt::get(impl->i32Ty, i)});
                        impl->builder->CreateStore(elem, gep);
                    }
                    auto* arrPtr = impl->builder->CreatePointerCast(arrAlloca, impl->i8PtrTy);
                    auto* result = impl->builder->CreateCall(impl->aotCreateArrayFunc,
                        {impl->vmCtx, arrPtr, llvm::ConstantInt::get(impl->i8Ty, count)},
                        "arr_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                } else {
                    // Empty array: pass nullptr
                    auto* result = impl->builder->CreateCall(impl->aotCreateArrayFunc,
                        {impl->vmCtx, llvm::ConstantPointerNull::get(impl->i8PtrTy),
                         llvm::ConstantInt::get(impl->i8Ty, 0)}, "arr_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                }
                break;
            }

            case OpCode::OP_OBJECT: {
                uint8_t count = impl->readByte();
                if (count > 0) {
                    // Allocate temp arrays for keys and values (interleaved on stack: k1, v1, ..., kN, vN)
                    auto* keysTy = llvm::ArrayType::get(impl->i64Ty, count);
                    auto* valsTy = llvm::ArrayType::get(impl->i64Ty, count);
                    auto* keysAlloca = impl->builder->CreateAlloca(keysTy, nullptr, "obj_keys");
                    auto* valsAlloca = impl->builder->CreateAlloca(valsTy, nullptr, "obj_vals");
                    for (int i = count - 1; i >= 0; i--) {
                        auto* valPtr = impl->popValue();
                        auto* keyPtr = impl->popValue();
                        auto* val = impl->builder->CreateLoad(impl->i64Ty, valPtr);
                        auto* key = impl->builder->CreateLoad(impl->i64Ty, keyPtr);
                        auto* valGep = impl->builder->CreateGEP(valsTy, valsAlloca,
                            {llvm::ConstantInt::get(impl->i32Ty, 0),
                             llvm::ConstantInt::get(impl->i32Ty, i)});
                        auto* keyGep = impl->builder->CreateGEP(keysTy, keysAlloca,
                            {llvm::ConstantInt::get(impl->i32Ty, 0),
                             llvm::ConstantInt::get(impl->i32Ty, i)});
                        impl->builder->CreateStore(val, valGep);
                        impl->builder->CreateStore(key, keyGep);
                    }
                    auto* keysPtr = impl->builder->CreatePointerCast(keysAlloca, impl->i8PtrTy);
                    auto* valsPtr = impl->builder->CreatePointerCast(valsAlloca, impl->i8PtrTy);
                    auto* result = impl->builder->CreateCall(impl->aotCreateObjectFunc,
                        {impl->vmCtx, keysPtr, valsPtr,
                         llvm::ConstantInt::get(impl->i8Ty, count)}, "obj_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                } else {
                    auto* result = impl->builder->CreateCall(impl->aotCreateObjectFunc,
                        {impl->vmCtx, llvm::ConstantPointerNull::get(impl->i8PtrTy),
                         llvm::ConstantPointerNull::get(impl->i8PtrTy),
                         llvm::ConstantInt::get(impl->i8Ty, 0)}, "obj_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                }
                break;
            }

            case OpCode::OP_INDEX_GET: {
                auto* idxPtr = impl->popValue();
                auto* objPtr = impl->popValue();
                auto* objVal = impl->builder->CreateLoad(impl->i64Ty, objPtr, "obj");
                auto* idxVal = impl->builder->CreateLoad(impl->i64Ty, idxPtr, "idx");
                // Tag dispatch in IR: fast path for Array, fallback to full helper
                auto* nanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* nanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* masked = impl->builder->CreateAnd(objVal, nanMask, "idx_masked");
                auto* isTagged = impl->builder->CreateICmpEQ(masked, nanBase, "idx_tagged");
                auto* shifted = impl->builder->CreateLShr(objVal,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "idx_shifted");
                auto* tagBits = impl->builder->CreateAnd(shifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "idx_tags");
                auto* isArr = impl->builder->CreateAnd(isTagged,
                    impl->builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(impl->i64Ty, 3)),
                    "idx_is_arr");
                auto* idxArrBB = llvm::BasicBlock::Create(ctx, "idx_arr", impl->func);
                auto* idxFbBB = llvm::BasicBlock::Create(ctx, "idx_fb", impl->func);
                auto* idxMgBB = llvm::BasicBlock::Create(ctx, "idx_mg", impl->func);
                impl->builder->CreateCondBr(isArr, idxArrBB, idxFbBB);
                // Array fast path: inline bounds check + GEP + emitValueToNan
                impl->builder->SetInsertPoint(idxArrBB);
                auto* arrPtr = impl->builder->CreateIntToPtr(
                    impl->builder->CreateAnd(objVal, llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK)),
                    impl->i8PtrTy, "arr_p");
                // Check idxVal is not tagged (numbers are raw doubles)
                auto* idxNanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* idxNanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* idxMasked = impl->builder->CreateAnd(idxVal, idxNanMask, "idx_m");
                auto* idxRaw = impl->builder->CreateICmpNE(idxMasked, idxNanBase, "idx_raw");
                auto* idxNumBB = llvm::BasicBlock::Create(ctx, "idx_num", impl->func);
                auto* idxBadBB = llvm::BasicBlock::Create(ctx, "idx_bad", impl->func);
                impl->builder->CreateCondBr(idxRaw, idxNumBB, idxBadBB);
                // Index is a number: bitcast to double, trunc to i64
                impl->builder->SetInsertPoint(idxNumBB);
                auto* idxDbl = impl->builder->CreateBitCast(idxVal, impl->doubleTy, "idx_d");
                auto* idxI64 = impl->builder->CreateFPToSI(idxDbl, impl->i64Ty, "idx_i");
                // Load vector start/finish from Array* (offset 16/24)
                auto* vecStart = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, arrPtr, 16), "vst");
                auto* vecFinish = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, arrPtr, 24), "vfn");
                // count = (finish - start) / 16 (sizeof(Value) = 16)
                auto* byteDiff = impl->builder->CreatePtrDiff(impl->i8Ty, vecFinish, vecStart, "bd");
                auto* countVal = impl->builder->CreateAShr(byteDiff,
                    llvm::ConstantInt::get(impl->i64Ty, 4), "cnt");
                auto* inBounds = impl->builder->CreateICmpULT(idxI64, countVal, "ib");
                auto* idxHitBB = llvm::BasicBlock::Create(ctx, "idx_hit", impl->func);
                impl->builder->CreateCondBr(inBounds, idxHitBB, idxBadBB);
                // In-bounds: GEP + emitValueToNan
                impl->builder->SetInsertPoint(idxHitBB);
                auto* elemOff = impl->builder->CreateMul(idxI64,
                    llvm::ConstantInt::get(impl->i64Ty, 16), "eo");
                auto* elemPtr = impl->builder->CreateGEP(impl->i8Ty, vecStart, elemOff, "ep");
                auto* arrHitVal = impl->emitValueToNan(elemPtr);
                impl->builder->CreateBr(idxMgBB);
                // Index is tagged or OOB → nil
                impl->builder->SetInsertPoint(idxBadBB);
                auto* nilResArr = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                impl->builder->CreateBr(idxMgBB);
                // Fallback path
                impl->builder->SetInsertPoint(idxFbBB);
                auto* fbR = impl->builder->CreateCall(impl->aotIndexGetFunc,
                    {impl->vmCtx, objVal, idxVal}, "idx_fb");
                impl->builder->CreateBr(idxMgBB);
                // Merge
                impl->builder->SetInsertPoint(idxMgBB);
                auto* phi = impl->builder->CreatePHI(impl->i64Ty, 3, "idx_r");
                phi->addIncoming(arrHitVal, idxHitBB);
                phi->addIncoming(nilResArr, idxBadBB);
                phi->addIncoming(fbR, idxFbBB);
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(phi, dest);
                break;
            }

            case OpCode::OP_INDEX_SET: {
                auto* valPtr = impl->popValue();
                auto* idxPtr = impl->popValue();
                auto* objPtr = impl->popValue();
                auto* objVal = impl->builder->CreateLoad(impl->i64Ty, objPtr, "obj");
                auto* idxVal = impl->builder->CreateLoad(impl->i64Ty, idxPtr, "idx");
                auto* val = impl->builder->CreateLoad(impl->i64Ty, valPtr, "val");
                // Tag dispatch in IR: fast path for Array, fallback to full helper
                auto* nanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* nanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* masked = impl->builder->CreateAnd(objVal, nanMask, "set_masked");
                auto* isTagged = impl->builder->CreateICmpEQ(masked, nanBase, "set_tagged");
                auto* shifted = impl->builder->CreateLShr(objVal,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "set_shifted");
                auto* tagBits = impl->builder->CreateAnd(shifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "set_tags");
                auto* isArr = impl->builder->CreateAnd(isTagged,
                    impl->builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(impl->i64Ty, 3)),
                    "set_is_arr");
                auto* setArrBB = llvm::BasicBlock::Create(ctx, "set_arr", impl->func);
                auto* setFbBB = llvm::BasicBlock::Create(ctx, "set_fb", impl->func);
                auto* setMgBB = llvm::BasicBlock::Create(ctx, "set_mg", impl->func);
                impl->builder->CreateCondBr(isArr, setArrBB, setFbBB);
                // Array fast path: inline bounds check + GEP + emitNanToValue
                impl->builder->SetInsertPoint(setArrBB);
                auto* arrPtr = impl->builder->CreateIntToPtr(
                    impl->builder->CreateAnd(objVal, llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK)),
                    impl->i8PtrTy, "set_arr_p");
                // Check idxVal not tagged
                auto* idxNanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* idxNanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* idxMasked = impl->builder->CreateAnd(idxVal, idxNanMask, "set_m");
                auto* idxRaw = impl->builder->CreateICmpNE(idxMasked, idxNanBase, "set_raw");
                auto* setNumBB = llvm::BasicBlock::Create(ctx, "set_num", impl->func);
                auto* setSkipBB = llvm::BasicBlock::Create(ctx, "set_skip", impl->func);
                impl->builder->CreateCondBr(idxRaw, setNumBB, setSkipBB);
                impl->builder->SetInsertPoint(setNumBB);
                auto* idxDbl = impl->builder->CreateBitCast(idxVal, impl->doubleTy, "set_d");
                auto* idxI64 = impl->builder->CreateFPToSI(idxDbl, impl->i64Ty, "set_i");
                auto* vecStart = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, arrPtr, 16), "vst_set");
                auto* vecFinish = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, arrPtr, 24), "vfn_set");
                auto* byteDiff = impl->builder->CreatePtrDiff(impl->i8Ty, vecFinish, vecStart, "bd_set");
                auto* countVal = impl->builder->CreateAShr(byteDiff,
                    llvm::ConstantInt::get(impl->i64Ty, 4), "cnt_set");
                auto* inBounds = impl->builder->CreateICmpULT(idxI64, countVal, "ib_set");
                auto* setHitBB = llvm::BasicBlock::Create(ctx, "set_hit", impl->func);
                impl->builder->CreateCondBr(inBounds, setHitBB, setSkipBB);
                impl->builder->SetInsertPoint(setHitBB);
                auto* elemOff = impl->builder->CreateMul(idxI64,
                    llvm::ConstantInt::get(impl->i64Ty, 16), "eo_set");
                auto* elemPtr = impl->builder->CreateGEP(impl->i8Ty, vecStart, elemOff, "ep_set");
                impl->emitNanToValue(val, elemPtr);
                impl->builder->CreateBr(setMgBB);
                // Tagged or OOB: skip store
                impl->builder->SetInsertPoint(setSkipBB);
                // Fallback path
                impl->builder->SetInsertPoint(setFbBB);
                impl->builder->CreateCall(impl->aotIndexSetFunc,
                    {impl->vmCtx, objVal, idxVal, val});
                impl->builder->CreateBr(setMgBB);
                // Merge: push the value back (same as input val)
                impl->builder->SetInsertPoint(setMgBB);
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_GET_PROPERTY: {
                uint8_t propIdx = impl->readByte();
                const Value& nameVal = impl->chunk->constants[propIdx];
                std::string propName = nameVal.type == ValueType::OBJ_STRING
                    ? nameVal.as.obj_string->chars : "?";
                auto* propNameStr = impl->builder->CreateGlobalStringPtr(propName, "prop_name");
                auto* objPtr = impl->popValue();
                auto* objVal = impl->builder->CreateLoad(impl->i64Ty, objPtr, "obj");
                auto* cacheGv = impl->createPropCache();
                auto* cachePtr = impl->builder->CreateBitCast(cacheGv, impl->i8PtrTy, "cache_ptr");
                // Tag dispatch: if Instance → try cache fast path
                auto* nanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* nanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* masked = impl->builder->CreateAnd(objVal, nanMask, "gp_masked");
                auto* isTagged = impl->builder->CreateICmpEQ(masked, nanBase, "gp_tagged");
                auto* shifted = impl->builder->CreateLShr(objVal,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "gp_shifted");
                auto* tagBits = impl->builder->CreateAnd(shifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "gp_tags");
                auto* isInst = impl->builder->CreateAnd(isTagged,
                    impl->builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(impl->i64Ty, 4)),
                    "gp_is_inst");
                auto* gpTryBB = llvm::BasicBlock::Create(ctx, "gp_try", impl->func);
                auto* gpFbBB = llvm::BasicBlock::Create(ctx, "gp_fb", impl->func);
                auto* gpMgBB = llvm::BasicBlock::Create(ctx, "gp_mg", impl->func);
                impl->builder->CreateCondBr(isInst, gpTryBB, gpFbBB);
                // Try cache fast path
                impl->builder->SetInsertPoint(gpTryBB);
                auto* payload = impl->builder->CreateAnd(objVal,
                    llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK), "gp_pay");
                auto* instPtr = impl->builder->CreateIntToPtr(payload, impl->i8PtrTy, "gp_i");
                // Inline cache hit check: if cache->klass == inst->klass
                auto* cacheKlass = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, cachePtr, 0), "cklass");
                auto* instKlass = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, instPtr, 16), "iklass");
                auto* klassMatch = impl->builder->CreateICmpEQ(cacheKlass, instKlass, "kl_match");
                auto* gpHitBB = llvm::BasicBlock::Create(ctx, "gp_hit", impl->func);
                impl->builder->CreateCondBr(klassMatch, gpHitBB, gpFbBB);
                // Cache hit: GEP into inlineFields[cache->inlineIndex].value → emitValueToNan
                impl->builder->SetInsertPoint(gpHitBB);
                auto* inlineIdx = impl->builder->CreateLoad(impl->i8Ty,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, cachePtr, 8), "fidx");
                auto* idxExt = impl->builder->CreateZExt(inlineIdx, impl->i32Ty, "fidx_ext");
                auto* fieldBase = impl->builder->CreateAdd(llvm::ConstantInt::get(impl->i32Ty, 24),
                    impl->builder->CreateMul(idxExt, llvm::ConstantInt::get(impl->i32Ty, 24)), "fb");
                auto* valStructPtr = impl->builder->CreateConstGEP1_32(impl->i8Ty,
                    impl->builder->CreateGEP(impl->i8Ty, instPtr, fieldBase), 8, "fval");
                auto* hitR = impl->emitValueToNan(valStructPtr);
                impl->builder->CreateBr(gpMgBB);
                // Fallback path (cache miss or non-Instance)
                impl->builder->SetInsertPoint(gpFbBB);
                auto* fbR = impl->builder->CreateCall(impl->aotGetPropCachedFunc,
                    {impl->vmCtx, objVal, propNameStr, cachePtr}, "gp_fb");
                impl->builder->CreateBr(gpMgBB);
                // Merge
                impl->builder->SetInsertPoint(gpMgBB);
                auto* gpPhi = impl->builder->CreatePHI(impl->i64Ty, 2, "gp_r");
                gpPhi->addIncoming(hitR, gpHitBB);
                gpPhi->addIncoming(fbR, gpFbBB);
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(gpPhi, dest);
                break;
            }

            case OpCode::OP_SET_PROPERTY: {
                uint8_t propIdx = impl->readByte();
                const Value& nameVal = impl->chunk->constants[propIdx];
                std::string propName = nameVal.type == ValueType::OBJ_STRING
                    ? nameVal.as.obj_string->chars : "?";
                auto* propNameStr = impl->builder->CreateGlobalStringPtr(propName, "prop_name");
                auto* valPtr = impl->popValue();
                auto* objPtr = impl->popValue();
                auto* objVal = impl->builder->CreateLoad(impl->i64Ty, objPtr, "obj");
                auto* val = impl->builder->CreateLoad(impl->i64Ty, valPtr, "val");
                auto* cacheGv = impl->createPropCache();
                auto* cachePtr = impl->builder->CreateBitCast(cacheGv, impl->i8PtrTy, "cache_ptr");
                // Tag dispatch: if Instance → try cache fast path
                auto* nanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* nanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* masked = impl->builder->CreateAnd(objVal, nanMask, "sp_masked");
                auto* isTagged = impl->builder->CreateICmpEQ(masked, nanBase, "sp_tagged");
                auto* shifted = impl->builder->CreateLShr(objVal,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "sp_shifted");
                auto* tagBits = impl->builder->CreateAnd(shifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "sp_tags");
                auto* isInst = impl->builder->CreateAnd(isTagged,
                    impl->builder->CreateICmpEQ(tagBits, llvm::ConstantInt::get(impl->i64Ty, 4)),
                    "sp_is_inst");
                auto* spTryBB = llvm::BasicBlock::Create(ctx, "sp_try", impl->func);
                auto* spFbBB = llvm::BasicBlock::Create(ctx, "sp_fb", impl->func);
                auto* spMgBB = llvm::BasicBlock::Create(ctx, "sp_mg", impl->func);
                impl->builder->CreateCondBr(isInst, spTryBB, spFbBB);
                // Try cache fast path: inline klass check + GEP store
                impl->builder->SetInsertPoint(spTryBB);
                auto* payload = impl->builder->CreateAnd(objVal,
                    llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK), "sp_pay");
                auto* instPtr = impl->builder->CreateIntToPtr(payload, impl->i8PtrTy, "sp_i");
                auto* cacheKlass = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, cachePtr, 0), "cklass_sp");
                auto* instKlass = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, instPtr, 16), "iklass_sp");
                auto* klassMatch = impl->builder->CreateICmpEQ(cacheKlass, instKlass, "kl_match_sp");
                auto* spHitBB = llvm::BasicBlock::Create(ctx, "sp_hit", impl->func);
                impl->builder->CreateCondBr(klassMatch, spHitBB, spFbBB);
                // Cache hit: GEP into inlineFields[idx].value, convert val to Value struct
                impl->builder->SetInsertPoint(spHitBB);
                auto* inlineIdx = impl->builder->CreateLoad(impl->i8Ty,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, cachePtr, 8), "fidx_sp");
                auto* idxExt = impl->builder->CreateZExt(inlineIdx, impl->i32Ty, "fidx_ext_sp");
                auto* fieldBase = impl->builder->CreateAdd(llvm::ConstantInt::get(impl->i32Ty, 24),
                    impl->builder->CreateMul(idxExt, llvm::ConstantInt::get(impl->i32Ty, 24)), "fb_sp");
                auto* valStructPtr = impl->builder->CreateConstGEP1_32(impl->i8Ty,
                    impl->builder->CreateGEP(impl->i8Ty, instPtr, fieldBase), 8, "fval_sp");
                impl->emitNanToValue(val, valStructPtr);
                impl->builder->CreateBr(spMgBB);
                // Cache miss or non-Instance → call full helper
                impl->builder->SetInsertPoint(spFbBB);
                impl->builder->CreateCall(impl->aotSetPropCachedFunc,
                    {impl->vmCtx, objVal, propNameStr, val, cachePtr});
                impl->builder->CreateBr(spMgBB);
                // Merge: push the value back
                impl->builder->SetInsertPoint(spMgBB);
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_THIS: {
                auto* src = impl->stackGEP(llvm::ConstantInt::get(impl->i32Ty, 0));
                auto* val = impl->builder->CreateLoad(impl->i64Ty, src, "this");
                auto* d = impl->pushValue();
                impl->builder->CreateStore(val, d);
                break;
            }

            case OpCode::OP_INVOKE: {
                uint8_t propIdx = impl->readByte();
                uint8_t argCount = impl->readByte();
                const Value& nameVal = impl->chunk->constants[propIdx];
                std::string methodName = nameVal.type == ValueType::OBJ_STRING
                    ? nameVal.as.obj_string->chars : "?";
                auto* methodNameStr = impl->builder->CreateGlobalStringPtr(methodName, "method_name");

                // Pop args and receiver from stack (args are pushed first, then receiver)
                if (argCount > 0) {
                    auto* argsTy = llvm::ArrayType::get(impl->i64Ty, argCount);
                    auto* argsAlloca = impl->builder->CreateAlloca(argsTy, nullptr, "invoke_args");
                    for (int i = argCount - 1; i >= 0; i--) {
                        auto* argPtr = impl->popValue();
                        auto* arg = impl->builder->CreateLoad(impl->i64Ty, argPtr);
                        auto* gep = impl->builder->CreateGEP(argsTy, argsAlloca,
                            {llvm::ConstantInt::get(impl->i32Ty, 0),
                             llvm::ConstantInt::get(impl->i32Ty, i)});
                        impl->builder->CreateStore(arg, gep);
                    }
                    auto* receiverPtr = impl->popValue();
                    auto* receiver = impl->builder->CreateLoad(impl->i64Ty, receiverPtr, "receiver");
                    auto* argsPtr = impl->builder->CreatePointerCast(argsAlloca, impl->i8PtrTy);
                    auto* result = impl->builder->CreateCall(impl->aotInvokeFunc,
                        {impl->vmCtx, receiver, methodNameStr, argsPtr,
                         llvm::ConstantInt::get(impl->i8Ty, argCount)}, "invoke_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                } else {
                    auto* receiverPtr = impl->popValue();
                    auto* receiver = impl->builder->CreateLoad(impl->i64Ty, receiverPtr, "receiver");
                    auto* result = impl->builder->CreateCall(impl->aotInvokeFunc,
                        {impl->vmCtx, receiver, methodNameStr,
                         llvm::ConstantPointerNull::get(impl->i8PtrTy),
                         llvm::ConstantInt::get(impl->i8Ty, 0)}, "invoke_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                }
                break;
            }

            case OpCode::OP_FOR_IN_INIT: {
                auto* iterablePtr = impl->popValue();
                auto* iterable = impl->builder->CreateLoad(impl->i64Ty, iterablePtr, "iterable");
                auto* keys = impl->builder->CreateCall(impl->aotForInInitFunc,
                    {impl->vmCtx, iterable}, "keys");
                // Push iterable, index=0.0, keys array
                impl->builder->CreateStore(iterable, impl->pushValue());
                impl->builder->CreateStore(impl->constValue(TAG_NUMBER, 0.0), impl->pushValue());
                impl->builder->CreateStore(keys, impl->pushValue());
                break;
            }

            case OpCode::OP_FOR_IN_NEXT: {
                uint8_t baseSlot = impl->readByte();
                uint8_t varSlot = impl->readByte();
                uint8_t offsetHi = impl->readByte();
                uint8_t offsetLo = impl->readByte();
                uint16_t offset = (static_cast<uint16_t>(offsetHi) << 8) | offsetLo;
                // Load index from baseSlot+1, keys from baseSlot+2
                auto* idxPtr = impl->stackGEP(llvm::ConstantInt::get(impl->i32Ty, baseSlot + 1));
                auto* keysPtr = impl->stackGEP(llvm::ConstantInt::get(impl->i32Ty, baseSlot + 2));
                auto* idxVal = impl->builder->CreateLoad(impl->i64Ty, idxPtr, "for_idx");
                auto* keysVal = impl->builder->CreateLoad(impl->i64Ty, keysPtr, "for_keys");
                // Inline aot_forInNext: check keys is Array, bounds check, GEP + emitValueToNan
                auto* fnNanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* fnNanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* fnMasked = impl->builder->CreateAnd(keysVal, fnNanMask, "fn_m");
                auto* fnTagged = impl->builder->CreateICmpEQ(fnMasked, fnNanBase, "fn_tgd");
                auto* fnShifted = impl->builder->CreateLShr(keysVal,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "fn_sh");
                auto* fnTagBits = impl->builder->CreateAnd(fnShifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "fn_tag");
                auto* fnIsArr = impl->builder->CreateAnd(fnTagged,
                    impl->builder->CreateICmpEQ(fnTagBits, llvm::ConstantInt::get(impl->i64Ty, 4)), "fn_arr");
                auto* fnArrBB = llvm::BasicBlock::Create(ctx, "fn_arr", impl->func);
                auto* fnDoneBB = llvm::BasicBlock::Create(ctx, "fn_done", impl->func);
                auto* fnNilBB = llvm::BasicBlock::Create(ctx, "fn_nil", impl->func);
                impl->builder->CreateCondBr(fnIsArr, fnArrBB, fnNilBB);
                // Array path: check index is number, bounds, GEP
                impl->builder->SetInsertPoint(fnArrBB);
                auto* fnIdxRaw = impl->builder->CreateICmpNE(
                    impl->builder->CreateAnd(idxVal, fnNanMask), fnNanBase, "fn_idx_raw");
                auto* fnIdxBB = llvm::BasicBlock::Create(ctx, "fn_idx", impl->func);
                impl->builder->CreateCondBr(fnIdxRaw, fnIdxBB, fnNilBB);
                impl->builder->SetInsertPoint(fnIdxBB);
                auto* fnIdxDbl = impl->builder->CreateBitCast(idxVal, impl->doubleTy, "fn_d");
                auto* fnIdxI64 = impl->builder->CreateFPToSI(fnIdxDbl, impl->i64Ty, "fn_i");
                auto* fnPay = impl->builder->CreateAnd(keysVal,
                    llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK), "fn_pay");
                auto* fnArrPtr = impl->builder->CreateIntToPtr(fnPay, impl->i8PtrTy, "fn_a");
                auto* fnVStart = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, fnArrPtr, 16), "fn_vs");
                auto* fnVFinish = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, fnArrPtr, 24), "fn_vf");
                auto* fnBDiff = impl->builder->CreatePtrDiff(impl->i8Ty, fnVFinish, fnVStart, "fn_bd");
                auto* fnCnt = impl->builder->CreateAShr(fnBDiff,
                    llvm::ConstantInt::get(impl->i64Ty, 4), "fn_cnt");
                auto* fnIB = impl->builder->CreateICmpULT(fnIdxI64, fnCnt, "fn_ib");
                auto* fnHitBB = llvm::BasicBlock::Create(ctx, "fn_hit", impl->func);
                impl->builder->CreateCondBr(fnIB, fnHitBB, fnNilBB);
                impl->builder->SetInsertPoint(fnHitBB);
                auto* fnEOff = impl->builder->CreateMul(fnIdxI64,
                    llvm::ConstantInt::get(impl->i64Ty, 16), "fn_eo");
                auto* fnEPtr = impl->builder->CreateGEP(impl->i8Ty, fnVStart, fnEOff, "fn_ep");
                auto* nextKey = impl->emitValueToNan(fnEPtr);
                impl->builder->CreateBr(fnDoneBB);
                // Nil path (non-array, non-number, or OOB)
                impl->builder->SetInsertPoint(fnNilBB);
                auto* fnNil = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                impl->builder->CreateBr(fnDoneBB);
                // Merge
                impl->builder->SetInsertPoint(fnDoneBB);
                auto* fnResult = impl->builder->CreatePHI(impl->i64Ty, 2, "fn_r");
                fnResult->addIncoming(nextKey, fnHitBB);
                fnResult->addIncoming(fnNil, fnNilBB);
                // Check if nil (done)
                auto* masked = impl->builder->CreateAnd(fnResult,
                    llvm::ConstantInt::get(impl->i64Ty, NAN_MASK));
                auto* isTagged = impl->builder->CreateICmpEQ(masked,
                    llvm::ConstantInt::get(impl->i64Ty, NAN_BASE), "is_tagged_fi");
                auto* shifted = impl->builder->CreateLShr(fnResult,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT));
                auto* tagBits = impl->builder->CreateAnd(shifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7));
                auto* isNil = impl->builder->CreateAnd(isTagged,
                    impl->builder->CreateICmpEQ(tagBits,
                        llvm::ConstantInt::get(impl->i64Ty, TAG_NIL)), "is_nil");
                size_t exitTarget = impl->ip + offset;
                auto* loopBodyBB = llvm::BasicBlock::Create(ctx, "for_body", impl->func);
                auto* exitBB = impl->getOrCreateBB(exitTarget, impl->func);
                impl->builder->CreateCondBr(isNil, exitBB, loopBodyBB);
                // Loop body: assign key to varSlot, increment index
                impl->builder->SetInsertPoint(loopBodyBB);
                auto* varPtr = impl->stackGEP(llvm::ConstantInt::get(impl->i32Ty, varSlot));
                impl->builder->CreateStore(fnResult, varPtr);
                // Increment index: index = index + 1.0 (NaN-boxed)
                auto* oneConst = impl->constValue(TAG_NUMBER, 1.0);
                // If index is a tagged NaN, we need to use fadd on the raw double
                // For simplicity, unpromote to double, add 1, re-box
                auto* idxDouble = impl->builder->CreateBitCast(idxVal, impl->doubleTy, "idx_dbl");
                auto* oneDouble = impl->builder->CreateBitCast(oneConst, impl->doubleTy, "one_dbl");
                auto* sumDouble = impl->builder->CreateFAdd(idxDouble, oneDouble, "idx_inc");
                auto* newIdx = impl->builder->CreateBitCast(sumDouble, impl->i64Ty, "new_idx");
                impl->builder->CreateStore(newIdx, idxPtr);
                break;
            }

            case OpCode::OP_OPTIONAL_CHAIN: {
                uint8_t propIdx = impl->readByte();
                // Check if receiver is nil — if so, push nil without calling helper
                auto* recvPtr = impl->popValue();
                auto* recvVal = impl->builder->CreateLoad(impl->i64Ty, recvPtr, "recv");
                auto* masked = impl->builder->CreateAnd(recvVal,
                    llvm::ConstantInt::get(impl->i64Ty, NAN_MASK));
                auto* isTagged = impl->builder->CreateICmpEQ(masked,
                    llvm::ConstantInt::get(impl->i64Ty, NAN_BASE), "is_tagged");
                auto* shifted = impl->builder->CreateLShr(recvVal,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "tag_s");
                auto* tagBits = impl->builder->CreateAnd(shifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7));
                auto* isNil = impl->builder->CreateAnd(isTagged,
                    impl->builder->CreateICmpEQ(tagBits,
                        llvm::ConstantInt::get(impl->i64Ty, TAG_NIL)), "is_nil");

                auto* getPropBB = llvm::BasicBlock::Create(ctx, "opt_getprop", impl->func);
                auto* nilBB = llvm::BasicBlock::Create(ctx, "opt_nil", impl->func);
                auto* mergeBB = llvm::BasicBlock::Create(ctx, "opt_merge", impl->func);
                impl->builder->CreateCondBr(isNil, nilBB, getPropBB);

                // Get property path
                impl->builder->SetInsertPoint(getPropBB);
                const Value& nameVal = impl->chunk->constants[propIdx];
                std::string propName = nameVal.type == ValueType::OBJ_STRING
                    ? nameVal.as.obj_string->chars : "?";
                auto* propNameStr = impl->builder->CreateGlobalStringPtr(propName, "prop_name");
                auto* cacheGv = impl->createPropCache();
                auto* cachePtr = impl->builder->CreateBitCast(cacheGv, impl->i8PtrTy, "cache_ptr");
                auto* propResult = impl->builder->CreateCall(impl->aotGetPropCachedFunc,
                    {impl->vmCtx, recvVal, propNameStr, cachePtr}, "prop_result");
                auto* propSlot = impl->pushValue();
                impl->builder->CreateStore(propResult, propSlot);
                impl->builder->CreateBr(mergeBB);

                // Nil path
                impl->builder->SetInsertPoint(nilBB);
                auto* nilSlot = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), nilSlot);
                impl->builder->CreateBr(mergeBB);

                impl->builder->SetInsertPoint(mergeBB);
                break;
            }

            case OpCode::OP_SPREAD: {
                auto* valPtr = impl->popValue();
                auto* val = impl->builder->CreateLoad(impl->i64Ty, valPtr, "spread_val");
                // Inline aot_spread: if Array, copy elements to buffer; else copy single value
                auto* bufTy = llvm::ArrayType::get(impl->i64Ty, 256);
                auto* bufAlloca = impl->builder->CreateAlloca(bufTy, nullptr, "spread_buf");
                auto* spNanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* spNanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* spMasked = impl->builder->CreateAnd(val, spNanMask, "sp_m");
                auto* spTagged = impl->builder->CreateICmpEQ(spMasked, spNanBase, "sp_tg");
                auto* spShifted = impl->builder->CreateLShr(val,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "sp_sh");
                auto* spTagBits = impl->builder->CreateAnd(spShifted,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "sp_tb");
                auto* spIsArr = impl->builder->CreateAnd(spTagged,
                    impl->builder->CreateICmpEQ(spTagBits, llvm::ConstantInt::get(impl->i64Ty, 4)), "sp_arr");
                auto* spArrBB = llvm::BasicBlock::Create(ctx, "sp_arr", impl->func);
                auto* spNonArrBB = llvm::BasicBlock::Create(ctx, "sp_non_arr", impl->func);
                auto* spCountBB = llvm::BasicBlock::Create(ctx, "sp_cnt", impl->func);
                impl->builder->CreateCondBr(spIsArr, spArrBB, spNonArrBB);
                // Non-array: buf[0] = val, count = 1
                impl->builder->SetInsertPoint(spNonArrBB);
                auto* spNonGep = impl->builder->CreateGEP(bufTy, bufAlloca,
                    {llvm::ConstantInt::get(impl->i32Ty, 0), llvm::ConstantInt::get(impl->i32Ty, 0)});
                impl->builder->CreateStore(val, spNonGep);
                impl->builder->CreateBr(spCountBB);
                // Array: iterate elements with count = min(size, 256)
                impl->builder->SetInsertPoint(spArrBB);
                auto* spPayload = impl->builder->CreateAnd(val,
                    llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK), "sp_pay");
                auto* spArrPtr = impl->builder->CreateIntToPtr(spPayload, impl->i8PtrTy, "sp_a");
                auto* spVStart = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, spArrPtr, 16), "sp_vs");
                auto* spVFinish = impl->builder->CreateLoad(impl->i8PtrTy,
                    impl->builder->CreateConstGEP1_32(impl->i8Ty, spArrPtr, 24), "sp_vf");
                auto* spBDiff = impl->builder->CreatePtrDiff(impl->i8Ty, spVFinish, spVStart, "sp_bd");
                auto* spSize = impl->builder->CreateAShr(spBDiff,
                    llvm::ConstantInt::get(impl->i64Ty, 4), "sp_sz");
                auto* spMaxC = llvm::ConstantInt::get(impl->i64Ty, 256);
                auto* spCmpL = impl->builder->CreateICmpULT(spSize, spMaxC, "sp_cmp");
                auto* spArrCnt = impl->builder->CreateSelect(spCmpL, spSize, spMaxC, "sp_acnt");
                auto* spLoopCheckBB = llvm::BasicBlock::Create(ctx, "sp_lchk", impl->func);
                auto* spLoopBodyBB = llvm::BasicBlock::Create(ctx, "sp_lbody", impl->func);
                auto* spLoopIdx = impl->builder->CreateAlloca(impl->i64Ty, nullptr, "sp_li");
                impl->builder->CreateStore(llvm::ConstantInt::get(impl->i64Ty, 0), spLoopIdx);
                impl->builder->CreateBr(spLoopCheckBB);
                impl->builder->SetInsertPoint(spLoopCheckBB);
                auto* spCurIx = impl->builder->CreateLoad(impl->i64Ty, spLoopIdx, "sp_cx");
                auto* spCont = impl->builder->CreateICmpULT(spCurIx, spArrCnt, "sp_cont");
                impl->builder->CreateCondBr(spCont, spLoopBodyBB, spCountBB);
                impl->builder->SetInsertPoint(spLoopBodyBB);
                auto* spBufGep = impl->builder->CreateGEP(bufTy, bufAlloca,
                    {llvm::ConstantInt::get(impl->i32Ty, 0),
                     impl->builder->CreateTrunc(spCurIx, impl->i32Ty, "sp_cx32")});
                auto* spSrcPtr = impl->builder->CreateGEP(impl->i8Ty, spVStart,
                    impl->builder->CreateMul(spCurIx,
                        llvm::ConstantInt::get(impl->i64Ty, 16), "sp_off"), "sp_sp");
                auto* spElem = impl->emitValueToNan(spSrcPtr);
                impl->builder->CreateStore(spElem, spBufGep);
                auto* spNextI = impl->builder->CreateAdd(spCurIx,
                    llvm::ConstantInt::get(impl->i64Ty, 1), "sp_nx");
                impl->builder->CreateStore(spNextI, spLoopIdx);
                impl->builder->CreateBr(spLoopCheckBB);
                // Merge counts
                impl->builder->SetInsertPoint(spCountBB);
                auto* spCntPHI = impl->builder->CreatePHI(impl->i8Ty, 2, "sp_cnt");
                spCntPHI->addIncoming(llvm::ConstantInt::get(impl->i8Ty, 1), spNonArrBB);
                spCntPHI->addIncoming(impl->builder->CreateTrunc(spArrCnt, impl->i8Ty, "sp_ac8"), spLoopCheckBB);
                // Push each element from buffer onto stack
                auto* spreadDoneBB = llvm::BasicBlock::Create(ctx, "spread_done", impl->func);
                auto* spreadCheckBB = llvm::BasicBlock::Create(ctx, "spread_check", impl->func);
                auto* spreadBodyBB = llvm::BasicBlock::Create(ctx, "spread_body", impl->func);
                auto* spreadIdx = impl->builder->CreateAlloca(impl->i8Ty, nullptr, "spread_i");
                impl->builder->CreateStore(llvm::ConstantInt::get(impl->i8Ty, 0), spreadIdx);
                impl->builder->CreateBr(spreadCheckBB);
                impl->builder->SetInsertPoint(spreadCheckBB);
                auto* curI = impl->builder->CreateLoad(impl->i8Ty, spreadIdx, "sp_i");
                auto* done = impl->builder->CreateICmpULT(curI, spCntPHI, "sp_done");
                impl->builder->CreateCondBr(done, spreadBodyBB, spreadDoneBB);
                impl->builder->SetInsertPoint(spreadBodyBB);
                auto* elemGep = impl->builder->CreateGEP(bufTy, bufAlloca,
                    {llvm::ConstantInt::get(impl->i32Ty, 0),
                     impl->builder->CreateZExt(curI, impl->i32Ty, "sp_i_ext")});
                auto* elem = impl->builder->CreateLoad(impl->i64Ty, elemGep, "sp_elem");
                impl->builder->CreateStore(elem, impl->pushValue());
                auto* nextI = impl->builder->CreateAdd(curI, llvm::ConstantInt::get(impl->i8Ty, 1), "sp_next");
                impl->builder->CreateStore(nextI, spreadIdx);
                impl->builder->CreateBr(spreadCheckBB);
                impl->builder->SetInsertPoint(spreadDoneBB);
                break;
            }

            // === Extended opcodes (fallback handlers) ===
            case OpCode::OP_TAIL_CALL: {
                uint8_t argCount = impl->readByte();
                llvm::Value* argsPtr = llvm::ConstantPointerNull::get(impl->i8PtrTy);
                if (argCount > 0) {
                    auto* argsTy = llvm::ArrayType::get(impl->i64Ty, argCount);
                    auto* argsAlloca = impl->builder->CreateAlloca(argsTy, nullptr, "tcall_args");
                    for (int i = argCount - 1; i >= 0; i--) {
                        auto* argPtr = impl->popValue();
                        auto* arg = impl->builder->CreateLoad(impl->i64Ty, argPtr, "tcall_arg");
                        auto* gep = impl->builder->CreateGEP(argsTy, argsAlloca,
                            {llvm::ConstantInt::get(impl->i32Ty, 0),
                             llvm::ConstantInt::get(impl->i32Ty, i)});
                        impl->builder->CreateStore(arg, gep);
                    }
                    argsPtr = impl->builder->CreatePointerCast(argsAlloca, impl->i8PtrTy);
                }
                auto* calleePtr = impl->popValue();
                auto* callee = impl->builder->CreateLoad(impl->i64Ty, calleePtr, "tcallee");
                auto* sentinel = llvm::ConstantInt::get(impl->i64Ty, AOT_SENTINEL);
                auto* directResult = impl->builder->CreateCall(impl->aotTryDirectCallFunc,
                    {impl->vmCtx, callee, argsPtr,
                     llvm::ConstantInt::get(impl->i8Ty, argCount)}, "tdirect_call");
                auto* isMiss = impl->builder->CreateICmpEQ(directResult, sentinel, "tis_miss");
                auto* tryDirectBB = impl->builder->GetInsertBlock();
                auto* callFallbackBB = llvm::BasicBlock::Create(ctx, "tcall_fb", impl->func);
                auto* callMergeBB = llvm::BasicBlock::Create(ctx, "tcall_mg", impl->func);
                impl->builder->CreateCondBr(isMiss, callFallbackBB, callMergeBB);

                impl->builder->SetInsertPoint(callFallbackBB);
                auto* fallbackResult = impl->builder->CreateCall(impl->aotCallFunc,
                    {impl->vmCtx, callee, argsPtr,
                     llvm::ConstantInt::get(impl->i8Ty, argCount)}, "tcall_fb_r");
                impl->builder->CreateBr(callMergeBB);

                impl->builder->SetInsertPoint(callMergeBB);
                auto* phi = impl->builder->CreatePHI(impl->i64Ty, 2, "tcall_r");
                phi->addIncoming(directResult, tryDirectBB);
                phi->addIncoming(fallbackResult, callFallbackBB);
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(phi, dest);
                break;
            }

            case OpCode::OP_THROW: {
                auto* excPtr = impl->popValue();
                auto* excVal = impl->builder->CreateLoad(impl->i64Ty, excPtr, "exception");
                impl->builder->CreateCall(impl->aotThrowErrorFunc, {impl->vmCtx, excVal});
                // aot_throwError never returns (calls exit())
                auto* unreachable = llvm::BasicBlock::Create(ctx, "throw_unreachable", impl->func);
                impl->builder->CreateUnreachable();
                impl->builder->SetInsertPoint(unreachable);
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_BREAK:
            case OpCode::OP_CONTINUE: {
                // Compiler transforms break/continue into OP_JUMP/OP_LOOP,
                // so these should never appear in bytecode. Handle as jump for safety.
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;
                impl->builder->CreateBr(impl->getOrCreateBB(target, impl->func));
                break;
            }

            case OpCode::OP_END_TRY: {
                impl->builder->CreateCall(impl->aotTryPopFunc, {impl->vmCtx});
                break;
            }

            case OpCode::OP_LOOP_HINT: {
                // No-op: JIT hint only
                break;
            }

            case OpCode::OP_VALIDATE_SAFE_FUNCTION: {
                // Peek at top of stack (callable), validate
                auto* topPtr = impl->peekValue();
                auto* topVal = impl->builder->CreateLoad(impl->i64Ty, topPtr, "safe_fn");
                impl->builder->CreateCall(impl->aotValidateSafeFuncFunc,
                    {impl->vmCtx, topVal, llvm::ConstantInt::get(impl->i32Ty, 0)});
                break;
            }

            case OpCode::OP_VALIDATE_SAFE_FILE_FUNCTION: {
                auto* topPtr = impl->peekValue();
                auto* topVal = impl->builder->CreateLoad(impl->i64Ty, topPtr, "safe_fn_f");
                impl->builder->CreateCall(impl->aotValidateSafeFileFuncFunc,
                    {impl->vmCtx, topVal});
                break;
            }

            case OpCode::OP_TRY: {
                uint16_t tryEnd = impl->readShort();
                uint16_t catchStart = impl->readShort();
                uint16_t finallyStart = impl->readShort();
                impl->builder->CreateCall(impl->aotTryPushFunc,
                    {impl->vmCtx,
                     llvm::ConstantInt::get(impl->i16Ty, tryEnd),
                     llvm::ConstantInt::get(impl->i16Ty, catchStart),
                     llvm::ConstantInt::get(impl->i16Ty, finallyStart)});
                break;
            }

            case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE: {
                uint8_t constIdx = impl->readByte();
                const Value& nameV = impl->chunk->constants[constIdx];
                std::string varName = nameV.type == ValueType::OBJ_STRING ? nameV.as.obj_string->chars : "?";
                // Construct error message at compile time and call runtimeError directly
                std::string errMsg;
                if (op == OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE) {
                    errMsg = "Variable '" + varName + "' must have a type annotation in safe file (.ntsc).";
                } else {
                    errMsg = "Variable '" + varName + "' must have a type annotation inside a safe block.";
                }
                auto* errStr = impl->builder->CreateGlobalStringPtr(errMsg, "safe_var_msg");
                impl->builder->CreateCall(impl->aotRuntimeErrorFunc, {impl->vmCtx, errStr});
                impl->builder->CreateUnreachable();
                break;
            }

            case OpCode::OP_TYPE_GUARD: {
                impl->readByte(); // skip operand (no-op in interpreter)
                break;
            }

            case OpCode::OP_LOGICAL_AND:
            case OpCode::OP_LOGICAL_OR: {
                // Compiler transforms logical AND/OR into OP_DUP + OP_JUMP_IF_FALSE + OP_POP patterns
                // These should never appear in bytecode. Handle as operand skip + push nil for safety.
                uint16_t offset = impl->readShort();
                (void)offset;
                impl->popValue();
                impl->popValue();
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            case OpCode::OP_SAY: {
                auto* a = impl->popValue();
                auto* val = impl->builder->CreateLoad(impl->i64Ty, a, "say_val");
                // Extract tag bits
                auto* sNanMask = llvm::ConstantInt::get(impl->i64Ty, NAN_MASK);
                auto* sNanBase = llvm::ConstantInt::get(impl->i64Ty, NAN_BASE);
                auto* sMasked = impl->builder->CreateAnd(val, sNanMask, "say_msk");
                auto* sTagged = impl->builder->CreateICmpEQ(sMasked, sNanBase, "say_tgd");
                auto* sNotTgd = impl->builder->CreateNot(sTagged, "say_ntgd");
                auto* sShift = impl->builder->CreateLShr(val,
                    llvm::ConstantInt::get(impl->i64Ty, TAG_SHIFT), "say_sh");
                auto* sTag = impl->builder->CreateAnd(sShift,
                    llvm::ConstantInt::get(impl->i64Ty, 0x7), "say_tag");
                auto* sPay = impl->builder->CreateAnd(val,
                    llvm::ConstantInt::get(impl->i64Ty, PAYLOAD_MASK), "say_pay");
                // Build blocks
                auto* sNumBB = llvm::BasicBlock::Create(ctx, "say_num", impl->func);
                auto* sTagBB = llvm::BasicBlock::Create(ctx, "say_tag", impl->func);
                auto* sNilBB = llvm::BasicBlock::Create(ctx, "say_nil", impl->func);
                auto* sNotNilBB = llvm::BasicBlock::Create(ctx, "say_nnil", impl->func);
                auto* sBoolBB = llvm::BasicBlock::Create(ctx, "say_bool", impl->func);
                auto* sNotBoolBB = llvm::BasicBlock::Create(ctx, "say_nbool", impl->func);
                auto* sTrueBB = llvm::BasicBlock::Create(ctx, "say_true", impl->func);
                auto* sFalseBB = llvm::BasicBlock::Create(ctx, "say_false", impl->func);
                auto* sStrBB = llvm::BasicBlock::Create(ctx, "say_str", impl->func);
                auto* sFbBB = llvm::BasicBlock::Create(ctx, "say_fb", impl->func);
                auto* sMgBB = llvm::BasicBlock::Create(ctx, "say_mg", impl->func);
                // notTagged → numBB else → tagBB
                impl->builder->CreateCondBr(sNotTgd, sNumBB, sTagBB);
                // Number path — inline printf("%lld" / "%g") with integer check
                impl->builder->SetInsertPoint(sNumBB);
                auto* sNumDbl = impl->builder->CreateBitCast(val, impl->doubleTy, "s_nd");
                auto* sTrunc = impl->builder->CreateFPToSI(sNumDbl, impl->i64Ty, "s_trunc");
                auto* sTruncDbl = impl->builder->CreateSIToFP(sTrunc, impl->doubleTy, "s_td");
                auto* sIsInt = impl->builder->CreateFCmpOEQ(sNumDbl, sTruncDbl, "s_is_int");
                auto* sIntBB = llvm::BasicBlock::Create(ctx, "say_int", impl->func);
                auto* sFltBB = llvm::BasicBlock::Create(ctx, "say_flt", impl->func);
                impl->builder->CreateCondBr(sIsInt, sIntBB, sFltBB);
                impl->builder->SetInsertPoint(sIntBB);
                auto* sFmtInt = impl->builder->CreateGlobalStringPtr("%lld\n", "s_fmt_i");
                impl->builder->CreateCall(impl->printfFunc, {sFmtInt, sTrunc});
                impl->builder->CreateBr(sMgBB);
                impl->builder->SetInsertPoint(sFltBB);
                auto* sFmtFlt = impl->builder->CreateGlobalStringPtr("%g\n", "s_fmt_f");
                impl->builder->CreateCall(impl->printfFunc, {sFmtFlt, sNumDbl});
                impl->builder->CreateBr(sMgBB);
                // Tag dispatch: isNil → nilBB else → notNilBB
                impl->builder->SetInsertPoint(sTagBB);
                auto* sIsNil = impl->builder->CreateICmpEQ(sTag,
                    llvm::ConstantInt::get(impl->i64Ty, 0), "say_nil_chk");
                impl->builder->CreateCondBr(sIsNil, sNilBB, sNotNilBB);
                // Nil path
                impl->builder->SetInsertPoint(sNilBB);
                auto* sNilStr = impl->builder->CreateGlobalStringPtr("nil", "s_nil");
                impl->builder->CreateCall(impl->putsFunc, {sNilStr});
                impl->builder->CreateBr(sMgBB);
                // Not nil: isBool → boolBB else → notBoolBB
                impl->builder->SetInsertPoint(sNotNilBB);
                auto* sIsBool = impl->builder->CreateICmpEQ(sTag,
                    llvm::ConstantInt::get(impl->i64Ty, 1), "say_bool_chk");
                impl->builder->CreateCondBr(sIsBool, sBoolBB, sNotBoolBB);
                // Bool: check payload bit 0
                impl->builder->SetInsertPoint(sBoolBB);
                auto* sBoolBit = impl->builder->CreateAnd(val,
                    llvm::ConstantInt::get(impl->i64Ty, 1), "say_bool_bit");
                auto* sIsTrue = impl->builder->CreateICmpNE(sBoolBit,
                    llvm::ConstantInt::get(impl->i64Ty, 0), "say_is_true");
                impl->builder->CreateCondBr(sIsTrue, sTrueBB, sFalseBB);
                // True path
                impl->builder->SetInsertPoint(sTrueBB);
                auto* sTrueStr = impl->builder->CreateGlobalStringPtr("true", "s_true");
                impl->builder->CreateCall(impl->putsFunc, {sTrueStr});
                impl->builder->CreateBr(sMgBB);
                // False path
                impl->builder->SetInsertPoint(sFalseBB);
                auto* sFalseStr = impl->builder->CreateGlobalStringPtr("false", "s_false");
                impl->builder->CreateCall(impl->putsFunc, {sFalseStr});
                impl->builder->CreateBr(sMgBB);
                // Not bool: isStr → strBB else → fallback
                impl->builder->SetInsertPoint(sNotBoolBB);
                auto* sIsStr = impl->builder->CreateICmpEQ(sTag,
                    llvm::ConstantInt::get(impl->i64Ty, 2), "say_str_chk");
                impl->builder->CreateCondBr(sIsStr, sStrBB, sFbBB);
                // String path: GEP into ObjString at known offsets (vtable=0, obj_type=8, is_marked=9, pad=10..15, chars._M_p=16, chars._M_len=24)
                impl->builder->SetInsertPoint(sStrBB);
                auto* sStrPtr = impl->builder->CreateIntToPtr(sPay, impl->i8PtrTy, "say_strp");
                auto* sSDataPtr = impl->builder->CreateGEP(impl->i8Ty, sStrPtr,
                    llvm::ConstantInt::get(impl->i32Ty, 16), "str_dp");
                auto* sSData = impl->builder->CreateLoad(impl->i8PtrTy, sSDataPtr, "str_d");
                auto* sSLenPtr = impl->builder->CreateGEP(impl->i8Ty, sStrPtr,
                    llvm::ConstantInt::get(impl->i32Ty, 24), "str_lp");
                auto* sSLen = impl->builder->CreateLoad(impl->i64Ty, sSLenPtr, "str_l");
                auto* sFmtStr = impl->builder->CreateGlobalStringPtr("%.*s\n", "s_fmt_s");
                auto* sSLen32 = impl->builder->CreateTrunc(sSLen, impl->i32Ty, "str_l32");
                impl->builder->CreateCall(impl->printfFunc, {sFmtStr, sSLen32, sSData});
                impl->builder->CreateBr(sMgBB);
                // Fallback: call aot_printValue
                impl->builder->SetInsertPoint(sFbBB);
                impl->builder->CreateCall(impl->aotPrintFunc, {impl->vmCtx, val});
                impl->builder->CreateBr(sMgBB);
                // Merge
                impl->builder->SetInsertPoint(sMgBB);
                break;
            }

            default: {
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

        if (llvm::verifyFunction(*impl->func, &llvm::outs())) {
            std::cerr << "LLVM function verification failed" << std::endl;
            return false;
        }
        return true;
    };

    // Compile sub-functions (scan constants for Function objects with chunks)
    {
        auto* mainEntryBB = entry;
        (void)mainEntryBB;
        int nextFuncIdx = 0;
        for (size_t i = 0; i < chunk->constants.size(); i++) {
            const Value& v = chunk->constants[i];
            if (v.type == ValueType::CALLABLE && v.as.callable->obj_type == ObjType::OBJ_FUNCTION) {
                auto* fn = static_cast<Function*>(v.as.callable);
                if (fn->chunk) {
                    int funcIdx = nextFuncIdx++;
                    fn->aotFuncIndex = funcIdx;

                    auto* savedFunc = impl->func;
                    auto* savedChunk = impl->chunk;
                    auto* savedConstants = impl->constantsGlobal;
                    auto* savedInternCache = impl->internCacheGlobal;
                    bool savedIsMain = impl->isMainFunc;

                    auto* subFnTy = llvm::FunctionType::get(impl->i64Ty, {
                        llvm::PointerType::get(impl->i8Ty, 0),
                        llvm::PointerType::get(impl->i8Ty, 0),
                        impl->i64Ty
                    }, false);
                    std::string subFnName = "neutron_func_" + std::to_string(funcIdx);
                    auto* subLLVMFn = llvm::Function::Create(subFnTy,
                        llvm::Function::InternalLinkage, subFnName, impl->module.get());
                    subLLVMFn->getArg(0)->setName("vm_ctx");
                    subLLVMFn->getArg(1)->setName("args");
                    subLLVMFn->getArg(2)->setName("arg_count");

                    impl->func = subLLVMFn;
                    impl->chunk = fn->chunk;
                    impl->constantsGlobal = impl->createConstantsForChunk(fn->chunk,
                        "constants_" + std::to_string(funcIdx));
                    impl->createInternCache();
                    impl->isMainFunc = false;

                    auto* subEntry = llvm::BasicBlock::Create(ctx, "entry", subLLVMFn);
                    impl->builder->SetInsertPoint(subEntry);
                    impl->setupStackLocals(subLLVMFn, subLLVMFn->getArg(0));

                    if (!emitCurrentFunc()) {
                        return false;
                    }

                    impl->builder->SetInsertPoint(entry);
                    auto* fnPtrCast = impl->builder->CreatePointerCast(subLLVMFn, impl->i8PtrTy);
                    impl->builder->CreateCall(impl->aotRegisterLlvmFuncFunc,
                        {llvm::ConstantInt::get(impl->i32Ty, funcIdx), fnPtrCast});

                    impl->func = savedFunc;
                    impl->chunk = savedChunk;
                    impl->constantsGlobal = savedConstants;
                    impl->internCacheGlobal = savedInternCache;
                    impl->isMainFunc = savedIsMain;
                }
            }
        }
    }

    impl->builder->SetInsertPoint(entry);

    // Compile main function
    if (!emitCurrentFunc()) {
        return false;
    }

    impl->module->print(llvm::errs(), nullptr);

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

    // Select CPU and features: use host-native when targeting the current machine
    std::string cpu = "generic";
    std::string features;
    if (targetPlatform == TargetPlatform::NATIVE) {
        cpu = llvm::sys::getHostCPUName().str();
        auto hostFeatures = llvm::sys::getHostCPUFeatures();
        for (const auto& f : hostFeatures) {
            if (f.second) features += "+";
            else          features += "-";
            features += f.first().str();
            features += ",";
        }
        if (!features.empty()) features.pop_back(); // trailing comma
    }

    auto* tm = target->createTargetMachine(llvm::Triple(targetTriple), cpu, features,
                                            {}, llvm::Reloc::PIC_);
    impl->module->setDataLayout(tm->createDataLayout());

    // Run LLVM optimization pipeline (O2)
    {
#ifdef NDEBUG
        llvm::errs() << "Optimizing with O2 (release)\n";
#else
        llvm::errs() << "=== BEFORE OPT ===\n";
        impl->module->print(llvm::errs(), nullptr);
#endif
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

#ifndef NDEBUG
    llvm::errs() << "=== AFTER OPT ===\n";
    impl->module->print(llvm::errs(), nullptr);
#endif

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
