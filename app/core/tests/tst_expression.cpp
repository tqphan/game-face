#include <game_face/core/expression.h>

#include <QTest>

#include <array>
#include <cmath>
#include <string_view>

using game_face::Expression;

namespace {

constexpr std::array<std::string_view, 4> kNames = {"x", "y", "jawOpen", "my.name"};

double eval(std::string_view text, std::array<double, 4> values = {10, 20, 30, 40})
{
    auto e = Expression::compile(text, kNames);
    if (!e)
        qFatal("compile failed for '%s': %s", std::string(text).c_str(), e.error().message.c_str());
    return e->evaluate(values);
}

std::string compileError(std::string_view text)
{
    auto e = Expression::compile(text, kNames);
    return e ? std::string() : e.error().message;
}

} // namespace

class TestExpression : public QObject {
    Q_OBJECT

private slots:
    void arithmeticAndPrecedence()
    {
        QCOMPARE(eval("1 + 2 * 3"), 7.0);
        QCOMPARE(eval("(1 + 2) * 3"), 9.0);
        QCOMPARE(eval("10 - 4 - 3"), 3.0);
        QCOMPARE(eval("2 ^ 3 ^ 2"), 512.0);   // right-associative
        QCOMPARE(eval("-2 ^ 2"), -4.0);       // ^ binds tighter than unary minus
        QCOMPARE(eval("2 ^ -1"), 0.5);
        QCOMPARE(eval("7 / 2"), 3.5);
        QCOMPARE(eval("x * 2 + y"), 40.0);
    }

    void moduloMatchesFiltrex()
    {
        QCOMPARE(eval("-1 mod 3"), 2.0);  // sign of the divisor
        QCOMPARE(eval("-1 % 3"), -1.0);   // JavaScript remainder
        QCOMPARE(eval("7 mod 3"), 1.0);
    }

    void comparisonsAndChains()
    {
        QCOMPARE(eval("x > 5"), 1.0);
        QCOMPARE(eval("x < 5"), 0.0);
        QCOMPARE(eval("x == 10"), 1.0);
        QCOMPARE(eval("x != 10"), 0.0);
        QCOMPARE(eval("x >= 10 and x <= 10"), 1.0);
        QCOMPARE(eval("5 < x < 15"), 1.0);
        QCOMPARE(eval("5 < x < 8"), 0.0);
        QCOMPARE(eval("1 < 2 == 1"), 0.0);  // chain: (1 < 2) and (2 == 1)
    }

    void logic()
    {
        QCOMPARE(eval("x > 5 and y > 5"), 1.0);
        QCOMPARE(eval("x > 50 or y > 5"), 1.0);
        QCOMPARE(eval("x > 50 or y > 5 and jawOpen > 50"), 0.0);  // and binds tighter
        QCOMPARE(eval("not 0"), 1.0);
        QCOMPARE(eval("not (x > 5)"), 0.0);
        // As in filtrex, `not` binds tighter than comparisons: (not 10) > 5.
        QCOMPARE(eval("not x > 5"), 0.0);
        QCOMPARE(eval("not x < 5"), 1.0);
    }

    void membership()
    {
        QCOMPARE(eval("x in (1, 10, 100)"), 1.0);
        QCOMPARE(eval("x in (1, 2)"), 0.0);
        QCOMPARE(eval("x not in (1, 2)"), 1.0);
        QCOMPARE(eval("x in (10)"), 1.0);
        QCOMPARE(eval("x in 10"), 1.0);
    }

    void conditionals()
    {
        QCOMPARE(eval("if x > 5 then 1 else 2"), 1.0);
        QCOMPARE(eval("if x > 50 then 1 else 2"), 2.0);
        QCOMPARE(eval("x > 5 ? 3 : 4"), 3.0);
        QCOMPARE(eval("1 + if x > 50 then 1 else 2"), 3.0);
    }

    void functions()
    {
        QCOMPARE(eval("abs(-3)"), 3.0);
        QCOMPARE(eval("ceil(1.2)"), 2.0);
        QCOMPARE(eval("floor(1.8)"), 1.0);
        QCOMPARE(eval("max(1, x, 3)"), 10.0);
        QCOMPARE(eval("min(x, y)"), 10.0);
        QCOMPARE(eval("round(2.5)"), 3.0);
        QCOMPARE(eval("round(-2.5)"), -2.0);  // JavaScript Math.round
        QCOMPARE(eval("sqrt(16)"), 4.0);
        QCOMPARE(eval("log10(1000)"), 3.0);
        QCOMPARE(eval("log2(8)"), 3.0);
    }

    void variables()
    {
        QCOMPARE(eval("jawOpen"), 30.0);
        QCOMPARE(eval("'jawOpen' + 1"), 31.0);  // quoted names are variables
        QCOMPARE(eval("my.name"), 40.0);        // dots are part of names
    }

    void truthiness()
    {
        auto e = Expression::compile("x - 10", kNames);
        QVERIFY(e.has_value());
        QVERIFY(!e->test(std::array<double, 4>{10, 0, 0, 0}));
        QVERIFY(e->test(std::array<double, 4>{11, 0, 0, 0}));

        auto nan = Expression::compile("0 / 0", kNames);
        QVERIFY(nan.has_value());
        QVERIFY(!nan->test(std::array<double, 4>{}));
    }

    void originalAppExpressions()
    {
        constexpr std::array<std::string_view, 2> names = {"browInnerUp", "eyeLookOutLeft"};
        auto start = Expression::compile("browInnerUp > 40", names);
        auto stop = Expression::compile("browInnerUp < 40", names);
        QVERIFY(start && stop);
        QVERIFY(start->test(std::array<double, 2>{41, 0}));
        QVERIFY(!start->test(std::array<double, 2>{40, 0}));
        QVERIFY(stop->test(std::array<double, 2>{39, 0}));
    }

    void errors_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("message");
        QTest::newRow("empty") << "" << "empty";
        QTest::newRow("blank") << "   " << "empty";
        QTest::newRow("unknown name") << "jawOpn > 5" << "unknown name 'jawOpn'";
        QTest::newRow("single =") << "x = 5" << "use '=='";
        QTest::newRow("&&") << "x > 1 && y > 1" << "use 'and'";
        QTest::newRow("||") << "x > 1 || y > 1" << "use 'or'";
        QTest::newRow("!") << "!x" << "use 'not'";
        QTest::newRow("unclosed paren") << "(x + 1" << "expected ')'";
        QTest::newRow("trailing") << "x y" << "unexpected 'y'";
        QTest::newRow("dangling op") << "x >" << "ends too early";
        QTest::newRow("string") << "x == \"a\"" << "text values";
        QTest::newRow("unclosed quote") << "'x" << "missing closing";
        QTest::newRow("unknown function") << "foo(1)" << "unknown function 'foo'";
        QTest::newRow("arity") << "abs(1, 2)" << "one argument";
        QTest::newRow("no args") << "max()" << "at least one";
        QTest::newRow("list outside in") << "(1, 2)" << "only allowed after 'in'";
        QTest::newRow("regex") << "x ~= 1" << "isn't supported";
        QTest::newRow("of") << "a of x" << "'of' isn't supported";
        QTest::newRow("leading dot") << ".5" << "leading digit";
        QTest::newRow("keyword") << "and" << "unexpected 'and'";
        QTest::newRow("missing then") << "if x 1 else 2" << "expected 'then'";
        QTest::newRow("missing colon") << "x ? 1" << "expected ':'";
    }

    void errors()
    {
        QFETCH(QString, text);
        QFETCH(QString, message);
        const std::string error = compileError(text.toStdString());
        QVERIFY2(!error.empty(), qPrintable("expected an error for: " + text));
        QVERIFY2(error.find(message.toStdString()) != std::string::npos, error.c_str());
    }

    void errorsIncludeColumn()
    {
        QCOMPARE(compileError("x > 1 and jawOpn"), std::string("column 11: unknown name 'jawOpn'"));
    }
};

QTEST_APPLESS_MAIN(TestExpression)
#include "tst_expression.moc"
