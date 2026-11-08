#pragma once

#include "Decompiler/BytecodeReader.h"

#include "Luau/Bytecode.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Luau::Decompiler
{

struct Insn
{
    uint32_t pc = 0;
    LuauOpcode op = LOP_NOP;
    uint32_t word = 0;
    uint32_t aux = 0;
    uint8_t length = 1;

    // control-flow target (pc) for jumps, loop instructions and LOADB skips; -1 otherwise. FASTCALL skips are not control flow here.
    int target = -1;

    // registers read, in source evaluation order, and registers written
    std::vector<uint8_t> uses;
    std::vector<uint8_t> defs;

    // CALL/RETURN/SETLIST with a MULTRET operand: base register of the value that produced the open top
    int multretBase = -1;
    // CALL with C=0 / GETVARARGS with B=0: the defined register is an open-ended value list
    bool producesMultret = false;
};

// A short-circuit expression compiled with jumps (`a and f()`, `if c then x else y`, comparisons materialized as booleans).
// Instructions [begin, end) evaluate a single value into `reg` and every path continues at instruction `end`.
struct ValueRegion
{
    uint32_t begin = 0;
    uint32_t end = 0;
    uint8_t reg = 0;
};

// The unit of the flow graph: one instruction, or one collapsed value region.
struct Unit
{
    uint32_t first = 0; // instruction index
    uint32_t last = 0;  // instruction index, inclusive
    int region = -1;

    std::vector<uint8_t> uses;
    std::vector<uint8_t> defs;
    std::vector<uint32_t> succs;
};

// A set of definitions and uses of one register connected through reaching definitions: one source-level value or variable.
struct Web
{
    uint8_t reg = 0;
    int defCount = 0;
    int useCount = 0;
    bool captured = false; // referenced by a closure; must be a named local
    bool loopVariable = false;
    int debugLocal = -1; // index into FunctionProto::locals
    std::vector<uint32_t> defUnits;
    std::vector<uint32_t> useUnits;
};

bool isConditionalJump(LuauOpcode op);

class FlowAnalysis
{
public:
    explicit FlowAnalysis(const FunctionProto& proto);

    const FunctionProto& proto;

    std::vector<Insn> insns;
    std::vector<int> insnAtPc; // pc -> instruction index, -1 for AUX words

    std::vector<ValueRegion> regions;
    std::vector<Unit> units;
    std::vector<uint32_t> unitOfInsn;
    std::vector<std::vector<uint32_t>> jumpSources; // unit -> units transferring control to it other than by falling through

    std::vector<Web> webs;
    std::vector<int> paramWebs;

    int unitAtPc(uint32_t pc) const;
    int defWeb(uint32_t unit, uint8_t reg) const;
    int useWeb(uint32_t unit, uint8_t reg) const;
    bool isConstructorStore(uint32_t unit) const;

    // Tries to recognize a value region starting at a conditional jump instruction.
    bool tryRegion(uint32_t begin, ValueRegion& region) const;

private:
    void decode();
    void computeLiveness();
    void detectRegions();
    void buildUnits();
    void markConstructorStores();
    void buildWebs();

    std::vector<std::vector<uint32_t>> insnSuccs;
    std::vector<std::vector<uint32_t>> insnPreds;
    std::vector<std::vector<uint64_t>> liveIn; // per instruction, 4x64 bits

    std::vector<bool> constructorStore;
    std::unordered_map<uint64_t, int> defWebs;
    std::unordered_map<uint64_t, int> useWebs;
};

} // namespace Luau::Decompiler
