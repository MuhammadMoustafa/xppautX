#ifndef XPP_EXPR_DERIVATIVE_H
#define XPP_EXPR_DERIVATIVE_H
/* Symbolic differentiation of a model's programs (W260): the exact
   Jacobian's derivatives. A program (expr_program.h) is read into a graph
   of values with no side effects, each value once (Symbolic), and every
   right-hand side is differentiated there by the chain rule, with respect
   to the time, the state variables and the constants, through the fixed
   variables, the derived parameters and the user functions, each of them
   read in place. compile_model (expr_native.cpp) writes the derivatives
   as C, so they cost about what the right-hand side costs. C++ only. */
#include "expr_native.h"
#include "expr_program.h"
#include "xpp_error.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace xpp {
struct Session;
}

namespace xpp::expr {

/* a value of a Symbolic graph: its node's index */
struct Term {
    int id = -1;
    friend bool operator==(Term, Term) = default;
};

/* A graph of values, each node once: a node asked for again is the one
   made before (so a derivative that reuses a value, exp's, computes it
   once), and identities are taken as it is built (x+0 is x, x*1 is x,
   x*0 is 0, INTEGER_POWER's x^1 is x, a choice between equal values is
   that value). Its nodes are numbered in the order made, so a node's
   arguments come before it. */
class Symbolic {
public:
    enum class Op : unsigned char {
        Number,     /* number */
        Constant,   /* c[index] */
        Variable,   /* v[index]: 0 the time, 1.. the state */
        Fun1,       /* fun1[index](args[0]) */
        Fun2,       /* fun2[index](args[0], args[1]); ADD..DIVIDE and IEEE_DIVIDE as the evaluator does them */
        Select,     /* args[1] when args[0] is not 0, else args[2]: a program's if */
        Table,      /* lookup table index at args[0] */
        TableSlope  /* that table's interpolation's slope at args[0] (tabular.h lookup_slope) */
    };
    struct Node {
        Op op = Op::Number;
        int index = 0;
        std::array<int, 3> args{-1, -1, -1};
        double number = 0;
    };
    /* thrown when a node past max_nodes is asked for */
    struct TooLarge {};

    explicit Symbolic(std::size_t max_nodes) : max_nodes_(max_nodes) {}

    Term number(double x);
    Term constant(int k);
    Term variable(int k);
    Term fun1(int i, Term x);
    Term fun2(int i, Term x, Term y);
    Term select(Term condition, Term then, Term otherwise);
    Term table(int i, Term x);
    Term table_slope(int i, Term x);
    /* the arithmetic a derivative is written with: IEEE's (the division
       too, as a derivative divides by what the value divided by) */
    Term add(Term x, Term y) { return fun2(ADD, x, y); }
    Term subtract(Term x, Term y) { return fun2(SUBTRACT, x, y); }
    Term multiply(Term x, Term y) { return fun2(MULTIPLY, x, y); }
    Term divide(Term x, Term y) { return fun2(IEEE_DIVIDE, x, y); }

    const Node &node(Term t) const { return nodes_[static_cast<std::size_t>(t.id)]; }
    std::size_t size() const { return nodes_.size(); }
    /* t is the number x */
    bool is(Term t, double x) const;

private:
    struct Hash {
        std::size_t operator()(const Node &n) const noexcept;
    };
    struct Same {
        bool operator()(const Node &a, const Node &b) const noexcept;
    };
    Term make(const Node &n);
    std::size_t max_nodes_;
    std::vector<Node> nodes_;
    std::unordered_map<Node, int, Hash, Same> index_;
};

/* d fun1[i](x)/dx as a value of x, null where fun1[i] has no derivative
   (expr_functions.cpp, beside fun1) */
using Fun1Derivative = Term (*)(Symbolic &, Term x);
extern const std::array<Fun1Derivative, FUN1_COUNT> fun1_derivative;
/* the partial derivatives of fun2[i](x,y) in x and in y, each null where
   there is none (expr_functions.cpp, beside fun2) */
struct Fun2Derivative {
    Term (*x)(Symbolic &, Term x, Term y) = nullptr;
    Term (*y)(Symbolic &, Term x, Term y) = nullptr;
};
extern const std::array<Fun2Derivative, FUN2_COUNT> fun2_derivative;

/* the column of a derivative: a variable v[k] (0 the time, 1.. the state)
   as k, a constant c[k] as constant_column(k) */
constexpr int constant_column(int k) { return -1 - k; }
constexpr int column_constant(int column) { return -1 - column; }

/* The model's right-hand sides differentiated: entry k says that
   d f_row / d column is value, for each derivative not identically 0 */
struct ModelDerivatives {
    struct Entry {
        int row = 0, column = 0;
        Term value;
    };
    Symbolic graph;
    std::vector<Entry> entries;
};

/* The derivatives of the Session's model, or why it has none: a model
   with Markov variables, delays, integral equations, algebraic equations
   or random numbers, an operation with no derivative, a function table,
   a program compile_model did not compile (programs: model_programs'),
   or a model too large (the place the reason is at). */
Result<ModelDerivatives> differentiate_model(const xpp::Session &s, std::span<const ModelProgram> programs);

} // namespace xpp::expr

#endif
