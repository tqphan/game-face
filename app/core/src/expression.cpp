#include "game_face/core/expression.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <locale>
#include <optional>
#include <sstream>
#include <vector>
#include <version>

namespace game_face {

namespace {

enum class Op {
    Add, Sub, Mul, Div, Mod, Rem, Pow,
    Eq, Ne, Lt, Le, Gt, Ge,
};

enum class Function { Abs, Ceil, Floor, Log, Log2, Log10, Max, Min, Round, Sqrt };

bool truthy(double v)
{
    return v != 0.0 && !std::isnan(v);
}

} // namespace

struct Expression::Node {
    enum class Kind { Number, Variable, Negate, Not, Binary, Compare, And, Or, Ternary, In, NotIn, Call };

    Kind kind;
    double number = 0;
    std::size_t variable = 0;
    Op op = Op::Add;
    std::vector<Op> compare_ops;  // Compare: one per adjacent operand pair
    Function function = Function::Abs;
    std::vector<std::unique_ptr<Node>> children;
};

namespace {

using Node = Expression::Node;
using NodePtr = std::unique_ptr<Node>;

NodePtr makeNode(Node::Kind kind)
{
    auto node = std::make_unique<Node>();
    node->kind = kind;
    return node;
}

NodePtr makeNode(Node::Kind kind, NodePtr a, NodePtr b = nullptr, NodePtr c = nullptr)
{
    auto node = makeNode(kind);
    node->children.push_back(std::move(a));
    if (b)
        node->children.push_back(std::move(b));
    if (c)
        node->children.push_back(std::move(c));
    return node;
}

// ---------------------------------------------------------------- lexer

enum class TokenType { Number, Name, QuotedName, String, Symbol, End };

struct Token {
    TokenType type;
    std::string text;
    double number = 0;
    std::size_t column = 0;  // 1-based
};

bool isNameStart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

bool isNameChar(char c)
{
    return isNameStart(c) || std::isdigit(static_cast<unsigned char>(c)) || c == '.';
}

std::string at(std::size_t column, const std::string& message)
{
    return "column " + std::to_string(column) + ": " + message;
}

Result<std::vector<Token>> tokenize(std::string_view s)
{
    std::vector<Token> tokens;
    std::size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        const std::size_t column = i + 1;

        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t end = i;
            while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end])))
                ++end;
            if (end + 1 < s.size() && s[end] == '.' && std::isdigit(static_cast<unsigned char>(s[end + 1]))) {
                ++end;
                while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end])))
                    ++end;
            }
            Token token{TokenType::Number, std::string(s.substr(i, end - i)), 0, column};
#if defined(__cpp_lib_to_chars)
            std::from_chars(s.data() + i, s.data() + end, token.number);
#else
            // Apple's libc++ has no floating-point from_chars yet.
            std::istringstream in(token.text);
            in.imbue(std::locale::classic());
            in >> token.number;
#endif
            tokens.push_back(std::move(token));
            i = end;
            continue;
        }

        if (isNameStart(c)) {
            std::size_t end = i + 1;
            while (end < s.size() && isNameChar(s[end]))
                ++end;
            tokens.push_back({TokenType::Name, std::string(s.substr(i, end - i)), 0, column});
            i = end;
            continue;
        }

        if (c == '\'' || c == '"') {
            std::string text;
            std::size_t end = i + 1;
            bool closed = false;
            while (end < s.size()) {
                if (s[end] == '\\' && end + 1 < s.size()) {
                    text += s[end + 1];
                    end += 2;
                } else if (s[end] == c) {
                    closed = true;
                    ++end;
                    break;
                } else {
                    text += s[end++];
                }
            }
            if (!closed)
                return failure(at(column, "missing closing " + std::string(1, c)));
            tokens.push_back({c == '\'' ? TokenType::QuotedName : TokenType::String, text, 0, column});
            i = end;
            continue;
        }

        static constexpr std::string_view kTwoChar[] = {"==", "!=", "<=", ">=", "~="};
        bool matched = false;
        for (auto op : kTwoChar) {
            if (s.substr(i, 2) == op) {
                tokens.push_back({TokenType::Symbol, std::string(op), 0, column});
                i += 2;
                matched = true;
                break;
            }
        }
        if (matched)
            continue;

        if (s.substr(i, 2) == "&&")
            return failure(at(column, "use 'and' instead of '&&'"));
        if (s.substr(i, 2) == "||")
            return failure(at(column, "use 'or' instead of '||'"));

        switch (c) {
        case '<': case '>': case '+': case '-': case '*': case '/': case '%': case '^':
        case '(': case ')': case ',': case '?': case ':':
            tokens.push_back({TokenType::Symbol, std::string(1, c), 0, column});
            ++i;
            continue;
        case '=':
            return failure(at(column, "use '==' to compare"));
        case '!':
            return failure(at(column, "use 'not' instead of '!'"));
        case '.':
            return failure(at(column, "numbers need a leading digit, e.g. 0.5"));
        default:
            return failure(at(column, "unexpected character '" + std::string(1, c) + "'"));
        }
    }
    tokens.push_back({TokenType::End, "", 0, s.size() + 1});
    return tokens;
}

// ---------------------------------------------------------------- parser

struct FunctionInfo {
    std::string_view name;
    Function function;
    bool variadic;
};

constexpr FunctionInfo kFunctions[] = {
    {"abs", Function::Abs, false},   {"ceil", Function::Ceil, false},
    {"floor", Function::Floor, false}, {"log", Function::Log, false},
    {"log2", Function::Log2, false}, {"log10", Function::Log10, false},
    {"max", Function::Max, true},    {"min", Function::Min, true},
    {"round", Function::Round, false}, {"sqrt", Function::Sqrt, false},
};

constexpr std::string_view kKeywords[] = {"and", "or", "not", "in", "if", "then", "else", "mod", "of"};

bool isKeyword(std::string_view text)
{
    for (auto keyword : kKeywords) {
        if (text == keyword)
            return true;
    }
    return false;
}

class Parser {
public:
    Parser(std::vector<Token> tokens, std::span<const std::string_view> variables)
        : tokens_(std::move(tokens)), variables_(variables)
    {
    }

    Result<NodePtr> parse()
    {
        if (peek().type == TokenType::End)
            return failure("expression is empty");
        auto node = ternary();
        if (!node)
            return node;
        if (peek().type != TokenType::End)
            return unexpected(peek());
        return node;
    }

private:
    const Token& peek(std::size_t ahead = 0) const
    {
        return tokens_[std::min(pos_ + ahead, tokens_.size() - 1)];
    }

    const Token& next() { return tokens_[pos_ < tokens_.size() - 1 ? pos_++ : pos_]; }

    bool isName(const Token& t, std::string_view text) const
    {
        return t.type == TokenType::Name && t.text == text;
    }

    bool isSymbol(const Token& t, std::string_view text) const
    {
        return t.type == TokenType::Symbol && t.text == text;
    }

    std::unexpected<Error> unexpected(const Token& t) const
    {
        if (t.type == TokenType::End)
            return failure(at(t.column, "expression ends too early"));
        return failure(at(t.column, "unexpected '" + t.text + "'"));
    }

    std::optional<std::unexpected<Error>> expectName(std::string_view text)
    {
        if (!isName(peek(), text))
            return failure(at(peek().column, "expected '" + std::string(text) + "'"));
        next();
        return std::nullopt;
    }

    std::optional<std::unexpected<Error>> expectSymbol(std::string_view text)
    {
        if (!isSymbol(peek(), text))
            return failure(at(peek().column, "expected '" + std::string(text) + "'"));
        next();
        return std::nullopt;
    }

    // a ? b : c   (if/then/else is handled in primary)
    Result<NodePtr> ternary()
    {
        auto condition = orExpr();
        if (!condition || !isSymbol(peek(), "?"))
            return condition;
        next();
        auto yes = ternary();
        if (!yes)
            return yes;
        if (auto e = expectSymbol(":"))
            return *e;
        auto no = ternary();
        if (!no)
            return no;
        return makeNode(Node::Kind::Ternary, std::move(*condition), std::move(*yes), std::move(*no));
    }

    Result<NodePtr> ifExpr()
    {
        next();  // "if"
        auto condition = ternary();
        if (!condition)
            return condition;
        if (auto e = expectName("then"))
            return *e;
        auto yes = ternary();
        if (!yes)
            return yes;
        if (auto e = expectName("else"))
            return *e;
        auto no = ternary();
        if (!no)
            return no;
        return makeNode(Node::Kind::Ternary, std::move(*condition), std::move(*yes), std::move(*no));
    }

    Result<NodePtr> orExpr()
    {
        auto left = andExpr();
        while (left && isName(peek(), "or")) {
            next();
            auto right = andExpr();
            if (!right)
                return right;
            left = makeNode(Node::Kind::Or, std::move(*left), std::move(*right));
        }
        return left;
    }

    Result<NodePtr> andExpr()
    {
        auto left = inExpr();
        while (left && isName(peek(), "and")) {
            next();
            auto right = inExpr();
            if (!right)
                return right;
            left = makeNode(Node::Kind::And, std::move(*left), std::move(*right));
        }
        return left;
    }

    Result<NodePtr> inExpr()
    {
        auto left = relation();
        while (left) {
            Node::Kind kind;
            if (isName(peek(), "in")) {
                next();
                kind = Node::Kind::In;
            } else if (isName(peek(), "not") && isName(peek(1), "in")) {
                next();
                next();
                kind = Node::Kind::NotIn;
            } else {
                break;
            }
            auto node = makeNode(kind, std::move(*left));
            if (auto e = inList(*node))
                return *e;
            left = std::move(node);
        }
        return left;
    }

    // (a, b, c) or a single operand; appended after the tested value.
    std::optional<std::unexpected<Error>> inList(Node& node)
    {
        if (!isSymbol(peek(), "(")) {
            auto item = relation();
            if (!item)
                return std::unexpected(item.error());
            node.children.push_back(std::move(*item));
            return std::nullopt;
        }
        next();
        while (true) {
            auto item = ternary();
            if (!item)
                return std::unexpected(item.error());
            node.children.push_back(std::move(*item));
            if (isSymbol(peek(), ",")) {
                next();
                continue;
            }
            return expectSymbol(")");
        }
    }

    static std::optional<Op> compareOp(const Token& t)
    {
        if (t.type != TokenType::Symbol)
            return std::nullopt;
        if (t.text == "==") return Op::Eq;
        if (t.text == "!=") return Op::Ne;
        if (t.text == "<") return Op::Lt;
        if (t.text == "<=") return Op::Le;
        if (t.text == ">") return Op::Gt;
        if (t.text == ">=") return Op::Ge;
        return std::nullopt;
    }

    // Chained: a < b <= c means (a < b and b <= c).
    Result<NodePtr> relation()
    {
        auto first = additive();
        if (!first)
            return first;
        if (isSymbol(peek(), "~="))
            return failure(at(peek().column, "'~=' (regular expressions) isn't supported"));
        if (!compareOp(peek()))
            return first;

        auto node = makeNode(Node::Kind::Compare, std::move(*first));
        while (auto op = compareOp(peek())) {
            next();
            auto operand = additive();
            if (!operand)
                return operand;
            node->compare_ops.push_back(*op);
            node->children.push_back(std::move(*operand));
        }
        return node;
    }

    Result<NodePtr> additive()
    {
        auto left = multiplicative();
        while (left && (isSymbol(peek(), "+") || isSymbol(peek(), "-"))) {
            const Op op = next().text == "+" ? Op::Add : Op::Sub;
            auto right = multiplicative();
            if (!right)
                return right;
            left = binary(op, std::move(*left), std::move(*right));
        }
        return left;
    }

    Result<NodePtr> multiplicative()
    {
        auto left = unary();
        while (left) {
            Op op;
            if (isSymbol(peek(), "*")) op = Op::Mul;
            else if (isSymbol(peek(), "/")) op = Op::Div;
            else if (isSymbol(peek(), "%")) op = Op::Rem;
            else if (isName(peek(), "mod")) op = Op::Mod;
            else break;
            next();
            auto right = unary();
            if (!right)
                return right;
            left = binary(op, std::move(*left), std::move(*right));
        }
        return left;
    }

    // `not` and unary minus bind tighter than * but looser than ^, as in
    // filtrex: `not x > 5` is `(not x) > 5`, `-2^2` is -4.
    Result<NodePtr> unary()
    {
        if (isName(peek(), "not")) {
            next();
            auto operand = unary();
            if (!operand)
                return operand;
            return makeNode(Node::Kind::Not, std::move(*operand));
        }
        if (isSymbol(peek(), "-")) {
            next();
            auto operand = unary();
            if (!operand)
                return operand;
            return makeNode(Node::Kind::Negate, std::move(*operand));
        }
        return power();
    }

    Result<NodePtr> power()
    {
        auto base = primary();
        if (!base || !isSymbol(peek(), "^"))
            return base;
        next();
        auto exponent = unary();  // right-associative
        if (!exponent)
            return exponent;
        return binary(Op::Pow, std::move(*base), std::move(*exponent));
    }

    Result<NodePtr> primary()
    {
        const Token& t = peek();
        switch (t.type) {
        case TokenType::Number: {
            auto node = makeNode(Node::Kind::Number);
            node->number = next().number;
            return node;
        }
        case TokenType::QuotedName:
            return variable(next());
        case TokenType::String:
            return failure(at(t.column, "text values aren't supported; use 'single quotes' for names"));
        case TokenType::End:
            return unexpected(t);
        case TokenType::Symbol:
            if (t.text == "(")
                return parenthesised();
            return unexpected(t);
        case TokenType::Name:
            break;
        }

        if (t.text == "if")
            return ifExpr();
        if (isKeyword(t.text))
            return unexpected(t);
        if (isSymbol(peek(1), "("))
            return call();
        if (isName(peek(1), "of"))
            return failure(at(peek(1).column, "'of' isn't supported"));
        return variable(next());
    }

    Result<NodePtr> parenthesised()
    {
        next();  // "("
        auto inner = ternary();
        if (!inner)
            return inner;
        if (isSymbol(peek(), ","))
            return failure(at(peek().column, "lists like (a, b) are only allowed after 'in'"));
        if (auto e = expectSymbol(")"))
            return *e;
        return inner;
    }

    Result<NodePtr> call()
    {
        const Token name = next();
        next();  // "("
        const FunctionInfo* info = nullptr;
        for (const auto& f : kFunctions) {
            if (f.name == name.text)
                info = &f;
        }
        if (!info)
            return failure(at(name.column, "unknown function '" + name.text + "'"));

        auto node = makeNode(Node::Kind::Call);
        node->function = info->function;
        if (!isSymbol(peek(), ")")) {
            while (true) {
                auto arg = ternary();
                if (!arg)
                    return arg;
                node->children.push_back(std::move(*arg));
                if (!isSymbol(peek(), ","))
                    break;
                next();
            }
        }
        if (auto e = expectSymbol(")"))
            return *e;

        const std::size_t count = node->children.size();
        if (info->variadic ? count == 0 : count != 1) {
            return failure(at(name.column, name.text + "() takes " +
                                               (info->variadic ? "at least one argument" : "one argument")));
        }
        return node;
    }

    Result<NodePtr> variable(const Token& t)
    {
        for (std::size_t i = 0; i < variables_.size(); ++i) {
            if (variables_[i] == t.text) {
                auto node = makeNode(Node::Kind::Variable);
                node->variable = i;
                return node;
            }
        }
        return failure(at(t.column, "unknown name '" + t.text + "'"));
    }

    static NodePtr binary(Op op, NodePtr a, NodePtr b)
    {
        auto node = makeNode(Node::Kind::Binary, std::move(a), std::move(b));
        node->op = op;
        return node;
    }

    std::vector<Token> tokens_;
    std::span<const std::string_view> variables_;
    std::size_t pos_ = 0;
};

// ---------------------------------------------------------------- evaluation

bool compare(Op op, double a, double b)
{
    switch (op) {
    case Op::Eq: return a == b;
    case Op::Ne: return a != b;
    case Op::Lt: return a < b;
    case Op::Le: return a <= b;
    case Op::Gt: return a > b;
    case Op::Ge: return a >= b;
    default: return false;
    }
}

double eval(const Node& n, std::span<const double> values)
{
    switch (n.kind) {
    case Node::Kind::Number:
        return n.number;
    case Node::Kind::Variable:
        return n.variable < values.size() ? values[n.variable] : 0.0;
    case Node::Kind::Negate:
        return -eval(*n.children[0], values);
    case Node::Kind::Not:
        return truthy(eval(*n.children[0], values)) ? 0.0 : 1.0;
    case Node::Kind::And:
        return truthy(eval(*n.children[0], values)) && truthy(eval(*n.children[1], values)) ? 1.0 : 0.0;
    case Node::Kind::Or:
        return truthy(eval(*n.children[0], values)) || truthy(eval(*n.children[1], values)) ? 1.0 : 0.0;
    case Node::Kind::Ternary:
        return truthy(eval(*n.children[0], values)) ? eval(*n.children[1], values)
                                                    : eval(*n.children[2], values);
    case Node::Kind::Compare: {
        double left = eval(*n.children[0], values);
        for (std::size_t i = 0; i < n.compare_ops.size(); ++i) {
            const double right = eval(*n.children[i + 1], values);
            if (!compare(n.compare_ops[i], left, right))
                return 0.0;
            left = right;
        }
        return 1.0;
    }
    case Node::Kind::In:
    case Node::Kind::NotIn: {
        const double value = eval(*n.children[0], values);
        bool found = false;
        for (std::size_t i = 1; i < n.children.size() && !found; ++i)
            found = value == eval(*n.children[i], values);
        return found == (n.kind == Node::Kind::In) ? 1.0 : 0.0;
    }
    case Node::Kind::Binary: {
        const double a = eval(*n.children[0], values);
        const double b = eval(*n.children[1], values);
        switch (n.op) {
        case Op::Add: return a + b;
        case Op::Sub: return a - b;
        case Op::Mul: return a * b;
        case Op::Div: return a / b;
        case Op::Rem: return std::fmod(a, b);
        case Op::Mod: return std::fmod(std::fmod(a, b) + b, b);  // filtrex: sign of divisor
        case Op::Pow: return std::pow(a, b);
        default: return 0.0;
        }
    }
    case Node::Kind::Call: {
        const double a = eval(*n.children[0], values);
        switch (n.function) {
        case Function::Abs: return std::fabs(a);
        case Function::Ceil: return std::ceil(a);
        case Function::Floor: return std::floor(a);
        case Function::Log: return std::log(a);
        case Function::Log2: return std::log2(a);
        case Function::Log10: return std::log10(a);
        case Function::Round: return std::floor(a + 0.5);  // JS Math.round
        case Function::Sqrt: return std::sqrt(a);
        case Function::Max:
        case Function::Min: {
            double result = a;
            for (std::size_t i = 1; i < n.children.size(); ++i) {
                const double v = eval(*n.children[i], values);
                result = n.function == Function::Max ? std::fmax(result, v) : std::fmin(result, v);
            }
            return result;
        }
        }
        return 0.0;
    }
    }
    return 0.0;
}

} // namespace

Result<Expression> Expression::compile(std::string_view text, std::span<const std::string_view> variables)
{
    auto tokens = tokenize(text);
    if (!tokens)
        return std::unexpected(tokens.error());
    Parser parser(std::move(*tokens), variables);
    auto root = parser.parse();
    if (!root)
        return std::unexpected(root.error());
    return Expression(std::shared_ptr<const Node>(std::move(*root)));
}

double Expression::evaluate(std::span<const double> values) const
{
    return root_ ? eval(*root_, values) : 0.0;
}

bool Expression::test(std::span<const double> values) const
{
    return truthy(evaluate(values));
}

} // namespace game_face
