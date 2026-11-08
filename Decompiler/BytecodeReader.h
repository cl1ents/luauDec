#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Luau::Decompiler
{

struct Constant
{
    enum class Kind
    {
        Nil,
        Boolean,
        Number,
        Integer,
        Vector,
        String,
        Import,
        Table,
        Closure,
        ClassShape,
    };

    Kind kind = Kind::Nil;

    bool boolean = false;
    double number = 0;
    int64_t integer = 0;
    double vector[4] = {};
    std::string string;

    // Import: global path, e.g. {"math", "floor"}
    std::vector<std::string> importPath;

    // Table: keys (constant indices) in serialized order; templateValues[i] is the constant index pre-filled for keys[i] or -1
    std::vector<uint32_t> templateKeys;
    std::vector<int32_t> templateValues;

    // Closure: proto id
    uint32_t proto = 0;
};

struct LocalVarInfo
{
    std::string name;
    uint32_t startpc = 0;
    uint32_t endpc = 0;
    uint8_t reg = 0;
};

struct FunctionProto
{
    uint32_t id = 0;
    uint8_t maxStackSize = 0;
    uint8_t numParams = 0;
    uint8_t numUpvalues = 0;
    bool isVararg = false;
    uint8_t flags = 0;

    std::vector<uint32_t> code;
    std::vector<Constant> constants;
    std::vector<uint32_t> children; // proto ids, indexed by NEWCLOSURE's D operand

    std::optional<std::string> debugName;

    bool hasDebugInfo = false;
    std::vector<LocalVarInfo> locals;
    std::vector<std::string> upvalueNames;

    std::vector<int> lines; // source line per instruction word; empty without line info
};

struct Chunk
{
    uint8_t version = 0;
    uint8_t typesVersion = 0;
    std::vector<FunctionProto> protos;
    uint32_t mainProto = 0;
};

// Parses a serialized bytecode blob; throws DecompileError on compile-error blobs, unsupported versions or malformed data.
Chunk readBytecode(std::string_view data);

} // namespace Luau::Decompiler
