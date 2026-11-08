#include "Decompiler/FunctionBuilder.h"

#include "luauDec/Decompile.h"

#include "Luau/BytecodeUtils.h"

#include <algorithm>
#include <cstring>

namespace Luau::Decompiler
{

namespace
{

bool isBoolConstant(AstExpr* e, bool value)
{
    auto b = e->as<AstExprConstantBool>();
    return b && b->value == value;
}

bool isComparison(AstExprBinary::Op op)
{
    switch (op)
    {
    case AstExprBinary::CompareEq:
    case AstExprBinary::CompareNe:
    case AstExprBinary::CompareLt:
    case AstExprBinary::CompareLe:
    case AstExprBinary::CompareGt:
    case AstExprBinary::CompareGe:
        return true;
    default:
        return false;
    }
}

bool isBooleanExpr(AstExpr* e)
{
    if (e->is<AstExprConstantBool>())
        return true;
    if (auto u = e->as<AstExprUnary>())
        return u->op == AstExprUnary::Op::Not;
    if (auto g = e->as<AstExprGroup>())
        return isBooleanExpr(g->expr);
    if (auto b = e->as<AstExprBinary>())
    {
        if (isComparison(b->op))
            return true;
        if (b->op == AstExprBinary::And || b->op == AstExprBinary::Or)
            return isBooleanExpr(b->left) && isBooleanExpr(b->right);
    }
    return false;
}

bool astEqual(AstExpr* a, AstExpr* b)
{
    if (a == b)
        return true;
    if (a->classIndex != b->classIndex)
        return false;

    if (a->is<AstExprConstantNil>() || a->is<AstExprVarargs>())
        return true;
    if (auto x = a->as<AstExprConstantBool>())
        return x->value == b->as<AstExprConstantBool>()->value;
    if (auto x = a->as<AstExprConstantNumber>())
        return x->value == b->as<AstExprConstantNumber>()->value;
    if (auto x = a->as<AstExprConstantString>())
    {
        auto y = b->as<AstExprConstantString>();
        return x->value.size == y->value.size && memcmp(x->value.data, y->value.data, x->value.size) == 0;
    }
    if (auto x = a->as<AstExprLocal>())
        return x->local == b->as<AstExprLocal>()->local;
    if (auto x = a->as<AstExprGlobal>())
        return strcmp(x->name.value, b->as<AstExprGlobal>()->name.value) == 0;
    if (auto x = a->as<AstExprIndexName>())
    {
        auto y = b->as<AstExprIndexName>();
        return x->op == y->op && strcmp(x->index.value, y->index.value) == 0 && astEqual(x->expr, y->expr);
    }
    if (auto x = a->as<AstExprIndexExpr>())
    {
        auto y = b->as<AstExprIndexExpr>();
        return astEqual(x->expr, y->expr) && astEqual(x->index, y->index);
    }
    if (auto x = a->as<AstExprGroup>())
        return astEqual(x->expr, b->as<AstExprGroup>()->expr);
    if (auto x = a->as<AstExprUnary>())
    {
        auto y = b->as<AstExprUnary>();
        return x->op == y->op && astEqual(x->expr, y->expr);
    }
    if (auto x = a->as<AstExprBinary>())
    {
        auto y = b->as<AstExprBinary>();
        return x->op == y->op && astEqual(x->left, y->left) && astEqual(x->right, y->right);
    }
    if (auto x = a->as<AstExprCall>())
    {
        auto y = b->as<AstExprCall>();
        if (x->self != y->self || x->args.size != y->args.size || !astEqual(x->func, y->func))
            return false;
        for (size_t i = 0; i < x->args.size; ++i)
            if (!astEqual(x->args.data[i], y->args.data[i]))
                return false;
        return true;
    }
    return false;
}

bool isNameChain(AstExpr* e)
{
    if (e->is<AstExprLocal>() || e->is<AstExprGlobal>())
        return true;
    if (auto i = e->as<AstExprIndexName>())
        return i->op == '.' && isNameChain(i->expr);
    return false;
}

bool isSimpleLValue(AstExpr* e)
{
    if (e->is<AstExprLocal>() || e->is<AstExprGlobal>())
        return true;
    if (auto i = e->as<AstExprIndexName>())
        return isSimpleLValue(i->expr);
    if (auto i = e->as<AstExprIndexExpr>())
        return isSimpleLValue(i->expr) && (i->index->is<AstExprConstantNumber>() || i->index->is<AstExprConstantString>() ||
                                              i->index->is<AstExprLocal>());
    return false;
}

const LocalVarInfo* paramDebugInfo(const FunctionProto& proto, uint8_t reg)
{
    for (const LocalVarInfo& local : proto.locals)
        if (local.reg == reg && local.startpc == 0)
            return &local;
    return nullptr;
}

} // namespace

// Without debug info `function T:m()` and `function T.m(self)` are indistinguishable; call the first parameter `self` when the
// closure is stored straight into a table field and indexes its first parameter like an object.
bool FunctionBuilder::looksLikeMethod(const FunctionProto& child, uint32_t next, uint8_t target) const
{
    if (child.numParams == 0 || child.hasDebugInfo || next >= flow.insns.size())
        return false;

    const Insn& store = flow.insns[next];
    if ((store.op != LOP_SETTABLEKS && store.op != LOP_SETUDATAKS) || LUAU_INSN_A(store.word) != target)
        return false;

    for (size_t pc = 0; pc < child.code.size();)
    {
        uint32_t insn = child.code[pc];
        LuauOpcode op = LuauOpcode(LUAU_INSN_OP(insn));
        if (op >= LOP__COUNT)
            return false;
        switch (op)
        {
        case LOP_GETTABLEKS:
        case LOP_SETTABLEKS:
        case LOP_NAMECALL:
        case LOP_GETTABLE:
        case LOP_SETTABLE:
            if (LUAU_INSN_B(insn) == 0)
                return true;
            break;
        default:
            break;
        }
        pc += getOpLength(op);
    }
    return false;
}

FunctionBuilder::FunctionBuilder(DecompileContext& ctx, const FunctionProto& proto, std::vector<AstLocal*> upvalues, size_t depth)
    : ctx(ctx)
    , proto(proto)
    , flow(proto)
    , upvalues(std::move(upvalues))
    , depth(depth)
{
    webLocals.assign(flow.webs.size(), nullptr);
    sharedLocals.insert(this->upvalues.begin(), this->upvalues.end());
    backJumps.resize(flow.units.size());

    for (uint32_t u = 0; u < flow.units.size(); ++u)
    {
        const Unit& unit = flow.units[u];
        if (unit.region >= 0)
            continue;
        const Insn& insn = flow.insns[unit.first];
        if ((insn.op == LOP_JUMPBACK || insn.op == LOP_JUMP || insn.op == LOP_JUMPX) && insn.target <= int(insn.pc))
            backJumps[flow.unitAtPc(insn.target)].push_back(u);
    }
}

AstName FunctionBuilder::name(const std::string& text)
{
    char* storage = static_cast<char*>(ctx.allocator.allocate(text.size() + 1));
    memcpy(storage, text.c_str(), text.size() + 1);
    return AstName(storage);
}

// Luau requires `return`, `break` and `continue` to end their block. Nothing can jump into the middle of a structured block,
// so statements after one of them never run; they appear when a shared exit is both repeated at a jump site and emitted
// where it was in the bytecode.
static void dropUnreachable(std::vector<AstStat*>& body)
{
    for (size_t i = 0; i < body.size(); ++i)
        if (body[i]->is<AstStatReturn>() || body[i]->is<AstStatBreak>() || body[i]->is<AstStatContinue>())
        {
            body.resize(i + 1);
            return;
        }
}

AstStatBlock* FunctionBuilder::buildBody(std::vector<AstLocal*>& params)
{
    std::vector<AstStat*> body;
    blocks.push_back(&body);

    for (uint8_t r = 0; r < proto.numParams; ++r)
    {
        const LocalVarInfo* info = paramDebugInfo(proto, r);
        std::string fallback = r == 0 && selfParameter ? "self" : "arg" + std::to_string(selfParameter ? r : r + 1);
        const char* paramName = ctx.namer.declare(info ? info->name : fallback);
        AstLocal* local = make<AstLocal>(AstName(paramName), Location(), nullptr, depth, 0, nullptr);
        params.push_back(local);
        if (flow.paramWebs[r] >= 0)
            webLocals[flow.paramWebs[r]] = local;
        if (flow.paramWebs[r] >= 0 && flow.webs[flow.paramWebs[r]].captured)
            sharedLocals.insert(local);

        Slot& slot = state.slots[r];
        slot.kind = Slot::Kind::Var;
        slot.local = local;
        slot.expr = make<AstExprLocal>(Location(), local, false);
    }

    LoopContext top;
    emitRange(0, uint32_t(flow.units.size()), top);
    flushPending(true);
    dropUnreachable(body);

    if (!body.empty())
        if (auto ret = body.back()->as<AstStatReturn>(); ret && ret->list.size == 0)
            body.pop_back();

    blocks.pop_back();
    return make<AstStatBlock>(Location(), array(body));
}

// ---------------------------------------------------------------- statements

void FunctionBuilder::emitStatement(AstStat* stat)
{
    if (pureDepth > 0)
        throw PureViolation{};

    flushPending(false);
    blocks.back()->push_back(stat);
    ++statementCount;
}

void FunctionBuilder::emitAssign(AstExpr* target, AstExpr* value)
{
    if (auto bin = value->as<AstExprBinary>())
    {
        switch (bin->op)
        {
        case AstExprBinary::Add:
        case AstExprBinary::Sub:
        case AstExprBinary::Mul:
        case AstExprBinary::Div:
        case AstExprBinary::FloorDiv:
        case AstExprBinary::Mod:
        case AstExprBinary::Pow:
        case AstExprBinary::Concat:
            if (isSimpleLValue(target) && astEqual(bin->left, target))
            {
                emitStatement(make<AstStatCompoundAssign>(Location(), bin->op, target, bin->right));
                return;
            }
            break;
        default:
            break;
        }
    }

    emitStatement(make<AstStatAssign>(Location(), array<AstExpr*>({target}), array<AstExpr*>({value})));
}

void FunctionBuilder::storeTable(int unit, uint8_t tableReg, AstExpr* table, AstExpr* key, AstExpr* value, const char* fieldName)
{
    AstExpr* target = fieldName ? field(table, fieldName) : make<AstExprIndexExpr>(Location(), table, key);

    if (auto func = value->as<AstExprFunction>(); func && fieldName && isNameChain(table) && isValidIdentifier(fieldName))
    {
        if (func->args.size > 0 && func->args.data[0]->name == "self")
        {
            func->self = func->args.data[0];
            func->args.data++;
            func->args.size--;
            target = make<AstExprIndexName>(Location(), table, name(fieldName), Location(), Position(0, 0), ':');
        }
        emitStatement(make<AstStatFunction>(Location(), target, func));
        return;
    }

    (void)unit;
    (void)tableReg;
    emitAssign(target, value);
}

// ---------------------------------------------------------------- registers

bool FunctionBuilder::isPure(AstExpr* e) const
{
    if (e->is<AstExprConstantNil>() || e->is<AstExprConstantBool>() || e->is<AstExprConstantNumber>() || e->is<AstExprConstantString>() ||
        e->is<AstExprConstantInteger>() || e->is<AstExprVarargs>() || e->is<AstExprFunction>())
        return true;
    if (auto g = e->as<AstExprGroup>())
        return isPure(g->expr);
    if (auto u = e->as<AstExprUnary>())
        return u->op != AstExprUnary::Op::Len && isPure(u->expr);
    if (auto b = e->as<AstExprBinary>())
        return isPure(b->left) && isPure(b->right);
    if (auto t = e->as<AstExprTable>())
    {
        for (const AstExprTable::Item& item : t->items)
            if ((item.key && !isPure(item.key)) || !isPure(item.value))
                return false;
        return true;
    }
    if (auto i = e->as<AstExprIfElse>())
        return isPure(i->condition) && isPure(i->trueExpr) && isPure(i->falseExpr);
    if (auto c = e->as<AstExprCall>())
    {
        // vector constants
        if (auto f = c->func->as<AstExprIndexName>(); f && f->expr->is<AstExprGlobal>() && f->expr->as<AstExprGlobal>()->name == "vector" &&
                                                         f->index == "create")
        {
            for (AstExpr* arg : c->args)
                if (!isPure(arg))
                    return false;
            return true;
        }
    }
    return false;
}

bool FunctionBuilder::isMovable(AstExpr* e) const
{
    if (e->is<AstExprGlobal>())
        return true;
    // within one expression nothing assigns a local, unless a call does it through a closure that shares it
    if (auto l = e->as<AstExprLocal>())
        return sharedLocals.count(l->local) == 0;
    if (auto i = e->as<AstExprIndexName>(); i && i->op == '.' && i->expr->is<AstExprGlobal>())
        return true;
    return isPure(e);
}

bool FunctionBuilder::mustBeVariable(int web, AstExpr* expr) const
{
    const Web& w = flow.webs[web];
    if (w.debugLocal >= 0 || w.captured || w.defCount > 1 || w.useCount > 1)
        return true;

    if (auto func = expr->as<AstExprFunction>(); func && func->debugname.value && *func->debugname.value && w.useUnits.size() == 1)
    {
        const Unit& use = flow.units[w.useUnits[0]];
        if (use.region >= 0)
            return true;
        LuauOpcode op = flow.insns[use.first].op;
        return op != LOP_SETGLOBAL && op != LOP_SETTABLEKS && op != LOP_SETUDATAKS;
    }

    return false;
}

AstLocal* FunctionBuilder::declareLocal(int web, const std::string& suggestion)
{
    std::string base = suggestion;
    if (web >= 0 && flow.webs[web].debugLocal >= 0)
        base = proto.locals[flow.webs[web].debugLocal].name;

    AstLocal* local = make<AstLocal>(AstName(ctx.namer.declare(base)), Location(), nullptr, depth, 0, nullptr);
    if (web >= 0)
    {
        webLocals[web] = local;
        if (flow.webs[web].captured)
            sharedLocals.insert(local);
    }
    return local;
}

void FunctionBuilder::define(int unit, uint8_t reg, AstExpr* expr, bool truncated, bool multret)
{
    retire(reg);

    Slot slot;
    slot.expr = expr;
    slot.truncated = truncated;
    slot.multret = multret;
    slot.table = expr->as<AstExprTable>();

    int web = regionDepth == 0 && unit >= 0 ? flow.defWeb(uint32_t(unit), reg) : -1;

    if (web >= 0 && webLocals[web])
    {
        AstLocal* local = webLocals[web];
        if (auto l = expr->as<AstExprLocal>(); !l || l->local != local)
            emitAssign(make<AstExprLocal>(Location(), local, false), expr);

        slot.kind = Slot::Kind::Var;
        slot.local = local;
        slot.web = web;
        slot.table = nullptr;
        state.slots[reg] = slot;
        return;
    }

    if (web >= 0 && mustBeVariable(web, expr))
    {
        if (multret)
            throw DecompileError("open value list stored in a local");

        if (pureDepth > 0)
            throw PureViolation{};
        AstLocal* local = declareLocal(web, suggestName(expr));
        if (auto func = expr->as<AstExprFunction>())
            emitStatement(make<AstStatLocalFunction>(Location(), local, func, false, Position::missing()));
        else if (expr->is<AstExprConstantNil>())
            emitStatement(make<AstStatLocal>(Location(), array<AstLocal*>({local}), AstArray<AstExpr*>{}, std::nullopt));
        else
            emitStatement(make<AstStatLocal>(Location(), array<AstLocal*>({local}), array<AstExpr*>({expr}), Location()));

        slot.kind = Slot::Kind::Var;
        slot.local = local;
        slot.web = web;
        slot.tableMark = statementCount;
        state.slots[reg] = slot;
        return;
    }

    slot.kind = Slot::Kind::Pending;
    slot.web = web;
    slot.seq = nextSeq++;
    state.slots[reg] = slot;
}

void FunctionBuilder::defineGroup(int unit, uint8_t first, uint8_t count, AstExpr* expr)
{
    bool variable = false;
    if (regionDepth == 0 && unit >= 0)
        for (uint8_t i = 0; i < count; ++i)
        {
            int web = flow.defWeb(uint32_t(unit), uint8_t(first + i));
            if (web >= 0 && (webLocals[web] || mustBeVariable(web, expr)))
                variable = true;
        }

    for (uint8_t i = 0; i < count; ++i)
        retire(uint8_t(first + i));

    int group = int(groups.size());
    groups.push_back({expr, first, count});

    uint64_t seq = nextSeq++;
    for (uint8_t i = 0; i < count; ++i)
    {
        Slot slot;
        slot.kind = Slot::Kind::Pending;
        slot.expr = expr;
        slot.group = group;
        slot.seq = seq;
        slot.web = regionDepth == 0 && unit >= 0 ? flow.defWeb(uint32_t(unit), uint8_t(first + i)) : -1;
        state.slots[first + i] = slot;
    }

    if (variable)
        materialize(first);
}

void FunctionBuilder::materialize(uint8_t reg)
{
    if (pureDepth > 0)
        throw PureViolation{};

    Slot& slot = state.slots[reg];
    if (slot.kind != Slot::Kind::Pending)
        return;
    if (slot.multret)
        throw DecompileError("cannot name an open value list");

    if (slot.group >= 0)
    {
        const Group& group = groups[slot.group];
        int id = slot.group;
        std::vector<AstLocal*> vars;
        std::vector<AstStat*> assigns;

        for (uint8_t i = 0; i < group.count; ++i)
        {
            Slot& member = state.slots[group.first + i];
            int web = member.kind == Slot::Kind::Pending && member.group == id ? member.web : -1;

            if (web >= 0 && webLocals[web])
            {
                // already a variable: declare a temporary and assign it afterwards
                AstLocal* temp = declareLocal(-1, "");
                vars.push_back(temp);
                assigns.push_back(make<AstStatAssign>(
                    Location(), array<AstExpr*>({make<AstExprLocal>(Location(), webLocals[web], false)}),
                    array<AstExpr*>({make<AstExprLocal>(Location(), temp, false)})
                ));
                member.kind = Slot::Kind::Var;
                member.local = webLocals[web];
                continue;
            }

            AstLocal* local = declareLocal(web, "");
            vars.push_back(local);
            if (member.kind == Slot::Kind::Pending && member.group == id)
            {
                member.kind = Slot::Kind::Var;
                member.local = local;
                member.group = -1;
            }
        }

        blocks.back()->push_back(make<AstStatLocal>(Location(), array(vars), array<AstExpr*>({group.expr}), Location()));
        ++statementCount;
        for (AstStat* assign : assigns)
        {
            blocks.back()->push_back(assign);
            ++statementCount;
        }
        return;
    }

    AstLocal* local = declareLocal(slot.web, suggestName(slot.expr));
    AstStat* stat;
    if (auto func = slot.expr->as<AstExprFunction>())
        stat = make<AstStatLocalFunction>(Location(), local, func, false, Position::missing());
    else
        stat = make<AstStatLocal>(Location(), array<AstLocal*>({local}), array<AstExpr*>({slot.expr}), Location());

    blocks.back()->push_back(stat);
    ++statementCount;

    slot.kind = Slot::Kind::Var;
    slot.local = local;
    slot.tableMark = statementCount;
}

void FunctionBuilder::flushPending(bool everything)
{
    std::vector<std::pair<uint64_t, int>> pending;
    for (int r = 0; r < 256; ++r)
    {
        const Slot& slot = state.slots[r];
        if (slot.kind != Slot::Kind::Pending)
            continue;

        bool unused = slot.web >= 0 && flow.webs[slot.web].useCount == 0;
        if (everything || unused || !isPure(slot.expr))
            pending.push_back({slot.seq, r});
    }

    std::sort(pending.begin(), pending.end());
    for (auto [seq, r] : pending)
        flushSlot(uint8_t(r), false);
}

// Emits a pending value: a call whose result is never read as a statement, anything else as a local.
void FunctionBuilder::flushSlot(uint8_t r, bool overwritten)
{
    Slot& slot = state.slots[r];
    if (slot.kind != Slot::Kind::Pending)
        return;

    bool unused = slot.group < 0 && (overwritten || (slot.web >= 0 && flow.webs[slot.web].useCount == 0));
    bool named = slot.web >= 0 && flow.webs[slot.web].debugLocal >= 0;
    if (unused && slot.expr->is<AstExprCall>() && !named)
    {
        // a call whose result is never read: keep it as a plain statement
        if (pureDepth > 0)
            throw PureViolation{};
        blocks.back()->push_back(make<AstStatExpr>(Location(), slot.expr));
        ++statementCount;
        slot = Slot{};
        return;
    }

    if (slot.multret && unused)
    {
        if (pureDepth > 0)
            throw PureViolation{};
        slot = Slot{};
        return;
    }

    materialize(r);
}

// The register of a pending value is about to be written without the value having been read. A value whose evaluation
// matters (a call, an index that may run a metamethod) still runs, in order: emit it, after anything pending before it.
void FunctionBuilder::retire(uint8_t reg)
{
    const Slot& target = state.slots[reg];
    if (target.kind != Slot::Kind::Pending || isPure(target.expr))
        return;
    if (pureDepth > 0)
        throw PureViolation{};

    std::vector<std::pair<uint64_t, int>> earlier;
    for (int r = 0; r < 256; ++r)
    {
        const Slot& s = state.slots[r];
        if (r != reg && s.kind == Slot::Kind::Pending && !isPure(s.expr) && s.seq < target.seq)
            earlier.push_back({s.seq, r});
    }
    std::sort(earlier.begin(), earlier.end());
    for (auto [seq, r] : earlier)
        flushSlot(uint8_t(r), false);

    flushSlot(reg, true);
}

std::vector<AstExpr*> FunctionBuilder::consume(const std::vector<uint8_t>& regs, int unit, bool lastIsMultiValue)
{
    auto boundLocal = [&](uint8_t reg) -> AstLocal* {
        if (regionDepth == 0 && unit >= 0)
        {
            int w = flow.useWeb(uint32_t(unit), reg);
            if (w >= 0 && webLocals[w])
                return webLocals[w];
        }
        const Slot& s = state.slots[reg];
        return s.kind == Slot::Kind::Var ? s.local : nullptr;
    };

    auto ordered = [&](const Slot& s) {
        return (s.kind == Slot::Kind::Pending && !isMovable(s.expr)) || (s.kind == Slot::Kind::Method && !s.methodPure);
    };

    for (int attempt = 0;; ++attempt)
    {
        bool ok = true;
        uint64_t last = 0, minSeq = UINT64_MAX, maxSeq = 0;
        std::vector<int> consumedGroups;
        std::vector<bool> inOperands(256, false);

        for (uint8_t reg : regs)
        {
            inOperands[reg] = true;
            if (boundLocal(reg))
                continue;
            const Slot& s = state.slots[reg];
            if (!ordered(s))
                continue;
            if (s.group >= 0)
            {
                if (std::find(consumedGroups.begin(), consumedGroups.end(), s.group) != consumedGroups.end())
                    continue;
                consumedGroups.push_back(s.group);
            }
            if (s.seq <= last)
                ok = false;
            last = s.seq;
            minSeq = std::min(minSeq, s.seq);
            maxSeq = std::max(maxSeq, s.seq);
        }

        if (ok && minSeq != UINT64_MAX)
            for (int r = 0; r < 256; ++r)
            {
                const Slot& s = state.slots[r];
                if (inOperands[r] || !ordered(s) || s.seq <= minSeq)
                    continue;
                if (s.group >= 0 && std::find(consumedGroups.begin(), consumedGroups.end(), s.group) != consumedGroups.end())
                    continue;
                ok = false;
            }

        if (ok)
            break;
        if (attempt > 0)
            throw DecompileError("cannot preserve evaluation order");

        std::vector<std::pair<uint64_t, int>> victims;
        for (int r = 0; r < 256; ++r)
        {
            const Slot& s = state.slots[r];
            if (ordered(s) && s.seq <= maxSeq)
            {
                if (s.kind == Slot::Kind::Method)
                    throw DecompileError("cannot preserve evaluation order around a method call");
                victims.push_back({s.seq, r});
            }
        }
        std::sort(victims.begin(), victims.end());
        for (auto [seq, r] : victims)
            materialize(uint8_t(r));
    }

    std::vector<AstExpr*> result;
    for (size_t i = 0; i < regs.size(); ++i)
    {
        uint8_t reg = regs[i];
        bool last = i + 1 == regs.size();

        if (AstLocal* local = boundLocal(reg))
        {
            result.push_back(make<AstExprLocal>(Location(), local, false));
            continue;
        }

        Slot& s = state.slots[reg];
        switch (s.kind)
        {
        case Slot::Kind::Pending:
        {
            if (s.group >= 0)
            {
                materialize(reg);
                result.push_back(make<AstExprLocal>(Location(), s.local, false));
                break;
            }
            if (s.multret && !last)
                throw DecompileError("open value list in the middle of an expression list");

            AstExpr* expr = s.expr;
            if (regionDepth > 0 && s.seq < regionSeqStart)
            {
                if (++regionConsumed[expr] > 1 && !isPure(expr))
                    throw DecompileError("short-circuit expression would evaluate a value twice");
            }
            if (s.truncated && last && lastIsMultiValue)
                expr = make<AstExprGroup>(Location(), expr);
            result.push_back(expr);
            s = Slot{};
            break;
        }
        case Slot::Kind::Method:
            result.push_back(s.expr);
            s = Slot{};
            state.slots[(reg + 1) & 0xff] = Slot{};
            break;
        case Slot::Kind::Var:
            result.push_back(make<AstExprLocal>(Location(), s.local, false));
            break;
        case Slot::Kind::Empty:
            throw DecompileError("read of undefined register R" + std::to_string(reg));
        }
    }
    return result;
}

AstExpr* FunctionBuilder::consumeOne(uint8_t reg, int unit)
{
    return consume({reg}, unit, false)[0];
}

void FunctionBuilder::hoist(uint32_t begin, uint32_t end)
{
    for (size_t w = 0; w < flow.webs.size(); ++w)
    {
        const Web& web = flow.webs[w];
        if (webLocals[w] || web.loopVariable)
            continue;

        bool defInside = false, outside = false;
        for (uint32_t u : web.defUnits)
        {
            if (u >= begin && u < end)
                defInside = true;
            else
                outside = true;
        }
        if (!defInside)
            continue;
        for (uint32_t u : web.useUnits)
            if (u < begin || u >= end)
                outside = true;
        if (!outside)
            continue;

        if (pureDepth > 0)
            throw PureViolation{};
        AstLocal* local = declareLocal(int(w), "");
        emitStatement(make<AstStatLocal>(Location(), array<AstLocal*>({local}), AstArray<AstExpr*>{}, std::nullopt));
    }
}

// ---------------------------------------------------------------- expressions

const std::string& FunctionBuilder::constantString(uint32_t index)
{
    if (index >= proto.constants.size() || proto.constants[index].kind != Constant::Kind::String)
        throw DecompileError("expected a string constant at K" + std::to_string(index));
    return proto.constants[index].string;
}

AstExpr* FunctionBuilder::field(AstExpr* object, const std::string& key)
{
    if (isValidIdentifier(key))
        return make<AstExprIndexName>(Location(), object, name(key), Location(), Position(0, 0), '.');

    char* data = static_cast<char*>(ctx.allocator.allocate(key.size() + 1));
    memcpy(data, key.data(), key.size());
    AstExpr* index = make<AstExprConstantString>(Location(), AstArray<char>{data, key.size()}, AstExprConstantString::QuoteStyle::QuotedSimple);
    return make<AstExprIndexExpr>(Location(), object, index);
}

AstExpr* FunctionBuilder::importExpr(uint32_t aux)
{
    uint32_t count = aux >> 30;
    uint32_t ids[3] = {(aux >> 20) & 1023, (aux >> 10) & 1023, aux & 1023};
    if (count == 0)
        throw DecompileError("empty import path");

    AstExpr* expr = make<AstExprGlobal>(Location(), name(constantString(ids[0])));
    for (uint32_t i = 1; i < count; ++i)
        expr = field(expr, constantString(ids[i]));
    return expr;
}

AstExpr* FunctionBuilder::constant(uint32_t index)
{
    if (index >= proto.constants.size())
        throw DecompileError("constant index out of range");

    const Constant& k = proto.constants[index];
    switch (k.kind)
    {
    case Constant::Kind::Nil:
        return make<AstExprConstantNil>(Location());
    case Constant::Kind::Boolean:
        return make<AstExprConstantBool>(Location(), k.boolean);
    case Constant::Kind::Number:
        return make<AstExprConstantNumber>(Location(), k.number);
    case Constant::Kind::Integer:
        return make<AstExprConstantInteger>(Location(), k.integer);
    case Constant::Kind::String:
    {
        char* data = static_cast<char*>(ctx.allocator.allocate(k.string.size() + 1));
        memcpy(data, k.string.data(), k.string.size());
        return make<AstExprConstantString>(Location(), AstArray<char>{data, k.string.size()}, AstExprConstantString::QuoteStyle::QuotedSimple);
    }
    case Constant::Kind::Vector:
    {
        AstExpr* ctor = field(make<AstExprGlobal>(Location(), name("vector")), "create");
        std::vector<AstExpr*> args;
        int components = k.vector[3] != 0 ? 4 : 3;
        for (int i = 0; i < components; ++i)
            args.push_back(make<AstExprConstantNumber>(Location(), k.vector[i]));
        return make<AstExprCall>(Location(), ctor, array(args), false, false, AstArray<AstTypeOrPack>{}, Location());
    }
    case Constant::Kind::Import:
    {
        AstExpr* expr = make<AstExprGlobal>(Location(), name(k.importPath.at(0)));
        for (size_t i = 1; i < k.importPath.size(); ++i)
            expr = field(expr, k.importPath[i]);
        return expr;
    }
    default:
        throw DecompileError("unsupported constant kind in expression");
    }
}

AstExpr* FunctionBuilder::closure(
    uint32_t insnIndex,
    const FunctionProto& child,
    uint32_t& next,
    uint8_t target,
    AstLocal*& selfLocal,
    bool& selfDeclared
)
{
    selfLocal = nullptr;
    selfDeclared = false;
    std::vector<AstLocal*> captured;

    uint32_t j = insnIndex + 1;
    for (; j < flow.insns.size() && flow.insns[j].op == LOP_CAPTURE; ++j)
    {
        const Insn& cap = flow.insns[j];
        uint8_t type = LUAU_INSN_A(cap.word), reg = LUAU_INSN_B(cap.word);

        if (type == LCT_UPVAL)
        {
            if (reg >= upvalues.size())
                throw DecompileError("capture of unknown upvalue");
            captured.push_back(upvalues[reg]);
            continue;
        }

        if (reg == target)
        {
            int web = regionDepth == 0 ? flow.defWeb(flow.unitOfInsn[insnIndex], target) : -1;
            if (pureDepth > 0)
                throw PureViolation{};
            selfDeclared = !(web >= 0 && webLocals[web]);
            selfLocal = selfDeclared ? declareLocal(web, child.debugName.value_or("fn")) : webLocals[web];
            captured.push_back(selfLocal);
            continue;
        }

        AstLocal* local = nullptr;
        if (regionDepth == 0)
        {
            int w = flow.useWeb(flow.unitOfInsn[j], reg);
            if (w >= 0)
                local = webLocals[w];
        }
        if (!local)
        {
            if (state.slots[reg].kind == Slot::Kind::Pending)
                materialize(reg);
            if (state.slots[reg].kind != Slot::Kind::Var)
                throw DecompileError("capture of an undefined register");
            local = state.slots[reg].local;
        }
        captured.push_back(local);
    }
    next = j;

    std::vector<AstLocal*> params;
    AstStatBlock* body;

    ctx.namer.pushScope();
    try
    {
        FunctionBuilder builder(ctx, child, captured, depth + 1);
        builder.selfParameter = looksLikeMethod(child, next, target);
        body = builder.buildBody(params);
    }
    catch (const DecompileError& e)
    {
        params.clear();
        ctx.namer.popScope();
        ctx.namer.pushScope();
        for (uint8_t i = 0; i < child.numParams; ++i)
            params.push_back(make<AstLocal>(AstName(ctx.namer.declare("arg" + std::to_string(i + 1))), Location(), nullptr, depth + 1, 0, nullptr));
        ctx.comments.push_back(std::string("luauDec: failed to decompile function: ") + e.what());
        AstStat* error = make<AstStatError>(Location(), AstArray<AstExpr*>{}, AstArray<AstStat*>{}, unsigned(ctx.comments.size() - 1));
        body = make<AstStatBlock>(Location(), array<AstStat*>({error}));
    }
    ctx.namer.popScope();

    return make<AstExprFunction>(
        Location(), AstArray<AstAttr*>{}, AstArray<AstGenericType*>{}, AstArray<AstGenericTypePack*>{}, nullptr, array(params), child.isVararg,
        Location(), body, depth + 1, name(child.debugName.value_or("")), nullptr
    );
}

AstExpr* FunctionBuilder::makeIfElse(AstExpr* cond, AstExpr* ifTrue, AstExpr* ifFalse)
{
    if (auto u = cond->as<AstExprUnary>(); u && u->op == AstExprUnary::Op::Not)
        return makeIfElse(u->expr, ifFalse, ifTrue);

    bool boolean = isBooleanExpr(cond);

    if (isBoolConstant(ifTrue, true) && isBoolConstant(ifFalse, false) && boolean)
        return cond;
    if (isBoolConstant(ifTrue, false) && isBoolConstant(ifFalse, true))
        return make<AstExprUnary>(Location(), AstExprUnary::Op::Not, cond);
    if (isBoolConstant(ifFalse, false) && boolean)
        return make<AstExprBinary>(Location(), AstExprBinary::And, cond, ifTrue);
    if (isBoolConstant(ifTrue, true) && boolean)
        return make<AstExprBinary>(Location(), AstExprBinary::Or, cond, ifFalse);

    // `c and x or y` compiles to: if c then (x or y) else y
    if (auto orExpr = ifTrue->as<AstExprBinary>(); orExpr && orExpr->op == AstExprBinary::Or && astEqual(orExpr->right, ifFalse))
        return make<AstExprBinary>(
            Location(), AstExprBinary::Or, make<AstExprBinary>(Location(), AstExprBinary::And, cond, orExpr->left), ifFalse
        );

    if (auto inner = ifTrue->as<AstExprIfElse>(); inner && astEqual(inner->falseExpr, ifFalse))
        return makeIfElse(make<AstExprBinary>(Location(), AstExprBinary::And, cond, inner->condition), inner->trueExpr, ifFalse);

    return make<AstExprIfElse>(Location(), cond, true, ifTrue, true, ifFalse);
}

// ---------------------------------------------------------------- value regions

AstExpr* FunctionBuilder::buildRegion(const ValueRegion& region)
{
    // the registers the region reads: a region that is a flow unit has its upward-exposed uses; a region nested in
    // another one (built as statements) is not a unit, so take what its instructions read
    uint32_t unit = flow.unitOfInsn[region.begin];
    bool isUnit = flow.units[unit].region >= 0 && flow.units[unit].first == region.begin;
    std::vector<uint8_t> inputs;
    if (isUnit)
        inputs = flow.units[unit].uses;
    else
        for (uint32_t k = region.begin; k < region.end; ++k)
            inputs.insert(inputs.end(), flow.insns[k].uses.begin(), flow.insns[k].uses.end());

    if (regionDepth == 0 && isUnit)
    {
        // instructions inside the region read registers, not def-use webs: point inputs that are named variables at
        // them (a variable assigned in both arms of an earlier `if` is not in its register's slot after the `if`)
        for (uint8_t r : inputs)
        {
            int w = flow.useWeb(unit, r);
            Slot& s = state.slots[r];
            if (w >= 0 && webLocals[w] && !(s.kind == Slot::Kind::Var && s.local == webLocals[w]))
            {
                s = Slot{};
                s.kind = Slot::Kind::Var;
                s.local = webLocals[w];
                s.web = w;
            }
        }
    }

    if (pureDepth == 0)
    {
        // the region reads its inputs after everything still pending; name inputs that were computed earlier than that
        uint64_t newest = 0;
        for (const Slot& s : state.slots)
            if (s.kind == Slot::Kind::Pending && !isMovable(s.expr))
                newest = std::max(newest, s.seq);

        uint64_t limit = 0;
        for (uint8_t r : inputs)
        {
            const Slot& s = state.slots[r];
            if (s.kind == Slot::Kind::Pending && !isMovable(s.expr) && s.seq < newest)
                limit = std::max(limit, s.seq);
        }

        std::vector<std::pair<uint64_t, int>> victims;
        for (int r = 0; r < 256; ++r)
            if (state.slots[r].kind == Slot::Kind::Pending && !isMovable(state.slots[r].expr) && state.slots[r].seq <= limit)
                victims.push_back({state.slots[r].seq, r});
        std::sort(victims.begin(), victims.end());
        for (auto [seq, r] : victims)
            materialize(uint8_t(r));

        // a read of one value of a multi-value call (`local ok, v = pcall(...)`) names the whole group with a statement,
        // which the region cannot hold: name it before the region
        for (uint8_t r : inputs)
            if (state.slots[r].kind == Slot::Kind::Pending && state.slots[r].group >= 0)
                materialize(r);

        // only the region's first test runs on every path; a value computed before the region and read further in runs
        // on some paths only if it moves into the expression
        const std::vector<uint8_t>& firstUses = flow.insns[region.begin].uses;
        for (uint8_t r : inputs)
        {
            const Slot& s = state.slots[r];
            if (s.kind == Slot::Kind::Pending && !isMovable(s.expr) && std::find(firstUses.begin(), firstUses.end(), r) == firstUses.end())
                materialize(r);
        }
    }

    State outer = state;
    auto savedConsumed = std::move(regionConsumed);
    uint64_t savedSeqStart = regionSeqStart;
    regionConsumed.clear();
    regionSeqStart = nextSeq;

    ++regionDepth;
    ++pureDepth;

    AstExpr* result;
    try
    {
        result = walkValue(region.begin, region.end, region.reg, region.begin);
    }
    catch (const PureViolation&)
    {
        --regionDepth;
        --pureDepth;
        if (pureDepth > 0)
            throw;
        state = outer;
        regionConsumed = std::move(savedConsumed);
        regionSeqStart = savedSeqStart;
        return regionStatements(region);
    }
    catch (const DecompileError&)
    {
        --regionDepth;
        --pureDepth;
        if (pureDepth > 0)
            throw;
        state = outer;
        regionConsumed = std::move(savedConsumed);
        regionSeqStart = savedSeqStart;
        return regionStatements(region);
    }
    catch (...)
    {
        --regionDepth;
        --pureDepth;
        throw;
    }

    --regionDepth;
    --pureDepth;

    state = outer;
    for (Slot& slot : state.slots)
        if (slot.kind == Slot::Kind::Pending && regionConsumed.count(slot.expr))
            slot = Slot{};

    for (auto& [expr, count] : regionConsumed)
        savedConsumed[expr] += count;
    regionConsumed = std::move(savedConsumed);
    regionSeqStart = savedSeqStart;

    return result;
}

// A value region that one expression cannot hold (it has statements, e.g. a function inlined with its early returns):
// declare a local and assign it on every path with ordinary statements.
AstExpr* FunctionBuilder::regionStatements(const ValueRegion& region)
{
    AstLocal* result = declareLocal(-1, "");
    emitStatement(make<AstStatLocal>(Location(), array<AstLocal*>({result}), AstArray<AstExpr*>{}, std::nullopt));
    walkStatements(region.begin, region.end, region.reg, region.begin, result);
    return make<AstExprLocal>(Location(), result, false);
}

AstStatBlock* FunctionBuilder::statementPath(uint32_t begin, uint32_t end, uint8_t reg, uint32_t regionBegin, AstLocal* result)
{
    std::vector<AstStat*> body;
    blocks.push_back(&body);
    ctx.namer.pushScope();
    State saved = state;
    try
    {
        walkStatements(begin, end, reg, regionBegin, result);
    }
    catch (...)
    {
        state = saved;
        ctx.namer.popScope();
        blocks.pop_back();
        throw;
    }
    state = saved;
    ctx.namer.popScope();
    blocks.pop_back();
    dropUnreachable(body);
    return make<AstStatBlock>(Location(), array(body));
}

// walkValue's statement form: every path ends with `result = <reg>`.
void FunctionBuilder::walkStatements(uint32_t i, uint32_t end, uint8_t reg, uint32_t regionBegin, AstLocal* result)
{
    auto resultExpr = [&] {
        return make<AstExprLocal>(Location(), result, false);
    };

    for (int steps = 0;; ++steps)
    {
        if (steps > 100000)
            throw DecompileError("short-circuit expression too complex");

        if (i == end)
        {
            emitAssign(resultExpr(), consumeOne(reg, -1));
            return;
        }
        if (i > end)
            throw DecompileError("short-circuit expression escapes its region");

        const Insn& insn = flow.insns[i];

        if (isConditionalJump(insn.op))
        {
            ValueRegion inner;
            if (i != regionBegin && flow.tryRegion(i, inner) && inner.end <= end)
            {
                define(-1, inner.reg, buildRegion(inner));
                i = inner.end;
                continue;
            }

            uint32_t target = uint32_t(flow.insnAtPc[insn.target]);

            if ((insn.op == LOP_JUMPIF || insn.op == LOP_JUMPIFNOT) && LUAU_INSN_A(insn.word) == reg && target == end)
            {
                // `x or rest` / `x and rest`: result = x, then the rest only when x lets it run
                emitAssign(resultExpr(), consumeOne(reg, -1));
                AstExpr* cond = resultExpr();
                if (insn.op == LOP_JUMPIF)
                    cond = make<AstExprUnary>(Location(), AstExprUnary::Op::Not, cond);
                AstStatBlock* rest = statementPath(i + 1, end, reg, regionBegin, result);
                emitStatement(make<AstStatIf>(Location(), cond, rest, nullptr, std::nullopt, std::nullopt));
                return;
            }

            AstExpr* cond = jumpCondition(insn, -1);
            // both paths start from this state: a value still pending here would run in a branch and again before the `if`
            flushPending(false);
            AstStatBlock* taken = statementPath(target, end, reg, regionBegin, result);
            AstStatBlock* fallthrough = statementPath(i + 1, end, reg, regionBegin, result);
            if (auto u = cond->as<AstExprUnary>(); u && u->op == AstExprUnary::Op::Not)
                emitStatement(make<AstStatIf>(Location(), u->expr, fallthrough, taken, std::nullopt, std::nullopt));
            else
                emitStatement(make<AstStatIf>(Location(), cond, taken, fallthrough, std::nullopt, std::nullopt));
            return;
        }

        if (insn.op == LOP_JUMP || insn.op == LOP_JUMPX)
        {
            i = uint32_t(flow.insnAtPc[insn.target]);
            continue;
        }

        if (insn.op == LOP_LOADB && insn.target >= 0)
        {
            define(-1, LUAU_INSN_A(insn.word), make<AstExprConstantBool>(Location(), LUAU_INSN_B(insn.word) != 0));
            i = uint32_t(flow.insnAtPc[insn.target]);
            continue;
        }

        i = emitInstruction(i, -1);
    }
}

AstExpr* FunctionBuilder::walkValue(uint32_t i, uint32_t end, uint8_t reg, uint32_t regionBegin)
{
    for (int steps = 0;; ++steps)
    {
        if (steps > 100000)
            throw DecompileError("short-circuit expression too complex");

        if (i == end)
            return consumeOne(reg, -1);
        if (i > end)
            throw DecompileError("short-circuit expression escapes its region");

        const Insn& insn = flow.insns[i];

        if (isConditionalJump(insn.op))
        {
            ValueRegion inner;
            if (i != regionBegin && flow.tryRegion(i, inner) && inner.end <= end)
            {
                AstExpr* value = buildRegion(inner);
                define(-1, inner.reg, value);
                i = inner.end;
                continue;
            }

            uint32_t target = uint32_t(flow.insnAtPc[insn.target]);

            if ((insn.op == LOP_JUMPIF || insn.op == LOP_JUMPIFNOT) && LUAU_INSN_A(insn.word) == reg && target == end)
            {
                AstExpr* value = consumeOne(reg, -1);
                AstExpr* rest = walkValue(i + 1, end, reg, regionBegin);
                return make<AstExprBinary>(Location(), insn.op == LOP_JUMPIF ? AstExprBinary::Or : AstExprBinary::And, value, rest);
            }

            AstExpr* cond = jumpCondition(insn, -1);
            State branch = state;
            AstExpr* taken = walkValue(target, end, reg, regionBegin);
            state = branch;
            AstExpr* fallthrough = walkValue(i + 1, end, reg, regionBegin);
            return makeIfElse(cond, taken, fallthrough);
        }

        if (insn.op == LOP_JUMP || insn.op == LOP_JUMPX)
        {
            i = uint32_t(flow.insnAtPc[insn.target]);
            continue;
        }

        if (insn.op == LOP_LOADB && insn.target >= 0)
        {
            define(-1, LUAU_INSN_A(insn.word), make<AstExprConstantBool>(Location(), LUAU_INSN_B(insn.word) != 0));
            i = uint32_t(flow.insnAtPc[insn.target]);
            continue;
        }

        i = emitInstruction(i, -1);
    }
}

// ---------------------------------------------------------------- conditions

AstExpr* FunctionBuilder::jumpCondition(const Insn& insn, int unit)
{
    uint32_t w = insn.word;
    uint8_t a = LUAU_INSN_A(w);

    auto compare = [&](AstExprBinary::Op op, bool negate) -> AstExpr* {
        uint8_t b = uint8_t(insn.aux);
        const Slot& sa = state.slots[a];
        const Slot& sb = state.slots[b];

        // operands were evaluated right-to-left: present the mirrored comparison so source order matches evaluation order
        bool swap = sa.kind == Slot::Kind::Pending && sb.kind == Slot::Kind::Pending && sa.seq > sb.seq;
        std::vector<AstExpr*> ops = swap ? consume({b, a}, unit, false) : consume({a, b}, unit, false);
        AstExpr* left = swap ? ops[1] : ops[0];
        AstExpr* right = swap ? ops[0] : ops[1];

        AstExpr* expr;
        bool constantLeft = left->is<AstExprConstantNumber>() || left->is<AstExprConstantString>();
        if ((swap || (constantLeft && !right->is<AstExprConstantNumber>())) && (op == AstExprBinary::CompareLt || op == AstExprBinary::CompareLe))
            expr = make<AstExprBinary>(
                Location(), op == AstExprBinary::CompareLt ? AstExprBinary::CompareGt : AstExprBinary::CompareGe, right, left
            );
        else if (swap)
            expr = make<AstExprBinary>(Location(), op, right, left);
        else
            expr = make<AstExprBinary>(Location(), op, left, right);

        return negate ? make<AstExprUnary>(Location(), AstExprUnary::Op::Not, expr) : expr;
    };

    auto compareConstant = [&](AstExpr* k) -> AstExpr* {
        AstExpr* value = consumeOne(a, unit);
        bool negate = LUAU_INSN_AUX_NOT(insn.aux) != 0;
        return make<AstExprBinary>(Location(), negate ? AstExprBinary::CompareNe : AstExprBinary::CompareEq, value, k);
    };

    switch (insn.op)
    {
    case LOP_JUMPIF:
        return consumeOne(a, unit);
    case LOP_JUMPIFNOT:
        return make<AstExprUnary>(Location(), AstExprUnary::Op::Not, consumeOne(a, unit));
    case LOP_JUMPIFEQ:
        return compare(AstExprBinary::CompareEq, false);
    case LOP_JUMPIFNOTEQ:
        return compare(AstExprBinary::CompareNe, false);
    case LOP_JUMPIFLE:
        return compare(AstExprBinary::CompareLe, false);
    case LOP_JUMPIFLT:
        return compare(AstExprBinary::CompareLt, false);
    case LOP_JUMPIFNOTLE:
        return compare(AstExprBinary::CompareLe, true);
    case LOP_JUMPIFNOTLT:
        return compare(AstExprBinary::CompareLt, true);
    case LOP_JUMPXEQKNIL:
        return compareConstant(make<AstExprConstantNil>(Location()));
    case LOP_JUMPXEQKB:
        return compareConstant(make<AstExprConstantBool>(Location(), (insn.aux & 1) != 0));
    case LOP_JUMPXEQKN:
    case LOP_JUMPXEQKS:
        return compareConstant(constant(LUAU_INSN_AUX_KV(insn.aux)));
    default:
        throw DecompileError("not a conditional jump");
    }
}

static AstExpr* negateCondition(Allocator& allocator, AstExpr* e)
{
    if (auto u = e->as<AstExprUnary>(); u && u->op == AstExprUnary::Op::Not)
        return u->expr;
    if (auto b = e->as<AstExprConstantBool>())
        return allocator.alloc<AstExprConstantBool>(Location(), !b->value);
    if (auto b = e->as<AstExprBinary>())
    {
        switch (b->op)
        {
        case AstExprBinary::CompareEq:
            return allocator.alloc<AstExprBinary>(Location(), AstExprBinary::CompareNe, b->left, b->right);
        case AstExprBinary::CompareNe:
            return allocator.alloc<AstExprBinary>(Location(), AstExprBinary::CompareEq, b->left, b->right);
        case AstExprBinary::And:
            return allocator.alloc<AstExprBinary>(
                Location(), AstExprBinary::Or, negateCondition(allocator, b->left), negateCondition(allocator, b->right)
            );
        case AstExprBinary::Or:
            return allocator.alloc<AstExprBinary>(
                Location(), AstExprBinary::And, negateCondition(allocator, b->left), negateCondition(allocator, b->right)
            );
        default:
            break;
        }
    }
    return allocator.alloc<AstExprUnary>(Location(), AstExprUnary::Op::Not, e);
}

bool FunctionBuilder::reduceChain(std::vector<ChainNode> nodes, uint32_t exitTrue, uint32_t exitFalse, AstExpr*& reachTrue)
{
    auto neg = [&](AstExpr* e) {
        return negateCondition(ctx.allocator, e);
    };
    auto bin = [&](AstExprBinary::Op op, AstExpr* l, AstExpr* r) -> AstExpr* {
        return make<AstExprBinary>(Location(), op, l, r);
    };

    while (nodes.size() > 1)
    {
        bool progress = false;

        for (size_t xi = 0; xi < nodes.size() && !progress; ++xi)
        {
            ChainNode& x = nodes[xi];
            for (size_t yi = 1; yi < nodes.size() && !progress; ++yi)
            {
                if (yi == xi)
                    continue;
                ChainNode y = nodes[yi];
                if (x.taken != y.start && x.fall != y.start)
                    continue;

                int preds = 0;
                for (const ChainNode& n : nodes)
                    preds += (n.taken == y.start) + (n.fall == y.start);
                if (preds != 1)
                    continue;

                AstExpr* cond = nullptr;
                if (x.fall == y.start && x.taken == y.taken)
                    cond = bin(AstExprBinary::Or, x.cond, y.cond);
                else if (x.fall == y.start && x.taken == y.fall)
                    cond = bin(AstExprBinary::And, neg(x.cond), y.cond);
                else if (x.taken == y.start && x.fall == y.fall)
                    cond = bin(AstExprBinary::And, x.cond, y.cond);
                else if (x.taken == y.start && x.fall == y.taken)
                    cond = bin(AstExprBinary::Or, neg(x.cond), y.cond);
                else
                    continue;

                x.cond = cond;
                x.taken = y.taken;
                x.fall = y.fall;
                nodes.erase(nodes.begin() + yi);
                progress = true;
            }
        }

        if (!progress)
            return false;
    }

    const ChainNode& n = nodes[0];
    if (n.taken == exitTrue && n.fall == exitFalse)
        reachTrue = n.cond;
    else if (n.taken == exitFalse && n.fall == exitTrue)
        reachTrue = neg(n.cond);
    else
        return false;
    return true;
}

uint32_t FunctionBuilder::processUntilJump(uint32_t unit, uint32_t end)
{
    for (uint32_t v = unit; v < end;)
    {
        if (v != unit && (!flow.jumpSources[v].empty() || !backJumps[v].empty()))
            throw PureViolation{};

        const Unit& u = flow.units[v];
        if (u.region >= 0)
        {
            define(int(v), flow.regions[u.region].reg, buildRegion(flow.regions[u.region]));
            ++v;
            continue;
        }

        const Insn& insn = flow.insns[u.first];
        if (isConditionalJump(insn.op))
            return v;
        if (insn.target >= 0 || insn.op == LOP_RETURN)
            throw PureViolation{};

        uint32_t next = emitInstruction(u.first, int(v));
        v = next >= flow.insns.size() ? uint32_t(flow.units.size()) : flow.unitOfInsn[next];
    }
    throw PureViolation{};
}

std::vector<FunctionBuilder::ChainNode> FunctionBuilder::parseChain(
    uint32_t unit,
    uint32_t end,
    bool pureFirst,
    uint32_t& exitTrue,
    uint32_t& exitFalse
)
{
    struct Snapshot
    {
        State state;
        uint64_t nextSeq;
    };

    std::vector<ChainNode> nodes;
    std::vector<Snapshot> snapshots;

    auto addNode = [&](uint32_t start, uint32_t c) {
        const Insn& insn = flow.insns[flow.units[c].first];
        ++pureDepth;
        AstExpr* cond;
        try
        {
            cond = jumpCondition(insn, int(c));
        }
        catch (...)
        {
            --pureDepth;
            throw;
        }
        --pureDepth;
        nodes.push_back({start, cond, uint32_t(flow.unitAtPc(insn.target)), c + 1});
        snapshots.push_back({state, nextSeq});
    };

    uint32_t first = unit;
    if (pureFirst)
    {
        ++pureDepth;
        try
        {
            first = processUntilJump(unit, end);
        }
        catch (...)
        {
            --pureDepth;
            throw;
        }
        --pureDepth;
        addNode(unit, first);
    }
    else
    {
        // the first condition may need to name values evaluated before it
        const Insn& insn = flow.insns[flow.units[first].first];
        AstExpr* cond = jumpCondition(insn, int(first));
        nodes.push_back({unit, cond, uint32_t(flow.unitAtPc(insn.target)), first + 1});
        snapshots.push_back({state, nextSeq});
    }

    while (true)
    {
        uint32_t cur = nodes.back().fall;
        if (cur >= end || !backJumps[cur].empty())
            break;

        bool foreign = false;
        for (uint32_t src : flow.jumpSources[cur])
        {
            bool fromChain = false;
            for (const ChainNode& n : nodes)
                fromChain |= n.fall - 1 == src;
            foreign |= !fromChain;
        }
        if (foreign)
            break;

        State saved = state;
        uint64_t savedSeq = nextSeq;
        try
        {
            ++pureDepth;
            uint32_t c = processUntilJump(cur, end);
            --pureDepth;
            addNode(cur, c);
        }
        catch (const PureViolation&)
        {
            pureDepth = std::max(0, pureDepth - 1);
            state = saved;
            nextSeq = savedSeq;
            break;
        }
        catch (const DecompileError&)
        {
            pureDepth = std::max(0, pureDepth - 1);
            state = saved;
            nextSeq = savedSeq;
            break;
        }
    }

    for (size_t k = nodes.size(); k-- > 0;)
    {
        uint32_t tk = nodes[k].fall;
        std::vector<uint32_t> exits;
        for (size_t i = 0; i <= k; ++i)
            for (uint32_t t : {nodes[i].taken, nodes[i].fall})
            {
                bool internal = false;
                for (size_t j = 1; j <= k; ++j)
                    internal |= nodes[j].start == t;
                if (!internal && std::find(exits.begin(), exits.end(), t) == exits.end())
                    exits.push_back(t);
            }

        if (exits.size() != 2 || std::find(exits.begin(), exits.end(), tk) == exits.end())
            continue;

        uint32_t other = exits[0] == tk ? exits[1] : exits[0];
        std::vector<ChainNode> prefix(nodes.begin(), nodes.begin() + k + 1);
        AstExpr* reach;
        if (!reduceChain(prefix, tk, other, reach))
            continue;

        state = snapshots[k].state;
        nextSeq = snapshots[k].nextSeq;
        exitTrue = tk;
        exitFalse = other;
        nodes.resize(k + 1);
        nodes[0].cond = reach;
        return nodes;
    }

    throw DecompileError("unrecognized conditional structure");
}

// ---------------------------------------------------------------- control flow

AstStatBlock* FunctionBuilder::emitBlock(uint32_t begin, uint32_t end, LoopContext& loop, std::vector<uint32_t> blockExits)
{
    return emitBlock(begin, end, loop, std::move(blockExits), Alternative{});
}

AstStatBlock* FunctionBuilder::emitBlock(uint32_t begin, uint32_t end, LoopContext& loop, std::vector<uint32_t> blockExits, Alternative blockAlternative)
{
    std::vector<AstStat*> body;
    blocks.push_back(&body);
    ctx.namer.pushScope();
    State saved = state;
    std::swap(exits, blockExits);
    std::swap(alternative, blockAlternative);

    emitRange(begin, end, loop);
    flushPending(true);
    dropUnreachable(body);

    std::swap(exits, blockExits);
    std::swap(alternative, blockAlternative);
    state = saved;
    ctx.namer.popScope();
    blocks.pop_back();
    return make<AstStatBlock>(Location(), array(body));
}

// The exits of a sub-block of the current block [.., end) that continues at `target`: reaching the current block's end
// continues at the current block's exits.
std::vector<uint32_t> FunctionBuilder::continuing(uint32_t target, uint32_t end) const
{
    std::vector<uint32_t> result = {target};
    if (target == end)
        result.insert(result.end(), exits.begin(), exits.end());
    return result;
}

void FunctionBuilder::emitRange(uint32_t begin, uint32_t end, LoopContext& loop)
{
    for (uint32_t u = begin; u < end;)
    {
        try
        {
            u = emitUnit(u, end, loop);
        }
        catch (const DecompileError& e)
        {
            std::string message = e.what();
            uint32_t pc = flow.insns[flow.units[u].first].pc;
            if (message.find(" near line ") == std::string::npos && pc < proto.lines.size())
                throw DecompileError(message + " near line " + std::to_string(proto.lines[pc]));
            throw;
        }
    }
}

uint32_t FunctionBuilder::emitUnit(uint32_t u, uint32_t end, LoopContext& loop)
{
    if (!flow.jumpSources[u].empty() || !backJumps[u].empty())
        flushPending(true);

    int latch = -1;
    for (uint32_t v : backJumps[u])
        if (v < end)
            latch = std::max(latch, int(v));
    if (latch >= 0)
        return emitLoop(u, uint32_t(latch), loop);

    const Unit& unit = flow.units[u];
    if (unit.region >= 0)
    {
        const ValueRegion& region = flow.regions[unit.region];
        define(int(u), region.reg, buildRegion(region));
        return u + 1;
    }

    const Insn& insn = flow.insns[unit.first];

    if (isConditionalJump(insn.op))
        return emitConditional(u, end, loop);

    switch (insn.op)
    {
    case LOP_JUMP:
    case LOP_JUMPX:
    case LOP_JUMPBACK:
        emitJump(u, end, loop);
        return u + 1;
    case LOP_FORNPREP:
        return emitNumericFor(u, loop);
    case LOP_FORGPREP:
    case LOP_FORGPREP_INEXT:
    case LOP_FORGPREP_NEXT:
        return emitGenericFor(u, loop);
    case LOP_LOADB:
        if (insn.target >= 0)
            throw DecompileError("unstructured boolean materialization");
        break;
    default:
        break;
    }

    uint32_t next = emitInstruction(unit.first, int(u));
    return next >= flow.insns.size() ? uint32_t(flow.units.size()) : flow.unitOfInsn[next];
}

void FunctionBuilder::emitJump(uint32_t unit, uint32_t end, LoopContext& loop)
{
    const Insn& insn = flow.insns[flow.units[unit].first];
    int target = flow.unitAtPc(insn.target);

    if (target == loop.breakUnit)
        emitStatement(make<AstStatBreak>(Location()));
    else if (target == loop.continueUnit || (target == loop.headerUnit && target <= int(unit)))
    {
        // the last statement of a block needs no `continue` only if falling off the block reaches the latch too
        bool fallsThrough = unit + 1 == end &&
                            (target == int(end) || std::find(exits.begin(), exits.end(), uint32_t(target)) != exits.end());
        if (!fallsThrough)
        {
            loop.usedContinue = true;
            emitStatement(make<AstStatContinue>(Location()));
        }
    }
    else if (target == loop.fallthroughUnit || target == int(end))
    {
        // jump to the end of the enclosing block: nothing to say
    }
    else if (unit + 1 == end && std::find(exits.begin(), exits.end(), uint32_t(target)) != exits.end())
    {
        // the last statement of the block jumps where the block continues anyway
    }
    else if (AstStat* ret = sharedReturn(uint32_t(target)))
        emitStatement(ret);
    else
        emitStatement(unstructuredJump(uint32_t(target)));
}

// Jumps with no structured equivalent (the compiler's O2 inliner turns early returns into them) are kept visible as comments.
AstStat* FunctionBuilder::unstructuredJump(uint32_t target)
{
    std::string where = "pc " + std::to_string(target < flow.units.size() ? flow.insns[flow.units[target].first].pc : proto.code.size());
    if (target < flow.units.size() && !proto.lines.empty())
        where = "line " + std::to_string(proto.lines[flow.insns[flow.units[target].first].pc]);

    ctx.comments.push_back("luauDec: unstructured jump to " + where + " (e.g. an inlined early return); following code may not run here");
    return make<AstStatError>(Location(), AstArray<AstExpr*>{}, AstArray<AstStat*>{}, unsigned(ctx.comments.size() - 1));
}

// The compiler threads jumps into a shared `return` at the end of a function; repeat that return at the jump site.
AstStat* FunctionBuilder::sharedReturn(uint32_t target)
{
    if (target >= flow.units.size() || flow.units[target].region >= 0)
        return nullptr;

    const Insn& ret = flow.insns[flow.units[target].first];
    if (ret.op != LOP_RETURN || LUAU_INSN_B(ret.word) == 0)
        return nullptr;

    std::vector<AstExpr*> values;
    int first = int(LUAU_INSN_A(ret.word));
    int count = int(LUAU_INSN_B(ret.word)) - 1;
    for (int r = first; r < first + count; ++r)
    {
        int w = flow.useWeb(target, uint8_t(r));
        if (w < 0 || !webLocals[w])
            return nullptr;
        values.push_back(make<AstExprLocal>(Location(), webLocals[w], false));
    }
    return make<AstStatReturn>(Location(), array(values));
}

uint32_t FunctionBuilder::emitConditional(uint32_t unit, uint32_t end, LoopContext& loop)
{
    uint32_t exitTrue, exitFalse;
    std::vector<ChainNode> chain = parseChain(unit, end, false, exitTrue, exitFalse);
    AstExpr* cond = chain[0].cond;

    auto single = [&](AstStat* stat) {
        return make<AstStatBlock>(Location(), array<AstStat*>({stat}));
    };

    if (int(exitFalse) == loop.breakUnit)
    {
        emitStatement(make<AstStatIf>(
            Location(), negateCondition(ctx.allocator, cond), single(make<AstStatBreak>(Location())), nullptr, std::nullopt, std::nullopt
        ));
        return exitTrue;
    }

    if (int(exitFalse) == loop.continueUnit && exitFalse > end)
    {
        loop.usedContinue = true;
        emitStatement(make<AstStatIf>(
            Location(), negateCondition(ctx.allocator, cond), single(make<AstStatContinue>(Location())), nullptr, std::nullopt, std::nullopt
        ));
        return exitTrue;
    }

    uint32_t thenEnd, elseBegin = 0, elseEnd;
    bool hasElse = false;

    if (exitFalse > exitTrue && exitFalse <= end)
    {
        thenEnd = exitFalse;
        elseEnd = exitFalse;

        if (exitFalse - 1 >= exitTrue)
        {
            const Unit& last = flow.units[exitFalse - 1];
            const Insn& jump = flow.insns[last.first];
            if (last.region < 0 && (jump.op == LOP_JUMP || jump.op == LOP_JUMPX) && jump.target > int(jump.pc))
            {
                int x = flow.unitAtPc(jump.target);
                if (x > int(exitFalse) && x <= int(end))
                {
                    hasElse = true;
                    thenEnd = exitFalse - 1;
                    elseBegin = exitFalse;
                    elseEnd = uint32_t(x);
                }
                else if (x > int(end) && std::find(exits.begin(), exits.end(), uint32_t(x)) != exits.end())
                {
                    hasElse = true;
                    thenEnd = exitFalse - 1;
                    elseBegin = exitFalse;
                    elseEnd = end;
                }
            }
        }
    }
    else if (int(exitFalse) == loop.fallthroughUnit && exitFalse >= end)
    {
        thenEnd = end;
        elseEnd = end;
    }
    else if (exitFalse > end && exitTrue <= end && alternative.end > alternative.begin && exitFalse == alternative.begin)
    {
        // a jump into the else arm of the `if` whose then arm this block is (`if a and b`, with `b` inlined with its
        // statements): the rest of this block runs only when the jump is not taken, else the else arm, which runs
        // here once more
        flushPending(true);
        hoist(unit, end);
        Alternative other = alternative;
        AstStatBlock* rest = emitBlock(exitTrue, end, loop, continuing(end, end), other);
        // the copy's own variables are new locals; variables it shares with the code after the `if` are hoisted already
        std::vector<AstLocal*> savedLocals = webLocals;
        AstStatBlock* otherBlock = emitBlock(other.begin, other.end, loop, other.exits);
        webLocals = std::move(savedLocals);
        AstStat* elseBody = otherBlock->body.size == 1 && otherBlock->body.data[0]->is<AstStatIf>() ? otherBlock->body.data[0] : otherBlock;
        emitStatement(make<AstStatIf>(Location(), cond, rest, elseBody, std::nullopt, std::nullopt));
        return end;
    }
    else if (exitFalse > end && exitTrue <= end && std::find(exits.begin(), exits.end(), exitFalse) != exits.end())
    {
        // a jump to where this block continues (e.g. an inlined early return out of an `if` arm) skips the rest of the
        // block: run the rest only when the jump is not taken
        flushPending(true);
        hoist(unit, end);
        AstStatBlock* rest = emitBlock(exitTrue, end, loop, continuing(end, end));
        emitStatement(make<AstStatIf>(Location(), cond, rest, nullptr, std::nullopt, std::nullopt));
        return end;
    }
    else if (AstStat* ret = sharedReturn(exitFalse))
    {
        emitStatement(make<AstStatIf>(
            Location(), negateCondition(ctx.allocator, cond), make<AstStatBlock>(Location(), array<AstStat*>({ret})), nullptr, std::nullopt,
            std::nullopt
        ));
        return exitTrue;
    }
    else
    {
        emitStatement(make<AstStatIf>(
            Location(), negateCondition(ctx.allocator, cond), make<AstStatBlock>(Location(), array<AstStat*>({unstructuredJump(exitFalse)})),
            nullptr, std::nullopt, std::nullopt
        ));
        return exitTrue;
    }

    flushPending(true);
    hoist(unit, elseEnd);

    Alternative elseArm;
    if (hasElse)
        elseArm = {elseBegin, elseEnd, continuing(elseEnd, end)};
    AstStatBlock* thenBlock = emitBlock(exitTrue, thenEnd, loop, continuing(hasElse ? elseEnd : thenEnd, end), elseArm);
    AstStat* elseBody = nullptr;
    if (hasElse)
    {
        AstStatBlock* elseBlock = emitBlock(elseBegin, elseEnd, loop, continuing(elseEnd, end));
        if (elseBlock->body.size == 1 && elseBlock->body.data[0]->is<AstStatIf>())
            elseBody = elseBlock->body.data[0];
        else if (elseBlock->body.size > 0)
            elseBody = elseBlock;
    }

    emitStatement(make<AstStatIf>(Location(), cond, thenBlock, elseBody, std::nullopt, std::nullopt));
    return elseEnd;
}

uint32_t FunctionBuilder::emitLoop(uint32_t unit, uint32_t latch, LoopContext& outer)
{
    (void)outer;
    uint32_t exit = latch + 1;

    flushPending(true);
    hoist(unit, exit);

    LoopContext loop;
    loop.breakUnit = int(exit);
    loop.continueUnit = int(latch);
    loop.headerUnit = int(unit);
    loop.fallthroughUnit = int(latch);

    State saved = state;
    uint64_t savedSeq = nextSeq;

    std::vector<ChainNode> chain;
    uint32_t exitTrue = 0, exitFalse = 0;
    try
    {
        chain = parseChain(unit, latch, true, exitTrue, exitFalse);
    }
    catch (const PureViolation&)
    {
        chain.clear();
    }
    catch (const DecompileError&)
    {
        chain.clear();
    }

    AstStat* stat;
    if (!chain.empty() && exitFalse == exit && exitTrue <= latch)
    {
        AstStatBlock* body = emitBlock(exitTrue, latch, loop);
        stat = make<AstStatWhile>(Location(), chain[0].cond, body, true, Location());
    }
    else
    {
        state = saved;
        nextSeq = savedSeq;
        AstStatBlock* body = emitBlock(unit, latch, loop);

        AstStatIf* tail = body->body.size > 0 ? body->body.data[body->body.size - 1]->as<AstStatIf>() : nullptr;
        if (tail && !loop.usedContinue && !tail->elsebody && tail->thenbody->body.size == 1 && tail->thenbody->body.data[0]->is<AstStatBreak>())
        {
            body->body.size--;
            stat = make<AstStatRepeat>(Location(), tail->condition, body, true);
        }
        else
            stat = make<AstStatWhile>(Location(), make<AstExprConstantBool>(Location(), true), body, true, Location());
    }

    state = saved;
    emitStatement(stat);
    return exit;
}

uint32_t FunctionBuilder::emitNumericFor(uint32_t unit, LoopContext& outer)
{
    (void)outer;
    const Insn& insn = flow.insns[flow.units[unit].first];
    uint8_t a = LUAU_INSN_A(insn.word);

    // The FORNLOOP jumps back to the instruction after FORNPREP. FORNPREP's own target is usually the instruction after
    // the FORNLOOP, but the optimizer threads it to wherever that instruction jumps (e.g. the latch of an outer loop).
    uint32_t bodyPc = insn.pc + insn.length;
    uint32_t loopUnit = 0;
    for (uint32_t u = unit + 1; u < flow.units.size(); ++u)
    {
        const Unit& candidate = flow.units[u];
        if (candidate.region >= 0)
            continue;
        const Insn& loopInsn = flow.insns[candidate.first];
        if (loopInsn.op == LOP_FORNLOOP && LUAU_INSN_A(loopInsn.word) == a && loopInsn.target == int(bodyPc))
        {
            loopUnit = u;
            break;
        }
    }
    if (loopUnit == 0)
        throw DecompileError("numeric for without FORNLOOP");
    uint32_t exit = loopUnit + 1;

    std::vector<AstExpr*> ops = consume({uint8_t(a + 2), a, uint8_t(a + 1)}, int(unit), false);
    AstExpr* step = ops[2];
    if (auto n = step->as<AstExprConstantNumber>(); n && n->value == 1)
        step = nullptr;

    flushPending(true);
    hoist(unit + 1, exit);

    ctx.namer.pushScope();
    int web = flow.defWeb(unit, uint8_t(a + 2));
    AstLocal* var = declareLocal(web, ctx.namer.firstAvailable({"i", "j", "k"}));

    State saved = state;
    state.slots[a] = Slot{};
    state.slots[a + 1] = Slot{};
    Slot& slot = state.slots[a + 2];
    slot = Slot{};
    slot.kind = Slot::Kind::Var;
    slot.local = var;
    slot.web = web;

    LoopContext loop;
    loop.breakUnit = int(exit);
    loop.continueUnit = int(loopUnit);
    loop.fallthroughUnit = int(loopUnit);

    AstStatBlock* body = emitBlock(unit + 1, loopUnit, loop);
    ctx.namer.popScope();
    state = saved;
    state.slots[a] = state.slots[a + 1] = state.slots[a + 2] = Slot{};

    emitStatement(make<AstStatFor>(Location(), var, ops[0], ops[1], step, body, true, Location()));
    return exit;
}

uint32_t FunctionBuilder::emitGenericFor(uint32_t unit, LoopContext& outer)
{
    (void)outer;
    const Insn& insn = flow.insns[flow.units[unit].first];
    uint8_t a = LUAU_INSN_A(insn.word);
    uint32_t loopUnit = uint32_t(flow.unitAtPc(insn.target));
    const Insn& loopInsn = flow.insns[flow.units[loopUnit].first];
    if (loopInsn.op != LOP_FORGLOOP)
        throw DecompileError("generic for without FORGLOOP");
    uint8_t count = uint8_t(loopInsn.aux & 0xff);
    uint32_t exit = loopUnit + 1;

    std::vector<AstExpr*> values;
    Slot& first = state.slots[a];
    if (first.kind == Slot::Kind::Pending && first.group >= 0 && groups[first.group].first == a && groups[first.group].count == 3 &&
        state.slots[a + 1].group == first.group && state.slots[a + 2].group == first.group)
    {
        values.push_back(groups[first.group].expr);
        state.slots[a] = state.slots[a + 1] = state.slots[a + 2] = Slot{};
    }
    else
    {
        values = consume({a, uint8_t(a + 1), uint8_t(a + 2)}, int(unit), false);
        while (values.size() > 1 && values.back()->is<AstExprConstantNil>())
            values.pop_back();
        if (auto g = values.back()->as<AstExprGroup>(); g && values.size() == 1)
            values.back() = g->expr;
    }

    flushPending(true);
    hoist(unit + 1, exit);

    ctx.namer.pushScope();
    std::vector<std::string> names = suggestLoopNames(values, count);
    std::vector<AstLocal*> vars;
    State saved = state;
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t reg = uint8_t(a + 3 + i);
        int web = flow.defWeb(loopUnit, reg);
        AstLocal* var = declareLocal(web, names[i]);
        vars.push_back(var);

        Slot& slot = state.slots[reg];
        slot = Slot{};
        slot.kind = Slot::Kind::Var;
        slot.local = var;
        slot.web = web;
    }

    LoopContext loop;
    loop.breakUnit = int(exit);
    loop.continueUnit = int(loopUnit);
    loop.fallthroughUnit = int(loopUnit);

    AstStatBlock* body = emitBlock(unit + 1, loopUnit, loop);
    ctx.namer.popScope();
    state = saved;
    for (int r = a; r < a + 3 + count; ++r)
        state.slots[r] = Slot{};

    emitStatement(make<AstStatForIn>(Location(), array(vars), array(values), body, true, Location(), true, Location()));
    return exit;
}

// ---------------------------------------------------------------- instructions

uint32_t FunctionBuilder::emitInstruction(uint32_t index, int unit)
{
    const Insn& insn = flow.insns[index];
    uint32_t w = insn.word;
    uint8_t a = LUAU_INSN_A(w), b = LUAU_INSN_B(w), c = LUAU_INSN_C(w);
    int d = LUAU_INSN_D(w);

    auto binary = [&](AstExprBinary::Op op, AstExpr* l, AstExpr* r) -> AstExpr* {
        return make<AstExprBinary>(Location(), op, l, r);
    };

    auto arith = [&](LuauOpcode op) {
        switch (op)
        {
        case LOP_ADD:
        case LOP_ADDK:
            return AstExprBinary::Add;
        case LOP_SUB:
        case LOP_SUBK:
        case LOP_SUBRK:
            return AstExprBinary::Sub;
        case LOP_MUL:
        case LOP_MULK:
            return AstExprBinary::Mul;
        case LOP_DIV:
        case LOP_DIVK:
        case LOP_DIVRK:
            return AstExprBinary::Div;
        case LOP_MOD:
        case LOP_MODK:
            return AstExprBinary::Mod;
        case LOP_POW:
        case LOP_POWK:
            return AstExprBinary::Pow;
        case LOP_IDIV:
        case LOP_IDIVK:
            return AstExprBinary::FloorDiv;
        case LOP_AND:
        case LOP_ANDK:
            return AstExprBinary::And;
        default:
            return AstExprBinary::Or;
        }
    };

    auto tableCtor = [&](uint8_t reg) -> AstExprTable* {
        Slot& s = state.slots[reg];
        if (!s.table)
            return nullptr;
        bool visible = s.kind == Slot::Kind::Pending || (s.kind == Slot::Kind::Var && s.tableMark == statementCount);
        bool isCtorStore = regionDepth > 0 || (unit >= 0 && flow.isConstructorStore(uint32_t(unit)));
        return visible && isCtorStore ? s.table : nullptr;
    };

    auto appendItem = [&](AstExprTable* table, AstExprTable::Item item) {
        std::vector<AstExprTable::Item>*& storage = ctx.tableStorage[table];
        if (!storage)
        {
            ctx.tableItems.emplace_back(table->items.begin(), table->items.end());
            storage = &ctx.tableItems.back();
        }
        // a call or `...` is only expanded in the last constructor slot; once another item follows, parentheses are redundant
        if (!storage->empty())
            if (auto group = storage->back().value->as<AstExprGroup>(); group && (group->expr->is<AstExprCall>() || group->expr->is<AstExprVarargs>()))
                storage->back().value = group->expr;
        storage->push_back(item);
        table->items = AstArray<AstExprTable::Item>{storage->data(), storage->size()};
    };

    auto recordKey = [](const std::string& key) {
        return isValidIdentifier(key) ? AstExprTable::Item::Kind::Record : AstExprTable::Item::Kind::General;
    };

    switch (insn.op)
    {
    case LOP_NOP:
    case LOP_BREAK:
    case LOP_COVERAGE:
    case LOP_PREPVARARGS:
    case LOP_CLOSEUPVALS:
    case LOP_CAPTURE:
    case LOP_FASTCALL:
    case LOP_FASTCALL1:
    case LOP_FASTCALL2:
    case LOP_FASTCALL2K:
    case LOP_FASTCALL3:
    case LOP_FASTPCALL:
    case LOP_NATIVECALL:
        break;

    case LOP_LOADNIL:
        define(unit, a, make<AstExprConstantNil>(Location()));
        break;
    case LOP_LOADB:
        define(unit, a, make<AstExprConstantBool>(Location(), b != 0));
        break;
    case LOP_LOADN:
        define(unit, a, make<AstExprConstantNumber>(Location(), double(d)));
        break;
    case LOP_LOADK:
        define(unit, a, constant(uint32_t(d)));
        break;
    case LOP_LOADKX:
        define(unit, a, constant(insn.aux));
        break;
    case LOP_MOVE:
    {
        bool truncated = state.slots[b].kind == Slot::Kind::Pending && state.slots[b].truncated;
        AstExpr* value = consumeOne(b, unit);
        define(unit, a, value, truncated && !value->is<AstExprLocal>());
        break;
    }

    case LOP_GETGLOBAL:
        define(unit, a, make<AstExprGlobal>(Location(), name(constantString(insn.aux))));
        break;
    case LOP_SETGLOBAL:
    {
        AstExpr* value = consumeOne(a, unit);
        AstExpr* target = make<AstExprGlobal>(Location(), name(constantString(insn.aux)));
        if (auto func = value->as<AstExprFunction>())
            emitStatement(make<AstStatFunction>(Location(), target, func));
        else
            emitAssign(target, value);
        break;
    }

    case LOP_GETUPVAL:
        if (b >= upvalues.size())
            throw DecompileError("unknown upvalue U" + std::to_string(b));
        define(unit, a, make<AstExprLocal>(Location(), upvalues[b], true));
        break;
    case LOP_SETUPVAL:
        if (b >= upvalues.size())
            throw DecompileError("unknown upvalue U" + std::to_string(b));
        emitAssign(make<AstExprLocal>(Location(), upvalues[b], true), consumeOne(a, unit));
        break;

    case LOP_GETIMPORT:
        define(unit, a, importExpr(insn.aux));
        break;

    case LOP_GETTABLE:
    {
        std::vector<AstExpr*> ops = consume({b, c}, unit, false);
        if (auto s = ops[1]->as<AstExprConstantString>(); s && isValidIdentifier(std::string(s->value.data, s->value.size)))
            define(unit, a, field(ops[0], std::string(s->value.data, s->value.size)));
        else
            define(unit, a, make<AstExprIndexExpr>(Location(), ops[0], ops[1]));
        break;
    }
    case LOP_GETTABLEKS:
    case LOP_GETUDATAKS:
    {
        uint32_t k = insn.op == LOP_GETUDATAKS ? LUAU_INSN_AUX_KV16(insn.aux) : insn.aux;
        define(unit, a, field(consumeOne(b, unit), constantString(k)));
        break;
    }
    case LOP_GETTABLEN:
        define(unit, a, make<AstExprIndexExpr>(Location(), consumeOne(b, unit), make<AstExprConstantNumber>(Location(), double(c + 1))));
        break;

    case LOP_SETTABLE:
    case LOP_SETTABLEKS:
    case LOP_SETUDATAKS:
    case LOP_SETTABLEN:
    {
        std::string fieldName;
        bool named = insn.op == LOP_SETTABLEKS || insn.op == LOP_SETUDATAKS;
        if (named)
            fieldName = constantString(insn.op == LOP_SETUDATAKS ? LUAU_INSN_AUX_KV16(insn.aux) : insn.aux);

        if (AstExprTable* table = tableCtor(b))
        {
            AstExpr* key;
            AstExpr* value;
            if (insn.op == LOP_SETTABLE)
            {
                std::vector<AstExpr*> ops = consume({c, a}, unit, false);
                key = ops[0];
                value = ops[1];
            }
            else
            {
                value = consumeOne(a, unit);
                key = named ? constant(insn.op == LOP_SETUDATAKS ? LUAU_INSN_AUX_KV16(insn.aux) : insn.aux)
                            : make<AstExprConstantNumber>(Location(), double(c + 1));
            }

            AstExprTable::Item::Kind kind = AstExprTable::Item::Kind::General;
            if (auto s = key->as<AstExprConstantString>())
                kind = recordKey(std::string(s->value.data, s->value.size));
            appendItem(table, {kind, key, value});
            break;
        }

        AstExpr* table;
        AstExpr* key = nullptr;
        AstExpr* value;
        if (insn.op == LOP_SETTABLE)
        {
            std::vector<AstExpr*> ops = consume({b, c, a}, unit, false);
            table = ops[0];
            key = ops[1];
            value = ops[2];
            if (auto s = key->as<AstExprConstantString>(); s && isValidIdentifier(std::string(s->value.data, s->value.size)))
            {
                fieldName = std::string(s->value.data, s->value.size);
                named = true;
            }
        }
        else
        {
            std::vector<AstExpr*> ops = consume({b, a}, unit, false);
            table = ops[0];
            value = ops[1];
            if (!named)
                key = make<AstExprConstantNumber>(Location(), double(c + 1));
        }
        storeTable(unit, b, table, key, value, named ? fieldName.c_str() : nullptr);
        break;
    }

    case LOP_NEWCLOSURE:
    case LOP_DUPCLOSURE:
    {
        uint32_t protoId;
        if (insn.op == LOP_NEWCLOSURE)
        {
            if (uint32_t(d) >= proto.children.size())
                throw DecompileError("closure index out of range");
            protoId = proto.children[d];
        }
        else
        {
            if (uint32_t(d) >= proto.constants.size() || proto.constants[d].kind != Constant::Kind::Closure)
                throw DecompileError("DUPCLOSURE without a closure constant");
            protoId = proto.constants[d].proto;
        }

        uint32_t next;
        AstLocal* self;
        bool selfDeclared;
        AstExpr* func = closure(index, ctx.chunk.protos[protoId], next, a, self, selfDeclared);
        if (self)
        {
            // a recursive function; an existing variable (e.g. `local f` declared earlier and captured by other closures)
            // is assigned, not declared again: a second `local function f` would leave the earlier captures nil
            if (selfDeclared)
                emitStatement(make<AstStatLocalFunction>(Location(), self, func->as<AstExprFunction>(), false, Position::missing()));
            else
                emitStatement(make<AstStatFunction>(Location(), make<AstExprLocal>(Location(), self, false), func->as<AstExprFunction>()));
            Slot& slot = state.slots[a];
            slot = Slot{};
            slot.kind = Slot::Kind::Var;
            slot.local = self;
        }
        else
            define(unit, a, func);
        return next;
    }

    case LOP_NAMECALL:
    case LOP_NAMECALLUDATA:
    {
        Slot& obj = state.slots[b];
        bool ordered = obj.kind == Slot::Kind::Pending && !isMovable(obj.expr);
        uint64_t seq = obj.seq;

        AstExpr* object;
        if (ordered)
        {
            object = obj.expr;
            if (regionDepth > 0 && obj.seq < regionSeqStart)
                ++regionConsumed[object];
            obj = Slot{};
        }
        else
            object = consumeOne(b, unit);

        uint32_t k = insn.op == LOP_NAMECALLUDATA ? LUAU_INSN_AUX_KV16(insn.aux) : insn.aux;
        const std::string& method = constantString(k);
        if (!isValidIdentifier(method))
            throw DecompileError("method name is not an identifier");

        Slot slot;
        slot.kind = Slot::Kind::Method;
        slot.expr = make<AstExprIndexName>(Location(), object, name(method), Location(), Position(0, 0), ':');
        slot.seq = ordered ? seq : nextSeq++;
        slot.methodPure = !ordered;
        state.slots[a] = slot;
        state.slots[(a + 1) & 0xff] = Slot{};
        break;
    }

    case LOP_CALL:
    case LOP_CALLFB:
    {
        bool method = state.slots[a].kind == Slot::Kind::Method;
        std::vector<uint8_t> regs = {a};
        int lastArg = b == 0 ? insn.multretBase : a + b - 1;
        for (int r = a + (method ? 2 : 1); r <= lastArg; ++r)
            regs.push_back(uint8_t(r));

        std::vector<AstExpr*> ops = consume(regs, unit, true);
        std::vector<AstExpr*> args(ops.begin() + 1, ops.end());
        AstExpr* call = make<AstExprCall>(Location(), ops[0], array(args), method, false, AstArray<AstTypeOrPack>{}, Location());

        if (c == 1)
            emitStatement(make<AstStatExpr>(Location(), call));
        else if (c == 0)
            define(unit, a, call, false, true);
        else if (c == 2)
            define(unit, a, call, true);
        else
            defineGroup(unit, a, uint8_t(c - 1), call);
        break;
    }

    case LOP_RETURN:
    {
        std::vector<uint8_t> regs;
        int lastValue = b == 0 ? insn.multretBase : a + b - 2;
        for (int r = a; r <= lastValue; ++r)
            regs.push_back(uint8_t(r));
        std::vector<AstExpr*> values = consume(regs, unit, true);
        emitStatement(make<AstStatReturn>(Location(), array(values)));
        break;
    }

    case LOP_ADD:
    case LOP_SUB:
    case LOP_MUL:
    case LOP_DIV:
    case LOP_MOD:
    case LOP_POW:
    case LOP_IDIV:
    case LOP_AND:
    case LOP_OR:
    {
        std::vector<AstExpr*> ops = consume({b, c}, unit, false);
        define(unit, a, binary(arith(insn.op), ops[0], ops[1]));
        break;
    }

    case LOP_ADDK:
    case LOP_SUBK:
    case LOP_MULK:
    case LOP_DIVK:
    case LOP_MODK:
    case LOP_POWK:
    case LOP_IDIVK:
    case LOP_ANDK:
    case LOP_ORK:
        define(unit, a, binary(arith(insn.op), consumeOne(b, unit), constant(c)));
        break;

    case LOP_SUBRK:
    case LOP_DIVRK:
        define(unit, a, binary(arith(insn.op), constant(b), consumeOne(c, unit)));
        break;

    case LOP_CONCAT:
    {
        std::vector<uint8_t> regs;
        for (int r = b; r <= c; ++r)
            regs.push_back(uint8_t(r));
        std::vector<AstExpr*> ops = consume(regs, unit, false);
        AstExpr* expr = ops.back();
        for (size_t i = ops.size() - 1; i-- > 0;)
            expr = binary(AstExprBinary::Concat, ops[i], expr);
        define(unit, a, expr);
        break;
    }

    case LOP_NOT:
        define(unit, a, make<AstExprUnary>(Location(), AstExprUnary::Op::Not, consumeOne(b, unit)));
        break;
    case LOP_MINUS:
        define(unit, a, make<AstExprUnary>(Location(), AstExprUnary::Op::Minus, consumeOne(b, unit)));
        break;
    case LOP_LENGTH:
        define(unit, a, make<AstExprUnary>(Location(), AstExprUnary::Op::Len, consumeOne(b, unit)));
        break;

    case LOP_NEWTABLE:
        define(unit, a, make<AstExprTable>(Location(), AstArray<AstExprTable::Item>{}));
        break;

    case LOP_DUPTABLE:
    {
        if (uint32_t(d) >= proto.constants.size() || proto.constants[d].kind != Constant::Kind::Table)
            throw DecompileError("DUPTABLE without a table template");
        const Constant& tpl = proto.constants[d];
        std::vector<AstExprTable::Item> items;
        for (size_t i = 0; i < tpl.templateKeys.size(); ++i)
        {
            if (tpl.templateValues[i] < 0)
                continue;
            AstExpr* key = constant(tpl.templateKeys[i]);
            AstExprTable::Item::Kind kind = AstExprTable::Item::Kind::General;
            if (auto s = key->as<AstExprConstantString>())
                kind = recordKey(std::string(s->value.data, s->value.size));
            items.push_back({kind, key, constant(uint32_t(tpl.templateValues[i]))});
        }
        auto* table = make<AstExprTable>(Location(), AstArray<AstExprTable::Item>{});
        for (const AstExprTable::Item& item : items)
            appendItem(table, item);
        define(unit, a, table);
        break;
    }

    case LOP_SETLIST:
    {
        std::vector<uint8_t> regs;
        int lastValue = c == 0 ? insn.multretBase : b + c - 2;
        for (int r = b; r <= lastValue; ++r)
            regs.push_back(uint8_t(r));

        AstExprTable* table = tableCtor(a);
        size_t listCount = 0;
        if (table)
            for (const AstExprTable::Item& item : table->items)
                listCount += item.kind == AstExprTable::Item::Kind::List;

        if (table && insn.aux == listCount + 1)
        {
            for (AstExpr* value : consume(regs, unit, true))
                appendItem(table, {AstExprTable::Item::Kind::List, nullptr, value});
            break;
        }

        if (c == 0)
            throw DecompileError("SETLIST with an open value list outside a constructor");

        std::vector<uint8_t> all = {a};
        all.insert(all.end(), regs.begin(), regs.end());
        std::vector<AstExpr*> ops = consume(all, unit, false);
        for (size_t i = 1; i < ops.size(); ++i)
            emitAssign(
                make<AstExprIndexExpr>(Location(), ops[0], make<AstExprConstantNumber>(Location(), double(insn.aux + i - 1))), ops[i]
            );
        break;
    }

    case LOP_GETVARARGS:
        if (b == 0)
            define(unit, a, make<AstExprVarargs>(Location()), false, true);
        else if (b == 2)
            define(unit, a, make<AstExprVarargs>(Location()), true);
        else if (b > 2)
            defineGroup(unit, a, uint8_t(b - 1), make<AstExprVarargs>(Location()));
        break;

    default:
        throw DecompileError("unsupported instruction at pc " + std::to_string(insn.pc) + " (opcode " + std::to_string(int(insn.op)) + ")");
    }

    return index + 1;
}

} // namespace Luau::Decompiler
