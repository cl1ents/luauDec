#include "Decompiler/BytecodeReader.h"

#include "luauDec/Decompile.h"

#include "Luau/Bytecode.h"

#include <cstring>

namespace Luau::Decompiler
{

namespace
{

class Reader
{
public:
    explicit Reader(std::string_view data)
        : data(data)
    {
    }

    template<typename T>
    T read()
    {
        need(sizeof(T));
        T result;
        memcpy(&result, data.data() + offset, sizeof(T));
        offset += sizeof(T);
        return result;
    }

    uint64_t readVarInt64()
    {
        uint64_t result = 0;
        unsigned shift = 0;
        uint8_t byte;

        do
        {
            if (shift >= 64)
                throw DecompileError("malformed bytecode: varint overflow");
            byte = read<uint8_t>();
            result |= uint64_t(byte & 127) << shift;
            shift += 7;
        } while (byte & 128);

        return result;
    }

    uint32_t readVarInt()
    {
        uint64_t value = readVarInt64();
        if (value > UINT32_MAX)
            throw DecompileError("malformed bytecode: varint overflow");
        return uint32_t(value);
    }

    std::string_view readBytes(size_t length)
    {
        need(length);
        std::string_view result = data.substr(offset, length);
        offset += length;
        return result;
    }

    void skip(size_t length)
    {
        need(length);
        offset += length;
    }

    void seek(size_t position)
    {
        if (position > data.size())
            throw DecompileError("malformed bytecode: truncated data");
        offset = position;
    }

    size_t position() const
    {
        return offset;
    }

private:
    void need(size_t length) const
    {
        if (data.size() - offset < length)
            throw DecompileError("malformed bytecode: truncated data");
    }

    std::string_view data;
    size_t offset = 0;
};

std::optional<std::string> readString(Reader& reader, const std::vector<std::string>& strings)
{
    uint32_t id = reader.readVarInt();
    if (id == 0)
        return std::nullopt;
    if (id > strings.size())
        throw DecompileError("malformed bytecode: string index out of range");
    return strings[id - 1];
}

const std::string& constantString(const FunctionProto& proto, uint32_t index)
{
    if (index >= proto.constants.size() || proto.constants[index].kind != Constant::Kind::String)
        throw DecompileError("malformed bytecode: expected a string constant");
    return proto.constants[index].string;
}

void readConstant(Reader& reader, const std::vector<std::string>& strings, FunctionProto& proto, Constant& k)
{
    uint8_t tag = reader.read<uint8_t>();

    switch (tag)
    {
    case LBC_CONSTANT_NIL:
        k.kind = Constant::Kind::Nil;
        break;

    case LBC_CONSTANT_BOOLEAN:
        k.kind = Constant::Kind::Boolean;
        k.boolean = reader.read<uint8_t>() != 0;
        break;

    case LBC_CONSTANT_NUMBER:
        k.kind = Constant::Kind::Number;
        k.number = reader.read<double>();
        break;

    case LBC_CONSTANT_INTEGER:
    {
        k.kind = Constant::Kind::Integer;
        bool negative = reader.read<uint8_t>() != 0;
        uint64_t magnitude = reader.readVarInt64();
        k.integer = negative ? int64_t(~magnitude + 1) : int64_t(magnitude);
        break;
    }

    case LBC_CONSTANT_VECTOR:
        k.kind = Constant::Kind::Vector;
        for (double& component : k.vector)
            component = reader.read<float>();
        break;

    case LBC_CONSTANT_VECTORD:
        k.kind = Constant::Kind::Vector;
        for (double& component : k.vector)
            component = reader.read<double>();
        break;

    case LBC_CONSTANT_STRING:
    {
        k.kind = Constant::Kind::String;
        std::optional<std::string> value = readString(reader, strings);
        if (!value)
            throw DecompileError("malformed bytecode: null string constant");
        k.string = std::move(*value);
        break;
    }

    case LBC_CONSTANT_IMPORT:
    {
        k.kind = Constant::Kind::Import;
        uint32_t id = reader.read<uint32_t>();
        uint32_t count = id >> 30;
        uint32_t ids[3] = {(id >> 20) & 1023, (id >> 10) & 1023, id & 1023};
        for (uint32_t i = 0; i < count; ++i)
            k.importPath.push_back(constantString(proto, ids[i]));
        break;
    }

    case LBC_CONSTANT_TABLE:
    {
        k.kind = Constant::Kind::Table;
        uint32_t keys = reader.readVarInt();
        for (uint32_t i = 0; i < keys; ++i)
        {
            k.templateKeys.push_back(reader.readVarInt());
            k.templateValues.push_back(-1);
        }
        break;
    }

    case LBC_CONSTANT_TABLE_WITH_CONSTANTS:
    {
        k.kind = Constant::Kind::Table;
        uint32_t keys = reader.readVarInt();
        for (uint32_t i = 0; i < keys; ++i)
        {
            k.templateKeys.push_back(reader.readVarInt());
            k.templateValues.push_back(reader.read<int32_t>());
        }
        break;
    }

    case LBC_CONSTANT_CLOSURE:
        k.kind = Constant::Kind::Closure;
        k.proto = reader.readVarInt();
        break;

    case LBC_CONSTANT_CLASS_SHAPE:
    {
        k.kind = Constant::Kind::ClassShape;
        reader.readVarInt(); // class name
        uint8_t privateFields = reader.read<uint8_t>();
        uint8_t privateMethods = reader.read<uint8_t>();
        uint32_t properties = reader.readVarInt();
        uint32_t methods = reader.readVarInt();
        uint32_t members = properties + methods - privateFields - privateMethods;
        for (uint32_t i = 0; i < members; ++i)
            reader.readVarInt();
        break;
    }

    default:
        throw DecompileError("malformed bytecode: unknown constant kind " + std::to_string(tag));
    }
}

void readProto(Reader& reader, const Chunk& chunk, const std::vector<std::string>& strings, FunctionProto& proto)
{
    size_t protoEnd = 0;
    if (chunk.version >= 12)
    {
        uint32_t protoSize = reader.readVarInt();
        protoEnd = reader.position() + protoSize;
    }

    proto.maxStackSize = reader.read<uint8_t>();
    proto.numParams = reader.read<uint8_t>();
    proto.numUpvalues = reader.read<uint8_t>();
    proto.isVararg = reader.read<uint8_t>() != 0;

    if (chunk.version >= 4)
    {
        proto.flags = reader.read<uint8_t>();
        if (chunk.typesVersion >= 1)
            reader.skip(reader.readVarInt());
    }

    uint32_t sizecode = reader.readVarInt();
    proto.code.resize(sizecode);
    for (uint32_t& insn : proto.code)
        insn = reader.read<uint32_t>();

    uint32_t sizek = reader.readVarInt();
    proto.constants.resize(sizek);
    for (Constant& k : proto.constants)
        readConstant(reader, strings, proto, k);

    uint32_t sizep = reader.readVarInt();
    proto.children.resize(sizep);
    for (uint32_t& child : proto.children)
    {
        child = reader.readVarInt();
        if (child >= proto.id)
            throw DecompileError("malformed bytecode: child proto must precede its parent");
    }

    reader.readVarInt(); // linedefined
    proto.debugName = readString(reader, strings);

    if (reader.read<uint8_t>() != 0)
    {
        uint8_t linegaplog2 = reader.read<uint8_t>();
        if (linegaplog2 > 31)
            throw DecompileError("malformed bytecode: bad line gap");
        size_t intervals = sizecode == 0 ? 0 : ((sizecode - 1) >> linegaplog2) + 1;
        std::vector<uint8_t> deltas(sizecode);
        uint8_t last = 0;
        for (uint8_t& delta : deltas)
            delta = last = uint8_t(last + reader.read<uint8_t>());

        std::vector<int32_t> baselines(intervals);
        int32_t line = 0;
        for (int32_t& baseline : baselines)
            baseline = line += reader.read<int32_t>();

        proto.lines.resize(sizecode);
        for (uint32_t pc = 0; pc < sizecode; ++pc)
            proto.lines[pc] = baselines[pc >> linegaplog2] + deltas[pc];
    }

    if (reader.read<uint8_t>() != 0)
    {
        proto.hasDebugInfo = true;

        uint32_t sizelocvars = reader.readVarInt();
        proto.locals.resize(sizelocvars);
        for (LocalVarInfo& local : proto.locals)
        {
            local.name = readString(reader, strings).value_or("");
            local.startpc = reader.readVarInt();
            local.endpc = reader.readVarInt();
            local.reg = reader.read<uint8_t>();
        }

        uint32_t sizeupvalues = reader.readVarInt();
        proto.upvalueNames.resize(sizeupvalues);
        for (std::string& name : proto.upvalueNames)
            name = readString(reader, strings).value_or("");
    }

    if (chunk.version >= 12)
    {
        reader.seek(protoEnd);
        return;
    }

    if (chunk.version >= 11)
    {
        uint32_t slots = reader.readVarInt();
        for (uint32_t i = 0; i < slots; ++i)
        {
            uint8_t kind = reader.read<uint8_t>();
            if (kind != LFT_CALLTARGET && kind != LFT_CONSTRUCT)
                throw DecompileError("malformed bytecode: unknown feedback slot kind");
            reader.readVarInt();
        }
    }
}

} // namespace

Chunk readBytecode(std::string_view data)
{
    Reader reader(data);
    Chunk chunk;

    chunk.version = reader.read<uint8_t>();

    if (chunk.version == 0)
        throw DecompileError("bytecode contains a compile error: " + std::string(data.substr(1)));

    if ((chunk.version < LBC_VERSION_MIN || chunk.version > LBC_VERSION_MAX) && chunk.version != LBC_VERSION_CLASSES)
        throw DecompileError(
            "unsupported bytecode version " + std::to_string(chunk.version) + " (supported: " + std::to_string(LBC_VERSION_MIN) + ".." +
            std::to_string(LBC_VERSION_MAX) + ")"
        );

    if (chunk.version >= 4)
    {
        chunk.typesVersion = reader.read<uint8_t>();
        if (chunk.typesVersion < LBC_TYPE_VERSION_MIN || chunk.typesVersion > LBC_TYPE_VERSION_MAX)
            throw DecompileError("unsupported bytecode type information version " + std::to_string(chunk.typesVersion));
    }

    std::vector<std::string> strings(reader.readVarInt());
    for (std::string& s : strings)
        s = std::string(reader.readBytes(reader.readVarInt()));

    if (chunk.typesVersion == 3)
    {
        // userdata type names: (index, name) pairs terminated by index 0
        while (reader.read<uint8_t>() != 0)
            reader.readVarInt();
    }

    chunk.protos.resize(reader.readVarInt());
    for (uint32_t i = 0; i < chunk.protos.size(); ++i)
    {
        chunk.protos[i].id = i;
        readProto(reader, chunk, strings, chunk.protos[i]);
    }

    chunk.mainProto = reader.readVarInt();
    if (chunk.mainProto >= chunk.protos.size())
        throw DecompileError("malformed bytecode: main proto index out of range");

    for (const FunctionProto& proto : chunk.protos)
        for (const Constant& k : proto.constants)
            if (k.kind == Constant::Kind::Closure && k.proto >= chunk.protos.size())
                throw DecompileError("malformed bytecode: closure constant out of range");

    return chunk;
}

} // namespace Luau::Decompiler
