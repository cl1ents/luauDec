// Decompiles a directory of bytecode files, compiles each result again and compares the two programs function by function.
//
//   luauDec.bench <dir> [-O<n>] [-g<n>] [-j<threads>] [-f<flag>=<bool>] [-m<global,...>] [--filter <text>] [--report <file.tsv>] [--keep <dir>]
//   luauDec.bench --show <file> [options]   prints the source and the first differing instructions of one file
//
// Each input is a serialized bytecode blob (any file name). For every file the bench records:
//   throw    decompile threw
//   partial  the source contains `luauDec: failed` comments
//   jumps    the source contains `luauDec: unstructured jump` comments (control flow it could not express)
//   compile  the source does not compile
//   shape    it compiles, but the function count differs
//   differ   some function's opcode sequence differs from the original
//   match    every function has the original opcode sequence
// Opcode sequences ignore operands, so a match means the same control flow and operations, not the same registers.
// The compile options should be the ones that produced the input; -O and -g set them.

#include "Decompiler/BytecodeReader.h"

#include "luauDec/Decompile.h"

#include "Luau/BytecodeUtils.h"
#include "Luau/Common.h"
#include "Luau/Compiler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace Luau::Decompiler;

struct Result
{
    std::string name;
    std::string status;
    int markers = 0;
    int jumps = 0;
    int protos = 0;
    int matchingProtos = 0;
    double seconds = 0;
    std::string detail;
};

std::vector<uint8_t> opcodes(const FunctionProto& proto)
{
    std::vector<uint8_t> ops;
    for (size_t pc = 0; pc < proto.code.size();)
    {
        LuauOpcode op = LuauOpcode(LUAU_INSN_OP(proto.code[pc]));
        ops.push_back(uint8_t(op));
        pc += op < LOP__COUNT ? Luau::getOpLength(op) : 1;
    }
    return ops;
}

std::string oneLine(std::string text, size_t limit = 200)
{
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\t', ' ');
    if (text.size() > limit)
        text = text.substr(0, limit) + "...";
    return text;
}

int countMarkers(const std::string& source, const char* marker = "luauDec: failed")
{
    int count = 0;
    for (size_t at = source.find(marker); at != std::string::npos; at = source.find(marker, at + 1))
        ++count;
    return count;
}

// The first source line that luauDec marked, for grouping failures by cause.
std::string firstMarker(const std::string& source)
{
    size_t at = source.find("luauDec: failed");
    if (at == std::string::npos)
        return {};
    size_t end = source.find('\n', at);
    std::string line = source.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (line.size() >= 2 && line.compare(line.size() - 2, 2, "]]") == 0)
        line.resize(line.size() - 2);
    return line;
}

// Pairs each original function with a recompiled one. The decompiler may emit functions in another order (e.g. a local
// function inlined into a later table constructor), so functions pair by debug name first and by order among the rest.
std::vector<std::pair<size_t, size_t>> pairProtos(const Chunk& a, const Chunk& b)
{
    std::map<std::string, std::vector<size_t>> named;
    std::vector<size_t> unnamed;
    for (size_t j = 0; j < b.protos.size(); ++j)
    {
        if (j == b.mainProto)
            continue;
        if (b.protos[j].debugName)
            named[*b.protos[j].debugName].push_back(j);
        else
            unnamed.push_back(j);
    }
    for (auto& [name, list] : named)
        std::reverse(list.begin(), list.end());
    std::reverse(unnamed.begin(), unnamed.end());

    std::vector<std::pair<size_t, size_t>> pairs = {{a.mainProto, b.mainProto}};
    std::vector<size_t> rest;
    for (size_t i = 0; i < a.protos.size(); ++i)
    {
        if (i == a.mainProto)
            continue;
        auto it = a.protos[i].debugName ? named.find(*a.protos[i].debugName) : named.end();
        if (it != named.end() && !it->second.empty())
        {
            pairs.push_back({i, it->second.back()});
            it->second.pop_back();
        }
        else
            rest.push_back(i);
    }
    // leftover recompiled functions (renamed or unnamed), in order
    for (auto& [name, list] : named)
        unnamed.insert(unnamed.end(), list.begin(), list.end());
    std::sort(unnamed.begin(), unnamed.end(), std::greater<size_t>());
    for (size_t i : rest)
    {
        if (unnamed.empty())
            break;
        pairs.push_back({i, unnamed.back()});
        unnamed.pop_back();
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

Result runOne(const std::filesystem::path& file, const Luau::CompileOptions& options, const std::filesystem::path& keep)
{
    Result result;
    result.name = file.filename().string();
    auto start = std::chrono::steady_clock::now();
    auto finish = [&](const char* status, std::string detail) {
        result.status = status;
        result.detail = oneLine(std::move(detail));
        result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return result;
    };

    std::ifstream in(file, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string bytecode = buffer.str();

    Chunk original;
    std::string source;
    try
    {
        original = readBytecode(bytecode);
        source = decompile(bytecode);
    }
    catch (const std::exception& e)
    {
        return finish("throw", e.what());
    }
    result.protos = int(original.protos.size());

    if (!keep.empty())
    {
        std::ofstream out(keep / (file.stem().string() + ".luau"), std::ios::binary);
        out << source;
    }

    result.markers = countMarkers(source);
    result.jumps = countMarkers(source, "luauDec: unstructured jump");
    if (result.markers > 0)
        return finish("partial", firstMarker(source));
    if (result.jumps > 0)
        return finish("jumps", source.substr(source.find("luauDec: unstructured jump"), 120));

    std::string recompiledBytecode = Luau::compile(source, options);
    if (recompiledBytecode.empty() || recompiledBytecode[0] == 0)
        return finish("compile", recompiledBytecode.empty() ? std::string() : recompiledBytecode.substr(1));

    Chunk recompiled = readBytecode(recompiledBytecode);
    if (recompiled.protos.size() != original.protos.size())
        return finish("shape", std::to_string(original.protos.size()) + " functions, recompiled " + std::to_string(recompiled.protos.size()));

    std::string firstDiff;
    for (auto [i, j] : pairProtos(original, recompiled))
    {
        const FunctionProto& a = original.protos[i];
        const FunctionProto& b = recompiled.protos[j];
        std::vector<uint8_t> opsA = opcodes(a);
        std::vector<uint8_t> opsB = opcodes(b);
        if (opsA == opsB)
        {
            ++result.matchingProtos;
            continue;
        }
        if (firstDiff.empty())
        {
            size_t k = 0;
            while (k < opsA.size() && k < opsB.size() && opsA[k] == opsB[k])
                ++k;
            auto opName = [](const std::vector<uint8_t>& ops, size_t k) -> std::string {
                return k < ops.size() ? std::to_string(ops[k]) : std::string("end");
            };
            firstDiff = "proto " + std::to_string(i) + " (" + (a.debugName ? *a.debugName : std::string("?")) + ") op " + std::to_string(k) +
                        ": " + opName(opsA, k) + " vs " + opName(opsB, k) + " (" + std::to_string(opsA.size()) + "/" +
                        std::to_string(opsB.size()) + " ops)";
        }
    }
    if (result.matchingProtos != result.protos)
        return finish("differ", firstDiff);
    return finish("match", {});
}

bool setFlag(const std::string& spec)
{
    size_t eq = spec.find('=');
    if (eq == std::string::npos)
        return false;
    std::string name = spec.substr(0, eq);
    bool value = spec.substr(eq + 1) == "true";
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
        if (name == flag->name)
        {
            flag->value = value;
            return true;
        }
    return false;
}

// Opcode names in LuauOpcode order, for --show.
const char* const kOpNames[] = {
    "NOP", "BREAK", "LOADNIL", "LOADB", "LOADN", "LOADK", "MOVE", "GETGLOBAL",
    "SETGLOBAL", "GETUPVAL", "SETUPVAL", "CLOSEUPVALS", "GETIMPORT", "GETTABLE", "SETTABLE", "GETTABLEKS",
    "SETTABLEKS", "GETTABLEN", "SETTABLEN", "NEWCLOSURE", "NAMECALL", "CALL", "RETURN", "JUMP",
    "JUMPBACK", "JUMPIF", "JUMPIFNOT", "JUMPIFEQ", "JUMPIFLE", "JUMPIFLT", "JUMPIFNOTEQ", "JUMPIFNOTLE",
    "JUMPIFNOTLT", "ADD", "SUB", "MUL", "DIV", "MOD", "POW", "ADDK",
    "SUBK", "MULK", "DIVK", "MODK", "POWK", "AND", "OR", "ANDK",
    "ORK", "CONCAT", "NOT", "MINUS", "LENGTH", "NEWTABLE", "DUPTABLE", "SETLIST",
    "FORNPREP", "FORNLOOP", "FORGLOOP", "FORGPREP_INEXT", "FASTCALL3", "FORGPREP_NEXT", "NATIVECALL", "GETVARARGS",
    "DUPCLOSURE", "PREPVARARGS", "LOADKX", "JUMPX", "FASTCALL", "COVERAGE", "CAPTURE", "SUBRK",
    "DIVRK", "FASTCALL1", "FASTCALL2", "FASTCALL2K", "FORGPREP", "JUMPXEQKNIL", "JUMPXEQKB", "JUMPXEQKN",
    "JUMPXEQKS", "IDIV", "IDIVK", "GETUDATAKS", "SETUDATAKS", "NAMECALLUDATA", "NEWCLASSMEMBER", "CALLFB",
    "CMPPROTO", "FASTPCALL", "NEWCLASS", "CONSTRUCT", "FINCONSTRUCT", "GETPRIVATEFIELD", "SETPRIVATEFIELD",
};
static_assert(sizeof(kOpNames) / sizeof(kOpNames[0]) == LOP__COUNT, "update kOpNames for the new opcodes");

std::string describe(const FunctionProto& proto, size_t pc)
{
    uint32_t insn = proto.code[pc];
    LuauOpcode op = LuauOpcode(LUAU_INSN_OP(insn));
    std::string text = op < LOP__COUNT ? kOpNames[op] : "op" + std::to_string(op);
    text += " A=" + std::to_string(LUAU_INSN_A(insn)) + " B=" + std::to_string(LUAU_INSN_B(insn)) + " C=" + std::to_string(LUAU_INSN_C(insn)) +
            " D=" + std::to_string(LUAU_INSN_D(insn));
    auto constantText = [&](uint32_t index) -> std::string {
        if (index >= proto.constants.size())
            return {};
        const Constant& k = proto.constants[index];
        switch (k.kind)
        {
        case Constant::Kind::String:
            return " \"" + k.string + "\"";
        case Constant::Kind::Number:
            return " " + std::to_string(k.number);
        case Constant::Kind::Import:
        {
            std::string path;
            for (const std::string& part : k.importPath)
                path += (path.empty() ? "" : ".") + part;
            return " " + path;
        }
        default:
            return {};
        }
    };
    if (op == LOP_GETIMPORT || op == LOP_LOADK)
        text += constantText(LUAU_INSN_D(insn));
    else if ((op == LOP_GETTABLEKS || op == LOP_SETTABLEKS || op == LOP_NAMECALL || op == LOP_GETGLOBAL || op == LOP_SETGLOBAL) && pc + 1 < proto.code.size())
        text += constantText(proto.code[pc + 1]);
    return text;
}

// Prints the decompiled source and the first differing function of both programs around the first difference.
void show(const std::filesystem::path& file, const Luau::CompileOptions& options)
{
    std::ifstream in(file, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string bytecode = buffer.str();
    std::string source = decompile(bytecode);
    std::printf("--- decompiled source ---\n%s-------------------------\n", source.c_str());
    std::string recompiledBytecode = Luau::compile(source, options);
    if (recompiledBytecode.empty() || recompiledBytecode[0] == 0)
    {
        std::printf("does not compile: %s\n", recompiledBytecode.empty() ? "" : recompiledBytecode.c_str() + 1);
        return;
    }
    Chunk a = readBytecode(bytecode);
    Chunk b = readBytecode(recompiledBytecode);
    for (auto [i, bi] : pairProtos(a, b))
    {
        std::vector<uint8_t> opsA = opcodes(a.protos[i]);
        std::vector<uint8_t> opsB = opcodes(b.protos[bi]);
        if (opsA == opsB)
            continue;
        auto pcs = [](const FunctionProto& proto) {
            std::vector<size_t> result;
            for (size_t pc = 0; pc < proto.code.size();)
            {
                result.push_back(pc);
                LuauOpcode op = LuauOpcode(LUAU_INSN_OP(proto.code[pc]));
                pc += op < LOP__COUNT ? Luau::getOpLength(op) : 1;
            }
            return result;
        };
        std::vector<size_t> pcA = pcs(a.protos[i]);
        std::vector<size_t> pcB = pcs(b.protos[bi]);
        size_t k = 0;
        while (k < opsA.size() && k < opsB.size() && opsA[k] == opsB[k])
            ++k;
        std::printf("proto %zu (%s): first difference at instruction %zu\n", i, a.protos[i].debugName ? a.protos[i].debugName->c_str() : "?", k);
        size_t from = k > 8 ? k - 8 : 0;
        for (size_t j = from; j < k + 8 && (j < pcA.size() || j < pcB.size()); ++j)
        {
            std::string left = j < pcA.size() ? describe(a.protos[i], pcA[j]) : "";
            std::string right = j < pcB.size() ? describe(b.protos[bi], pcB[j]) : "";
            std::printf("%c%4zu %-58s | %s\n", j == k ? '>' : ' ', j, left.c_str(), right.c_str());
        }
        return;
    }
    std::printf("%zu functions, recompiled %zu; all compared functions match\n", a.protos.size(), b.protos.size());
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path dir;
    std::filesystem::path report;
    std::filesystem::path keep;
    std::filesystem::path showFile;
    std::string filter;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    Luau::CompileOptions options;
    options.optimizationLevel = 1;
    options.debugLevel = 1;
    std::vector<std::string> mutableGlobals;
    std::vector<const char*> mutableGlobalNames;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("-O", 0) == 0)
            options.optimizationLevel = std::atoi(arg.c_str() + 2);
        else if (arg.rfind("-g", 0) == 0)
            options.debugLevel = std::atoi(arg.c_str() + 2);
        else if (arg.rfind("-j", 0) == 0)
            threads = std::max(1, std::atoi(arg.c_str() + 2));
        else if (arg.rfind("-f", 0) == 0)
        {
            if (!setFlag(arg.substr(2)))
            {
                std::fprintf(stderr, "unknown flag %s\n", arg.c_str() + 2);
                return 2;
            }
        }
        else if (arg.rfind("-m", 0) == 0)
        {
            // comma-separated globals that the program may assign, e.g. Roblox's -mgame,Game,workspace,Workspace,script,shared,plugin
            std::stringstream names(arg.substr(2));
            for (std::string name; std::getline(names, name, ',');)
                if (!name.empty())
                    mutableGlobals.push_back(name);
        }
        else if (arg == "--filter" && i + 1 < argc)
            filter = argv[++i];
        else if (arg == "--report" && i + 1 < argc)
            report = argv[++i];
        else if (arg == "--keep" && i + 1 < argc)
            keep = argv[++i];
        else if (arg == "--show" && i + 1 < argc)
            showFile = argv[++i];
        else if (dir.empty())
            dir = arg;
        else
        {
            std::fprintf(stderr, "unexpected argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (dir.empty() && showFile.empty())
    {
        std::fprintf(
            stderr, "usage: %s <dir> [-O<n>] [-g<n>] [-j<n>] [-f<flag>=<bool>] [-m<global,...>] [--filter text] [--report out.tsv] [--keep dir] [--show file]\n", argv[0]
        );
        return 2;
    }
    for (const std::string& name : mutableGlobals)
        mutableGlobalNames.push_back(name.c_str());
    mutableGlobalNames.push_back(nullptr);
    if (!mutableGlobals.empty())
        options.mutableGlobals = mutableGlobalNames.data();
    if (!showFile.empty())
    {
        show(showFile, options);
        return 0;
    }
    if (!keep.empty())
        std::filesystem::create_directories(keep);

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        if (entry.is_regular_file() && entry.path().filename().string().find(filter) != std::string::npos)
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());

    std::vector<Result> results(files.size());
    std::atomic<size_t> next{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t)
        pool.emplace_back([&] {
            for (size_t i; (i = next++) < files.size();)
                results[i] = runOne(files[i], options, keep);
        });
    for (std::thread& t : pool)
        t.join();

    std::map<std::string, int> counts;
    int protos = 0;
    int matchingProtos = 0;
    int markers = 0;
    int jumps = 0;
    for (const Result& r : results)
    {
        counts[r.status]++;
        protos += r.protos;
        matchingProtos += r.matchingProtos;
        markers += r.markers;
        jumps += r.jumps;
    }

    if (!report.empty())
    {
        std::ofstream out(report);
        out << "file\tstatus\tmarkers\tjumps\tprotos\tmatching\tseconds\tdetail\n";
        for (const Result& r : results)
            out << r.name << '\t' << r.status << '\t' << r.markers << '\t' << r.jumps << '\t' << r.protos << '\t' << r.matchingProtos << '\t'
                << r.seconds << '\t' << r.detail << '\n';
    }

    std::printf("%zu files:", results.size());
    for (const char* status : {"match", "differ", "shape", "compile", "jumps", "partial", "throw"})
        std::printf(" %s=%d", status, counts[status]);
    std::printf(
        "\nfunctions with the original opcodes (in match/differ files): %d; luauDec failure markers: %d; unstructured jumps: %d\n", matchingProtos,
        markers, jumps
    );
    return 0;
}
