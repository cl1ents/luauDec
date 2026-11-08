#include "Decompiler/Naming.h"

#include "Luau/Allocator.h"

#include <cctype>
#include <cstring>

namespace Luau::Decompiler
{

namespace
{

const std::unordered_set<std::string> keywords = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while", "continue", "export", "type", "typeof",
};

std::string lowerFirst(std::string s)
{
    if (!s.empty())
        s[0] = char(tolower((unsigned char)s[0]));
    return s;
}

std::string stringArg(AstExprCall* call, size_t index = 0)
{
    if (call->args.size > index)
        if (auto s = call->args.data[index]->as<AstExprConstantString>())
            return std::string(s->value.data, s->value.size);
    return "";
}

std::string calleeName(AstExpr* func)
{
    if (auto g = func->as<AstExprGlobal>())
        return g->name.value;
    if (auto i = func->as<AstExprIndexName>())
        return i->index.value;
    return "";
}

std::string fromCall(AstExprCall* call)
{
    std::string name = calleeName(call->func);

    if (name == "GetService" || name == "FindFirstChild" || name == "WaitForChild" || name == "FindFirstChildOfClass" ||
        name == "FindFirstChildWhichIsA" || name == "FindFirstAncestor")
        return stringArg(call);

    if (name == "require" && call->args.size == 1)
    {
        if (auto i = call->args.data[0]->as<AstExprIndexName>())
            return i->index.value;
        std::string path = stringArg(call);
        size_t slash = path.find_last_of("/.");
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }

    if (name == "new")
    {
        std::string cls = stringArg(call);
        if (!cls.empty())
            return lowerFirst(cls);
        if (auto i = call->func->as<AstExprIndexName>())
            if (auto g = i->expr->as<AstExprGlobal>())
                return lowerFirst(g->name.value);
    }

    for (const char* prefix : {"Get", "get", "Find", "find", "Create", "create", "Load", "load"})
    {
        size_t len = strlen(prefix);
        if (name.size() > len && name.compare(0, len, prefix) == 0 && isupper((unsigned char)name[len]))
            return lowerFirst(name.substr(len));
    }

    return "";
}

} // namespace

bool isValidIdentifier(const std::string& name)
{
    if (name.empty() || name.size() > 40 || isdigit((unsigned char)name[0]))
        return false;
    for (char c : name)
        if (!isalnum((unsigned char)c) && c != '_')
            return false;
    return keywords.count(name) == 0;
}

Namer::Namer(Allocator& allocator, std::unordered_set<std::string> reserved)
    : allocator(allocator)
    , reserved(std::move(reserved))
{
    scopes.emplace_back();
}

void Namer::pushScope()
{
    scopes.emplace_back();
}

void Namer::popScope()
{
    for (const std::string& name : scopes.back())
        visible.erase(name);
    scopes.pop_back();
}

bool Namer::isTaken(const std::string& name) const
{
    return visible.count(name) || reserved.count(name) || keywords.count(name);
}

std::string Namer::firstAvailable(std::initializer_list<const char*> candidates) const
{
    for (const char* candidate : candidates)
        if (!isTaken(candidate))
            return candidate;
    return *candidates.begin();
}

const char* Namer::declare(const std::string& requested)
{
    bool generic = !isValidIdentifier(requested);
    std::string base = generic ? "v" : requested;

    std::string name = base;
    if (generic || isTaken(name))
    {
        for (int i = base == "v" ? 1 : 2;; ++i)
        {
            name = base + std::to_string(i);
            if (!isTaken(name))
                break;
        }
    }

    visible.insert(name);
    scopes.back().push_back(name);

    char* storage = static_cast<char*>(allocator.allocate(name.size() + 1));
    memcpy(storage, name.c_str(), name.size() + 1);
    return storage;
}

std::string suggestName(AstExpr* init)
{
    if (!init)
        return "";
    if (auto g = init->as<AstExprGroup>())
        return suggestName(g->expr);
    if (auto call = init->as<AstExprCall>())
        return fromCall(call);
    if (auto i = init->as<AstExprIndexName>())
        return i->index.value;
    if (auto g = init->as<AstExprGlobal>())
        return g->name.value;
    if (init->is<AstExprTable>())
        return "tbl";
    if (init->is<AstExprConstantString>() || init->is<AstExprInterpString>())
        return "str";
    if (init->is<AstExprConstantBool>())
        return "flag";
    if (auto f = init->as<AstExprFunction>())
        return f->debugname.value && *f->debugname.value ? f->debugname.value : "fn";
    if (auto u = init->as<AstExprUnary>(); u && u->op == AstExprUnary::Op::Len)
        return "count";
    if (auto b = init->as<AstExprBinary>(); b && b->op == AstExprBinary::Concat)
        return "str";
    return "";
}

std::vector<std::string> suggestLoopNames(const std::vector<AstExpr*>& values, size_t count)
{
    std::string iter;
    if (!values.empty())
        if (auto call = values[0]->as<AstExprCall>())
            iter = calleeName(call->func);

    std::vector<std::string> names;
    if (count == 1)
        names = {"v"};
    else if (count == 2)
    {
        names = {iter == "ipairs" ? "i" : "k", "v"};
        if (!values.empty())
            if (auto call = values[0]->as<AstExprCall>(); call && call->args.size >= 1)
                if (auto inner = call->args.data[0]->as<AstExprCall>())
                {
                    std::string source = calleeName(inner->func);
                    if (source == "GetChildren")
                        names[1] = "child";
                    else if (source == "GetDescendants")
                        names[1] = "descendant";
                    else if (source == "GetPlayers")
                        names[1] = "player";
                }
    }
    else
        for (size_t i = 0; i < count; ++i)
            names.push_back("v");
    return names;
}

} // namespace Luau::Decompiler
