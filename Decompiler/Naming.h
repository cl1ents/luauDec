#pragma once

#include "Luau/Ast.h"

#include <initializer_list>
#include <string>
#include <unordered_set>
#include <vector>

namespace Luau
{
class Allocator;
}

namespace Luau::Decompiler
{

// Hands out local variable names that never shadow a visible local or capture a global referenced anywhere in the chunk.
class Namer
{
public:
    Namer(Allocator& allocator, std::unordered_set<std::string> reserved);

    void pushScope();
    void popScope();

    // Registers a fresh name derived from `base` in the innermost scope; returns storage that lives as long as the allocator.
    const char* declare(const std::string& base);

    // The first candidate not yet visible, or the first candidate when all are taken (declare will suffix it).
    std::string firstAvailable(std::initializer_list<const char*> candidates) const;

private:
    bool isTaken(const std::string& name) const;

    Allocator& allocator;
    std::unordered_set<std::string> reserved;
    std::vector<std::vector<std::string>> scopes;
    std::unordered_set<std::string> visible;
};

// A descriptive base name for a local initialized with `init`, or "" when nothing better than a generic name applies.
std::string suggestName(AstExpr* init);

// Names for generic for-loop variables, derived from the iterated expressions.
std::vector<std::string> suggestLoopNames(const std::vector<AstExpr*>& values, size_t count);

bool isValidIdentifier(const std::string& name);

} // namespace Luau::Decompiler
