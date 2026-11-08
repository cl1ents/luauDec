#include "luauDec/Decompile.h"

#include "Decompiler/BytecodeReader.h"
#include "Decompiler/Emitter.h"
#include "Decompiler/FunctionBuilder.h"

#include "Luau/BytecodeUtils.h"

namespace Luau::Decompiler
{

namespace
{

// Every global the chunk touches; locals never take these names so they can't capture a global reference.
std::unordered_set<std::string> collectGlobals(const Chunk& chunk)
{
    std::unordered_set<std::string> globals;

    for (const FunctionProto& proto : chunk.protos)
    {
        auto addString = [&](uint32_t index) {
            if (index < proto.constants.size() && proto.constants[index].kind == Constant::Kind::String)
                globals.insert(proto.constants[index].string);
        };

        for (const Constant& k : proto.constants)
            if (k.kind == Constant::Kind::Import && !k.importPath.empty())
                globals.insert(k.importPath[0]);

        for (size_t pc = 0; pc < proto.code.size();)
        {
            uint32_t insn = proto.code[pc];
            LuauOpcode op = LuauOpcode(LUAU_INSN_OP(insn));
            if (op >= LOP__COUNT)
                break;
            if ((op == LOP_GETGLOBAL || op == LOP_SETGLOBAL) && pc + 1 < proto.code.size())
                addString(proto.code[pc + 1]);
            if (op == LOP_GETIMPORT && pc + 1 < proto.code.size())
                addString((proto.code[pc + 1] >> 20) & 1023);
            pc += getOpLength(op);
        }
    }

    globals.insert("vector");
    return globals;
}

} // namespace

std::string decompile(std::string_view bytecode)
{
    Chunk chunk = readBytecode(bytecode);

    Allocator allocator;
    DecompileContext ctx(chunk, allocator, collectGlobals(chunk));

    AstStatBlock* root;
    try
    {
        FunctionBuilder builder(ctx, chunk.protos[chunk.mainProto], {}, 0);
        std::vector<AstLocal*> params;
        root = builder.buildBody(params);
    }
    catch (const DecompileError& e)
    {
        ctx.comments.push_back(std::string("luauDec: failed to decompile main chunk: ") + e.what());
        AstStat* error = allocator.alloc<AstStatError>(Location(), AstArray<AstExpr*>{}, AstArray<AstStat*>{}, unsigned(ctx.comments.size() - 1));
        AstStat** body = static_cast<AstStat**>(allocator.allocate(sizeof(AstStat*)));
        body[0] = error;
        root = allocator.alloc<AstStatBlock>(Location(), AstArray<AstStat*>{body, 1});
    }

    return emit(*root, ctx.comments);
}

} // namespace Luau::Decompiler
