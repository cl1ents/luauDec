#include "Decompiler/FlowAnalysis.h"

#include "Decompiler/Decompile.h"

#include "Luau/BytecodeUtils.h"

#include <algorithm>
#include <numeric>

namespace Luau::Decompiler
{

namespace
{

using RegSet = std::vector<uint64_t>; // 4 words

RegSet emptySet()
{
    return RegSet(4, 0);
}

void setReg(RegSet& set, uint8_t reg)
{
    set[reg >> 6] |= uint64_t(1) << (reg & 63);
}

void clearReg(RegSet& set, uint8_t reg)
{
    set[reg >> 6] &= ~(uint64_t(1) << (reg & 63));
}

bool hasReg(const RegSet& set, uint8_t reg)
{
    return (set[reg >> 6] >> (reg & 63)) & 1;
}

void pushRange(std::vector<uint8_t>& out, int from, int to)
{
    for (int r = from; r <= to; ++r)
        out.push_back(uint8_t(r));
}

struct UnionFind
{
    std::vector<int> parent;

    int add()
    {
        parent.push_back(int(parent.size()));
        return parent.back();
    }

    int find(int x)
    {
        while (parent[x] != x)
        {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }

    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b)
            parent[std::max(a, b)] = std::min(a, b);
    }
};

uint64_t key(uint32_t unit, uint8_t reg)
{
    return (uint64_t(unit) << 8) | reg;
}

bool isUnconditionalJump(LuauOpcode op)
{
    return op == LOP_JUMP || op == LOP_JUMPBACK || op == LOP_JUMPX;
}

bool isStatementOp(const Insn& insn)
{
    switch (insn.op)
    {
    case LOP_SETGLOBAL:
    case LOP_SETUPVAL:
    case LOP_CLOSEUPVALS:
    case LOP_RETURN:
    case LOP_FORNPREP:
    case LOP_FORNLOOP:
    case LOP_FORGPREP:
    case LOP_FORGPREP_INEXT:
    case LOP_FORGPREP_NEXT:
    case LOP_FORGLOOP:
    case LOP_CMPPROTO:
    case LOP_NEWCLASSMEMBER:
    case LOP_NEWCLASS:
    case LOP_CONSTRUCT:
    case LOP_FINCONSTRUCT:
    case LOP_GETPRIVATEFIELD:
    case LOP_SETPRIVATEFIELD:
        return true;
    case LOP_CALL:
    case LOP_CALLFB:
        return LUAU_INSN_C(insn.word) == 1;
    default:
        return false;
    }
}

bool isTableStore(LuauOpcode op)
{
    switch (op)
    {
    case LOP_SETTABLE:
    case LOP_SETTABLEKS:
    case LOP_SETTABLEN:
    case LOP_SETUDATAKS:
    case LOP_SETLIST:
        return true;
    default:
        return false;
    }
}

uint8_t tableOperand(const Insn& insn)
{
    return insn.op == LOP_SETLIST ? LUAU_INSN_A(insn.word) : LUAU_INSN_B(insn.word);
}

} // namespace

bool isConditionalJump(LuauOpcode op)
{
    switch (op)
    {
    case LOP_JUMPIF:
    case LOP_JUMPIFNOT:
    case LOP_JUMPIFEQ:
    case LOP_JUMPIFLE:
    case LOP_JUMPIFLT:
    case LOP_JUMPIFNOTEQ:
    case LOP_JUMPIFNOTLE:
    case LOP_JUMPIFNOTLT:
    case LOP_JUMPXEQKNIL:
    case LOP_JUMPXEQKB:
    case LOP_JUMPXEQKN:
    case LOP_JUMPXEQKS:
        return true;
    default:
        return false;
    }
}

FlowAnalysis::FlowAnalysis(const FunctionProto& proto)
    : proto(proto)
{
    decode();
    computeLiveness();
    detectRegions();
    buildUnits();
    markConstructorStores();
    buildWebs();
}

int FlowAnalysis::unitAtPc(uint32_t pc) const
{
    if (pc >= insnAtPc.size())
        return int(units.size());
    int insn = insnAtPc[pc];
    return insn < 0 ? -1 : int(unitOfInsn[insn]);
}

int FlowAnalysis::defWeb(uint32_t unit, uint8_t reg) const
{
    auto it = defWebs.find(key(unit, reg));
    return it == defWebs.end() ? -1 : it->second;
}

int FlowAnalysis::useWeb(uint32_t unit, uint8_t reg) const
{
    auto it = useWebs.find(key(unit, reg));
    return it == useWebs.end() ? -1 : it->second;
}

bool FlowAnalysis::isConstructorStore(uint32_t unit) const
{
    return constructorStore[unit];
}

void FlowAnalysis::decode()
{
    const std::vector<uint32_t>& code = proto.code;
    insnAtPc.assign(code.size(), -1);

    int top = -1;

    for (uint32_t pc = 0; pc < code.size();)
    {
        Insn insn;
        insn.pc = pc;
        insn.word = code[pc];
        insn.op = LuauOpcode(LUAU_INSN_OP(insn.word));
        if (insn.op >= LOP__COUNT)
            throw DecompileError("unknown opcode " + std::to_string(int(insn.op)) + " at pc " + std::to_string(pc));

        insn.length = uint8_t(getOpLength(insn.op));
        if (pc + insn.length > code.size())
            throw DecompileError("truncated instruction at pc " + std::to_string(pc));
        if (insn.length == 2)
            insn.aux = code[pc + 1];

        uint32_t w = insn.word;
        int a = LUAU_INSN_A(w), b = LUAU_INSN_B(w), c = LUAU_INSN_C(w);

        if (!isFastCall(insn.op))
        {
            int target = getJumpTarget(w, pc);
            if (target >= 0)
                insn.target = target;
        }

        auto openTop = [&]() {
            if (top < 0)
                throw DecompileError("MULTRET operand without a producer at pc " + std::to_string(pc));
            int base = top;
            top = -1;
            return base;
        };

        switch (insn.op)
        {
        case LOP_LOADNIL:
        case LOP_LOADB:
        case LOP_LOADN:
        case LOP_LOADK:
        case LOP_LOADKX:
        case LOP_GETGLOBAL:
        case LOP_GETUPVAL:
        case LOP_GETIMPORT:
        case LOP_NEWCLOSURE:
        case LOP_DUPCLOSURE:
        case LOP_NEWTABLE:
        case LOP_DUPTABLE:
            insn.defs = {uint8_t(a)};
            break;

        case LOP_MOVE:
        case LOP_GETTABLEKS:
        case LOP_GETUDATAKS:
        case LOP_GETTABLEN:
        case LOP_ADDK:
        case LOP_SUBK:
        case LOP_MULK:
        case LOP_DIVK:
        case LOP_MODK:
        case LOP_POWK:
        case LOP_IDIVK:
        case LOP_ANDK:
        case LOP_ORK:
        case LOP_NOT:
        case LOP_MINUS:
        case LOP_LENGTH:
            insn.uses = {uint8_t(b)};
            insn.defs = {uint8_t(a)};
            break;

        case LOP_SUBRK:
        case LOP_DIVRK:
            insn.uses = {uint8_t(c)};
            insn.defs = {uint8_t(a)};
            break;

        case LOP_GETTABLE:
        case LOP_ADD:
        case LOP_SUB:
        case LOP_MUL:
        case LOP_DIV:
        case LOP_MOD:
        case LOP_POW:
        case LOP_IDIV:
        case LOP_AND:
        case LOP_OR:
            insn.uses = {uint8_t(b), uint8_t(c)};
            insn.defs = {uint8_t(a)};
            break;

        case LOP_SETGLOBAL:
        case LOP_SETUPVAL:
        case LOP_JUMPIF:
        case LOP_JUMPIFNOT:
        case LOP_JUMPXEQKNIL:
        case LOP_JUMPXEQKB:
        case LOP_JUMPXEQKN:
        case LOP_JUMPXEQKS:
        case LOP_CMPPROTO:
            insn.uses = {uint8_t(a)};
            break;

        case LOP_SETTABLE:
            insn.uses = {uint8_t(b), uint8_t(c), uint8_t(a)};
            break;

        case LOP_SETTABLEKS:
        case LOP_SETUDATAKS:
        case LOP_SETTABLEN:
            insn.uses = {uint8_t(b), uint8_t(a)};
            break;

        case LOP_NAMECALL:
        case LOP_NAMECALLUDATA:
            insn.uses = {uint8_t(b)};
            insn.defs = {uint8_t(a), uint8_t(a + 1)};
            break;

        case LOP_CALL:
        case LOP_CALLFB:
            if (b == 0)
            {
                insn.multretBase = openTop();
                pushRange(insn.uses, a, insn.multretBase);
            }
            else
                pushRange(insn.uses, a, a + b - 1);

            if (c == 0)
            {
                insn.defs = {uint8_t(a)};
                insn.producesMultret = true;
                top = a;
            }
            else
                pushRange(insn.defs, a, a + c - 2);
            break;

        case LOP_RETURN:
            if (b == 0)
            {
                insn.multretBase = openTop();
                pushRange(insn.uses, a, insn.multretBase);
            }
            else
                pushRange(insn.uses, a, a + b - 2);
            break;

        case LOP_JUMPIFEQ:
        case LOP_JUMPIFLE:
        case LOP_JUMPIFLT:
        case LOP_JUMPIFNOTEQ:
        case LOP_JUMPIFNOTLE:
        case LOP_JUMPIFNOTLT:
            if (insn.aux > 255)
                throw DecompileError("bad comparison register at pc " + std::to_string(pc));
            insn.uses = {uint8_t(a), uint8_t(insn.aux)};
            break;

        case LOP_CONCAT:
            pushRange(insn.uses, b, c);
            insn.defs = {uint8_t(a)};
            break;

        case LOP_SETLIST:
            insn.uses = {uint8_t(a)};
            if (c == 0)
            {
                insn.multretBase = openTop();
                pushRange(insn.uses, b, insn.multretBase);
            }
            else
                pushRange(insn.uses, b, b + c - 2);
            break;

        case LOP_FORNPREP:
            insn.uses = {uint8_t(a + 2), uint8_t(a), uint8_t(a + 1)};
            insn.defs = {uint8_t(a + 2)};
            break;

        case LOP_FORNLOOP:
            insn.defs = {uint8_t(a + 2)};
            break;

        case LOP_FORGPREP:
        case LOP_FORGPREP_INEXT:
        case LOP_FORGPREP_NEXT:
            insn.uses = {uint8_t(a), uint8_t(a + 1), uint8_t(a + 2)};
            break;

        case LOP_FORGLOOP:
            pushRange(insn.defs, a + 3, a + 2 + int(insn.aux & 0xff));
            break;

        case LOP_GETVARARGS:
            if (b == 0)
            {
                insn.defs = {uint8_t(a)};
                insn.producesMultret = true;
                top = a;
            }
            else
                pushRange(insn.defs, a, a + b - 2);
            break;

        case LOP_CAPTURE:
            if (a == LCT_VAL || a == LCT_REF)
                insn.uses = {uint8_t(b)};
            break;

        default:
            break;
        }

        insnAtPc[pc] = int(insns.size());
        insns.push_back(std::move(insn));
        pc += insns.back().length;
    }

    insnSuccs.resize(insns.size());
    insnPreds.resize(insns.size());

    for (uint32_t i = 0; i < insns.size(); ++i)
    {
        const Insn& insn = insns[i];
        bool fallsThrough = true;

        if (insn.target >= 0)
        {
            if (uint32_t(insn.target) >= code.size() || insnAtPc[insn.target] < 0)
                throw DecompileError("jump to invalid target at pc " + std::to_string(insn.pc));
            insnSuccs[i].push_back(uint32_t(insnAtPc[insn.target]));

            if (isUnconditionalJump(insn.op) || insn.op == LOP_LOADB || insn.op == LOP_FORGPREP || insn.op == LOP_FORGPREP_INEXT ||
                insn.op == LOP_FORGPREP_NEXT)
                fallsThrough = false;
        }

        if (insn.op == LOP_RETURN)
            fallsThrough = false;

        if (fallsThrough)
        {
            if (i + 1 >= insns.size())
                throw DecompileError("control flow falls off the end of the function");
            insnSuccs[i].push_back(i + 1);
        }

        for (uint32_t s : insnSuccs[i])
            insnPreds[s].push_back(i);
    }
}

void FlowAnalysis::computeLiveness()
{
    liveIn.assign(insns.size(), emptySet());

    bool changed = true;
    while (changed)
    {
        changed = false;

        for (size_t i = insns.size(); i-- > 0;)
        {
            RegSet live = emptySet();
            for (uint32_t s : insnSuccs[i])
                for (int w = 0; w < 4; ++w)
                    live[w] |= liveIn[s][w];

            for (uint8_t d : insns[i].defs)
                clearReg(live, d);
            for (uint8_t u : insns[i].uses)
                setReg(live, u);

            if (live != liveIn[i])
            {
                liveIn[i] = std::move(live);
                changed = true;
            }
        }
    }
}

void FlowAnalysis::detectRegions()
{
    for (uint32_t i = 0; i < insns.size(); ++i)
    {
        if (!isConditionalJump(insns[i].op))
            continue;

        ValueRegion region;
        if (tryRegion(i, region))
        {
            regions.push_back(region);
            i = region.end - 1;
        }
    }
}

bool FlowAnalysis::tryRegion(uint32_t begin, ValueRegion& region) const
{
    const Insn& first = insns[begin];
    // entering at the start from earlier code is fine; a back edge would make this a loop header
    bool forwardEntryOnly = std::all_of(insnPreds[begin].begin(), insnPreds[begin].end(), [&](uint32_t p) {
        return p < begin;
    });
    if (first.target <= int(first.pc) || !forwardEntryOnly)
        return false;

    uint32_t maxTarget = uint32_t(first.target);
    uint32_t end = begin;

    RegSet definedHere = emptySet();

    for (uint32_t k = begin + 1;; ++k)
    {
        if (k >= insns.size())
            return false;

        if (insns[k].pc == maxTarget)
        {
            end = k;
            break;
        }

        const Insn& insn = insns[k];

        if (isStatementOp(insn))
            return false;

        if (isTableStore(insn.op) && !hasReg(definedHere, tableOperand(insn)))
            return false;

        if (insn.target >= 0)
        {
            if (insn.target <= int(insn.pc))
                return false;
            maxTarget = std::max(maxTarget, uint32_t(insn.target));
        }

        for (uint8_t d : insn.defs)
            setReg(definedHere, d);
    }

    // no control transfers into the middle of the region from outside
    for (uint32_t k = begin + 1; k < end; ++k)
        for (uint32_t p : insnPreds[k])
            if (p < begin || p >= end)
                return false;

    // registers defined in the region that are still needed afterwards: at most one, the region's value
    RegSet liveOut = emptySet();
    int liveCount = 0;
    int resultReg = -1;
    for (int r = 0; r < 256; ++r)
        if (hasReg(definedHere, uint8_t(r)) && hasReg(liveIn[end], uint8_t(r)))
        {
            ++liveCount;
            resultReg = r;
        }

    if (liveCount > 1)
        return false;

    // definitely-defined registers along every path to `end`
    std::vector<RegSet> definedOut(end - begin, RegSet(4, ~uint64_t(0)));
    std::vector<bool> reached(end - begin, false);
    reached[0] = true;
    RegSet atEnd(4, ~uint64_t(0));
    bool endReached = false;

    for (uint32_t k = begin; k < end; ++k)
    {
        if (!reached[k - begin])
            continue;

        RegSet in = emptySet();
        bool any = false;
        for (uint32_t p : insnPreds[k])
        {
            if (p < begin || p >= end || !reached[p - begin])
                continue;
            if (!any)
                in = definedOut[p - begin];
            else
                for (int w = 0; w < 4; ++w)
                    in[w] &= definedOut[p - begin][w];
            any = true;
        }
        if (k == begin)
            in = emptySet();

        for (uint8_t d : insns[k].defs)
            setReg(in, d);
        definedOut[k - begin] = in;

        for (uint32_t s : insnSuccs[k])
        {
            if (s == end)
            {
                for (int w = 0; w < 4; ++w)
                    atEnd[w] &= in[w];
                endReached = true;
            }
            else if (s > begin && s < end)
                reached[s - begin] = true;
            else
                return false;
        }
    }

    if (!endReached)
        return false;

    bool priorPaths = false;

    if (liveCount == 1)
    {
        priorPaths = !hasReg(atEnd, uint8_t(resultReg));
    }
    else
    {
        int candidates = 0;
        for (int r = 0; r < 256; ++r)
            if (hasReg(atEnd, uint8_t(r)) && hasReg(definedHere, uint8_t(r)))
            {
                ++candidates;
                resultReg = r;
            }
        if (candidates != 1)
            return false;
    }

    uint8_t reg = uint8_t(resultReg);

    if (priorPaths)
    {
        if (begin == 0)
            return false;
        const Insn& prior = insns[begin - 1];
        if (std::find(prior.defs.begin(), prior.defs.end(), reg) == prior.defs.end() || prior.target >= 0)
            return false;
        bool testsResult = (first.op == LOP_JUMPIF || first.op == LOP_JUMPIFNOT) && LUAU_INSN_A(first.word) == reg;
        if (prior.op != LOP_LOADB && !testsResult)
            return false;
    }

    // every value computed inside is read at most once along all paths (the result once more, at `end`); other values die inside
    for (uint32_t k = begin; k < end; ++k)
    {
        for (uint8_t d : insns[k].defs)
        {
            bool isTable = insns[k].op == LOP_NEWTABLE || insns[k].op == LOP_DUPTABLE;
            int uses = 0;
            bool reachesEnd = false;

            std::vector<bool> reaches(end - begin, false);
            for (uint32_t s : insnSuccs[k])
            {
                if (s == end)
                    reachesEnd = true;
                else if (s > k && s < end)
                    reaches[s - begin] = true;
            }

            for (uint32_t j = k + 1; j < end; ++j)
            {
                if (!reaches[j - begin])
                    continue;

                const Insn& next = insns[j];
                bool storeIntoTable = isTable && isTableStore(next.op) && tableOperand(next) == d;
                // `v and ...` / `v or ...`: testing the result and passing it through is a single use
                bool resultTest = d == reg && (next.op == LOP_JUMPIF || next.op == LOP_JUMPIFNOT) && LUAU_INSN_A(next.word) == d &&
                                  insnAtPc[next.target] == int(end);
                for (size_t u = 0; u < next.uses.size(); ++u)
                    if (next.uses[u] == d && !(storeIntoTable && u == 0) && !resultTest)
                        ++uses;

                if (std::find(next.defs.begin(), next.defs.end(), d) != next.defs.end())
                    continue;

                for (uint32_t s : insnSuccs[j])
                {
                    if (s == end)
                        reachesEnd = true;
                    else if (s > j && s < end)
                        reaches[s - begin] = true;
                }
            }

            if (d == reg)
            {
                if (uses + (reachesEnd ? 1 : 0) > 1)
                    return false;
            }
            else if ((reachesEnd && hasReg(liveIn[end], d)) || uses != 1)
                return false;
        }
    }

    // `if x then x = f() end` on a named local reads better as a statement than as `x = x and f()`
    for (const LocalVarInfo& local : proto.locals)
        if (priorPaths && insns[begin - 1].op != LOP_LOADB && local.reg == reg && local.startpc <= first.pc && first.pc < local.endpc)
            return false;

    region.begin = begin;
    region.end = end;
    region.reg = reg;
    return true;
}

void FlowAnalysis::buildUnits()
{
    unitOfInsn.assign(insns.size(), 0);
    size_t nextRegion = 0;

    for (uint32_t i = 0; i < insns.size();)
    {
        Unit unit;
        unit.first = i;

        if (nextRegion < regions.size() && regions[nextRegion].begin == i)
        {
            const ValueRegion& region = regions[nextRegion];
            unit.last = region.end - 1;
            unit.region = int(nextRegion);
            unit.defs = {region.reg};

            // upward-exposed uses of the region
            std::vector<RegSet> live(region.end - region.begin, emptySet());
            for (uint32_t k = region.end; k-- > region.begin;)
            {
                RegSet set = emptySet();
                for (uint32_t s : insnSuccs[k])
                    if (s < region.end)
                        for (int w = 0; w < 4; ++w)
                            set[w] |= live[s - region.begin][w];
                for (uint8_t d : insns[k].defs)
                    clearReg(set, d);
                for (uint8_t u : insns[k].uses)
                    setReg(set, u);
                live[k - region.begin] = std::move(set);
            }
            // one entry per read of the incoming value, on any path through the region, so `t == t` or both arms of an
            // if-expression reading `x` count as separate uses
            for (int r = 0; r < 256; ++r)
            {
                if (!hasReg(live[0], uint8_t(r)))
                    continue;

                std::vector<bool> outerReaches(region.end - region.begin, false);
                outerReaches[0] = true;
                int reads = 0;
                for (uint32_t k = region.begin; k < region.end; ++k)
                {
                    if (!outerReaches[k - region.begin])
                        continue;
                    reads += int(std::count(insns[k].uses.begin(), insns[k].uses.end(), uint8_t(r)));
                    if (std::find(insns[k].defs.begin(), insns[k].defs.end(), uint8_t(r)) != insns[k].defs.end())
                        continue;
                    for (uint32_t s : insnSuccs[k])
                        if (s < region.end)
                            outerReaches[s - region.begin] = true;
                }
                for (int i = 0; i < std::max(reads, 1); ++i)
                    unit.uses.push_back(uint8_t(r));
            }

            ++nextRegion;
        }
        else
        {
            unit.last = i;
            unit.uses = insns[i].uses;
            unit.defs = insns[i].defs;
        }

        for (uint32_t k = unit.first; k <= unit.last; ++k)
            unitOfInsn[k] = uint32_t(units.size());

        i = unit.last + 1;
        units.push_back(std::move(unit));
    }

    jumpSources.resize(units.size());

    for (uint32_t u = 0; u < units.size(); ++u)
    {
        Unit& unit = units[u];
        if (unit.region >= 0)
        {
            unit.succs = {unitOfInsn[regions[unit.region].end]};
            continue;
        }

        const Insn& insn = insns[unit.first];
        for (uint32_t s : insnSuccs[unit.first])
        {
            uint32_t su = unitOfInsn[s];
            unit.succs.push_back(su);
            if (insn.target >= 0 && s == uint32_t(insnAtPc[insn.target]))
                jumpSources[su].push_back(u);
        }
    }
}

void FlowAnalysis::markConstructorStores()
{
    constructorStore.assign(units.size(), false);

    for (uint32_t u = 0; u < units.size(); ++u)
    {
        if (units[u].region >= 0)
            continue;

        const Insn& insn = insns[units[u].first];
        if (insn.op != LOP_NEWTABLE && insn.op != LOP_DUPTABLE)
            continue;

        uint8_t table = LUAU_INSN_A(insn.word);

        for (uint32_t v = u + 1; v < units.size(); ++v)
        {
            if (!jumpSources[v].empty())
                break;

            const Unit& unit = units[v];
            if (unit.region < 0)
            {
                const Insn& next = insns[unit.first];
                if (next.target >= 0 || next.op == LOP_RETURN)
                    break;

                if (isTableStore(next.op) && tableOperand(next) == table && std::count(next.uses.begin(), next.uses.end(), table) == 1)
                {
                    constructorStore[v] = true;
                    continue;
                }
            }

            if (std::find(unit.uses.begin(), unit.uses.end(), table) != unit.uses.end())
                break;

            bool clobbers = false;
            for (uint8_t d : unit.defs)
                clobbers |= d <= table;
            if (clobbers)
                break;
        }
    }
}

void FlowAnalysis::buildWebs()
{
    struct DefSite
    {
        uint32_t unit; // UINT32_MAX: function entry (parameter)
        uint8_t reg;
    };

    std::vector<DefSite> sites;
    UnionFind uf;

    std::vector<std::vector<int>> defsByReg(256);
    std::unordered_map<uint64_t, int> siteOf;

    for (uint8_t r = 0; r < proto.numParams; ++r)
    {
        sites.push_back({UINT32_MAX, r});
        defsByReg[r].push_back(uf.add());
    }

    for (uint32_t u = 0; u < units.size(); ++u)
        for (uint8_t d : units[u].defs)
        {
            int id = uf.add();
            sites.push_back({u, d});
            defsByReg[d].push_back(id);
            siteOf[key(u, d)] = id;
        }

    std::vector<std::vector<uint32_t>> preds(units.size());
    for (uint32_t u = 0; u < units.size(); ++u)
        for (uint32_t s : units[u].succs)
            preds[s].push_back(u);

    // reaching definitions, one register at a time
    std::vector<std::pair<uint32_t, uint8_t>> useList;
    std::vector<std::vector<int>> useReaching;

    for (int r = 0; r < 256; ++r)
    {
        const std::vector<int>& defs = defsByReg[r];
        if (defs.empty())
            continue;

        size_t words = (defs.size() + 63) / 64;
        std::unordered_map<int, size_t> bitOf;
        for (size_t i = 0; i < defs.size(); ++i)
            bitOf[defs[i]] = i;

        std::vector<std::vector<uint64_t>> out(units.size(), std::vector<uint64_t>(words, 0));
        std::vector<int> unitDef(units.size(), -1);
        for (uint32_t u = 0; u < units.size(); ++u)
            for (uint8_t d : units[u].defs)
                if (d == r)
                    unitDef[u] = siteOf[key(u, uint8_t(r))];

        std::vector<uint64_t> entry(words, 0);
        if (r < proto.numParams)
            entry[0] |= 1; // the parameter is defs[0]

        auto inOf = [&](uint32_t u) {
            std::vector<uint64_t> in = u == 0 ? entry : std::vector<uint64_t>(words, 0);
            for (uint32_t p : preds[u])
                for (size_t w = 0; w < words; ++w)
                    in[w] |= out[p][w];
            return in;
        };

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (uint32_t u = 0; u < units.size(); ++u)
            {
                std::vector<uint64_t> next;
                if (unitDef[u] >= 0)
                {
                    next.assign(words, 0);
                    size_t bit = bitOf[unitDef[u]];
                    next[bit / 64] |= uint64_t(1) << (bit % 64);
                }
                else
                    next = inOf(u);

                if (next != out[u])
                {
                    out[u] = std::move(next);
                    changed = true;
                }
            }
        }

        for (uint32_t u = 0; u < units.size(); ++u)
        {
            if (std::find(units[u].uses.begin(), units[u].uses.end(), r) == units[u].uses.end())
                continue;

            std::vector<uint64_t> in = inOf(u);
            std::vector<int> reaching;
            for (size_t i = 0; i < defs.size(); ++i)
                if ((in[i / 64] >> (i % 64)) & 1)
                    reaching.push_back(defs[i]);

            for (size_t i = 1; i < reaching.size(); ++i)
                uf.unite(reaching[0], reaching[i]);

            useList.push_back({u, uint8_t(r)});
            useReaching.push_back(std::move(reaching));
        }
    }

    // captured registers: closures refer to them by name. Captured by reference: later stores in the same scope update the closure's view.
    std::vector<int> capturedSites;
    for (size_t i = 0; i < useList.size(); ++i)
    {
        auto [u, r] = useList[i];
        if (units[u].region >= 0 || useReaching[i].empty())
            continue;

        const Insn& insn = insns[units[u].first];
        if (insn.op != LOP_CAPTURE)
            continue;

        capturedSites.push_back(useReaching[i][0]);

        if (LUAU_INSN_A(insn.word) == LCT_REF)
        {
            for (uint32_t v = u + 1; v < units.size(); ++v)
            {
                const Unit& unit = units[v];
                if (unit.region < 0)
                {
                    const Insn& next = insns[unit.first];
                    if (next.op == LOP_CLOSEUPVALS && LUAU_INSN_A(next.word) <= r)
                        break;
                }
                for (uint8_t d : unit.defs)
                    if (d == r)
                        uf.unite(useReaching[i][0], siteOf[key(v, r)]);
            }
        }
    }

    // debug info: every store to a declared local belongs to that local
    std::vector<int> siteLocal(sites.size(), -1);
    for (size_t li = 0; li < proto.locals.size(); ++li)
    {
        const LocalVarInfo& local = proto.locals[li];
        uint32_t lowerBound = 0;
        for (const LocalVarInfo& other : proto.locals)
            if (&other != &local && other.reg == local.reg && other.endpc <= local.startpc)
                lowerBound = std::max(lowerBound, other.endpc);

        int init = -1;
        uint32_t initPc = 0;
        std::vector<int> members;

        for (int id : defsByReg[local.reg])
        {
            const DefSite& site = sites[id];
            if (site.unit == UINT32_MAX)
            {
                if (local.startpc == 0)
                    members.push_back(id);
                continue;
            }

            uint32_t pc = insns[units[site.unit].first].pc;
            if (local.startpc <= pc && pc < local.endpc)
                members.push_back(id);
            else if (pc < local.startpc && pc >= lowerBound && (init < 0 || pc >= initPc))
            {
                init = id;
                initPc = pc;
            }
        }

        // generic for variables are written by FORGLOOP at the bottom of the loop, not by anything before the body
        int header = local.startpc > 0 && local.startpc - 1 < insnAtPc.size() ? insnAtPc[local.startpc - 1] : -1;
        if (header >= 0 && (insns[header].op == LOP_FORGPREP || insns[header].op == LOP_FORGPREP_INEXT || insns[header].op == LOP_FORGPREP_NEXT))
        {
            init = -1;
            auto it = siteOf.find(key(unitOfInsn[insnAtPc[insns[header].target]], local.reg));
            if (it != siteOf.end())
                members.push_back(it->second);
        }

        if (init >= 0 && !(local.startpc == 0 && local.reg < proto.numParams))
            members.push_back(init);

        if (members.empty())
            continue;

        for (int id : members)
        {
            uf.unite(members[0], id);
            siteLocal[id] = int(li);
        }
    }

    // materialize webs
    std::unordered_map<int, int> webOfRoot;
    auto webOf = [&](int site) {
        int root = uf.find(site);
        auto it = webOfRoot.find(root);
        if (it != webOfRoot.end())
            return it->second;
        int id = int(webs.size());
        webs.emplace_back();
        webs.back().reg = sites[site].reg;
        webOfRoot[root] = id;
        return id;
    };

    for (size_t id = 0; id < sites.size(); ++id)
    {
        int w = webOf(int(id));
        Web& web = webs[w];
        web.defCount++;
        if (sites[id].unit != UINT32_MAX)
        {
            web.defUnits.push_back(sites[id].unit);
            defWebs[key(sites[id].unit, sites[id].reg)] = w;

            const Unit& unit = units[sites[id].unit];
            if (unit.region < 0)
            {
                LuauOpcode op = insns[unit.first].op;
                if (op == LOP_FORNPREP || op == LOP_FORNLOOP || op == LOP_FORGLOOP)
                    web.loopVariable = true;
            }
        }
        if (siteLocal[id] >= 0)
            web.debugLocal = siteLocal[id];
    }

    for (uint8_t r = 0; r < proto.numParams; ++r)
        paramWebs.push_back(webOf(r));

    for (int site : capturedSites)
        webs[webOf(site)].captured = true;

    for (size_t i = 0; i < useList.size(); ++i)
    {
        if (useReaching[i].empty())
            continue;

        auto [u, r] = useList[i];
        int w = webOf(useReaching[i][0]);
        useWebs[key(u, r)] = w;

        const Unit& unit = units[u];
        int count = int(std::count(unit.uses.begin(), unit.uses.end(), r));

        if (constructorStore[u] && tableOperand(insns[unit.first]) == r)
            --count;

        webs[w].useCount += count;
        if (count > 0)
            webs[w].useUnits.push_back(u);
    }
}

} // namespace Luau::Decompiler
