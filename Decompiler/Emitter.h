#pragma once

#include "Luau/Ast.h"

#include <string>
#include <vector>

namespace Luau::Decompiler
{

// Renders a decompiled AST as Luau source text.
//
// Luau's own prettyPrint relies on source locations for whitespace, indentation and grouping, which a synthesized AST does not have;
// the emitter derives layout from the tree instead and inserts the parentheses that operator precedence requires.
//
// AstStatError nodes are rendered as `-- <comments[messageIndex]>` comment lines.
std::string emit(AstStatBlock& root, const std::vector<std::string>& comments);

} // namespace Luau::Decompiler
