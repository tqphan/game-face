#pragma once

#include "result.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace game_face {

// Numeric expressions compatible with the filtrex syntax used by the original
// app's advanced bindings, e.g. "browInnerUp > 40 and jawOpen < 10".
//
// Supported (same precedence as filtrex, lowest first):
//   if a then b else c,  a ? b : c
//   or   and   in / not in (list: x in (1, 2, 3))
//   == != < <= > >= (chainable: 10 < x < 20)
//   + -   * / mod %   not, unary -   ^ (right-associative)
//   abs ceil floor log log2 log10 max min round sqrt
//   variables: identifiers or 'quoted names'
// Booleans are 1 and 0; anything non-zero (and not NaN) is true.
// Not supported: text values, `of`, `~=`, exists(), empty(). Unknown variable
// names are compile errors (filtrex silently evaluated them as undefined).
class Expression {
public:
    struct Node;

    // `variables` names the values passed to evaluate(), by index.
    static Result<Expression> compile(std::string_view text,
                                      std::span<const std::string_view> variables);

    double evaluate(std::span<const double> values) const;
    bool test(std::span<const double> values) const;

private:
    explicit Expression(std::shared_ptr<const Node> root) : root_(std::move(root)) {}
    std::shared_ptr<const Node> root_;
};

} // namespace game_face
