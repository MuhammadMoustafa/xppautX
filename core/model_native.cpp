/* The sole owner of translating validated RPN into native code. No model text enters the translation: every
   name and subscript is an index, every number a double (expr_ir.h), and xpp_mir.cpp compiles what is written
   here. */
#include "model_native.h"
#include "expr_decode.h"
#include "model_derivatives.h"
#include "expr_ir.h"
#include "expr_program.h"
#include "expr_tables.h"
#include "native_code.h"
#include "xpp_util.h"
#include "xpp_text.h"
#include "xpp_math.h"
#include "xpp_globals.h"
#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <unordered_map>
#include <string_view>
#include <thread>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include "xpp_log.h"
#include "xpp_mem.h"
#include "expr.h"
#include "model.h"
#include "model_files.h"
#include "model_place.h"
#include "model_options.h"
#include "model_numerics.h"
#include "model_record.h"
#include "odex.h"
#include "odex.h"

namespace xpp {
namespace {
struct Tree {
    int ir = -1;       /* the value, an expr_ir.h node: a tree on the lowering's stack */
    bool mut = false;  /* reads state a later statement may change (a variable, the sum's index) */
    int power = 0;     /* a literal whole n INTEGER_POWER multiplies out (math::is_integer_power), else 0 */
};

/* A conditional being read: cond is the tested value, base the stack depth
   under the branches, mark and then_mark the statements written so far at
   the MYIF and at the MYTHEN. */
struct Frame {
    Tree cond, then_value;
    size_t base;
    size_t mark = 0, then_mark = 0;
};

/* Sums nest this deep at most: each level is one recursion of the
   translator, and a model's own sums nest two or three. */
constexpr int MAX_SUM_DEPTH = 16;

/* The interpreter's own helpers the generated code calls, as names and
   what they take (the ParserState, doubles, an int, in that order): the code does the same operations in the
   same order and these do the arithmetic, so the results are the interpreter's, bit for bit. */
enum class Helper : size_t { Table, TableSlope, Network, Vector, Kernel, Delay, DelayShift, Shift, IShift, Set, Uniform, Poisson, Normal, Argument, FunctionEnd, Count };
struct HelperText {
    std::string_view name;
    bool parser;
    int doubles;
    bool index;
    std::string_view what; /* in words, for a back end with no spelling of it (IrCallee::what) */
};
constexpr std::array<HelperText, static_cast<size_t>(Helper::Count)> helper_text{{
    {"h_tab", true, 1, true, "a lookup table"},
    {"h_tabs", true, 1, true, "a lookup table's slope"},
    {"h_net", true, 1, true, "a network"},
    {"h_vec", true, 1, true, "a vectorizer"},
    {"h_ker", true, 0, true, "an integral equation's kernel"},
    {"h_del", true, 2, false, "delay()"},
    {"h_dsh", true, 3, false, "del_shft()"},
    {"h_sft", true, 2, false, "shift()"},
    {"h_ish", false, 2, false, "ishift()"},
    {"h_set", true, 3, false, "set()"},
    {"h_ru", true, 1, false, "ran()"},
    {"h_rp", true, 1, false, "poisson()"},
    {"h_rn", true, 2, false, "normal()"},
    {"h_arg", true, 0, true, "a user function's argument"},
    {"h_end", true, 1, true, "the end of a user function"},
}};

/* The helpers with the signatures of helper_text, so the generated code
   calls each through its own type; S is the ParserState the native function
   was given (its tables, the generator and the stacks the interpreter has). */
ParserState &parser(void *S) { return *static_cast<ParserState *>(S); }
double h_tab(void *S, double x, int i) { return lookup((*parser(S).tables)[i], x); }
double h_tabs(void *S, double x, int i) { return lookup_slope((*parser(S).tables)[i], x); }
double h_net(void *S, double x, int i) { ParserState &p = parser(S); return p.host->network_value(*p.owner, x, i); }
double h_vec(void *S, double x, int i) { ParserState &p = parser(S); return p.host->vector_value(*p.owner, x, i); }
double h_ker(void *S, int i) { ParserState &p = parser(S); return p.host->kernel_value(*p.owner, i); }
double h_del(void *S, double delay, double i) { ParserState &p = parser(S); return p.host->delay(*p.owner, delay, i); }
double h_dsh(void *S, double delay, double shift, double variable) { ParserState &p = parser(S); return p.host->delay_shift(*p.owner, delay, shift, variable); }
double h_sft(void *S, double shift, double variable) { return expr::do_shift(parser(S), shift, variable); }
double h_set(void *S, double shift, double variable, double value) { return expr::do_set(parser(S), shift, variable, value); }
double h_ru(void *S, double x) { return expr::do_random_uniform(*parser(S).random, x); }
double h_rp(void *S, double mean) { return expr::do_random_poisson(*parser(S).random, mean); }
double h_rn(void *S, double mean, double sd) { return expr::do_random_normal(*parser(S).random, mean, sd); }

double h_arg(void *S,int i) {const auto &stack=parser(S).stack;return stack.args[stack.nargs-1-i];}
double h_end(void *S,double value,int n) {parser(S).stack.nargs-=n;return value;}

const void *helper_address(Helper h)
{
    switch (h) {
    case Helper::Table: return reinterpret_cast<const void *>(&h_tab);
    case Helper::TableSlope: return reinterpret_cast<const void *>(&h_tabs);
    case Helper::Network: return reinterpret_cast<const void *>(&h_net);
    case Helper::Vector: return reinterpret_cast<const void *>(&h_vec);
    case Helper::Kernel: return reinterpret_cast<const void *>(&h_ker);
    case Helper::Delay: return reinterpret_cast<const void *>(&h_del);
    case Helper::DelayShift: return reinterpret_cast<const void *>(&h_dsh);
    case Helper::Shift: return reinterpret_cast<const void *>(&h_sft);
    case Helper::IShift: return reinterpret_cast<const void *>(&expr::do_ishift);
    case Helper::Set: return reinterpret_cast<const void *>(&h_set);
    case Helper::Uniform: return reinterpret_cast<const void *>(&h_ru);
    case Helper::Poisson: return reinterpret_cast<const void *>(&h_rp);
    case Helper::Normal: return reinterpret_cast<const void *>(&h_rn);
    case Helper::Argument: return reinterpret_cast<const void *>(&h_arg);
    case Helper::FunctionEnd: return reinterpret_cast<const void *>(&h_end);
    case Helper::Count: break;
    }
    return nullptr;
}

/* A user function whose body is one expression of its arguments (no
   statement, temporary or loop) is written into each call instead of
   called: the XPP division guard is a node of every division of a converted
   model, and a copy keeps a call's arguments in registers. A body of more
   nodes than this (counted as a tree, a shared node each time it is used)
   stays a call, so nested expansions stay small: 64 is about the 512
   characters of text the limit was when the code was C. */
constexpr int MAX_INLINE_NODES = 64;
/* the fewest copies of an array equation written as a loop: fewer are written out, each as its own equation, since the loop's
   counter and addressing cost a short run more than the copies' instructions do */
constexpr size_t MIN_REPEAT_COPIES = 4;
/* a node's tree size is counted up to this, no further */
constexpr int TREE_SIZE_CAP = 1 << 20;

/* A node's hash, from its fields as they are (a number by its bits): a call's identity is its arguments, its own args being
   where they sit in the module's list, so a call hashes its arguments and no more. */
std::uint64_t node_hash(const IrNode &n, std::span<const int> arguments)
{
    std::uint64_t h = std::bit_cast<std::uint64_t>(n.number);
    const auto mix = [&h](std::uint64_t x) { h = hash_mix(h, x); };
    const auto word = [](int lo, int hi) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(hi)) << 32 | static_cast<std::uint32_t>(lo); };
    mix(word(static_cast<int>(n.op), n.index));
    mix(word(n.callee, n.stride));
    if (n.op == IrOp::Call) {
        for (const int a : arguments) mix(static_cast<std::uint32_t>(a));
        mix(arguments.size());
    } else {
        mix(word(n.args[0], n.args[1]));
        mix(static_cast<std::uint32_t>(n.args[2]));
    }
    return h ^ (h >> 29);
}

struct Generator {
    const SessionParts &s;
    Place place;
    IrModule ir;
    std::vector<int> tree_size;                     /* of each ir.nodes entry, up to TREE_SIZE_CAP */
    /* the function being written: one node for one expression, found by its fields in an open-addressing table of node ids. A slot
       belongs to the function it was filled for (its stamp), so a new function starts with every slot empty; each node's hash is
       kept by id, so the table grows without hashing a node again. */
    struct NodeSlot {
        int node = -1;
        int function = -1;
    };
    std::vector<NodeSlot> node_slots_ = std::vector<NodeSlot>(1024); /* a power of two, at least twice the nodes in the function */
    std::vector<std::uint64_t> node_hashes_;                       /* by node: its fields' hash */
    int stamp_ = 0;                                                /* the function being written */
    size_t nodes_in_function_ = 0;
    std::unordered_map<std::string, int> callee_of; /* by callee name */
    std::vector<int> callee_user;                   /* by callee: the user function it calls, else -1 */
    std::vector<std::set<int>> calls; /* the user functions each user function calls */
    std::vector<char> pure_function; /* a user function written so far that changes no state */
    std::vector<int> inline_body;    /* a user function's body as a node, -1 when it is called */
    /* the function being written: the pure calls already made at its top
       level, by their node, each held in a temporary (pure_call) */
    std::unordered_map<int, Tree> calls_made;
    /* the right-hand side's programs whose function the one right-hand-side
       function calls (rhs_value), each with its symbol */
    std::map<const Program *, std::string> called;
    /* an index over calls_made, so a variable's write finds the calls that
       may read it without scanning them all (W559): each call's node by
       the variables it names or reaches through a pure user function, and
       the calls that may read any variable. An entry whose call was
       forgotten is skipped when it is looked up. */
    std::unordered_map<int, std::vector<int>> call_readers;
    std::vector<int> call_readers_any;
    /* the function being written: its statements in order, how many
       temporaries and loop counters it declares, how deep in sums it is,
       and whether it changes state or draws (its callers then call it
       as a statement) */
    std::vector<IrStatement> out;
    int temps = 0, loops = 0, depth = 0;
    bool impure = false;
    bool array_pure = true;
    std::vector<char> array_pure_function;
    std::set<int> variable_reads;
    std::vector<std::set<int>> function_reads;
    bool dynamic = false; /* a network updates its first two variable operands */
    /* while the copies of an array equation are written as one body (rhs): by instruction place, the step of its variable or
       constant operand from one copy to the next, which its node carries (IrNode::stride); else empty */
    std::vector<int> strides;
    /* the IR nodes that the copies of a derivative's Repeats would add beyond their bodies: one per copy of each node that
       reads a stepping operand (varying_nodes). The build's trigger counts them, so a Jacobian compiles when it did before
       the Repeat (pending_build); its code is the Repeat's, so the compile is cheaper, not later. */
    size_t repeat_nodes = 0;
    std::vector<char> allowed; /* user functions with a fully compiled dependency graph */

    explicit Generator(const SessionParts &session) : s(session) { ir.zero_divisor = expr::ZERO_DIVISOR; }

    /* the id of the node with these fields, or -1 when there is none; `slot` is where it is, or the empty slot it goes in */
    int find_node(const IrNode &n, std::span<const int> arguments, std::uint64_t hash, size_t &slot) const
    {
        const size_t mask = node_slots_.size() - 1;
        for (slot = hash & mask;; slot = (slot + 1) & mask) {
            const NodeSlot &s = node_slots_[slot];
            if (s.function != stamp_) return -1;
            if (node_hashes_[static_cast<size_t>(s.node)] == hash && same_node(s.node, n, arguments)) return s.node;
        }
    }
    bool same_node(int id, const IrNode &n, std::span<const int> arguments) const
    {
        const IrNode &s = ir.nodes[static_cast<size_t>(id)];
        if (s.op != n.op || s.index != n.index || s.callee != n.callee || s.stride != n.stride
            || std::bit_cast<std::uint64_t>(s.number) != std::bit_cast<std::uint64_t>(n.number))
            return false;
        if (n.op == IrOp::Call) return std::ranges::equal(ir.arguments(s), arguments);
        return s.args == n.args;
    }
    void grow_nodes()
    {
        const std::vector<NodeSlot> old = std::exchange(node_slots_, std::vector<NodeSlot>(2 * node_slots_.size()));
        const size_t mask = node_slots_.size() - 1;
        for (const NodeSlot &s : old) {
            if (s.function != stamp_) continue;
            size_t slot = node_hashes_[static_cast<size_t>(s.node)] & mask;
            while (node_slots_[slot].function == stamp_) slot = (slot + 1) & mask;
            node_slots_[slot] = s;
        }
    }

    /* The node for this expression: made once, however often it is written (so two writings of one
       expression are one node, which is what lets a call be recognized as made before). A call's arguments
       are the nodes it is given. */
    int node(IrNode n, std::span<const int> arguments = {})
    {
        const std::uint64_t hash = node_hash(n, arguments);
        size_t slot = 0;
        if (const int found = find_node(n, arguments, hash, slot); found >= 0) return found;
        int size = 1;
        const auto add = [&](int a) { size = std::min(size + tree_size[static_cast<size_t>(a)], TREE_SIZE_CAP); };
        for (const int a : n.args)
            if (a >= 0) add(a);
        for (const int a : arguments) add(a);
        if (n.op == IrOp::Call) {
            n.args = {static_cast<int>(ir.call_arguments.size()), static_cast<int>(arguments.size()), -1};
            ir.call_arguments.insert(ir.call_arguments.end(), arguments.begin(), arguments.end());
        }
        const int id = static_cast<int>(ir.nodes.size());
        ir.nodes.push_back(n);
        tree_size.push_back(size);
        node_hashes_.push_back(hash);
        node_slots_[slot] = {id, stamp_};
        if (2 * ++nodes_in_function_ > node_slots_.size()) grow_nodes();
        return id;
    }
    int op(IrOp o, int a = -1, int b = -1, int c = -1, int index = 0)
    {
        IrNode n;
        n.op = o;
        n.index = index;
        n.args = {a, b, c};
        return node(n);
    }
    /* a constant or variable at `index`, stepping `stride` a pass of the Repeat it is in (0 outside one) */
    int leaf(IrOp o, int index, int stride)
    {
        IrNode n;
        n.op = o;
        n.index = index;
        n.stride = stride;
        return node(n);
    }
    int number(double x)
    {
        IrNode n;
        n.number = x;
        return node(n);
    }
    /* the callee `target`, entered once by its name */
    int callee(IrCallee target, int user = -1)
    {
        const auto [found, added] = callee_of.emplace(target.name, static_cast<int>(ir.callees.size()));
        if (added) {
            ir.callees.push_back(std::move(target));
            callee_user.push_back(user);
        }
        return found->second;
    }
    int call(int callee_index, std::span<const int> args, int index = 0)
    {
        IrNode n;
        n.op = IrOp::Call;
        n.index = index;
        n.callee = callee_index;
        return node(n, args);
    }
    int helper_call(Helper h, std::initializer_list<int> args, int index = 0)
    {
        const HelperText &t = helper_text[static_cast<size_t>(h)];
        return call(callee({std::string(t.name), helper_address(h), t.parser, t.doubles, t.index, -1, std::string(t.what)}), std::span<const int>(args.begin(), args.size()), index);
    }
    int math_call(bool binary, int i, std::initializer_list<int> args)
    {
        const void *address = binary ? xpp::math::resolve(expr::fun2[static_cast<size_t>(i)]) : xpp::math::resolve(expr::fun1[static_cast<size_t>(i)]);
        IrCallee target{xpp::format("f{}_{}", binary ? 2 : 1, i), address, false, binary ? 2 : 1, false, COM(binary ? FUN2TYPE : FUN1TYPE, i), {}};
        target.negation = !binary && i == expr::NEGATE_FUNCTION;
        return call(callee(std::move(target)), std::span<const int>(args.begin(), args.size()));
    }
    int user_call(int function, std::span<const int> args)
    {
        return call(callee({xpp::concat("u", function), nullptr, false, s.model().compiler.narg_fun[function], false}, function), args);
    }
    bool atom(const Tree &n) const
    {
        switch (ir.nodes[static_cast<size_t>(n.ir)].op) {
        case IrOp::Number: case IrOp::Constant: case IrOp::Variable: case IrOp::Temporary: case IrOp::Argument: return true;
        default: return false;
        }
    }
    /* a copy of the inlined body at node `body`, each argument node replaced by the call's own */
    int instantiate(int body, const std::vector<int> &arguments)
    {
        IrNode n = ir.nodes[static_cast<size_t>(body)];
        if (n.op == IrOp::Argument) return arguments.at(static_cast<size_t>(n.index));
        if (n.op == IrOp::Call) {
            std::vector<int> mine;
            for (const int a : ir.arguments(n)) mine.push_back(instantiate(a, arguments));
            return call(n.callee, mine, n.index);
        }
        for (int &a : n.args)
            if (a >= 0) a = instantiate(a, arguments);
        return node(n);
    }

    /* how a refusal reads: the compile's (the program stays interpreted), or an export's (it is not written) */
    std::string_view refusal_before = "not compiled: ", refusal_after = "; running the interpreter";
    std::unexpected<Error> refuse(std::string reason) const
    {
        return fail("compile", std::string(refusal_before) + reason + std::string(refusal_after), place);
    }

    /* the helper's call, noted: only a table lookup keeps a function free of the parser's state */
    int helper(Helper h, std::initializer_list<int> args, int index = 0)
    {
        if (h != Helper::Table && h != Helper::TableSlope && h != Helper::IShift) array_pure = false;
        return helper_call(h, args, index);
    }

    /* The interpreter does a state-changing operation where its token is,
       so the generated code does it as a statement of its own, in order,
       and what the stack holds that a statement may change is first
       written to a temporary. */
    Tree statement(int value)
    {
        const int k = temps++;
        out.push_back({IrStatement::Kind::Temporary, k, value});
        return {op(IrOp::Temporary, -1, -1, -1, k), false, 0};
    }
    void flush(std::vector<Tree> &stack, size_t from)
    {
        for (size_t i = from; i < stack.size(); ++i)
            if (stack[i].mut) stack[i] = statement(stack[i].ir);
    }
    Tree effect(std::vector<Tree> &stack, size_t from, int call_node)
    {
        flush(stack, from);
        impure = true;
        forget_calls();
        return statement(call_node);
    }

    /* A literal: the same value, the same node, so two writings of one
       expression are one. */
    Tree literal(double x)
    {
        return {number(x), false, math::is_integer_power(x) ? static_cast<int>(x) : 0};
    }

    /* The call of a math function (a table's fun1/fun2: pure, they
       read and change no state) as a node. A call made before in this
       function, the same node, is its temporary again; else, at the
       function's top level (in no conditional or sum, whose statements
       may not run), it is written once to a temporary now. The same
       operations on the same values in the same order: the call is only
       made once instead of twice. A state-changing statement forgets the
       calls made (effect, a sum), since they may read what it
       changes. */
    Tree pure_call(int tree, bool mut, bool top)
    {
        if (depth > 0) return {tree, mut, 0};
        if (const auto made = calls_made.find(tree); made != calls_made.end()) return made->second;
        if (!top) return {tree, mut, 0};
        Tree t = statement(tree);
        index_call(tree);
        calls_made.emplace(tree, t);
        return t;
    }

    /* x^n for a literal whole n (INTEGER_POWER), multiplied out as the
       interpreter does (math::power_by_squaring), each product a
       temporary, so x and each square are computed once */
    Tree integer_power(const Tree &x, int n)
    {
        const Tree a = atom(x) ? x : statement(x.ir);
        return math::power_by_squaring(a, n, [&](const Tree &p, const Tree &q) { return statement(op(IrOp::Multiply, p.ir, q.ir)); });
    }

    /* The two branches met (other is the else value, already off the
       stack): a conditional expression when neither wrote a statement,
       else an if with a temporary for the value. */
    void join(std::vector<Tree> &stack, const Frame &f, const Tree &other, size_t outer_base)
    {
        if (out.size() == f.mark) {
            stack.push_back({op(IrOp::Select, f.cond.ir, other.ir, f.then_value.ir), f.cond.mut || other.mut || f.then_value.mut, 0});
            return;
        }
        const std::vector<IrStatement> branches(out.begin() + static_cast<std::ptrdiff_t>(f.mark), out.end());
        out.resize(f.mark);
        flush(stack, outer_base);
        const int result = temps++;
        const auto split = branches.begin() + static_cast<std::ptrdiff_t>(f.then_mark - f.mark);
        out.push_back({IrStatement::Kind::IfBegin, 0, f.cond.ir});
        out.insert(out.end(), split, branches.end());
        out.push_back({IrStatement::Kind::Temporary, result, other.ir});
        out.push_back({IrStatement::Kind::IfElse});
        out.insert(out.end(), branches.begin(), split);
        out.push_back({IrStatement::Kind::Temporary, result, f.then_value.ir});
        out.push_back({IrStatement::Kind::IfEnd});
        stack.push_back({op(IrOp::Temporary, -1, -1, -1, result), false, 0});
    }

    /* The value of the instructions from k up to the end of the program
       or of a sum's body (its ENDSUM), as one expression: the
       interpreter's stack is simulated over trees, so each operation is
       done on the operands the interpreter pops, in its order, and the
       back end keeps temporaries in registers instead of a stack array. The
       instructions are expr_decode.h's, already checked: only what the
       code cannot be written for is refused here. k ends after the terminator.
       Iterative; only a sum's body is a recursion, MAX_SUM_DEPTH deep. */
    Result<Tree> sequence(const std::vector<expr::Instr> &code, size_t &k, bool user, int index)
    {
        std::vector<Tree> stack;
        std::vector<Frame> frames;
        auto lower = [&] { return frames.empty() ? size_t{0} : frames.back().base; };
        auto pop = [&] { Tree n = std::move(stack.back()); stack.pop_back(); return n; };
        while (k < code.size()) {
            const expr::Instr &i = code[k++];
            const int op_code = i.op, type = i.type, in = i.in;
            if (op_code == expr::JOIN) {
                const Frame f = std::move(frames.back());
                frames.pop_back();
                const Tree other = pop();
                join(stack, f, other, lower());
            } else if (op_code == ENDSUM) {
                return pop();
            } else if (op_code == NUMSYM) {
                stack.push_back(literal(i.number));
            } else if (op_code == MYIF) {
                Tree cond = pop();
                frames.push_back({std::move(cond), {}, stack.size()});
                frames.back().mark = out.size();
            } else if (op_code == MYTHEN) {
                Frame &f = frames.back();
                f.then_value = pop();
                f.then_mark = out.size();
            } else if (op_code == INDXCOM) {
                stack.push_back(literal(expr::CURRENT_INDEX));
            } else if (op_code == SUMSYM) {
                if (depth >= MAX_SUM_DEPTH) return refuse("sums nested too deep");
                const Tree high = pop(), low = pop();
                flush(stack, lower());
                impure = true;
                forget_calls();
                const int n = loops++;
                const int total = temps++;
                /* the bounds are truncated to int and the index widened as the interpreter's casts do */
                out.push_back({IrStatement::Kind::LoopBegin, n, low.ir, high.ir, total});
                out.push_back({IrStatement::Kind::Constant, expr::SUM_INDEX, op(IrOp::LoopCounter, -1, -1, -1, n)});
                ++depth;
                auto value = sequence(code, k, user, index);
                --depth;
                if (!value) return value;
                out.push_back({IrStatement::Kind::LoopEnd, n, value->ir, -1, total});
                stack.push_back({op(IrOp::Temporary, -1, -1, -1, total), false, 0});
            } else if (op_code == ENDDELAY || op_code == ENDSHIFT || op_code == ENDISHIFT) {
                const Tree first = pop(), second = pop();
                if (op_code == ENDISHIFT) {
                    stack.push_back({helper(Helper::IShift, {first.ir, second.ir}), first.mut || second.mut, 0});
                } else if (op_code == ENDSHIFT) {
                    stack.push_back({helper(Helper::Shift, {first.ir, second.ir}), true, 0});
                } else {
                    stack.push_back(effect(stack, lower(), helper(Helper::Delay, {first.ir, second.ir})));
                }
            } else if (op_code == ENDDELSHFT) {
                const Tree tau = pop(), shift = pop(), variable = pop();
                stack.push_back(effect(stack, lower(), helper(Helper::DelayShift, {tau.ir, shift.ir, variable.ir})));
            } else if (op_code == ENDSET) {
                const Tree value = pop(), shift = pop(), variable = pop();
                stack.push_back(effect(stack, lower(), helper(Helper::Set, {shift.ir, variable.ir, value.ir})));
            } else if (op_code == RANDUNI || op_code == RANDPOI) {
                const Tree x = pop();
                stack.push_back(effect(stack, lower(), helper(op_code == RANDUNI ? Helper::Uniform : Helper::Poisson, {x.ir})));
            } else if (op_code == RANDNORM) {
                const Tree sd = pop(), mean = pop();
                stack.push_back(effect(stack, lower(), helper(Helper::Normal, {mean.ir, sd.ir})));
            } else if (type == SVARTYPE || type == SCONTYPE) {
                /* the variable or constant a delay, shift or set names, as the number the interpreter pushes */
                const int named = type == SCONTYPE ? COM(CONTYPE, in) : COM(VARTYPE, in);
                stack.push_back({number(named), false, 0});
            } else if (type == CONTYPE || type == VARTYPE) {
                if (dynamic && i.pc<2) {
                    if (type!=VARTYPE) return refuse("an invalid network operand");
                    /* The network changes these tokens during each pair evaluation.
                       do_shift bounds the selected variable just as the interpreter
                       does for a computed index; no model name enters the code. */
                    stack.push_back({helper(Helper::Shift, {number(0), op(IrOp::Operand, -1, -1, -1, i.pc)}), true, 0});
                } else {
                    if (type == VARTYPE) variable_reads.insert(in);
                    if (type == CONTYPE && in == expr::SUM_INDEX && depth == 0) array_pure = false;
                    IrNode operand;
                    operand.op = type == CONTYPE ? IrOp::Constant : IrOp::Variable;
                    operand.index = in;
                    if (!strides.empty()) operand.stride = strides[k - 1];
                    stack.push_back({node(operand), type == VARTYPE || in == expr::SUM_INDEX, 0});
                }
            } else if (type == USTACKTYPE) {
                stack.push_back({op(IrOp::Argument, -1, -1, -1, in), false, 0});
            } else if (type == FUN1TYPE) {
                const Tree x = pop();
                stack.push_back(pure_call(math_call(false, in, {x.ir}), x.mut, frames.empty()));
            } else if (type == FUN2TYPE) {
                const Tree right = pop(), left = pop();
                if (in == expr::INTEGER_POWER && right.power) {
                    stack.push_back(integer_power(left, right.power));
                    continue;
                }
                int tree = -1;
                if (in == expr::ADD || in == expr::MULTIPLY) {
                    tree = op(in == expr::ADD ? IrOp::Add : IrOp::Multiply, right.ir, left.ir);
                } else if (in == expr::SUBTRACT || in == expr::IEEE_DIVIDE) {
                    tree = op(in == expr::SUBTRACT ? IrOp::Subtract : IrOp::Divide, left.ir, right.ir);
                } else if (in == expr::DIVIDE) {
                    /* the guarded division: a zero divisor becomes ZERO_DIVISOR */
                    tree = op(IrOp::GuardedDivide, left.ir, right.ir);
                } else {
                    stack.push_back(pure_call(math_call(true, in, {left.ir, right.ir}), left.mut || right.mut, frames.empty()));
                    continue;
                }
                stack.push_back({tree, left.mut || right.mut, 0});
            } else if (type == UFUNTYPE) {
                if (!allowed.empty() && !allowed[in]) return refuse("calls a refused user function");
                if (i.count != s.model().compiler.narg_fun[in]) return refuse("a call with the wrong number of arguments");
                if (user) calls[index].insert(in);
                const size_t first = stack.size() - static_cast<size_t>(i.count);
                /* inlined: each argument once, a name as it is, an expression in a temporary that
                   the same expression shares (pure_call; only where a statement may be written) */
                std::vector<int> arguments;
                bool inlined = inline_body[in] >= 0;
                if (inlined)
                    for (size_t a = first; a < stack.size(); ++a) {
                        const Tree argument = atom(stack[a]) ? stack[a] : pure_call(stack[a].ir, stack[a].mut, frames.empty());
                        if (!atom(argument)) { inlined = false; break; }
                        arguments.push_back(argument.ir);
                    }
                int tree;
                if (inlined) {
                    tree = instantiate(inline_body[in], arguments);
                } else {
                    arguments.clear();
                    for (size_t a = first; a < stack.size(); ++a) arguments.push_back(stack[a].ir);
                    tree = user_call(in, arguments);
                }
                stack.resize(first);
                if (!user) {
                    if (!array_pure_function[in]) array_pure = false;
                    variable_reads.insert(function_reads[in].begin(),function_reads[in].end());
                }
                stack.push_back(pure_function[in] ? Tree{tree, true, 0} : effect(stack, lower(), tree));
            } else if (type == KERTYPE) {
                stack.push_back({helper(Helper::Kernel, {}, in), false, 0});
            } else {
                /* a lookup table, network or vectorizer: the arrays the helpers index are the interpreter's own */
                const Tree x = pop();
                const Helper h = type == TABTYPE ? Helper::Table : type == NETTYPE ? Helper::Network : Helper::Vector;
                stack.push_back({helper(h, {x.ir}, in), type != TABTYPE || x.mut, 0});
            }
        }
        return pop();
    }

    /* A new function: no statements, temporaries, loops, nodes or calls made yet. */
    void begin()
    {
        out.clear();
        ++stamp_;
        nodes_in_function_ = 0;
        forget_calls();
        temps = loops = depth = 0;
        impure = false;
        array_pure = true;
        variable_reads.clear();
    }

    /* Program p's value in the function being written, its statements
       appended to out. */
    Result<Tree> value(const std::vector<int> &p, int index, bool user)
    {
        if (p.empty()) return refuse("an invalid program length");
        const auto code = expr::decode(s.parser, p, user ? s.model().compiler.narg_fun[index] : -1);
        if (!code) return refuse(code.error());
        size_t k = 0;
        return sequence(*code, k, user, index);
    }

    /* The function being written, with its statements and the value it returns (a node), or none */
    IrFunction finish(IrFunction::Kind kind, std::string name, int result)
    {
        IrFunction f;
        f.kind = kind;
        f.name = std::move(name);
        f.statements = std::move(out);
        f.result = result;
        f.temporaries = temps;
        f.loops = loops;
        out.clear();
        return f;
    }

    /* One function for a program (an equation's, or user function index's). */
    Result<IrFunction> program(const std::vector<int> &p, int index, bool user, std::string_view name={})
    {
        begin();
        const auto result = value(p, index, user);
        if (!result) return std::unexpected(result.error());
        if (user) {
            pure_function[index] = !impure;
            if (!impure && out.empty() && temps == 0 && loops == 0 && tree_size[static_cast<size_t>(result->ir)] <= MAX_INLINE_NODES)
                inline_body[index] = result->ir;
        }
        IrFunction f = finish(user ? IrFunction::Kind::User : IrFunction::Kind::Equation, user ? xpp::concat("u", index) : std::string(name), result->ir);
        if (user) f.doubles = s.model().compiler.narg_fun[index];
        return f;
    }

    /* One function, name, that works the derived quantities out in order (W846): each formula's value written into its constant
       as evaluate_derived does, so the next formula reads it; the calls made are forgotten after each, since the constant a
       later formula reads has changed. */
    Result<IrFunction> derived(std::span<const CompilerTables::DerivedQuantity> list, std::string_view name)
    {
        begin();
        for (const auto &d : list) {
            place = d.where;
            dynamic = false;
            const auto result = value(d.form.rpn, 0, false);
            if (!result) return std::unexpected(result.error());
            out.push_back({IrStatement::Kind::Constant, d.index, result->ir});
            forget_calls();
        }
        return finish(IrFunction::Kind::Rhs, std::string(name), -1);
    }

    /* The bridge the interpreter calls for a user function: it has already pushed the arguments in reverse
       order, and the bridge drops them as ENDFUN does; equations call the function directly. */
    IrFunction bridge(int index, std::string name)
    {
        begin();
        std::vector<int> arguments;
        for (int a = 0; a < s.model().compiler.narg_fun[index]; ++a) arguments.push_back(helper(Helper::Argument, {}, a));
        const Tree z = statement(user_call(index, arguments));
        return finish(IrFunction::Kind::Equation, std::move(name), helper(Helper::FunctionEnd, {z.ir}, s.model().compiler.narg_fun[index]));
    }

    /* The calls made are all forgotten. */
    void forget_calls()
    {
        calls_made.clear();
        call_readers.clear();
        call_readers_any.clear();
    }

    /* Index a call by what it may read: the variables its tree names, and through a call given the Session
       (the Session's state): a user function's that reads k or what is not its arguments or variables
       (array_pure_function) reads those it lists, and a helper's other than a table's may read any variable.
       Iterative over the tree's distinct nodes. */
    void index_call(int tree)
    {
        std::vector<int> pending{tree};
        std::set<int> seen;
        bool any = false;
        while (!pending.empty()) {
            const int id = pending.back();
            pending.pop_back();
            if (!seen.insert(id).second) continue;
            const IrNode &n = ir.nodes[static_cast<size_t>(id)];
            if (n.op == IrOp::Variable) {
                call_readers[n.index].push_back(tree);
            } else if (n.op == IrOp::Call) {
                const size_t target = static_cast<size_t>(n.callee);
                const IrCallee &callee_ref = ir.callees[target];
                if (callee_ref.address != nullptr && callee_ref.parser && callee_ref.name != helper_text[static_cast<size_t>(Helper::Table)].name
                    && callee_ref.name != helper_text[static_cast<size_t>(Helper::TableSlope)].name)
                    any = true;
                if (callee_ref.address == nullptr) {
                    const int f = callee_user[target];
                    if (f >= 0 && array_pure_function[static_cast<size_t>(f)])
                        for (int k : function_reads[static_cast<size_t>(f)]) call_readers[k].push_back(tree);
                    else
                        any = true;
                }
                for (const int a : ir.arguments(n)) pending.push_back(a);
            } else {
                for (const int a : n.args)
                    if (a >= 0) pending.push_back(a);
            }
        }
        if (any) call_readers_any.push_back(tree);
    }

    /* Writing variable k forgets the calls that may read it. */
    void forget_calls_reading(int k)
    {
        for (const int tree : call_readers_any) calls_made.erase(tree);
        call_readers_any.clear();
        if (const auto readers = call_readers.find(k); readers != call_readers.end()) {
            for (const int tree : readers->second) calls_made.erase(tree);
            call_readers.erase(readers);
        }
    }

    /* The right-hand side's program p's value in the function being
       written: the function already compiled for it (W703: called when it has
       a sum, whose run is long beside the call, and so not written and
       compiled twice), else p written inline, so a math call
       two programs make on the same values is made once. A call is a
       statement, as an effect is: it forgets the calls made. A program
       that is not dynamic reads no R. */
    Result<Tree> rhs_value(const Program &p)
    {
        const auto function = called.find(&p);
        if (function == called.end()) return value(p.rpn, 0, false);
        forget_calls();
        return statement(call(callee({function->second, nullptr, false, 0, false}), {}));
    }

    /* One function, name, for the right-hand side in its order: each
       fixed program's value written into its variable, first and on, as
       fixed_values (my_rhs.cpp) sets them one by one (W468), then, when
       the caller gives o, each equation's value into o[i], as
       equation_values does (W469); so a math call two of them make on the
       same values is made once. Writing a variable forgets the calls that
       may read it (forget_calls_reading); nothing reads o. The
       programs in called are called, not written (rhs_value). */
    Result<IrFunction> rhs(std::span<const Program> fixed, int first, std::span<const Program> equations, std::string_view name)
    {
        begin();
        for (size_t i = 0; i < fixed.size(); ++i) {
            const auto result = rhs_value(fixed[i]);
            if (!result) return std::unexpected(result.error());
            const int variable = first + static_cast<int>(i);
            out.push_back({IrStatement::Kind::Variable, variable, result->ir});
            forget_calls_reading(variable);
        }
        if (!equations.empty()) out.push_back({IrStatement::Kind::ReturnWithoutOutput});
        for (size_t i = 0; i < equations.size();) {
            const size_t copies = repeated_copies(equations, i);
            if (copies == 0) {
                const auto result = rhs_value(equations[i]);
                if (!result) return std::unexpected(result.error());
                out.push_back({IrStatement::Kind::Output, static_cast<int>(i), result->ir});
                ++i;
                continue;
            }
            /* equation i written once, its element operands and its output stepping with the Repeat's counter; the calls it
               made are of one pass, so they are forgotten after it */
            out.push_back({IrStatement::Kind::RepeatBegin, static_cast<int>(copies)});
            const auto result = value(equations[i].rpn, 0, false);
            strides.clear();
            if (!result) return std::unexpected(result.error());
            out.push_back({IrStatement::Kind::Output, static_cast<int>(i), result->ir, -1, -1, 1});
            out.push_back({IrStatement::Kind::RepeatEnd});
            forget_calls();
            i += copies;
        }
        return finish(IrFunction::Kind::Rhs, std::string(name), -1);
    }

    /* How many equations from `first` on are one array equation's copies, to be written as one Repeat (0 for a single
       equation): at least MIN_REPEAT_COPIES consecutive programs that are one formula at consecutive indices
       (expr::index_strides). Sets `strides` for the body when it finds them. A program that is a call of its own function
       (rhs_value: a sum) or does not decode is no copy; its error is the one rhs_value gives. */
    size_t repeated_copies(std::span<const Program> equations, size_t first)
    {
        const auto decoded = [&](size_t i) -> std::optional<std::vector<expr::Instr>> {
            if (i >= equations.size() || called.contains(&equations[i])) return std::nullopt;
            auto code = expr::decode(s.parser, equations[i].rpn, -1);
            if (!code) return std::nullopt;
            return std::move(*code);
        };
        const auto code = decoded(first), second = decoded(first + 1);
        if (!code || !second) return 0;
        const auto found = expr::index_strides(*code, *second);
        if (!found) return 0;
        size_t copies = 2;
        while (const auto next = decoded(first + copies)) {
            if (!expr::is_indexed_copy(*code, *found, *next, static_cast<int>(copies))) break;
            ++copies;
        }
        if (copies < MIN_REPEAT_COPIES) return 0;
        strides = *found;
        return copies;
    }

    /* One term's graph as derivatives writes it (repeated_terms): an entry per leaf occurrence and per node, in the order
       written, a node's operands as entries (-1 for none); the terms' values are roots. A leaf names its graph node, so the
       step its index takes from one copy to the next is the same at each of its occurrences. */
    struct TermEntry {
        expr::Symbolic::Op op = expr::Symbolic::Op::Number;
        int index = 0;
        std::uint64_t bits = 0; /* a number's bits */
        std::array<int, 3> args{-1, -1, -1};
        int graph = -1; /* a leaf's node; -1 for a node */
    };
    struct TermBlock {
        std::vector<TermEntry> entries;
        std::vector<int> roots; /* each term's value: its entry */
    };
    /* A run of copies of derivative rows written as one Repeat: `copies` runs of `period` terms each, one formula at indices
       a step apart; `strides` gives each leaf's graph node its index's step per copy (0 or 1). */
    struct RepeatedTerms {
        size_t period = 0, copies = 0;
        std::unordered_map<int, int> strides;
    };

    static bool is_leaf_op(expr::Symbolic::Op op)
    {
        return op == expr::Symbolic::Op::Number || op == expr::Symbolic::Op::Constant || op == expr::Symbolic::Op::Variable;
    }

    /* Each graph node's signature, a hash of its operator, its number and its operands' signatures (args are numbered before
       their node); a leaf's index is left out, since the copies of an array differ by it. Equal signatures are necessary for
       two rows to be copies, so repeated_terms compares these before it builds any block. */
    static std::vector<std::uint64_t> node_signatures(const expr::Symbolic &g)
    {
        std::vector<std::uint64_t> sig(g.size());
        for (size_t id = 0; id < g.size(); ++id) {
            const expr::Symbolic::Node &n = g.node({static_cast<int>(id)});
            std::uint64_t h = hash_mix(static_cast<std::uint64_t>(n.op), std::bit_cast<std::uint64_t>(n.number));
            if (!is_leaf_op(n.op)) h = hash_mix(h, static_cast<std::uint64_t>(n.index));
            for (const int a : n.args) h = hash_mix(h, a < 0 ? ~0ULL : sig[static_cast<size_t>(a)]);
            sig[id] = h;
        }
        return sig;
    }

    /* A node-indexed workspace for term_block: `at` is a node's entry in the block being made (-1 for none), and `touched`
       the nodes set, so the next block clears just those */
    struct BlockScratch {
        std::vector<int> at;
        std::vector<int> touched;
    };

    /* The block of `count` terms from `first` as derivatives writes them: one memo over the block, so a node the terms
       share is one entry, and each leaf occurrence is an entry of its own */
    static TermBlock term_block(const expr::Symbolic &g, std::span<const expr::Term> terms, size_t first, size_t count, BlockScratch &scratch)
    {
        TermBlock block;
        std::vector<int> &at = scratch.at;
        const auto ref = [&](int id) {
            if (!is_leaf_op(g.node({id}).op)) return at[static_cast<size_t>(id)];
            const expr::Symbolic::Node &n = g.node({id});
            TermEntry leaf;
            leaf.op = n.op;
            leaf.index = n.index;
            leaf.bits = std::bit_cast<std::uint64_t>(n.number);
            leaf.graph = id;
            block.entries.push_back(leaf);
            return static_cast<int>(block.entries.size()) - 1;
        };
        for (size_t j = first; j < first + count; ++j) {
            const int root = terms[j].id;
            std::vector<std::pair<int, bool>> pending{{root, false}};
            while (!pending.empty()) {
                const auto [id, arguments_done] = pending.back();
                pending.pop_back();
                if (is_leaf_op(g.node({id}).op) || at[static_cast<size_t>(id)] >= 0) continue;
                const expr::Symbolic::Node &n = g.node({id});
                if (arguments_done) {
                    TermEntry entry;
                    entry.op = n.op;
                    entry.index = n.index;
                    entry.bits = std::bit_cast<std::uint64_t>(n.number);
                    for (size_t a = 0; a < n.args.size(); ++a)
                        if (n.args[a] >= 0) entry.args[a] = ref(n.args[a]);
                    block.entries.push_back(entry);
                    at[static_cast<size_t>(id)] = static_cast<int>(block.entries.size()) - 1;
                    scratch.touched.push_back(id);
                    continue;
                }
                pending.push_back({id, true});
                for (auto it = n.args.rbegin(); it != n.args.rend(); ++it)
                    if (*it >= 0 && !is_leaf_op(g.node({*it}).op) && at[static_cast<size_t>(*it)] < 0) pending.push_back({*it, false});
            }
            block.roots.push_back(ref(root));
        }
        for (const int id : scratch.touched) at[static_cast<size_t>(id)] = -1;
        scratch.touched.clear();
        return block;
    }

    /* whether entry b is entry a `n` copies on, a leaf's index stepping `step` per copy (a node is the same as its own) */
    static bool advanced(const TermEntry &a, const TermEntry &b, int step, int n)
    {
        if (a.op != b.op || a.bits != b.bits || a.args != b.args) return false;
        if (a.op == expr::Symbolic::Op::Variable || a.op == expr::Symbolic::Op::Constant) return b.index == a.index + step * n;
        return a.index == b.index;
    }

    /* The copies of derivative rows that begin at term `first`: runs of `period` terms (a row's terms), at most `available`
       of them in a row (the runs of the same size from `first` on). Each copy is the first one's formula at indices a step
       apart (0 or 1 a copy: an array's element or a constant beside it), compared entry by entry (term_block), so its
       operations and operands are the first copy's and its values are the same bit for bit. None (copies 0) when fewer than
       MIN_REPEAT_COPIES rows are such copies. */
    static RepeatedTerms repeated_terms(const expr::Symbolic &g, std::span<const expr::Term> terms, std::span<const std::uint64_t> term_sig,
                                        size_t first, size_t period, size_t available, BlockScratch &scratch)
    {
        if (available < MIN_REPEAT_COPIES || !std::ranges::equal(term_sig.subspan(first, period), term_sig.subspan(first + period, period))) return {};
        const TermBlock head = term_block(g, terms, first, period, scratch), second = term_block(g, terms, first + period, period, scratch);
        if (head.entries.size() != second.entries.size() || head.roots != second.roots) return {};
        std::vector<int> step(head.entries.size(), 0);
        bool same = true;
        for (size_t p = 0; p < head.entries.size() && same; ++p) {
            const TermEntry &a = head.entries[p], &b = second.entries[p];
            if (a.op == expr::Symbolic::Op::Variable || a.op == expr::Symbolic::Op::Constant) step[p] = b.index - a.index;
            same = (step[p] == 0 || step[p] == 1) && advanced(a, b, step[p], 1);
        }
        /* a leaf the head reads at two steps is no copy; refused before the copies are compared */
        RepeatedTerms found{period, 0, {}};
        for (size_t p = 0; p < head.entries.size() && same; ++p) {
            const TermEntry &a = head.entries[p];
            if (a.graph < 0 || !(a.op == expr::Symbolic::Op::Variable || a.op == expr::Symbolic::Op::Constant)) continue;
            const auto [where, added] = found.strides.emplace(a.graph, step[p]);
            same = added || where->second == step[p];
        }
        if (!same) return {};
        /* the copies run on while each block is the head's formula; the first that is not ends them (a row of the boundary,
           say), and the copies before it are the run */
        size_t copies = 2;
        while (copies < available) {
            const TermBlock next = term_block(g, terms, first + copies * period, period, scratch);
            bool matched = next.roots == head.roots && next.entries.size() == head.entries.size();
            for (size_t p = 0; p < head.entries.size() && matched; ++p) matched = advanced(head.entries[p], next.entries[p], step[p], static_cast<int>(copies));
            if (!matched) break;
            ++copies;
        }
        if (copies < MIN_REPEAT_COPIES) return {};
        found.copies = copies;
        return found;
    }

    /* How many of the IR nodes a Repeat's body wrote (from node `first_node` on, its statements from `first_statement` on in
       `out`) differ from one copy to the next: one that reads a stepping operand, or a temporary whose value does. */
    size_t varying_nodes(size_t first_node, size_t first_statement) const
    {
        std::unordered_map<int, int> value_of; /* a temporary's slot: its value's node */
        for (size_t i = first_statement; i < out.size(); ++i)
            if (out[i].kind == IrStatement::Kind::Temporary) value_of[out[i].index] = out[i].value;
        std::vector<char> varies(ir.nodes.size() - first_node, 0);
        const auto flag = [&](int id) { return id >= static_cast<int>(first_node) && varies[static_cast<size_t>(id) - first_node] != 0; };
        size_t count = 0;
        for (size_t id = first_node; id < ir.nodes.size(); ++id) {
            const IrNode &n = ir.nodes[id];
            bool differs = (n.op == IrOp::Variable || n.op == IrOp::Constant) && n.stride != 0;
            if (n.op == IrOp::Temporary) {
                const auto found = value_of.find(n.index);
                differs = found != value_of.end() && flag(found->second);
            } else
                for (const int a : n.args) differs = differs || flag(a);
            varies[id - first_node] = differs;
            count += differs ? 1 : 0;
        }
        return count;
    }

    /* One function, name, writing the value of terms[k] into o[k]
       (W260): each node the terms need once, each a temporary (W715: in a
       register, which halves the instructions MIR compiles against a
       workspace array it loaded and stored). The nodes are written term by
       term (W816): a term's nodes after the nodes they read, each written
       just before its first use, and a term's value to o right after its
       own nodes. In the graph's order (the order made, which differentiates
       one state after another) a 1000-state ring's j_states held 4170 of its
       21001 values live at once, and MIR's register allocation (its spill
       slots and the live ranges it sums) grows with that count; the
       operations and their order within each value are the same, so every
       value is the same bit for bit. The terms of one row (one equation's
       derivatives, `rows` gives each term's) are a run, and a run of copies
       (an array's equations, repeated_terms) is one Repeat (W822): the first
       copy written once, its leaves stepping with the counter and its outputs
       one run apart, each copy's nodes its own, so the values are the same
       bit for bit too. */
    void derivatives(const expr::Symbolic &g, std::span<const expr::Term> terms, std::span<const int> rows, std::string_view name)
    {
        begin();
        /* the nodes written so far in the function (main), or in a Repeat's first copy (body): a node's temporary,
           and the step of each leaf of the body */
        struct Memo {
            std::unordered_map<int, int> slot;
            std::unordered_map<int, int> steps;
        };
        Memo main_memo;
        Memo *memo = &main_memo;
        /* a graph node's value as an operand: a leaf as itself, any other as the temporary it was written to */
        auto operand = [&](int id) {
            const expr::Symbolic::Node &n = g.node({id});
            const auto step = [&]() {
                const auto found = memo->steps.find(id);
                return found == memo->steps.end() ? 0 : found->second;
            };
            switch (n.op) {
            case expr::Symbolic::Op::Number: return number(n.number);
            case expr::Symbolic::Op::Constant: return leaf(IrOp::Constant, n.index, step());
            case expr::Symbolic::Op::Variable: return leaf(IrOp::Variable, n.index, step());
            default: return op(IrOp::Temporary, -1, -1, -1, memo->slot.at(id));
            }
        };
        /* node id's temporary, written after its arguments' (the caller has written them) */
        auto write = [&](int id) {
            const expr::Symbolic::Node &n = g.node({id});
            int value = -1;
            switch (n.op) {
            case expr::Symbolic::Op::Number:
            case expr::Symbolic::Op::Constant:
            case expr::Symbolic::Op::Variable:
                return;
            case expr::Symbolic::Op::Fun1:
                value = math_call(false, n.index, {operand(n.args[0])});
                break;
            case expr::Symbolic::Op::Fun2: {
                const int x = operand(n.args[0]), y = operand(n.args[1]);
                switch (n.index) {
                case expr::ADD: value = op(IrOp::Add, x, y); break;
                case expr::SUBTRACT: value = op(IrOp::Subtract, x, y); break;
                case expr::MULTIPLY: value = op(IrOp::Multiply, x, y); break;
                case expr::DIVIDE: value = op(IrOp::GuardedDivide, x, y); break;
                case expr::IEEE_DIVIDE: value = op(IrOp::Divide, x, y); break;
                case expr::INTEGER_POWER:
                    /* x^n multiplied out as the interpreter does, each product a temporary */
                    if (const expr::Symbolic::Node &e = g.node({n.args[1]});
                        e.op == expr::Symbolic::Op::Number && math::is_integer_power(e.number)) {
                        value = math::power_by_squaring(Tree{x, false, 0}, static_cast<int>(e.number), [&](const Tree &p, const Tree &q) {
                            return statement(op(IrOp::Multiply, p.ir, q.ir));
                        }).ir;
                        break;
                    }
                    [[fallthrough]];
                default:
                    value = math_call(true, n.index, {x, y});
                }
                break;
            }
            case expr::Symbolic::Op::Select:
                value = op(IrOp::Select, operand(n.args[0]), operand(n.args[2]), operand(n.args[1]));
                break;
            case expr::Symbolic::Op::Table:
            case expr::Symbolic::Op::TableSlope:
                value = helper_call(n.op == expr::Symbolic::Op::Table ? Helper::Table : Helper::TableSlope, {operand(n.args[0])}, n.index);
                break;
            }
            const int k = temps++;
            memo->slot.emplace(id, k);
            out.push_back({IrStatement::Kind::Temporary, k, value});
        };
        /* the nodes term j reads, written in post order (each argument before its node), iteratively: a graph
           deep as a model's expression must not take the host's stack */
        const auto write_term = [&](size_t j) {
            std::vector<std::pair<int, bool>> pending{{terms[j].id, false}}; /* a node, and whether its arguments are already on their way */
            while (!pending.empty()) {
                const auto [id, arguments_done] = pending.back();
                pending.pop_back();
                if (is_leaf_op(g.node({id}).op) || memo->slot.contains(id)) continue;
                if (arguments_done) {
                    write(id);
                    continue;
                }
                pending.push_back({id, true});
                const auto &args = g.node({id}).args;
                for (auto it = args.rbegin(); it != args.rend(); ++it)
                    if (*it >= 0 && !is_leaf_op(g.node({*it}).op) && !memo->slot.contains(*it)) pending.push_back({*it, false});
            }
        };
        /* the rows: runs of terms of one row, and for each run how many runs from it on have its size (the copies it may start) */
        std::vector<std::pair<size_t, size_t>> runs; /* a run's first term and its count of terms */
        for (size_t j = 0; j < terms.size();) {
            size_t end = j + 1;
            while (end < terms.size() && rows[end] == rows[j]) ++end;
            runs.emplace_back(j, end - j);
            j = end;
        }
        std::vector<size_t> same_size(runs.size(), 1);
        for (size_t r = runs.size(); r-- > 0;)
            if (r + 1 < runs.size() && runs[r + 1].second == runs[r].second) same_size[r] = same_size[r + 1] + 1;
        BlockScratch scratch{std::vector<int>(g.size(), -1), {}};
        std::vector<std::uint64_t> term_sig(terms.size()), node_sig = node_signatures(g);
        for (size_t j = 0; j < terms.size(); ++j) term_sig[j] = node_sig[static_cast<size_t>(terms[j].id)];
        for (size_t r = 0; r < runs.size();) {
            const auto [first, count] = runs[r];
            const RepeatedTerms repeated = repeated_terms(g, terms, term_sig, first, count, same_size[r], scratch);
            if (repeated.copies == 0) {
                for (size_t j = first; j < first + count; ++j) {
                    write_term(j);
                    out.push_back({IrStatement::Kind::Output, static_cast<int>(j), operand(terms[j].id)});
                }
                ++r;
                continue;
            }
            /* the first copy, written once with its own nodes: its leaves step with the Repeat's counter, its outputs by the run's size */
            Memo body;
            body.steps = repeated.strides;
            memo = &body;
            const size_t body_nodes = ir.nodes.size(), body_statements = out.size();
            out.push_back({IrStatement::Kind::RepeatBegin, static_cast<int>(repeated.copies)});
            for (size_t j = first; j < first + count; ++j) {
                write_term(j);
                out.push_back({IrStatement::Kind::Output, static_cast<int>(j), operand(terms[j].id), -1, -1, static_cast<int>(count)});
            }
            out.push_back({IrStatement::Kind::RepeatEnd});
            repeat_nodes += (repeated.copies - 1) * varying_nodes(body_nodes, body_statements);
            memo = &main_memo;
            r += repeated.copies;
        }
        ir.functions.push_back(finish(IrFunction::Kind::Jacobian, std::string(name), -1));
    }

    /* Every user function written alone, then those whose whole dependency graph compiled kept (`allowed`): their
       bodies are the first of ir.functions, in the functions' order; a refused one is a warning. */
    Result<> user_functions(std::vector<Error> &warnings)
    {
        const Model &m=s.model();
        if (m.compiler.nfun<0 || static_cast<size_t>(m.compiler.nfun)>m.compiler.ufun_programs.size()) return refuse("an out-of-range function count");
        calls.resize(static_cast<size_t>(m.compiler.nfun));
        pure_function.assign(static_cast<size_t>(m.compiler.nfun),0);
        inline_body.assign(static_cast<size_t>(m.compiler.nfun),-1);
        array_pure_function.assign(static_cast<size_t>(m.compiler.nfun),0);
        function_reads.resize(static_cast<size_t>(m.compiler.nfun));
        std::vector<IrFunction> bodies(static_cast<size_t>(m.compiler.nfun));
        std::vector<char> written(static_cast<size_t>(m.compiler.nfun),0); /* the user functions that compiled alone */
        std::vector<Error> refusals(static_cast<size_t>(m.compiler.nfun));
        for (int i=0;i<m.compiler.nfun;++i) {
            place=model_place(m,m.compiler.ufun_names[i]);
            if (m.compiler.narg_fun[i]<0) {
                refusals[i]=refuse("an out-of-range argument count").error();
                continue;
            }
            if (auto r=program(m.compiler.ufun_programs[i].rpn,i,true);!r) refusals[i]=r.error();
            else { bodies[i]=std::move(*r); written[i]=1; array_pure_function[i]=array_pure; function_reads[i]=variable_reads; }
        }
        /* Only functions whose entire dependency graph compiled are callable.
           Iterative topological removal also refuses recursion, without following
           untrusted input on the host stack. Impurity was conservative during
           translation, so removing a function cannot change a caller's ordering. */
        allowed.assign(static_cast<size_t>(m.compiler.nfun),0);
        for (int pass=0;pass<m.compiler.nfun;++pass)
            for (int i=0;i<m.compiler.nfun;++i)
                if (!allowed[i] && written[i]
                    && std::all_of(calls[i].begin(),calls[i].end(),[&](int j){return allowed[j];})) allowed[i]=1;
        for (int pass=0;pass<m.compiler.nfun;++pass)
            for (int i=0;i<m.compiler.nfun;++i)
                if (!allowed[i] || std::any_of(calls[i].begin(),calls[i].end(),[&](int j){return !array_pure_function[j];})) array_pure_function[i]=0;
        for (int pass=0;pass<m.compiler.nfun;++pass)
            for (int i=0;i<m.compiler.nfun;++i)
                for (int j:calls[i]) function_reads[i].insert(function_reads[j].begin(),function_reads[j].end());
        for (int i=0;i<m.compiler.nfun;++i) {
            if (!allowed[i]) {
                place=model_place(m,m.compiler.ufun_names[i]);
                const Error error=!written[i]?refusals[i]
                    :refuse("calls a refused or recursive user function").error();
                warnings.push_back(error);
                continue;
            }
            ir.functions.push_back(std::move(bodies[i]));
        }
        return {};
    }

};
}

Result<ModelIr> model_ir(const SessionParts &s)
{
    const Model &m=s.model();
    Generator g{s};
    g.refusal_before="cannot be written: ";
    g.refusal_after="";
    std::vector<Error> unreachable; /* a user function that did not translate matters only to a program that calls it (the Generator refuses that call) */
    if (const Result<> functions=g.user_functions(unreachable);!functions) return std::unexpected(functions.error());
    ModelIr out;
    for (int i=0;i<m.compiler.nfun;++i)
        if (g.allowed[i]) out.origin.push_back({m.compiler.ufun_names[i],model_place(m,m.compiler.ufun_names[i])});
    const auto add=[&](const Program &p,const std::string &name,Place place,std::vector<size_t> *list)->Result<> {
        g.place=place;
        g.dynamic=false;
        auto function=g.program(p.rpn,0,false,name);
        if (!function) return std::unexpected(function.error());
        g.ir.functions.push_back(std::move(*function));
        out.origin.push_back({name,std::move(place)});
        if (list) list->push_back(g.ir.functions.size()-1);
        return {};
    };
    for (int i=0;i<m.node;++i)
        if (const Result<> r=add(m.programs[static_cast<size_t>(i)],m.uvar_names[static_cast<size_t>(i)],model_place(m,m.uvar_names[static_cast<size_t>(i)]),&out.equations);!r) return std::unexpected(r.error());
    /* the equations' statements are the model's Ode, Map and Volterra statements in order: the k-th is equation k */
    size_t equation=0;
    int open_group=0;
    for (const odex::Statement &st:m.record->statements) {
        if (st.kind!=odex::Statement::Kind::Ode && st.kind!=odex::Statement::Kind::Map && st.kind!=odex::Statement::Kind::Volterra) continue;
        const odex::ArrayCopy &a=st.array;
        if (st.kind==odex::Statement::Kind::Ode && a.group!=0 && !a.interleaved && equation<static_cast<size_t>(m.node)) {
            if (a.group==open_group) ++out.arrays.back().count;
            else out.arrays.push_back({equation,1,a.index,a.lo,a.hi,a.step});
            open_group=a.group;
        } else open_group=0;
        ++equation;
    }
    std::erase_if(out.arrays,[](const ModelIr::ArrayLine &line){return line.count<2;});
    for (int i=m.node;i<m.node+m.fix_var;++i) {
        const std::string &name=m.fixinfo[static_cast<size_t>(i-m.node)].name;
        if (const Result<> r=add(m.programs[static_cast<size_t>(i)],name,model_place(m,name),&out.fixed);!r) return std::unexpected(r.error());
    }
    for (int i=m.node;i<m.neq;++i) { /* extra (my_rhs.cpp): the aux column i is program i + fix_var */
        const std::string &name=m.uvar_names[static_cast<size_t>(i)];
        if (const Result<> r=add(m.programs[static_cast<size_t>(i+m.fix_var)],name,model_place(m,name),&out.aux);!r) return std::unexpected(r.error());
    }
    for (const auto &d:m.compiler.derived) {
        if (const Result<> r=add(d.form,"",d.where,nullptr);!r) return std::unexpected(r.error());
        out.derived.push_back({g.ir.functions.size()-1,d.index});
    }
    out.ir=std::move(g.ir);
    return out;
}

std::vector<ModelProgram> model_programs(Model &m)
{
    std::vector<ModelProgram> entries;
    auto add=[&](Program &p,std::string symbol,Place place,int user=-1,bool dynamic=false) {
        if (p.rpn.empty()) return;
        if (place.source.empty()) place.source=model_source_line(m,place.file,place.line);
        entries.push_back({&p,std::move(symbol),std::move(place),user,dynamic});
    };
    for (size_t i=0;i<m.programs.size();++i) {
        if (m.programs[i].rpn.empty()) continue;
        const std::string &name=i<static_cast<size_t>(m.node)?m.uvar_names[i]
            :i<static_cast<size_t>(m.node+m.fix_var)?m.fixinfo[i-m.node].name:m.uvar_names[i-m.fix_var];
        add(m.programs[i],xpp::format("p_eq{}",i),model_place(m,name));
    }
    for (size_t i=0;i<m.compiler.ufun_programs.size();++i)
        if (!m.compiler.ufun_programs[i].rpn.empty()) add(m.compiler.ufun_programs[i],xpp::format("p_fun{}",i),model_place(m,m.compiler.ufun_names[i]),static_cast<int>(i));
    for (size_t i=0;i<m.svars.size();++i) add(m.svars[i].form,xpp::format("p_solv{}",i),m.svars[i].where);
    for (size_t i=0;i<m.aeqns.size();++i) add(m.aeqns[i].form,xpp::format("p_dae{}",i),m.aeqns[i].where);
    for (size_t i=0;i<m.markov.size();++i)
        for (size_t j=0;j<m.markov[i].command.size();++j)
            add(m.markov[i].command[j],xpp::format("p_markov{}_{}",i,j),model_place(m,m.markov[i].name));
    for (size_t i=0;i<m.compiler.kernels.size();++i) {
        add(m.compiler.kernels[i].formula,xpp::format("p_kernel{}",i),m.compiler.kernels[i].where);
        add(m.compiler.kernels[i].kerform,xpp::format("p_convolution{}",i),m.compiler.kernels[i].where);
    }
    for (size_t i=0;i<m.flags.size();++i) {
        if (m.flags[i].comcond.rpn.empty()) continue;
        const Place place=model_statement_place(m,odex::Statement::Kind::Event,static_cast<int>(i));
        add(m.flags[i].comcond,xpp::format("p_condition{}",i),place);
        for (size_t j=0;j<m.flags[i].comrhs.size();++j) add(m.flags[i].comrhs[j],xpp::format("p_event{}_{}",i,j),place);
    }
    for (size_t i=0;i<m.compiler.derived.size();++i) add(m.compiler.derived[i].form,xpp::format("p_derived{}",i),m.compiler.derived[i].where);
    for (size_t i=0;i<m.networks.size();++i)
        if (!m.networks[i].f.rpn.empty()) add(m.networks[i].f,xpp::format("p_network{}",i),model_place(m,m.networks[i].name),-1,true);
    return entries;
}

namespace {
/* Why the model's right-hand side is not pure (Model::rhs_impurity), or
   "": every program native and reading only the constants, the variables
   and the immutable lookup tables, nothing running between the fixed
   variables and the equations. Networks and DAEs run between them even when
   no equation calls their helpers; function tables can rebuild on parameter
   changes (or when autoeval is toggled), so they are never frozen. */
std::string rhs_impurity(const SessionParts &s)
{
    const Model &m=s.model();
    if (m.nsvar>0) return "algebraic solve variables (a DAE)";
    if (m.compiler.ndelays>0) return "delays";
    if (m.naeqn>0) return "algebraic equations";
    if (m.nnetwork>0) return "networks";
    if (m.nmarkov>0) return "Markov variables";
    if (m.compiler.nkernel>0) return "integral equations";
    for (int i=0;i<s.ntable;++i) if (s.tables[i].flag==2) return "a function table that is rebuilt";
    for (int i=0;i<m.node+m.fix_var;++i) {
        const auto &p=m.programs[i];
        const int available=i<m.node?m.node+m.fix_var:i;
        if (!p.compiled && !p.in_rhs) return "a right-hand side that is not compiled";
        if (!p.array_pure) return "a right-hand side that uses shifts, random numbers or sums over an index";
        if (!std::all_of(p.native_reads.begin(),p.native_reads.end(),[&](int v){return v<=available;}))
            return "a fixed variable that reads one after it";
    }
    for (const auto &d:m.compiler.derived)
        if ((!d.form.compiled && !d.form.in_rhs) || !d.form.array_pure || !d.form.native_reads.empty()) return "a derived parameter that is not compiled or reads variables";
    return "";
}

/* what couples m's states beyond what their equations read, or "" (Model::subsystems) */
std::string subsystem_joined_by(const SessionParts &s)
{
    const Model &m=s.model();
    if (m.flag_count()>0) return "global flags (events)";
    if (m.bc_defined>0) return "boundary conditions";
    return rhs_impurity(s);
}

/* The tiers (W821). TinyCC compiles a model's code at once, on the core thread (compile_programs, make_jacobian), and a run starts
   on it: the work it does is counted in the IR nodes each call evaluates (ParserState::native_work: the programs', the right-hand
   side's and the exact Jacobian's, the module functions they call included). MIR's build follows (pending_build): with a processor to
   spare it starts on its thread at once and takes over at the next right-hand side; with none, the thread and the run would share
   one CPU, so the run pays for the compile, and the build is due once the work already done in TinyCC's code is worth the compile
   (ski-rental: the cost of waiting is what the run has already spent, so a run that ends sooner never pays more than twice
   what it would have, and one that does not end pays the compile once). The compile's cost is estimated from the module's size,
   the nodes its code holds (IrSize::emitted) and MIR's set-up (MIR_SET_UP_NODES); the work done is counted in node evaluations,
   so the due count is NATIVE_EVALUATIONS_PER_NODE of them for each node of the code.
   Measured (W821, instructions on one pinned CPU, `taskset -c 2`, `TMPDIR` empty): MIR's compile costs 39-76 k instructions a node
   of the code, set-up included (clustor 39 k of 18 k nodes, mu26 46 k of 1.5 k, kuramot100 76 k of 1.9 k, lecar 67 k of 104, izh
   68 k of 44), and a node evaluation of TinyCC's code 6.6-12 (mu26 6.6, lecar 8, clustor 9.4, izh 12; the integrator's own work
   included), so the break-even is some 45 k / 6.6 = 6800 evaluations a node; W821's 10000 kept mu26's run of 10 M evaluations, which
   ends at 60% of the break-even, from starting a compile that would cost it 60% more than the whole run. What the ratio does
   not see is the saving MIR's code makes a node, from 0.8 instructions (mu26) to 13 (lecdiff1600): at 10000 the long runs that
   save most started their compile after 3 M evaluations (lecdiff) or 17-21 M (mu26, kuramot100), 0.5% to 9% of their runs, and
   20000 doubles both.
   A model whose evaluations are mostly outside its nodes (clustor's networks: MIR saves 0.4 M of 3.9 M instructions per time unit of
   500) never repays a compile of 0.64 G: at 10000 a node it started 6% before the end of python/clustor-dopri8's run (2.76 G
   instructions, 2.13 G without; W841, #922), so 20000 (2.13 G; the other models of the batch move 0-2%: lecdiff200 316 to 321 M,
   mu26 2605 to 2614 M, kuramot100 27.96 to 27.99 G, lecar, amari and screen/clustor none).
   Without TinyCC's code (the interpreter runs until MIR is ready) the rule before W821 holds (W717, W817): a processor to spare
   starts the build after INTERPRETED_OPS_BEFORE_BUILD interpreted instructions; none, after INTERPRETED_OPS_PER_NODE_BEFORE_BUILD
   ops a node (measured on one pinned CPU, instructions at once / 100 / 400 / 1500 ops a node: lecdiff1600 7.25 / 7.32 / 7.50 /
   8.13 G, mu26 141 / 146 / 160 / 209 M, lecdiff200 447 / 456 / 477 / 558 M, izh_2003_rs 16.2 / 16.4 / 17.0 / 19.3 M, lecar 19.5 /
   19.7 / 20.3 / 22.6 M). */
constexpr std::uint64_t INTERPRETED_OPS_BEFORE_BUILD=1000000;
constexpr std::uint64_t INTERPRETED_OPS_PER_NODE_BEFORE_BUILD=100;
constexpr std::uint64_t NATIVE_EVALUATIONS_PER_NODE=20000;
/* MIR's context, generator and code region, as the compile of this many nodes (measured: izh's compile of 44 nodes costs 3 M, lecdiff's of 158 costs 9 M) */
constexpr std::uint64_t MIR_SET_UP_NODES=150;
constexpr std::uint64_t NO_WORK_DUE=std::numeric_limits<std::uint64_t>::max();
/* A function's size in IR nodes, counted by their trees (a shared node counts at each use): `evaluated` is what one run through it
   evaluates, the work of one native call (ParserState::native_work), the bodies of the module functions it calls included;
   `emitted` is what its own code holds, the work of compiling it. A Repeat's body is evaluated once per pass (its count), a loop's
   once per pass when both bounds are numbers (the count they make; one for a computed bound), a conditional's both arms (an upper
   bound): the array equations of one line are one loop, so the count is theirs. */
struct IrSize {
    std::uint64_t evaluated = 0, emitted = 0;
};
class IrSizes {
public:
    explicit IrSizes(const IrModule &ir) : ir_(ir), memo_(ir.functions.size()), state_(ir.functions.size(), 0)
    {
        for (size_t k = 0; k < ir.functions.size(); ++k) index_.emplace(ir.functions[k].name, k);
    }
    IrSize of(const IrFunction &f) const { return of(static_cast<size_t>(&f - ir_.functions.data())); }
    IrSize of(std::string_view name) const { return of(index_.at(std::string(name))); }
    /* what the code of every function holds */
    std::uint64_t emitted() const
    {
        std::uint64_t total = 0;
        for (const IrFunction &f : ir_.functions) total += of(f).emitted;
        return total;
    }

private:
    const IrModule &ir_;
    mutable std::vector<IrSize> memo_;
    mutable std::vector<char> state_; /* by function: 0 not yet, 1 being measured (a call cycle adds nothing), 2 done */
    std::unordered_map<std::string, size_t> index_;

    IrSize of(size_t k) const;
    /* the tree at `root` (the nodes it holds, and what the module functions it calls evaluate) */
    struct Tree {
        std::uint64_t nodes = 0, callees = 0;
    };
    Tree tree(int root) const;
};

IrSizes::Tree IrSizes::tree(int root) const
{
    Tree size;
    std::vector<int> pending{root};
    while (!pending.empty()) {
        const IrNode &n = ir_.nodes[static_cast<size_t>(pending.back())];
        pending.pop_back();
        ++size.nodes;
        if (n.op != IrOp::Call) {
            for (int a : n.args)
                if (a >= 0) pending.push_back(a);
            continue;
        }
        for (int a : ir_.arguments(n)) pending.push_back(a);
        const IrCallee &callee = ir_.callees[static_cast<size_t>(n.callee)];
        if (callee.address != nullptr) continue;
        if (const auto found = index_.find(callee.name); found != index_.end()) size.callees += of(found->second).evaluated;
    }
    return size;
}

IrSize IrSizes::of(size_t k) const
{
    if (state_[k] == 2) return memo_[k];
    if (state_[k] == 1) return {};
    state_[k] = 1;
    const IrFunction &f = ir_.functions[k];
    IrSize size;
    std::uint64_t factor = 1;
    std::vector<std::uint64_t> outer; /* the factor outside each open Repeat or loop */
    const auto add = [&](int root) {
        if (root < 0) return;
        const Tree t = tree(root);
        size.evaluated += factor * (t.nodes + t.callees);
        size.emitted += t.nodes;
    };
    for (const IrStatement &s : f.statements) {
        switch (s.kind) {
        case IrStatement::Kind::RepeatBegin:
            outer.push_back(factor);
            factor *= static_cast<std::uint64_t>(std::max(1, s.index));
            break;
        case IrStatement::Kind::LoopBegin: {
            add(s.value);
            add(s.other);
            outer.push_back(factor);
            std::uint64_t count = 1;
            if (s.value >= 0 && s.other >= 0 && ir_.nodes[static_cast<size_t>(s.value)].op == IrOp::Number && ir_.nodes[static_cast<size_t>(s.other)].op == IrOp::Number) {
                const long long low = static_cast<long long>(ir_.nodes[static_cast<size_t>(s.value)].number), high = static_cast<long long>(ir_.nodes[static_cast<size_t>(s.other)].number);
                count = high >= low ? static_cast<std::uint64_t>(high - low + 1) : 1;
            }
            factor *= count;
            break;
        }
        case IrStatement::Kind::RepeatEnd:
        case IrStatement::Kind::LoopEnd:
            add(s.value);
            factor = outer.back();
            outer.pop_back();
            break;
        default:
            add(s.value);
            add(s.other);
            break;
        }
    }
    factor = 1;
    add(f.result);
    state_[k] = 2;
    return memo_[k] = size;
}

bool spare_processor() { return program.physical_threads()>1; }
std::shared_ptr<NativeBuild> pending_build(const SessionParts &s, IrModule ir, Place place, bool tier_one, std::uint64_t code_nodes, size_t repeat_nodes = 0)
{
    auto build=std::make_shared<NativeBuild>();
    build->ir=std::move(ir);
    build->place=std::move(place);
    /* the count the interpreter's trigger is due by: the IR nodes, and those the copies of a Repeat would have had (Generator::repeat_nodes) */
    const size_t nodes=build->ir.nodes.size()+repeat_nodes;
    build->due=s.parser.interpreted_ops+(spare_processor()?INTERPRETED_OPS_BEFORE_BUILD:INTERPRETED_OPS_PER_NODE_BEFORE_BUILD*nodes);
    build->due_work=!tier_one?NO_WORK_DUE:s.parser.native_work+(spare_processor()?0:NATIVE_EVALUATIONS_PER_NODE*(code_nodes+MIR_SET_UP_NODES));
    return build;
}

/* The programs' code placed in the model: each program's function and the right-hand side's, every address found before any is
   set, so code that lacks one changes nothing; the code in use before (TinyCC's, when MIR's comes) is released by the
   assignment, which no call runs across (the core thread, at the start of a right-hand side, or before the threads start) */
Result<> place_programs(Model &m, std::unique_ptr<native::Code> &code, const std::string &rhs_symbol, const std::string &derived_symbol, bool equations)
{
    const auto entries=model_programs(m);
    std::vector<std::pair<Program *,void *>> functions;
    for (const auto &entry:entries) {
        if (!entry.code->compiled) continue;
        auto address=code->function(entry.symbol);
        if (!address) return std::unexpected(address.error());
        functions.emplace_back(entry.code,*address);
    }
    void *rhs=nullptr;
    if (!rhs_symbol.empty()) {
        auto address=code->function(rhs_symbol);
        if (!address) return std::unexpected(address.error());
        rhs=*address;
    }
    void *derived=nullptr;
    if (!derived_symbol.empty()) {
        auto address=code->function(derived_symbol);
        if (!address) return std::unexpected(address.error());
        derived=*address;
    }
    for (const auto &[program,address]:functions) program->native=reinterpret_cast<decltype(Program::native)>(address);
    if (!rhs_symbol.empty()) m.native_rhs=reinterpret_cast<decltype(Model::native_rhs)>(rhs);
    if (!derived_symbol.empty()) m.compiler.native_derived=reinterpret_cast<decltype(CompilerTables::native_derived)>(derived);
    m.native_equations=equations;
    m.native_program=std::move(code);
    return {};
}

Result<std::vector<Error>> compile_programs(const SessionParts &s, const std::vector<ModelProgram> &entries)
{
    Model &m=s.model();
    /* every program from its RPN again: no code of before, native or the interpreter's (a test rewrites RPN and translates) */
    for (const auto &entry:entries) { entry.code->forget_code(); entry.code->in_rhs=false; entry.code->array_pure=false; entry.code->native_reads.clear(); }
    m.native_rhs=nullptr;
    m.native_rhs_nodes=0;
    m.native_equations=false;
    m.compiler.native_derived=nullptr;
    m.compiler.native_derived_nodes=0;
    m.compiler.native_derived_count=0;
    m.native_program.reset();
    m.native_build.reset();
    if (!s.numerics->compile || entries.empty()) return {};
    Generator g{s};
    std::vector<Error> warnings;
    if (const Result<> written=g.user_functions(warnings);!written) return std::unexpected(written.error());
    std::vector<const ModelProgram *> emitted;
    std::vector<size_t> function_of; /* each emitted program's function in g.ir.functions */
    for (const auto &entry:entries) {
        g.place=entry.place;
        if (entry.user_index>=0) {
            const int i=entry.user_index;
            if (i>=m.compiler.nfun || !g.allowed[i]) continue;
            g.ir.functions.push_back(g.bridge(i,entry.symbol));
        } else {
            g.dynamic=entry.dynamic_operands;
            auto r=g.program(entry.code->rpn,0,false,entry.symbol);
            if (!r) {
                warnings.push_back(r.error());
                continue;
            }
            g.ir.functions.push_back(std::move(*r));
        }
        entry.code->array_pure=entry.user_index<0 && g.array_pure;
        if (entry.user_index<0) entry.code->native_reads.assign(g.variable_reads.begin(),g.variable_reads.end());
        if (g.loops>0 && entry.user_index<0) g.called.emplace(entry.code,entry.symbol);
        emitted.push_back(&entry);
        function_of.push_back(g.ir.functions.size()-1);
    }
    /* the fixed variables and the equations once more, as one function
       (W468, W469): the fixed variables when each compiled alone, the
       equations too when each did and nothing runs between them and the
       fixed variables (my_rhs: no network, no DAE) */
    const std::string rhs_symbol="p_rhs";
    std::set<const Program *> native;
    for (const auto *entry:emitted) native.insert(entry->code);
    const auto compiled_alone=[&](const Program &p){return native.contains(&p);};
    const auto fixed_begin=m.programs.begin()+m.node;
    bool rhs=static_cast<size_t>(m.node+m.fix_var)<=m.programs.size()
        && std::all_of(fixed_begin,fixed_begin+m.fix_var,compiled_alone);
    bool equations=rhs && m.node>0 && m.nnetwork==0 && m.nsvar==0 && m.naeqn==0
        && std::all_of(m.programs.begin(),fixed_begin,compiled_alone);
    rhs=rhs && (m.fix_var>0 || equations);
    if (rhs) {
        g.place=m.fix_var>0?model_place(m,m.fixinfo[0].name):model_place(m,m.uvar_names[0]);
        g.dynamic=false;
        const std::span<const Program> equation_programs=equations?std::span<const Program>{m.programs.data(),static_cast<size_t>(m.node)}:std::span<const Program>{};
        auto r=g.rhs({m.programs.data()+m.node,static_cast<size_t>(m.fix_var)},m.node+1,equation_programs,rhs_symbol);
        if (r) {
            g.ir.functions.push_back(std::move(*r));
        } else {
            warnings.push_back(r.error());
            rhs=equations=false;
        }
    }
    /* W846: the derived quantities as one function when each compiled alone and none has a sum (a sum's function is called, not
       written, as p_rhs does it): a fit or an autoX run works them all out at each evaluation, so one call a formula was most of
       a short run's time. Their own functions are then not compiled. */
    const std::string derived_symbol="p_derived";
    bool fused=!m.compiler.derived.empty() && std::all_of(m.compiler.derived.begin(),m.compiler.derived.end(),
        [&](const auto &d){return compiled_alone(d.form) && !g.called.contains(&d.form);});
    if (fused) {
        auto r=g.derived(m.compiler.derived,derived_symbol);
        if (r) g.ir.functions.push_back(std::move(*r));
        else {warnings.push_back(r.error());fused=false;}
    }
    /* W708: when p_rhs writes the equations, an equation's own function is
       not compiled: nothing calls it in a run with p_rhs but a Markov
       variable's or an integral equation's evaluation (they keep theirs),
       and the interpreter serves any other caller of evaluate; one with a
       sum is called by p_rhs and keeps its own. W715: a fixed variable's
       likewise when p_rhs computes the fixed variables: every evaluation of
       them goes through fixed_values (my_rhs.cpp), which calls p_rhs. */
    const size_t compiled_count=emitted.size();
    std::vector<Program *> in_rhs;
    const Program *const first_equation=m.programs.data(),*const first_fixed=first_equation+m.node,*const fixed_end=first_fixed+m.fix_var;
    const bool equations_in_rhs=equations && m.nmarkov==0 && m.compiler.nkernel==0;
    if (equations_in_rhs || rhs || fused) {
        /* one pass, in order: erasing each moved the rest of the functions and of the list every time */
        std::vector<char> dropped(g.ir.functions.size(),0);
        std::vector<const ModelProgram *> still;
        for (size_t k=0;k<emitted.size();++k) {
            Program *code=emitted[k]->code;
            const bool in_p_rhs=(equations_in_rhs && code>=first_equation && code<first_fixed) || (rhs && code>=first_fixed && code<fixed_end)
                || (fused && std::any_of(m.compiler.derived.begin(),m.compiler.derived.end(),[&](const auto &d){return &d.form==code;}));
            if (!in_p_rhs || g.called.contains(code)) {still.push_back(emitted[k]);continue;}
            dropped[function_of[k]]=1;
            in_rhs.push_back(code);
        }
        size_t kept=0;
        for (size_t f=0;f<g.ir.functions.size();++f)
            if (!dropped[f]) {if (kept!=f) g.ir.functions[kept]=std::move(g.ir.functions[f]);++kept;}
        g.ir.functions.resize(kept);
        emitted=std::move(still);
    }
    if (emitted.empty() && !rhs && !fused) return warnings;
    /* what one call of each function evaluates, for the work a run has done (ParserState::native_work) */
    const IrSizes sizes(g.ir);
    const std::uint64_t code_nodes=sizes.emitted();
    for (const auto *entry:emitted) entry->code->native_nodes=sizes.of(entry->symbol).evaluated;
    if (rhs) m.native_rhs_nodes=sizes.of(rhs_symbol).evaluated;
    if (fused) {
        m.compiler.native_derived_nodes=sizes.of(derived_symbol).evaluated;
        m.compiler.native_derived_count=m.compiler.derived.size();
    }
    for (const auto *entry:emitted) entry->code->compiled=true;
    for (Program *code:in_rhs) code->in_rhs=true;
    const Place place=entries.front().place;
    const std::string placed_rhs=rhs?rhs_symbol:std::string();
    const std::string placed_derived=fused?derived_symbol:std::string();
    /* TinyCC's code first (W821): the run starts on it at once; MIR's build follows */
    bool tier_one=false;
    if (auto tiny=native::Code::compile(g.ir,native::Compiler::TinyCC,place)) {
        if (auto placed=place_programs(m,*tiny,placed_rhs,placed_derived,equations)) tier_one=true;
        else log(XPP_LOG_WARN,"{}; MIR compiles instead\n",placed.error().text());
    } else log(XPP_LOG_WARN,"{}; MIR compiles instead\n",tiny.error().text());
    m.native_build=pending_build(s,std::move(g.ir),place,tier_one,code_nodes);
    if (rhs) m.native_build->rhs_symbol=rhs_symbol;
    m.native_build->derived_symbol=placed_derived;
    m.native_build->equations=equations;
    log(XPP_LOG_INFO,"Compiled {} of {} programs ({} nodes{})\n",compiled_count,entries.size(),m.native_build->ir.nodes.size(),tier_one?", TinyCC's code in use":"");
    return warnings;
}

/* the build's thread, detached (a run with no processor to spare waits for it in `taken`): it reads the module the build holds and writes only the build's result, then
   says so; an allocation it cannot make ends the program as one on the core thread would, since no exception
   may leave a thread. A run that ends first leaves it to end with the process, so the rule for it, and for any thread the core
   leaves running at exit() (xpp_http's, the stdin reader), is that it touches only what it owns through its own
   reference or what is never destroyed (xpp_mir.cpp `compiling`, NativeBuild::finish): no static object of a library, and no std::atomic wait or
   notify, whose waiter pool is one (W837). */
void start_build(const SessionParts &s, std::shared_ptr<NativeBuild> build)
{
    build->started=true;
    log(XPP_LOG_INFO,"MIR compiles {} nodes on a thread, after {} interpreted instructions and {} native node evaluations\n",build->ir.nodes.size(),s.parser.interpreted_ops,s.parser.native_work);
    std::thread([build=std::move(build)]() mutable noexcept {
        try {
            build->result=native::Code::compile(build->ir,native::Compiler::MIR,build->place);
        } catch (const std::bad_alloc &) {
            out_of_memory("the model's native code");
        }
        build->finish();
        build.reset(); /* the compiled code goes here when the Model went first */
    }).detach();
}

/* The build once its thread is done, with its result: started when it is due (now, when waiting) and taken when
   it has finished (waited for, when waiting), else null. Taken, it is no longer pending. */
std::shared_ptr<NativeBuild> taken(const SessionParts &s, std::shared_ptr<NativeBuild> &pending, bool wait)
{
    if (!pending) return nullptr;
    if (!pending->started) {
        if (!wait && !pending->needs_attention(s.parser.interpreted_ops,s.parser.native_work)) return nullptr;
        start_build(s,pending);
        wait=wait||!spare_processor(); /* with no processor to spare the run waits for its compile */
    }
    if (!wait && !pending->finished.load(std::memory_order_acquire)) return nullptr;
    const std::shared_ptr<NativeBuild> build=std::move(pending);
    build->wait_finished(); /* blocks until the thread says it is done; no busy wait */
    return build;
}

/* a compile that failed (MIR's, or TinyCC's placed): logged once with its place; the code in use stays, and with none the
   programs are interpreted (rhs_impurity says why) */
void compile_failed(const SessionParts &s, const Error &error)
{
    Model &m=s.model();
    log(XPP_LOG_WARN,"{}\n",error.text());
    if (m.native_program) return;
    for (const auto &entry:model_programs(m)) {entry.code->compiled=false;entry.code->native=nullptr;}
    m.rhs_impurity=error.text();
}

/* the programs' code taken from their build (build_native) */
void take_programs(const SessionParts &s, bool wait)
{
    Model &m=s.model();
    const std::shared_ptr<NativeBuild> build=taken(s,m.native_build,wait);
    if (!build) return;
    /* the programs by their names again, not by pointers kept since the translation: a vector of them may have
       grown (a derived parameter added, the browser's column), and a program edited since (dae_fun.cpp's guess)
       is no longer compiled */
    if (!build->result) return compile_failed(s,build->result.error());
    if (auto placed=place_programs(m,*build->result,build->rhs_symbol,build->derived_symbol,build->equations); !placed) return compile_failed(s,placed.error());
    log(XPP_LOG_INFO,"The native code is in use (MIR, {})\n",wait?"waited for":"built while the run went on");
}

/* the exact Jacobian's functions placed (place_programs' rule): each one's address first, then all set */
Result<> place_jacobian(ExactJacobian &j, std::unique_ptr<native::Code> &code)
{
    std::vector<std::pair<JacobianProgram *,void *>> functions;
    const auto address=[&](const std::string &name,JacobianProgram &p) -> Result<> {
        auto found=code->function(name);
        if (!found) return std::unexpected(found.error());
        functions.emplace_back(&p,*found);
        return {};
    };
    for (auto &[k,column]:j.by_constant)
        if (auto r=address(xpp::format("j_c{}",k),column); !r) return r;
    if (auto r=address("j_time",j.time); !r) return r;
    if (auto r=address("j_states",j.states); !r) return r;
    for (const auto &[p,fn]:functions) p->function=reinterpret_cast<decltype(JacobianProgram::function)>(fn);
    j.code=std::move(code);
    return {};
}

/* the exact Jacobian's functions taken from their build (build_native): until then, and after a failure of MIR's (logged),
   the code in use stays, or its derivatives are evaluated from their graph, bit for bit the same */
void take_jacobian(const SessionParts &s, bool wait)
{
    ExactJacobian &j=s.model().jacobian;
    const std::shared_ptr<NativeBuild> build=taken(s,j.build,wait);
    if (!build) return;
    const auto failed=[&](const Error &error) {
        log(XPP_LOG_WARN,"{}\n",error.text());
        if (j.code) return;
        j.states.function=nullptr;
        j.time.function=nullptr;
        for (auto &[k,column]:j.by_constant) column.function=nullptr;
    };
    if (!build->result) return failed(build->result.error());
    if (auto placed=place_jacobian(j,*build->result); !placed) return failed(placed.error());
    log(XPP_LOG_INFO,"The exact Jacobian's native code is in use (MIR, {})\n",wait?"waited for":"built while the run went on");
}

}

void build_native(const SessionParts &s, bool wait)
{
    take_programs(s,wait);
    take_jacobian(s,wait);
}

bool native_ready(const SessionParts &s)
{
    Model &m=s.model();
    /* a run on threads never has its code swapped while they run: with TinyCC's in use, MIR's build is dropped and the threads run
       TinyCC's code (W821); otherwise the build is waited for, as before (W715) */
    if (m.native_program && m.native_program->compiler()==native::Compiler::TinyCC) m.native_build.reset();
    else take_programs(s,true);
    if (m.jacobian.code && m.jacobian.code->compiler()==native::Compiler::TinyCC) m.jacobian.build.reset();
    else take_jacobian(s,true);
    return s.parser.host->pure_rhs_ready(*s.parser.owner);
}

namespace {

/* The model's exact Jacobian (W260): its derivatives (expr_derivative.h),
   evaluated from their graph until MIR has compiled them as a program of
   their own (a build like the programs', W717), one function for the
   state's columns, one for the time's and one for each constant's that a
   right-hand side depends on; or why there is none. */
ExactJacobian make_jacobian(const SessionParts &s, const std::vector<ModelProgram> &entries)
{
    ExactJacobian j;
    if (!s.numerics->compile) {
        j.unavailable = "the model is interpreted (option compile is false)";
        return j;
    }
    auto derived = expr::differentiate_model(s, entries);
    if (!derived) {
        j.unavailable = derived.error().text();
        return j;
    }
    const int node = s.model().node;
    std::vector<expr::Term> states, time;
    /* each constant's terms and their rows, by the constant's index */
    std::map<int, std::pair<std::vector<expr::Term>, std::vector<int>>> by_constant;
    for (const auto &e : derived->entries) {
        if (e.column > 0 && e.column <= node) {
            states.push_back(e.value);
            j.states.rows.push_back(e.row);
            j.states.columns.push_back(e.column - 1);
        } else if (e.column == 0) {
            time.push_back(e.value);
            j.time.rows.push_back(e.row);
        } else {
            auto &[terms, rows] = by_constant[expr::column_constant(e.column)];
            terms.push_back(e.value);
            rows.push_back(e.row);
        }
    }
    Generator g{s};
    const expr::Symbolic &graph = derived->graph;
    const auto program = [&](JacobianProgram &p, std::vector<expr::Term> terms, std::string_view name) {
        p.terms = std::move(terms);
        p.nodes = expr::needed_nodes(graph, p.terms);
        g.derivatives(graph, p.terms, p.rows, name);
    };
    program(j.states, std::move(states), "j_states");
    program(j.time, std::move(time), "j_time");
    for (auto &[k, column] : by_constant) {
        j.by_constant.emplace_back(k, JacobianProgram{nullptr, std::move(column.second), {}});
        program(j.by_constant.back().second, std::move(column.first), xpp::format("j_c{}", k));
    }
    const IrSizes sizes(g.ir);
    j.states.native_nodes = sizes.of("j_states").evaluated;
    j.time.native_nodes = sizes.of("j_time").evaluated;
    for (auto &[k, column] : j.by_constant) column.native_nodes = sizes.of(xpp::format("j_c{}", k)).evaluated;
    const std::uint64_t code_nodes = sizes.emitted();
    const Place place = entries.empty() ? Place{} : entries.front().place;
    /* TinyCC's code first (W821): the Jacobian runs on it from its first step; MIR's build follows */
    bool tier_one = false;
    if (auto tiny = native::Code::compile(g.ir, native::Compiler::TinyCC, place)) {
        if (auto placed = place_jacobian(j, *tiny); placed) tier_one = true;
        else log(XPP_LOG_WARN, "{}; MIR compiles instead\n", placed.error().text());
    } else log(XPP_LOG_WARN, "{}; MIR compiles instead\n", tiny.error().text());
    j.build = pending_build(s, std::move(g.ir), place, tier_one, code_nodes, g.repeat_nodes);
    j.graph = std::make_shared<expr::Symbolic>(std::move(derived->graph));
    log(XPP_LOG_INFO, "Made the exact Jacobian: {} values, {} derivatives ({} nodes to compile)\n", j.graph->size(), derived->entries.size(), j.build->ir.nodes.size());
    return j;
}

}

std::vector<std::vector<int>> equation_reads(const Model &m)
{
    const int n=m.node,programs=n+m.fix_var;
    /* the states each fixed variable and equation reads; a fixed variable
       reads only those before it (Model::rhs_impurity), so one pass in order */
    std::vector<std::vector<int>> reads(static_cast<size_t>(programs));
    for (int p=n;p<programs+n;p++) {
        const int q=p<programs?p:p-programs; /* the fixed variables, then the equations */
        std::vector<int> &mine=reads[static_cast<size_t>(q)]; /* sorted below: work in the size of what is read, never of n */
        for (const int v:m.programs[static_cast<size_t>(q)].native_reads) {
            if (v>=1 && v<=n) mine.push_back(v-1);
            else if (v>n && v<=programs) mine.insert(mine.end(),reads[static_cast<size_t>(v-1)].begin(),reads[static_cast<size_t>(v-1)].end());
        }
        if (q<n) mine.push_back(q); /* a map's f(x)-x reads its own state */
        std::ranges::sort(mine);
        mine.erase(std::unique(mine.begin(),mine.end()),mine.end());
    }
    reads.resize(static_cast<size_t>(n));
    return reads;
}

std::string run_joined_by(const SessionParts &s)
{
    if (s.numerics->subsystem_split != SubsystemSplit::run) return "";
    if (s.numerics->poincare_stop) return "a Poincare stop";
    if (s.numerics->poincare_map != PoincareMap::none) return "a Poincare section";
    return "";
}

bool has_subsystems(const SessionParts &s)
{
    return !s.model().subsystems.empty() && run_joined_by(s).empty();
}

std::string describe_subsystems(const SessionParts &s)
{
    const Model &m=s.model();
    if (const std::string run=run_joined_by(s); !run.empty() && !m.subsystems.empty())
        return xpp::format("1 subsystem, joined by {} (subsystem_split: run)", run);
    if (m.subsystems.empty())
        return m.subsystem_joined_by.empty() ? "1 subsystem: the equations read each other's states"
                                             : xpp::format("1 subsystem, joined by {}", m.subsystem_joined_by);
    /* a model of hundreds shows its first ones */
    constexpr size_t SHOWN = 8;
    std::string text = xpp::format("{} independent subsystems: ", m.subsystems.size());
    for (size_t c = 0; c < std::min(SHOWN, m.subsystems.size()); ++c) {
        text += c ? ", {" : "{";
        for (size_t i = 0; i < m.subsystems[c].size(); ++i)
            text += xpp::format("{}{}", i ? ", " : "", m.uvar_names[static_cast<size_t>(m.subsystems[c][i])]);
        text += '}';
    }
    return m.subsystems.size() > SHOWN ? text + ", ..." : text;
}

namespace {
/* the model's programs, each with its stack sized, for compile_programs */
std::vector<ModelProgram> sized_programs(const SessionParts &s)
{
    Model &m=s.model();
    auto entries=model_programs(m);
    for(const auto &entry:entries)
        size_expr_stack(s.parser,entry.code->rpn,entry.user_index<0?-1:m.compiler.narg_fun[entry.user_index]);
    return entries;
}
}

Result<std::vector<Error>> translate_model(const SessionParts &s)
{
    Model &m=s.model();
    m.compile_pending=false;
    const auto entries=sized_programs(s);
    auto compiled=compile_programs(s,entries);
    m.rhs_impurity=rhs_impurity(s);
    m.subsystem_joined_by=subsystem_joined_by(s);
    m.subsystems.clear();
    if (m.subsystem_joined_by.empty()) {
        auto parts=connected_components(equation_reads(m));
        if (parts.size()>1) m.subsystems=std::move(parts);
    }
    log(XPP_LOG_INFO,"{}\n",describe_subsystems(s));
    Place requested=std::move(m.jacobian.requested);
    m.jacobian=ExactJacobian{};
    m.jacobian.requested=std::move(requested);
    /* the exact Jacobian a model asks for is made now (its derivatives: the run evaluates them from the first
       step); its compile and the programs' start on their threads when due (build_native) */
    if (s.numerics->jacobian==JACOBIAN_EXACT) ensure_exact_jacobian(s);
    return compiled;
}

void ensure_compiled(const SessionParts &s)
{
    if (!s.model().compile_pending) return;
    const auto compiled=translate_model(s);
    if (!compiled) log(XPP_LOG_WARN,"{}\n",compiled.error().text());
    else for (const Error &warning:*compiled) log(XPP_LOG_WARN,"{}\n",warning.text());
}

bool ensure_exact_jacobian(const SessionParts &s)
{
    ensure_compiled(s); /* its derivatives need the programs compiled; compiling makes a new Jacobian, so before the reference */
    ExactJacobian &j=s.model().jacobian;
    if (!j.tried) {
        Place requested=std::move(j.requested);
        j=make_jacobian(s,model_programs(s.model()));
        j.requested=std::move(requested);
        j.tried=true;
    }
    return j.available();
}

Result<std::vector<Error>> compile_loaded(const SessionParts &s)
{
    auto compiled=translate_model(s);
    if (!compiled) compiled=std::vector<Error>{compiled.error()};
    /* the exact Jacobian a model asks for is known to exist only now
       (W260): none is an error at the @ line, not a quiet difference */
    const Model &m=s.model();
    if (s.numerics->jacobian==JACOBIAN_EXACT && !m.jacobian.available())
        return std::unexpected(Error{"options",option_error_text("integration.jacobian.kind","exact","the model has no exact Jacobian: "+m.jacobian.unavailable),
                                     m.jacobian.requested});
    return compiled;
}
}
