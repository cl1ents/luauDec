#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace Luau::Decompiler
{

// Raised for bytecode that cannot be decompiled at all: compile-error blobs, unsupported versions, truncated data.
struct DecompileError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Decompiles a serialized Luau bytecode blob (versions LBC_VERSION_MIN..LBC_VERSION_MAX) back into Luau source.
std::string decompile(std::string_view bytecode);

} // namespace Luau::Decompiler
