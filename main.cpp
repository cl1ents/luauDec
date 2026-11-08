#include "luauDec/Decompile.h"

#include "Luau/Compiler.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

static void usage(const char* argv0)
{
    std::cerr << "Usage: " << argv0 << " [-O<n>] [-g<n>] <file>\n"
              << "  <file> is Luau bytecode, or Luau source which is compiled first (-O/-g select the compile options).\n"
              << "  Use - to read from stdin.\n";
}

static bool looksLikeBytecode(const std::string& data)
{
    if (data.empty())
        return false;
    unsigned char version = static_cast<unsigned char>(data[0]);
    // source text starts with printable characters; bytecode with a small version byte (0 = compile error blob)
    return version < 32 && version != '\t' && version != '\n' && version != '\r';
}

int main(int argc, char** argv)
{
    Luau::CompileOptions options;
    const char* path = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "-O", 2) == 0 && argv[i][2])
            options.optimizationLevel = atoi(argv[i] + 2);
        else if (strncmp(argv[i], "-g", 2) == 0 && argv[i][2])
            options.debugLevel = atoi(argv[i] + 2);
        else if (!path)
            path = argv[i];
        else
        {
            usage(argv[0]);
            return 2;
        }
    }

    if (!path)
    {
        usage(argv[0]);
        return 2;
    }

    std::string data;
    if (strcmp(path, "-") == 0)
        data.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    else
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "luauDec: cannot open " << path << "\n";
            return 1;
        }
        data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::string bytecode = looksLikeBytecode(data) ? data : Luau::compile(data, options);

    try
    {
        std::cout << Luau::Decompiler::decompile(bytecode);
    }
    catch (const Luau::Decompiler::DecompileError& e)
    {
        std::cerr << "luauDec: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
