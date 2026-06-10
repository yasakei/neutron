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

    // Function table for runtime-resolved function closures
    // Populated at startup by the C wrapper from the chunk's function-typed constants
    llvm::GlobalVariable* funcTableGlobal = nullptr;

    // External function declarations
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

    // Initialize LLVM types
    void initTypes() {
        i8Ty = llvm::Type::getInt8Ty(context);
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
    }

    // Create the constants global array from chunk (as NaN-boxed i64 values)
    void createConstants() {
        size_t n = chunk->constants.size();
        auto* arrTy = llvm::ArrayType::get(i64Ty, n);

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

        switch (op) {
            case OpCode::OP_RETURN: {
                impl->popValue();
                impl->builder->CreateRet(llvm::ConstantInt::get(impl->i32Ty, 0));
                break;
            }

            case OpCode::OP_CONSTANT: {
                uint8_t index = impl->readByte();
                // For string constants, intern at runtime via helper
                if (index < impl->chunk->constants.size() &&
                    impl->chunk->constants[index].type == ValueType::OBJ_STRING) {
                    std::string strContent = impl->chunk->constants[index].as.obj_string->chars;
                    auto* strPtr = impl->builder->CreateGlobalStringPtr(strContent, "const_str");
                    auto* interned = impl->builder->CreateCall(impl->aotInternFunc,
                        {impl->vmCtx, strPtr}, "interned");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(interned, dest);
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
                    auto* interned = impl->builder->CreateCall(impl->aotInternFunc,
                        {impl->vmCtx, strPtr}, "interned");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(interned, dest);
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
                auto* targetBB = impl->getOrCreateBB(target, func);
                impl->builder->CreateBr(targetBB);
                break;
            }

            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = impl->readShort();
                size_t target = impl->ip + offset;

                auto* val = impl->popValue();
                auto* truthy = impl->computeTruthy(val);

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
                auto* localInt = impl->builder->CreateFPToSI(impl->loadData(localPtr), impl->i64Ty, "local_int");
                auto* constInt = impl->builder->CreateFPToSI(impl->loadData(constPtr), impl->i64Ty, "const_int");
                auto* cond = impl->builder->CreateICmpSLT(localInt, constInt, "loop_cond");

                auto* exitBB = impl->getOrCreateBB(exitTarget, func);
                auto* contBB = llvm::BasicBlock::Create(ctx, "cont_less", func);
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
            case OpCode::OP_SET_GLOBAL_TYPED:
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
                (void)typeByte;
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
                    auto* calleePtr = impl->popValue();
                    auto* callee = impl->builder->CreateLoad(impl->i64Ty, calleePtr, "callee");
                    auto* argsPtr = impl->builder->CreatePointerCast(argsAlloca, impl->i8PtrTy);
                    auto* result = impl->builder->CreateCall(impl->aotCallFunc,
                        {impl->vmCtx, callee, argsPtr,
                         llvm::ConstantInt::get(impl->i8Ty, argCount)}, "call_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                } else {
                    auto* calleePtr = impl->popValue();
                    auto* callee = impl->builder->CreateLoad(impl->i64Ty, calleePtr, "callee");
                    auto* result = impl->builder->CreateCall(impl->aotCallFunc,
                        {impl->vmCtx, callee,
                         llvm::ConstantPointerNull::get(impl->i8PtrTy),
                         llvm::ConstantInt::get(impl->i8Ty, 0)}, "call_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                }
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
                auto* result = impl->builder->CreateCall(impl->aotIndexGetFunc,
                    {impl->vmCtx, objVal, idxVal}, "idx_result");
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(result, dest);
                break;
            }

            case OpCode::OP_INDEX_SET: {
                auto* valPtr = impl->popValue();
                auto* idxPtr = impl->popValue();
                auto* objPtr = impl->popValue();
                auto* objVal = impl->builder->CreateLoad(impl->i64Ty, objPtr, "obj");
                auto* idxVal = impl->builder->CreateLoad(impl->i64Ty, idxPtr, "idx");
                auto* val = impl->builder->CreateLoad(impl->i64Ty, valPtr, "val");
                impl->builder->CreateCall(impl->aotIndexSetFunc,
                    {impl->vmCtx, objVal, idxVal, val});
                // OP_INDEX_SET pushes the value back
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(val, dest);
                break;
            }

            case OpCode::OP_GET_PROPERTY: {
                uint8_t propIdx = impl->readByte();
                // Get property name from constant pool
                const Value& nameVal = impl->chunk->constants[propIdx];
                std::string propName = nameVal.type == ValueType::OBJ_STRING
                    ? nameVal.as.obj_string->chars : "?";
                auto* propNameStr = impl->builder->CreateGlobalStringPtr(propName, "prop_name");
                auto* objPtr = impl->popValue();
                auto* objVal = impl->builder->CreateLoad(impl->i64Ty, objPtr, "obj");
                auto* cacheGv = impl->createPropCache();
                auto* cachePtr = impl->builder->CreateBitCast(cacheGv, impl->i8PtrTy, "cache_ptr");
                auto* result = impl->builder->CreateCall(impl->aotGetPropCachedFunc,
                    {impl->vmCtx, objVal, propNameStr, cachePtr}, "prop_result");
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(result, dest);
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
                impl->builder->CreateCall(impl->aotSetPropCachedFunc,
                    {impl->vmCtx, objVal, propNameStr, val, cachePtr});
                // OP_SET_PROPERTY pushes the value back
                auto* dest = impl->pushValue();
                impl->builder->CreateStore(val, dest);
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
                impl->popValue();
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
                uint16_t offset = (static_cast<uint16_t>(offsetHi) << 8) | offsetLo;
                impl->ip += offset;
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

                auto* getPropBB = llvm::BasicBlock::Create(ctx, "opt_getprop", func);
                auto* nilBB = llvm::BasicBlock::Create(ctx, "opt_nil", func);
                auto* mergeBB = llvm::BasicBlock::Create(ctx, "opt_merge", func);
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
                impl->popValue();
                auto* d = impl->pushValue();
                impl->builder->CreateStore(impl->constValue(TAG_NIL, 0.0), d);
                break;
            }

            // === Extended opcodes (fallback handlers) ===
            case OpCode::OP_TAIL_CALL: {
                uint8_t argCount = impl->readByte();
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
                    auto* calleePtr = impl->popValue();
                    auto* callee = impl->builder->CreateLoad(impl->i64Ty, calleePtr, "tcallee");
                    auto* argsPtr = impl->builder->CreatePointerCast(argsAlloca, impl->i8PtrTy);
                    auto* result = impl->builder->CreateCall(impl->aotCallFunc,
                        {impl->vmCtx, callee, argsPtr,
                         llvm::ConstantInt::get(impl->i8Ty, argCount)}, "tcall_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                } else {
                    auto* calleePtr = impl->popValue();
                    auto* callee = impl->builder->CreateLoad(impl->i64Ty, calleePtr, "tcallee");
                    auto* result = impl->builder->CreateCall(impl->aotCallFunc,
                        {impl->vmCtx, callee,
                         llvm::ConstantPointerNull::get(impl->i8PtrTy),
                         llvm::ConstantInt::get(impl->i8Ty, 0)}, "tcall_result");
                    auto* dest = impl->pushValue();
                    impl->builder->CreateStore(result, dest);
                }
                break;
            }

            case OpCode::OP_THROW: {
                impl->popValue();
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
                break;

            case OpCode::OP_TRY: {
                impl->readShort(); impl->readShort(); impl->readShort();
                break;
            }

            case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE: {
                impl->readByte();
                break;
            }

            case OpCode::OP_TYPE_GUARD: {
                impl->readByte();
                break;
            }

            case OpCode::OP_LOGICAL_AND:
            case OpCode::OP_LOGICAL_OR: {
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
                impl->builder->CreateCall(impl->aotPrintFunc, {impl->vmCtx, val});
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
