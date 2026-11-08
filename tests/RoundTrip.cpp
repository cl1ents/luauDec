#include "lua.h"
#include "lualib.h"

#include "Luau/Compiler.h"

#include "Decompiler/Decompile.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

constexpr long long kInterruptBudget = 10'000'000;

struct Config
{
    int optimizationLevel;
    int debugLevel;
    int vectorPrecision;

    Luau::CompileOptions options() const
    {
        Luau::CompileOptions result;
        result.optimizationLevel = optimizationLevel;
        result.debugLevel = debugLevel;
        result.vectorPrecision = vectorPrecision;
        return result;
    }

    std::string name() const
    {
        std::string result = "O" + std::to_string(optimizationLevel) + "/g" + std::to_string(debugLevel);
        if (vectorPrecision != 0)
            result += "/vp" + std::to_string(vectorPrecision);
        return result;
    }
};

struct RunResult
{
    bool ok = false;
    std::string output;
    std::string error;
};

struct InterruptBudget
{
    long long interrupts = 0;
};

int capturePrint(lua_State* L)
{
    std::string& out = *static_cast<std::string*>(lua_tolightuserdata(L, lua_upvalueindex(1)));

    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i)
    {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (i > 1)
            out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    out += '\n';
    return 0;
}

void budgetInterrupt(lua_State* L, int gc)
{
    // gc >= 0 means the interrupt fires from inside a GC step, where raising errors is not allowed
    if (gc >= 0)
        return;

    InterruptBudget* budget = static_cast<InterruptBudget*>(lua_callbacks(L)->userdata);
    if (++budget->interrupts > kInterruptBudget)
        luaL_error(L, "instruction budget exceeded (possible infinite loop)");
}

RunResult run(const std::string& bytecode, const char* chunkname)
{
    RunResult result;
    InterruptBudget budget;

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushlightuserdata(L, &result.output);
    lua_pushcclosure(L, capturePrint, "print", 1);
    lua_setglobal(L, "print");

    lua_callbacks(L)->userdata = &budget;
    lua_callbacks(L)->interrupt = budgetInterrupt;

    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) != 0)
    {
        result.error = std::string("load failed: ") + lua_tostring(L, -1);
    }
    else if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        size_t len = 0;
        const char* msg = lua_tolstring(L, -1, &len);
        result.error = msg ? std::string(msg, len) : std::string("(non-string error)");
    }
    else
    {
        result.ok = true;
    }

    lua_callbacks(L)->interrupt = nullptr;
    lua_close(L);
    return result;
}

std::string numbered(const std::string& text)
{
    std::ostringstream out;
    std::istringstream in(text);
    std::string line;
    int lineNo = 1;
    while (std::getline(in, line))
    {
        char prefix[16];
        std::snprintf(prefix, sizeof(prefix), "%4d| ", lineNo++);
        out << prefix << line << '\n';
    }
    return out.str();
}

std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
        lines.push_back(line);
    return lines;
}

std::string firstDifference(const std::string& expected, const std::string& actual)
{
    std::vector<std::string> a = splitLines(expected);
    std::vector<std::string> b = splitLines(actual);

    size_t common = std::min(a.size(), b.size());
    for (size_t i = 0; i < common; ++i)
    {
        if (a[i] != b[i])
            return "output line " + std::to_string(i + 1) + ":\n    expected: " + a[i] + "\n    actual:   " + b[i];
    }

    if (a.size() != b.size())
    {
        std::string hint = "output line count differs: expected " + std::to_string(a.size()) + ", actual " + std::to_string(b.size());
        if (a.size() > common)
            hint += "\n    first missing line: " + a[common];
        else
            hint += "\n    first extra line: " + b[common];
        return hint;
    }

    return "outputs differ only in trailing newline handling";
}

bool readFile(const std::filesystem::path& path, std::string& contents)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    contents = buffer.str();
    return true;
}

// set while running a case that the corpus file marks as an expected failure
bool quietFailures = false;

void reportFailure(
    const std::string& file,
    const Config& config,
    const std::string& reason,
    const std::string* decompiled = nullptr,
    const std::string& hint = {}
)
{
    if (quietFailures)
        return;
    std::printf("FAIL %s [%s]: %s\n", file.c_str(), config.name().c_str(), reason.c_str());
    if (!hint.empty())
        std::printf("  %s\n", hint.c_str());
    if (decompiled)
        std::printf("--- decompiled source ---\n%s-------------------------\n", numbered(*decompiled).c_str());
    std::fflush(stdout);
}

bool runCase(const std::string& file, const std::string& source, const Config& config)
{
    Luau::CompileOptions options = config.options();

    std::string bytecode = Luau::compile(source, options);
    if (bytecode.empty() || bytecode[0] == 0)
    {
        reportFailure(file, config, "corpus file does not compile: " + (bytecode.empty() ? std::string() : bytecode.substr(1)));
        return false;
    }

    RunResult original = run(bytecode, "=original");

    std::string decompiled;
    try
    {
        decompiled = Luau::Decompiler::decompile(bytecode);
    }
    catch (const std::exception& e)
    {
        reportFailure(file, config, std::string("decompile threw: ") + e.what());
        return false;
    }

    std::string recompiledBytecode = Luau::compile(decompiled, options);
    if (recompiledBytecode.empty() || recompiledBytecode[0] == 0)
    {
        reportFailure(
            file,
            config,
            "decompiled source does not compile",
            &decompiled,
            recompiledBytecode.empty() ? std::string() : recompiledBytecode.substr(1)
        );
        return false;
    }

    RunResult recompiled = run(recompiledBytecode, "=recompiled");

    if (original.ok != recompiled.ok)
    {
        std::string hint = original.ok ? "recompiled error: " + recompiled.error : "original error: " + original.error;
        reportFailure(file, config, original.ok ? "recompiled program errors" : "recompiled program does not error", &decompiled, hint);
        return false;
    }

    // error messages are not compared since they embed chunk names and line numbers; output printed before the error still must match
    if (original.output != recompiled.output)
    {
        reportFailure(file, config, "output mismatch", &decompiled, firstDifference(original.output, recompiled.output));
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <corpus-dir> [name-filter]\n", argv[0]);
        return 2;
    }

    std::filesystem::path corpus = argv[1];
    std::string filter = argc > 2 ? argv[2] : "";

    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(corpus, ec))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".luau" &&
            entry.path().filename().string().find(filter) != std::string::npos)
            files.push_back(entry.path());
    }
    if (ec)
    {
        std::fprintf(stderr, "cannot read corpus directory %s: %s\n", corpus.string().c_str(), ec.message().c_str());
        return 2;
    }
    std::sort(files.begin(), files.end());

    if (files.empty())
    {
        std::fprintf(stderr, "no .luau files matching '%s' found in %s\n", filter.c_str(), corpus.string().c_str());
        return 2;
    }

    std::vector<Config> configs;
    for (int opt = 0; opt <= 2; ++opt)
        for (int debug = 0; debug <= 2; ++debug)
            configs.push_back({opt, debug, 0});
    configs.push_back({2, 1, 1});

    int passed = 0;
    int failed = 0;
    int expectedFailures = 0;

    for (const auto& path : files)
    {
        std::string file = path.filename().string();
        std::string source;
        if (!readFile(path, source))
        {
            std::printf("FAIL %s: cannot read file\n", file.c_str());
            failed += int(configs.size());
            continue;
        }

        // `-- roundtrip: xfail O2` documents a known decompiler limitation for that optimization level
        bool xfailO2 = source.find("-- roundtrip: xfail O2") != std::string::npos;

        for (const Config& config : configs)
        {
            bool xfail = xfailO2 && config.optimizationLevel == 2;
            quietFailures = xfail;
            bool ok = runCase(file, source, config);
            quietFailures = false;

            if (xfail && ok)
            {
                std::printf("XPASS %s [%s]: remove the xfail marker\n", file.c_str(), config.name().c_str());
                ++failed;
            }
            else if (xfail)
                ++expectedFailures;
            else if (ok)
                ++passed;
            else
                ++failed;
        }
    }

    std::printf("%d passed, %d failed, %d expected failures\n", passed, failed, expectedFailures);
    return failed == 0 ? 0 : 1;
}
