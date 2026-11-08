#pragma once

#include "Decompiler/BytecodeReader.h"
#include "Decompiler/FlowAnalysis.h"
#include "Decompiler/Naming.h"

#include "Luau/Allocator.h"
#include "Luau/Ast.h"

#include <array>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Luau::Decompiler
{

struct DecompileContext
{
    DecompileContext(const Chunk& chunk, Allocator& allocator, std::unordered_set<std::string> globals)
        : chunk(chunk)
        , allocator(allocator)
        , namer(allocator, std::move(globals))
    {
    }

    const Chunk& chunk;
    Allocator& allocator;
    Namer namer;
    std::vector<std::string> comments;
    std::deque<std::vector<AstExprTable::Item>> tableItems;
    std::unordered_map<AstExprTable*, std::vector<AstExprTable::Item>*> tableStorage;
};

// Reconstructs one function: structured control flow from jumps, expressions from register traffic, locals from def-use webs.
class FunctionBuilder
{
public:
    FunctionBuilder(DecompileContext& ctx, const FunctionProto& proto, std::vector<AstLocal*> upvalues, size_t depth);

    // name the first parameter `self` (set by the parent for closures that look like methods)
    bool selfParameter = false;

    AstStatBlock* buildBody(std::vector<AstLocal*>& params);

private:
    struct Slot
    {
        enum class Kind
        {
            Empty,
            Pending,
            Var,
            Method,
        };

        Kind kind = Kind::Empty;
        AstExpr* expr = nullptr;
        AstLocal* local = nullptr;
        int web = -1;
        uint64_t seq = 0;
        bool truncated = false; // a single value taken from a call or `...`
        bool multret = false;
        int group = -1;
        AstExprTable* table = nullptr;
        size_t tableMark = 0;
        bool methodPure = false;
    };

    struct State
    {
        std::array<Slot, 256> slots;
    };

    struct Group
    {
        AstExpr* expr = nullptr;
        uint8_t first = 0;
        uint8_t count = 0;
    };

    struct LoopContext
    {
        int breakUnit = -1;
        int continueUnit = -1;
        int headerUnit = -1;
        int fallthroughUnit = -1;
        bool usedContinue = false;
    };

    struct ChainNode
    {
        uint32_t start;
        AstExpr* cond; // condition under which `taken` is reached
        uint32_t taken;
        uint32_t fall;
    };

    struct PureViolation
    {
    };

    // blocks
    AstStatBlock* emitBlock(uint32_t begin, uint32_t end, LoopContext& loop);
    void emitRange(uint32_t begin, uint32_t end, LoopContext& loop);
    uint32_t emitLoop(uint32_t unit, uint32_t latch, LoopContext& outer);
    uint32_t emitConditional(uint32_t unit, uint32_t end, LoopContext& loop);
    uint32_t emitNumericFor(uint32_t unit, LoopContext& outer);
    uint32_t emitGenericFor(uint32_t unit, LoopContext& outer);
    void emitJump(uint32_t unit, uint32_t end, LoopContext& loop);
    uint32_t emitInstruction(uint32_t insnIndex, int unit);

    // conditions
    std::vector<ChainNode> parseChain(uint32_t unit, uint32_t end, bool pureFirst, uint32_t& exitTrue, uint32_t& exitFalse);
    AstExpr* jumpCondition(const Insn& insn, int unit);
    bool reduceChain(std::vector<ChainNode> nodes, uint32_t exitTrue, uint32_t exitFalse, AstExpr*& reachTrue);
    uint32_t processUntilJump(uint32_t unit, uint32_t end);

    // value regions
    AstExpr* buildRegion(const ValueRegion& region);
    AstExpr* walkValue(uint32_t insn, uint32_t end, uint8_t reg, uint32_t regionBegin);
    AstExpr* makeIfElse(AstExpr* cond, AstExpr* ifTrue, AstExpr* ifFalse);

    // registers
    std::vector<AstExpr*> consume(const std::vector<uint8_t>& regs, int unit, bool lastIsMultiValue);
    AstExpr* consumeOne(uint8_t reg, int unit);
    void define(int unit, uint8_t reg, AstExpr* expr, bool truncated = false, bool multret = false);
    void defineGroup(int unit, uint8_t first, uint8_t count, AstExpr* expr);
    bool mustBeVariable(int web, AstExpr* expr) const;
    AstLocal* declareLocal(int web, const std::string& suggestion);
    void materialize(uint8_t reg);
    void flushPending(bool everything);
    void hoist(uint32_t begin, uint32_t end);

    // statements
    void emitStatement(AstStat* stat);
    void emitAssign(AstExpr* target, AstExpr* value);
    void storeTable(int unit, uint8_t tableReg, AstExpr* table, AstExpr* key, AstExpr* value, const char* field);

    // expressions
    AstExpr* constant(uint32_t index);
    AstExpr* closure(uint32_t insnIndex, const FunctionProto& child, uint32_t& next, uint8_t target, AstLocal*& selfLocal);
    AstExpr* field(AstExpr* object, const std::string& name);
    AstExpr* importExpr(uint32_t aux);
    const std::string& constantString(uint32_t index);
    bool isPure(AstExpr* expr) const;
    uint32_t emitUnit(uint32_t unit, uint32_t end, LoopContext& loop);
    AstStat* sharedReturn(uint32_t target);
    AstStat* unstructuredJump(uint32_t target);
    bool looksLikeMethod(const FunctionProto& child, uint32_t next, uint8_t target) const;
    // within one expression, global library paths may be read out of order: builtin calls load them after their arguments
    bool isMovable(AstExpr* expr) const;

    template<typename T, typename... Args>
    T* make(Args&&... args)
    {
        return ctx.allocator.alloc<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    AstArray<T> array(const std::vector<T>& items)
    {
        AstArray<T> result;
        result.size = items.size();
        result.data = items.empty() ? nullptr : static_cast<T*>(ctx.allocator.allocate(sizeof(T) * items.size()));
        for (size_t i = 0; i < items.size(); ++i)
            new (result.data + i) T(items[i]);
        return result;
    }

    AstName name(const std::string& text);

    DecompileContext& ctx;
    const FunctionProto& proto;
    FlowAnalysis flow;
    std::vector<AstLocal*> upvalues;
    size_t depth;

    State state;
    uint64_t nextSeq = 1;
    std::vector<Group> groups;
    std::vector<AstLocal*> webLocals;

    std::vector<std::vector<AstStat*>*> blocks;
    size_t statementCount = 0;
    int pureDepth = 0;
    int regionDepth = 0;
    std::unordered_map<AstExpr*, int> regionConsumed;
    uint64_t regionSeqStart = 0;

    std::vector<std::vector<uint32_t>> backJumps;
};

} // namespace Luau::Decompiler
