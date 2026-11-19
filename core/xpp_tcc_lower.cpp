/* The TinyCC lowering (W821): an IrModule (expr_ir.h) written as C text, which xpp_tcc.cpp compiles in memory. Every node and
   statement is written as MIR's lowering (xpp_mir.cpp) evaluates it, so the two give the same bits: a computed value is taken
   where its instruction stands, a stored one (a memory operand, a number, a loop counter) is read where it is used, a call's
   arguments are taken in order before the call, a select's arms run only where they are chosen, and a guarded division takes
   its divisor as MIR does. The text names only what the lowering generated (m<index> for a module function, x<index> for an
   external one, r, t, i, lo, hi, labels), reads every number from the table N, and holds nothing of the model. */
#include "xpp_tcc.h"
#include "ir_shape.h"
#include "xpp_text.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xpp::tcc {
namespace {

/* A value of the expression being written: C text that reads it where it is used. A computed value (an arithmetic operation)
   is what MIR computes where it stands: when a later statement of the same expression is written, it is taken into a local
   first, so its reads stay where MIR's are. A stored value (a memory operand, a local, a number) is read where it is used. */
struct Lowered {
    std::string text;
    bool computed = false;
};

/* the C type of a hidden parameter */
const char *c_type(ir::Hidden h)
{
    switch (h) {
    case ir::Hidden::Parser: return "void *";
    case ir::Hidden::Rpn: return "int *";
    default: return "double *";
    }
}

/* the parameters of a list, comma-separated; "void" for none */
std::string parameter_list(const std::vector<std::string> &parameters)
{
    if (parameters.empty()) return "void";
    std::string text;
    for (const std::string &p : parameters) text += (text.empty() ? "" : ", ") + p;
    return text;
}

class Lowering {
public:
    Lowering(const IrModule &module, Place place) : ir_(module), place_(std::move(place)) {}
    Result<Source> run();

private:
    const IrModule &ir_;
    Place place_;
    Source source_;
    std::string failure_; /* why the module cannot be written: the first thing found, and nothing more is written */
    std::unordered_map<std::string, size_t> function_named_; /* the IR index of each module function, by its name */
    std::vector<std::string> callee_name_;                   /* by callee: the name its text calls it by, once declared */
    std::vector<const IrFunction *> callee_module_;          /* by callee: its module function, null for an external one */
    std::string prototypes_, definitions_;
    /* the function being written */
    const IrFunction *f_ = nullptr;
    std::string body_;
    int locals_ = 0, max_locals_ = 0; /* the locals of the statement being written, and of the function */
    int labels_ = 0;
    std::vector<int> loop_labels_; /* by loop: its label number */
    std::vector<int> branches_;    /* the open conditionals' label numbers */
    bool repeating_ = false;       /* inside a Repeat: its counter is rp, its count, and its label */
    int repeat_count_ = 0, repeat_label_ = 0;

    static bool returns(const IrFunction &f) { return f.kind == IrFunction::Kind::User || f.kind == IrFunction::Kind::Equation; }
    std::string signature(size_t k) const;
    void function_text(size_t k);
    bool declare(size_t callee);
    void statement(const IrStatement &s);
    void assign(const std::string &target, int node);
    Lowered operand(int node, int depth);
    Lowered binary(const char *op, const IrNode &n, int depth);
    Lowered guarded(const IrNode &n, int depth);
    Lowered select(const IrNode &n, int depth);
    Lowered call(const IrNode &n, int depth);
    std::string element(const char *base, int index, int stride);
    void repeat_begin(const IrStatement &s);
    void repeat_end();
    Lowered taken(Lowered value, size_t mark);
    std::string next_local();
    std::string hidden_value(ir::Hidden h) const;
    int number_index(double x);
};

std::string Lowering::signature(size_t k) const
{
    const IrFunction &f = ir_.functions[k];
    std::vector<std::string> parameters;
    for (ir::Hidden h : ir::own_hidden(f)) parameters.push_back(std::string(c_type(h)) + ir::hidden_name(h));
    for (int a = 0; a < f.doubles; ++a) parameters.push_back("double a" + std::to_string(a));
    return xpp::concat(returns(f) ? "double m" : "void m", std::to_string(k), "(", parameter_list(parameters), ")");
}

Result<Source> Lowering::run()
{
    /* the table's number 0 is the zero divisor, GuardedDivide's */
    source_.numbers.push_back(ir_.zero_divisor);
    for (size_t k = 0; k < ir_.functions.size(); ++k) {
        if (!function_named_.emplace(ir_.functions[k].name, k).second) return fail("TinyCC", "a function named twice: " + ir_.functions[k].name, place_);
    }
    callee_name_.assign(ir_.callees.size(), "");
    callee_module_.assign(ir_.callees.size(), nullptr);
    for (size_t k = 0; k < ir_.functions.size() && failure_.empty(); ++k) function_text(k);
    if (!failure_.empty()) return fail("TinyCC", failure_, place_);
    source_.text = prototypes_ + xpp::concat("double N[", std::to_string(source_.numbers.size()), "];\n") + definitions_;
    for (size_t k = 0; k < ir_.functions.size(); ++k) source_.functions.emplace_back(ir_.functions[k].name, "m" + std::to_string(k));
    return std::move(source_);
}

void Lowering::function_text(size_t k)
{
    f_ = &ir_.functions[k];
    body_.clear();
    locals_ = max_locals_ = labels_ = 0;
    loop_labels_.assign(static_cast<size_t>(f_->loops), 0);
    branches_.clear();
    repeating_ = false;
    for (const IrStatement &s : f_->statements) {
        locals_ = 0;
        statement(s);
        if (!failure_.empty()) return;
    }
    body_ += "finish: ;\n";
    if (returns(*f_)) {
        locals_ = 0;
        const Lowered result = operand(f_->result, 0);
        body_ += "return " + result.text + ";\n";
    }
    /* the locals are declared here, all at the top: a jump (a label, a loop's or a conditional's) never enters a declaration's scope */
    std::string text = signature(k) + "\n{\n";
    if (max_locals_ > 0) {
        std::vector<std::string> names;
        for (int r = 0; r < max_locals_; ++r) names.push_back("r" + std::to_string(r));
        text += "double " + parameter_list(names) + ";\n";
    }
    for (int l = 0; l < f_->loops; ++l) text += xpp::concat("long long i", std::to_string(l), ", lo", std::to_string(l), ", hi", std::to_string(l), ";\n");
    text += "long long rp;\n";
    if (f_->temporaries > 0) {
        std::vector<std::string> names;
        for (int t = 0; t < f_->temporaries; ++t) names.push_back("t" + std::to_string(t));
        text += "double " + parameter_list(names) + ";\n";
    }
    definitions_ += text + body_ + "}\n";
}

/* the callee's prototype, made when first called: a module function's is its signature, an external one's the parameters
   its call shape gives (the parser, its doubles, the index); its address is the symbol TinyCC resolves the name to */
bool Lowering::declare(size_t callee)
{
    if (!callee_name_[callee].empty()) return true;
    const IrCallee &target = ir_.callees[callee];
    if (target.address == nullptr) {
        const auto found = function_named_.find(target.name);
        if (found == function_named_.end()) {
            failure_ = "a call of an undefined function " + target.name;
            return false;
        }
        callee_module_[callee] = &ir_.functions[found->second];
        callee_name_[callee] = "m" + std::to_string(found->second);
        prototypes_ += signature(found->second) + ";\n";
        return true;
    }
    callee_name_[callee] = "x" + std::to_string(callee);
    source_.symbols.push_back({callee_name_[callee], target.address});
    const ir::CallShape shape = ir::call_shape(target, nullptr);
    std::vector<std::string> parameters;
    for (ir::Hidden h : shape.before) parameters.push_back(c_type(h));
    for (int a = 0; a < shape.doubles; ++a) parameters.push_back("double");
    if (shape.index) parameters.push_back("int");
    prototypes_ += "double " + callee_name_[callee] + "(" + parameter_list(parameters) + ");\n";
    return true;
}

void Lowering::statement(const IrStatement &s)
{
    switch (s.kind) {
    case IrStatement::Kind::Temporary: assign("t" + std::to_string(s.index), s.value); break;
    case IrStatement::Kind::Variable: assign("v[" + std::to_string(s.index) + "]", s.value); break;
    case IrStatement::Kind::Constant: assign("c[" + std::to_string(s.index) + "]", s.value); break;
    case IrStatement::Kind::Output: assign(element("o", s.index, s.stride), s.value); break;
    case IrStatement::Kind::RepeatBegin: repeat_begin(s); break;
    case IrStatement::Kind::RepeatEnd: repeat_end(); break;
    case IrStatement::Kind::ReturnWithoutOutput: body_ += "if (o == 0) goto finish;\n"; break;
    case IrStatement::Kind::LoopBegin: {
        /* the bounds are ints, as MIR's truncation makes them (a double to 64 bits, then the low 32 sign-extended); the loop
           runs from low to high inclusive, and not at all when low > high */
        const int label = labels_++;
        loop_labels_[static_cast<size_t>(s.index)] = label;
        const Lowered low = operand(s.value, 0);
        body_ += xpp::concat("lo", std::to_string(s.index), " = (int)(long long)(", low.text, ");\n");
        const Lowered high = operand(s.other, 0);
        body_ += xpp::concat("hi", std::to_string(s.index), " = (int)(long long)(", high.text, ");\n");
        body_ += "t" + std::to_string(s.slot) + " = 0.0;\n";
        const std::string i = "i" + std::to_string(s.index), lo = "lo" + std::to_string(s.index), hi = "hi" + std::to_string(s.index);
        const std::string l = std::to_string(label);
        body_ += xpp::concat("if (", lo, " > ", hi, ") goto done", l, ";\n", i, " = ", lo, ";\ntop", l, ": ;\n");
        break;
    }
    case IrStatement::Kind::LoopEnd: {
        const int label = loop_labels_[static_cast<size_t>(s.index)];
        const Lowered value = operand(s.value, 0);
        const std::string total = "t" + std::to_string(s.slot);
        body_ += xpp::concat(total, " = ", total, " + (", value.text, ");\n");
        const std::string i = "i" + std::to_string(s.index), hi = "hi" + std::to_string(s.index), l = std::to_string(label);
        body_ += xpp::concat("if (", i, " == ", hi, ") goto done", l, ";\n", i, " = ", i, " + 1;\ngoto top", l, ";\ndone", l, ": ;\n");
        break;
    }
    case IrStatement::Kind::IfBegin: {
        /* the then arm runs when the condition is 0; a NaN is not 0, so it takes the else arm, as MIR's branch does */
        const int label = labels_++;
        branches_.push_back(label);
        const Lowered cond = operand(s.value, 0);
        body_ += xpp::concat("if ((", cond.text, ") != 0.0) goto else", std::to_string(label), ";\n");
        break;
    }
    case IrStatement::Kind::IfElse:
    case IrStatement::Kind::IfEnd: {
        if (branches_.empty()) {
            failure_ = "a conditional's else or end with no begin";
            return;
        }
        const std::string l = std::to_string(branches_.back());
        if (s.kind == IrStatement::Kind::IfElse) {
            body_ += xpp::concat("goto end", l, "; else", l, ": ;\n");
        } else {
            body_ += xpp::concat("end", l, ": ;\n");
            branches_.pop_back();
        }
        break;
    }
    }
}

void Lowering::assign(const std::string &target, int node)
{
    const Lowered value = operand(node, 0);
    body_ += target + " = " + value.text + ";\n";
}

std::string Lowering::next_local()
{
    max_locals_ = std::max(max_locals_, ++locals_);
    return "r" + std::to_string(locals_ - 1);
}

/* the value of a computed one taken into a local, when statements were written after it: `mark` is where they start */
Lowered Lowering::taken(Lowered value, size_t mark)
{
    if (!value.computed || body_.size() == mark) return value;
    const std::string local = next_local();
    body_.insert(mark, local + " = " + value.text + ";\n");
    return {local, false};
}

int Lowering::number_index(double x)
{
    source_.numbers.push_back(x);
    return static_cast<int>(source_.numbers.size()) - 1;
}

/* element `index` of the array `base`: in a Repeat, a stride of 1 steps it with the counter, as MIR's lowering does (a stride of
   any other value, or outside a Repeat, is refused) */
std::string Lowering::element(const char *base, int index, int stride)
{
    const std::string text = std::string(base) + "[" + std::to_string(index);
    if (stride == 0) return text + "]";
    if (stride != 1 || !repeating_) {
        failure_ = "an element stride outside a repeat or other than 1";
        return text + "]";
    }
    return text + " + rp]";
}

/* a pass of the Repeat: its counter starts at 0 and steps to its count (at least 1), the body running that many times */
void Lowering::repeat_begin(const IrStatement &s)
{
    if (repeating_ || s.index < 1) {
        failure_ = "a repeat inside a repeat or of no pass";
        return;
    }
    repeating_ = true;
    repeat_count_ = s.index;
    repeat_label_ = labels_++;
    body_ += "rp = 0;\ntop" + std::to_string(repeat_label_) + ": ;\n";
}

void Lowering::repeat_end()
{
    if (!repeating_) {
        failure_ = "the end of a repeat that did not begin";
        return;
    }
    repeating_ = false;
    const std::string l = std::to_string(repeat_label_);
    body_ += "rp = rp + 1;\nif (rp < " + std::to_string(repeat_count_) + ") goto top" + l + ";\n";
}

Lowered Lowering::operand(int node, int depth)
{
    if (!failure_.empty()) return {"0.0", false};
    if (depth > ir::MAX_TREE_DEPTH) {
        failure_ = xpp::format("an expression nested deeper than {}", ir::MAX_TREE_DEPTH);
        return {"0.0", false};
    }
    const IrNode &n = ir_.nodes[static_cast<size_t>(node)];
    switch (n.op) {
    case IrOp::Number: return {"N[" + std::to_string(number_index(n.number)) + "]", false};
    case IrOp::Constant: return {element("c", n.index, n.stride), false};
    case IrOp::Variable: return {element("v", n.index, n.stride), false};
    case IrOp::Temporary: return {"t" + std::to_string(n.index), false};
    case IrOp::Argument: return {"a" + std::to_string(n.index), false};
    case IrOp::LoopCounter: return {"((double)i" + std::to_string(n.index) + ")", false};
    case IrOp::Operand: return {"((double)R[" + std::to_string(n.index) + "])", false};
    case IrOp::Add: return binary("+", n, depth);
    case IrOp::Subtract: return binary("-", n, depth);
    case IrOp::Multiply: return binary("*", n, depth);
    case IrOp::Divide: return binary("/", n, depth);
    case IrOp::GuardedDivide: return guarded(n, depth);
    case IrOp::Select: return select(n, depth);
    case IrOp::Call: return call(n, depth);
    }
    return {"0.0", false};
}

Lowered Lowering::binary(const char *op, const IrNode &n, int depth)
{
    Lowered left = operand(n.args[0], depth + 1);
    const size_t mark = body_.size();
    const Lowered right = operand(n.args[1], depth + 1);
    left = taken(std::move(left), mark);
    return {"(" + left.text + " " + op + " " + right.text + ")", true};
}

/* args[0] / (args[1] == 0 ? zero_divisor : args[1]): both operands first, then the divisor (a NaN is not 0, so it is kept), then
   the division where MIR's stands */
Lowered Lowering::guarded(const IrNode &n, int depth)
{
    Lowered left = operand(n.args[0], depth + 1);
    const size_t mark = body_.size();
    const Lowered right = operand(n.args[1], depth + 1);
    left = taken(std::move(left), mark);
    const std::string divisor = next_local();
    body_ += divisor + " = " + right.text + ";\n";
    body_ += "if (" + divisor + " == 0.0) " + divisor + " = N[0];\n";
    const std::string result = next_local();
    body_ += result + " = " + left.text + " / " + divisor + ";\n";
    return {result, false};
}

/* args[0] == 0 ? args[1] : args[2]: the condition is taken first, and only the arm chosen runs (a NaN condition is not 0) */
Lowered Lowering::select(const IrNode &n, int depth)
{
    const Lowered cond = operand(n.args[0], depth + 1);
    const std::string condition = next_local();
    body_ += condition + " = " + cond.text + ";\n";
    const std::string result = next_local();
    body_ += "if (" + condition + " == 0.0) {\n";
    const Lowered then_value = operand(n.args[1], depth + 1);
    body_ += result + " = " + then_value.text + ";\n} else {\n";
    const Lowered else_value = operand(n.args[2], depth + 1);
    body_ += result + " = " + else_value.text + ";\n}\n";
    return {result, false};
}

/* the call with its arguments taken in order, each before the next (the callee and its doubles as the call shape gives them) */
Lowered Lowering::call(const IrNode &n, int depth)
{
    if (!declare(static_cast<size_t>(n.callee))) return {"0.0", false};
    const size_t callee = static_cast<size_t>(n.callee);
    const ir::CallShape shape = ir::call_shape(ir_.callees[callee], callee_module_[callee]);
    std::vector<std::string> arguments;
    for (ir::Hidden h : shape.before) arguments.push_back(hidden_value(h));
    for (int a : ir_.arguments(n)) {
        const Lowered value = operand(a, depth + 1);
        const std::string local = next_local();
        body_ += local + " = " + value.text + ";\n";
        arguments.push_back(local);
    }
    if (shape.index) arguments.push_back(std::to_string(n.index));
    const std::string result = next_local();
    body_ += result + " = " + callee_name_[callee] + "(" + parameter_list(arguments) + ");\n";
    return {result, false};
}

/* what a call passes for a hidden parameter: the caller's own pointers, and null for the RPN and the output (as MIR's call does) */
std::string Lowering::hidden_value(ir::Hidden h) const
{
    return h == ir::Hidden::Rpn || h == ir::Hidden::Output ? "0" : ir::hidden_name(h);
}

} // namespace

Result<Source> lower(const IrModule &module, const Place &place)
{
    return Lowering(module, place).run();
}

} // namespace xpp::tcc
