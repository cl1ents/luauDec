#include "Decompiler/Emitter.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace Luau::Decompiler
{

namespace
{

constexpr int kPrecLowest = 0;
constexpr int kPrecMul = 6;
constexpr int kPrecUnary = 7;
constexpr int kPrecAtom = 10;

constexpr size_t kMaxInlineTableLength = 100;

int binaryPrecedence(AstExprBinary::Op op)
{
    switch (op)
    {
    case AstExprBinary::Or:
        return 1;
    case AstExprBinary::And:
        return 2;
    case AstExprBinary::CompareNe:
    case AstExprBinary::CompareEq:
    case AstExprBinary::CompareLt:
    case AstExprBinary::CompareLe:
    case AstExprBinary::CompareGt:
    case AstExprBinary::CompareGe:
        return 3;
    case AstExprBinary::Concat:
        return 4;
    case AstExprBinary::Add:
    case AstExprBinary::Sub:
        return 5;
    case AstExprBinary::Mul:
    case AstExprBinary::Div:
    case AstExprBinary::FloorDiv:
    case AstExprBinary::Mod:
        return kPrecMul;
    case AstExprBinary::Pow:
        return 8;
    default:
        throw std::logic_error("emit: unknown binary operator");
    }
}

const char* binaryToken(AstExprBinary::Op op)
{
    switch (op)
    {
    case AstExprBinary::Add:
        return "+";
    case AstExprBinary::Sub:
        return "-";
    case AstExprBinary::Mul:
        return "*";
    case AstExprBinary::Div:
        return "/";
    case AstExprBinary::FloorDiv:
        return "//";
    case AstExprBinary::Mod:
        return "%";
    case AstExprBinary::Pow:
        return "^";
    case AstExprBinary::Concat:
        return "..";
    case AstExprBinary::CompareNe:
        return "~=";
    case AstExprBinary::CompareEq:
        return "==";
    case AstExprBinary::CompareLt:
        return "<";
    case AstExprBinary::CompareLe:
        return "<=";
    case AstExprBinary::CompareGt:
        return ">";
    case AstExprBinary::CompareGe:
        return ">=";
    case AstExprBinary::And:
        return "and";
    case AstExprBinary::Or:
        return "or";
    default:
        throw std::logic_error("emit: unknown binary operator");
    }
}

bool isRightAssociative(AstExprBinary::Op op)
{
    return op == AstExprBinary::Concat || op == AstExprBinary::Pow;
}

bool isKeyword(std::string_view s)
{
    static constexpr std::string_view kKeywords[] = {
        "and",   "break", "do",  "else", "elseif", "end",    "false",  "for",  "function", "if",   "in",
        "local", "nil",   "not", "or",   "repeat", "return", "then",   "true", "until",    "while",
    };

    for (std::string_view keyword : kKeywords)
        if (s == keyword)
            return true;

    return false;
}

bool isIdentifier(std::string_view s)
{
    if (s.empty())
        return false;

    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        bool digit = c >= '0' && c <= '9';

        if (!alpha && !(digit && i > 0))
            return false;
    }

    return !isKeyword(s);
}

std::string formatNumber(double value)
{
    if (std::isnan(value))
        return "0/0";

    if (std::isinf(value))
        return value > 0 ? "math.huge" : "-math.huge";

    char buffer[64];

    if (value == std::floor(value) && std::fabs(value) < 9007199254740992.0)
    {
        if (value == 0 && std::signbit(value))
            return "-0";

        snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
        return buffer;
    }

    for (int precision = 1; precision <= 17; ++precision)
    {
        snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        if (strtod(buffer, nullptr) == value)
            break;
    }

    return buffer;
}

std::string formatInteger(int64_t value)
{
    char buffer[32];

    // The magnitude of INT64_MIN is not representable as a decimal literal; hex literals cover the full bit pattern.
    if (value == INT64_MIN)
        return "0x8000000000000000i";

    if (value < 0)
        snprintf(buffer, sizeof(buffer), "-%" PRId64 "i", -value);
    else
        snprintf(buffer, sizeof(buffer), "%" PRId64 "i", value);

    return buffer;
}

void appendEscaped(std::string& out, const char* data, size_t size, bool interpolated)
{
    for (size_t i = 0; i < size; ++i)
    {
        unsigned char c = static_cast<unsigned char>(data[i]);

        switch (c)
        {
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\a':
            out += "\\a";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\v':
            out += "\\v";
            break;
        case '"':
            out += interpolated ? "\"" : "\\\"";
            break;
        case '`':
            out += interpolated ? "\\`" : "`";
            break;
        case '{':
            out += interpolated ? "\\{" : "{";
            break;
        default:
            if (c < 0x20 || c == 0x7F)
            {
                char buffer[8];
                snprintf(buffer, sizeof(buffer), "\\%03u", unsigned(c));
                out += buffer;
            }
            else
            {
                out += char(c);
            }
            break;
        }
    }
}

std::string quoteString(const char* data, size_t size)
{
    std::string out = "\"";
    appendEscaped(out, data, size, false);
    out += '"';
    return out;
}

void appendIndent(std::string& out, int indent)
{
    out.append(size_t(indent), '\t');
}

bool isPrefixExpression(AstExpr* expr)
{
    return expr->is<AstExprLocal>() || expr->is<AstExprGlobal>() || expr->is<AstExprCall>() || expr->is<AstExprIndexName>() ||
           expr->is<AstExprIndexExpr>() || expr->is<AstExprGroup>();
}

int precedence(AstExpr* expr)
{
    if (AstExprBinary* binary = expr->as<AstExprBinary>())
        return binaryPrecedence(binary->op);

    if (expr->is<AstExprUnary>())
        return kPrecUnary;

    if (AstExprConstantNumber* number = expr->as<AstExprConstantNumber>())
    {
        if (std::isnan(number->value))
            return kPrecMul;

        return std::signbit(number->value) ? kPrecUnary : kPrecAtom;
    }

    if (AstExprConstantInteger* integer = expr->as<AstExprConstantInteger>())
        return integer->value < 0 && integer->value != INT64_MIN ? kPrecUnary : kPrecAtom;

    if (expr->is<AstExprIfElse>() || expr->is<AstExprFunction>())
        return kPrecLowest;

    return kPrecAtom;
}

class Emitter
{
public:
    explicit Emitter(const std::vector<std::string>& comments)
        : comments(comments)
    {
    }

    void block(std::string& out, AstStatBlock* block, int indent)
    {
        bool outer = atBlockStart;
        atBlockStart = true;
        for (AstStat* stat : block->body)
        {
            statement(out, stat, indent);
            atBlockStart = false;
        }
        atBlockStart = outer;
    }

private:
    const std::vector<std::string>& comments;
    // Luau rejects a leading `;` at the start of a block, and only needs it after a statement that could continue as a call
    bool atBlockStart = true;

    void line(std::string& out, int indent, const std::string& text)
    {
        appendIndent(out, indent);
        if (!text.empty() && text[0] == '(' && !atBlockStart)
            out += ';';
        out += text;
        out += '\n';
    }

    void statement(std::string& out, AstStat* stat, int indent)
    {
        if (AstStatBlock* inner = stat->as<AstStatBlock>())
        {
            line(out, indent, "do");
            block(out, inner, indent + 1);
            line(out, indent, "end");
        }
        else if (AstStatLocal* local = stat->as<AstStatLocal>())
        {
            std::string text = "local ";
            for (size_t i = 0; i < local->vars.size; ++i)
            {
                if (i > 0)
                    text += ", ";
                text += local->vars.data[i]->name.value;
            }

            if (local->values.size > 0)
                text += " = " + list(local->values, indent);

            line(out, indent, text);
        }
        else if (AstStatAssign* assign = stat->as<AstStatAssign>())
        {
            line(out, indent, list(assign->vars, indent) + " = " + list(assign->values, indent));
        }
        else if (AstStatCompoundAssign* compound = stat->as<AstStatCompoundAssign>())
        {
            line(out, indent, expr(compound->var, indent) + " " + binaryToken(compound->op) + "= " + expr(compound->value, indent));
        }
        else if (AstStatExpr* exprStat = stat->as<AstStatExpr>())
        {
            line(out, indent, expr(exprStat->expr, indent));
        }
        else if (AstStatReturn* ret = stat->as<AstStatReturn>())
        {
            line(out, indent, ret->list.size > 0 ? "return " + list(ret->list, indent) : "return");
        }
        else if (AstStatIf* ifStat = stat->as<AstStatIf>())
        {
            line(out, indent, "if " + expr(ifStat->condition, indent) + " then");
            block(out, ifStat->thenbody, indent + 1);

            AstStat* elsebody = ifStat->elsebody;
            while (AstStatIf* elseif = elsebody ? elsebody->as<AstStatIf>() : nullptr)
            {
                line(out, indent, "elseif " + expr(elseif->condition, indent) + " then");
                block(out, elseif->thenbody, indent + 1);
                elsebody = elseif->elsebody;
            }

            if (elsebody)
            {
                line(out, indent, "else");
                if (AstStatBlock* elseBlock = elsebody->as<AstStatBlock>())
                    block(out, elseBlock, indent + 1);
                else
                    statement(out, elsebody, indent + 1);
            }

            line(out, indent, "end");
        }
        else if (AstStatWhile* whileStat = stat->as<AstStatWhile>())
        {
            line(out, indent, "while " + expr(whileStat->condition, indent) + " do");
            block(out, whileStat->body, indent + 1);
            line(out, indent, "end");
        }
        else if (AstStatRepeat* repeat = stat->as<AstStatRepeat>())
        {
            line(out, indent, "repeat");
            block(out, repeat->body, indent + 1);
            line(out, indent, "until " + expr(repeat->condition, indent));
        }
        else if (AstStatFor* forStat = stat->as<AstStatFor>())
        {
            std::string text = "for ";
            text += forStat->var->name.value;
            text += " = " + expr(forStat->from, indent) + ", " + expr(forStat->to, indent);
            if (forStat->step)
                text += ", " + expr(forStat->step, indent);
            text += " do";

            line(out, indent, text);
            block(out, forStat->body, indent + 1);
            line(out, indent, "end");
        }
        else if (AstStatForIn* forIn = stat->as<AstStatForIn>())
        {
            std::string text = "for ";
            for (size_t i = 0; i < forIn->vars.size; ++i)
            {
                if (i > 0)
                    text += ", ";
                text += forIn->vars.data[i]->name.value;
            }
            text += " in " + list(forIn->values, indent) + " do";

            line(out, indent, text);
            block(out, forIn->body, indent + 1);
            line(out, indent, "end");
        }
        else if (stat->is<AstStatBreak>())
        {
            line(out, indent, "break");
        }
        else if (stat->is<AstStatContinue>())
        {
            line(out, indent, "continue");
        }
        else if (AstStatFunction* function = stat->as<AstStatFunction>())
        {
            AstExprIndexName* method = function->name->as<AstExprIndexName>();
            bool implicitSelf = method && method->op == ':';

            line(out, indent, "function " + expr(function->name, indent) + functionTail(function->func, !implicitSelf, indent));
        }
        else if (AstStatLocalFunction* localFunction = stat->as<AstStatLocalFunction>())
        {
            std::string text = "local function ";
            text += localFunction->name->name.value;
            text += functionTail(localFunction->func, true, indent);
            line(out, indent, text);
        }
        else if (AstStatError* error = stat->as<AstStatError>())
        {
            std::string_view message = error->messageIndex < comments.size() ? std::string_view(comments[error->messageIndex]) : std::string_view();

            size_t start = 0;
            while (true)
            {
                size_t end = message.find('\n', start);
                std::string_view part = message.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

                appendIndent(out, indent);
                out += part.empty() ? "--" : "-- ";
                out += part;
                out += '\n';

                if (end == std::string_view::npos)
                    break;
                start = end + 1;
            }
        }
        else
        {
            throw std::logic_error("emit: unsupported statement node");
        }
    }

    std::string list(const AstArray<AstExpr*>& exprs, int indent)
    {
        std::string text;
        for (size_t i = 0; i < exprs.size; ++i)
        {
            if (i > 0)
                text += ", ";
            text += expr(exprs.data[i], indent);
        }
        return text;
    }

    std::string operand(AstExpr* e, int minPrecedence, int indent)
    {
        if (precedence(e) < minPrecedence)
            return "(" + expr(e, indent) + ")";

        return expr(e, indent);
    }

    std::string prefix(AstExpr* e, int indent)
    {
        if (isPrefixExpression(e))
            return expr(e, indent);

        return "(" + expr(e, indent) + ")";
    }

    // Renders `(params) <body> end` for a function whose header is printed by the caller at `indent`.
    std::string functionTail(AstExprFunction* func, bool includeSelf, int indent)
    {
        std::string text = "(";
        bool first = true;

        auto param = [&](const char* name)
        {
            if (!first)
                text += ", ";
            text += name;
            first = false;
        };

        if (includeSelf && func->self)
            param(func->self->name.value);

        for (AstLocal* arg : func->args)
            param(arg->name.value);

        if (func->vararg)
            param("...");

        text += ")";

        if (func->body->body.size == 0)
            return text + " end";

        text += '\n';
        block(text, func->body, indent + 1);
        appendIndent(text, indent);
        text += "end";
        return text;
    }

    std::string table(AstExprTable* table, int indent)
    {
        if (table->items.size == 0)
            return "{}";

        std::vector<std::string> items;
        items.reserve(table->items.size);

        bool multiline = false;
        size_t inlineLength = 2;

        for (const AstExprTable::Item& item : table->items)
        {
            std::string value = expr(item.value, indent + 1);
            std::string text;

            switch (item.kind)
            {
            case AstExprTable::Item::Kind::List:
                text = std::move(value);
                break;
            case AstExprTable::Item::Kind::Record:
                if (AstExprConstantString* key = item.key->as<AstExprConstantString>())
                {
                    std::string_view name(key->value.data, key->value.size);
                    if (isIdentifier(name))
                        text = std::string(name) + " = " + value;
                    else
                        text = "[" + quoteString(key->value.data, key->value.size) + "] = " + value;
                    break;
                }
                [[fallthrough]];
            case AstExprTable::Item::Kind::General:
                text = "[" + expr(item.key, indent + 1) + "] = " + value;
                break;
            }

            if (text.find('\n') != std::string::npos)
                multiline = true;

            inlineLength += text.size() + 2;
            items.push_back(std::move(text));
        }

        if (inlineLength > kMaxInlineTableLength)
            multiline = true;

        std::string text = "{";

        if (multiline)
        {
            text += '\n';
            for (const std::string& item : items)
            {
                appendIndent(text, indent + 1);
                text += item;
                text += ",\n";
            }
            appendIndent(text, indent);
        }
        else
        {
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (i > 0)
                    text += ", ";
                text += items[i];
            }
        }

        text += "}";
        return text;
    }

    std::string interpolatedString(AstExprInterpString* interp, int indent)
    {
        std::string text = "`";

        for (size_t i = 0; i < interp->strings.size; ++i)
        {
            const AstArray<char>& part = interp->strings.data[i];
            appendEscaped(text, part.data, part.size, true);

            if (i < interp->expressions.size)
            {
                std::string hole = expr(interp->expressions.data[i], indent);

                // `{{` is rejected by the lexer as a likely typo for an escaped brace.
                text += hole.empty() || hole[0] != '{' ? "{" : "{ ";
                text += hole;
                text += "}";
            }
        }

        text += "`";
        return text;
    }

    std::string expr(AstExpr* e, int indent)
    {
        if (AstExprGroup* group = e->as<AstExprGroup>())
            return "(" + expr(group->expr, indent) + ")";

        if (e->is<AstExprConstantNil>())
            return "nil";

        if (AstExprConstantBool* boolean = e->as<AstExprConstantBool>())
            return boolean->value ? "true" : "false";

        if (AstExprConstantNumber* number = e->as<AstExprConstantNumber>())
            return formatNumber(number->value);

        if (AstExprConstantInteger* integer = e->as<AstExprConstantInteger>())
            return formatInteger(integer->value);

        if (AstExprConstantString* string = e->as<AstExprConstantString>())
            return quoteString(string->value.data, string->value.size);

        if (AstExprLocal* local = e->as<AstExprLocal>())
            return local->local->name.value;

        if (AstExprGlobal* global = e->as<AstExprGlobal>())
            return global->name.value;

        if (e->is<AstExprVarargs>())
            return "...";

        if (AstExprCall* call = e->as<AstExprCall>())
        {
            std::string text;

            AstExprIndexName* method = call->func->as<AstExprIndexName>();
            if (call->self && method)
                text = prefix(method->expr, indent) + ":" + method->index.value;
            else
                text = prefix(call->func, indent);

            return text + "(" + list(call->args, indent) + ")";
        }

        if (AstExprIndexName* index = e->as<AstExprIndexName>())
        {
            std::string object = prefix(index->expr, indent);

            if (index->op == ':')
                return object + ":" + index->index.value;

            std::string_view name = index->index.value;
            if (isIdentifier(name))
                return object + "." + index->index.value;

            return object + "[" + quoteString(name.data(), name.size()) + "]";
        }

        if (AstExprIndexExpr* index = e->as<AstExprIndexExpr>())
            return prefix(index->expr, indent) + "[" + expr(index->index, indent) + "]";

        if (AstExprFunction* function = e->as<AstExprFunction>())
            return "function" + functionTail(function, true, indent);

        if (AstExprTable* tableExpr = e->as<AstExprTable>())
            return table(tableExpr, indent);

        if (AstExprUnary* unary = e->as<AstExprUnary>())
        {
            std::string inner = operand(unary->expr, kPrecUnary, indent);

            switch (unary->op)
            {
            case AstExprUnary::Op::Not:
                return "not " + inner;
            case AstExprUnary::Op::Minus:
                return inner[0] == '-' ? "- " + inner : "-" + inner;
            case AstExprUnary::Op::Len:
                return "#" + inner;
            }
        }

        if (AstExprBinary* binary = e->as<AstExprBinary>())
        {
            int prec = binaryPrecedence(binary->op);
            bool right = isRightAssociative(binary->op);

            std::string text = operand(binary->left, right ? prec + 1 : prec, indent);
            text += " ";
            text += binaryToken(binary->op);
            text += " ";

            // A unary right operand binds only its own operand, so it never needs grouping there: `2 ^ -1`, `a - -b`.
            if (precedence(binary->right) == kPrecUnary)
                text += expr(binary->right, indent);
            else
                text += operand(binary->right, right ? prec : prec + 1, indent);

            return text;
        }

        if (AstExprIfElse* ifElse = e->as<AstExprIfElse>())
        {
            std::string text = "if " + expr(ifElse->condition, indent) + " then " + expr(ifElse->trueExpr, indent);

            AstExpr* falseExpr = ifElse->falseExpr;
            while (AstExprIfElse* elseif = falseExpr->as<AstExprIfElse>())
            {
                text += " elseif " + expr(elseif->condition, indent) + " then " + expr(elseif->trueExpr, indent);
                falseExpr = elseif->falseExpr;
            }

            return text + " else " + expr(falseExpr, indent);
        }

        if (AstExprInterpString* interp = e->as<AstExprInterpString>())
            return interpolatedString(interp, indent);

        throw std::logic_error("emit: unsupported expression node");
    }
};

} // namespace

std::string emit(AstStatBlock& root, const std::vector<std::string>& comments)
{
    Emitter emitter(comments);

    std::string out;
    emitter.block(out, &root, 0);
    return out;
}

} // namespace Luau::Decompiler
