/* The model's native code, from the Generator's trees (expr_ir.h) through MIR's own API: each tree becomes the
   same IEEE operations in the same order as the interpreter does them, one double register per value (MIR's
   generator allocates them), nothing fused or reassociated (MIR has no such pattern; the build keeps
   -ffp-contract=off). Everything the code calls is an import named in the module's callee table, loaded into
   MIR's environment by us and found only there. */
#include "xpp_mir.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <csetjmp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <cstdarg>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "xpp_io.h"
#include "xpp_log.h"
#include "mir.h"
#include "mir-gen.h"

namespace xpp::mir {

namespace {
/* A tree deeper than this is refused: the lowering recurses on it, so a hostile deep expression cannot exhaust
   the host's stack (8 MB on every platform, Makefile's MAINSTACK). The parser's own limit is 256 levels
   (odex_parse.cpp max_expression_depth, long operator chains included) and an inlined user function adds at
   most its 64 nodes at a call; a level costs about 0.7 KB of stack, 5 KB under AddressSanitizer, so 1024 fits
   all of them. */
constexpr int MAX_TREE_DEPTH = 1024;
constexpr int DOUBLE_BYTES = 8;
constexpr int INT_BYTES = 4;
/* what one diagnostic is cut to */
constexpr size_t MESSAGE_MAX = 512;

/* The optimization level of MIR's generator for each kind of function (IrFunction::Kind order), chosen by
   measurement (W714, docs/studies/w570-faster-compiler.md): 2 for what runs at every step, the right-hand
   side, the equations and the user functions it calls (level 1 costs 2-11% more instructions than level 2 over
   a full run of mu26, ma23 and kuramot100, level 0 more than TinyCC's code did); 1 for the exact Jacobian, a
   long straight line of its values in registers (W715): on mu26 its level 2 compile costs 56 M cycles against
   level 1's 33 M and gains nothing in the run (1351 M instructions against 1248 M over 400 time units), and
   level 0's compile saves 24 M only to lose them in the run (1250 M), and more over a longer one. */
constexpr std::array<unsigned, 4> LEVEL{2, 2, 2, 1};

/* MIR reports an error through a function that must not return and has no user data: `raised` (ours
   and MIR's both) records the text and jumps back to the compile that called MIR (`run`): the thread's own, since
   a compile runs on the model's build thread (W715) or on the core thread (the exact Jacobian). */
struct Escape {
    std::jmp_buf exit{};
    std::array<char, MESSAGE_MAX> text{};
    const std::unordered_map<std::string, unsigned> *levels = nullptr; /* by function, for `generate` */
};
constinit thread_local Escape escape{};
/* One compile at a time: MIR's generator sorts its static pattern table at each MIR_gen_init
   (mir-gen-x86_64.c patterns_init), so two contexts may not generate at once. */
std::mutex compiling;
/* The compiles running on threads of their own (W715): the program's exit waits for them, since static destruction
   would otherwise destroy what a compile uses (`compiling`) under it (macOS CI 43443622373: SIGABRT at the exit of
   a short run, which ends before its compile). */
std::atomic<int> in_background{0};

void wait_for_background_compiles()
{
    for (int running = in_background.load(); running != 0; running = in_background.load()) in_background.wait(running);
}

/* MIR's error function; ours report through it too, with the kind of MIR's closest to what is wrong */
void MIR_NO_RETURN raised(MIR_error_type_t, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vsnprintf(escape.text.data(), escape.text.size(), format, args);
    va_end(args);
    std::longjmp(escape.exit, 1);
}

/* MIR_link's per-function step: the level of the function's kind, then MIR's own generation */
void generate(MIR_context_t ctx, MIR_item_t item)
{
    if (item != nullptr && item->item_type == MIR_func_item) MIR_gen_set_optimize_level(ctx, escape.levels->at(item->u.func->name));
    MIR_set_gen_interface(ctx, item);
}
} // namespace

/* MIR's heap, kept in a list so that a compile MIR failed inside (its state unknown, so it cannot be
   finished) frees every block it holds without MIR's help. Each block is a header and the bytes MIR sees,
   aligned as malloc's are. MIR_alloc's callbacks must not throw: an allocation that fails is nullptr, which MIR
   reports as its own error. */
struct Arena {
    struct alignas(16) Block {
        Block *previous, *next;
        size_t size;
    };
    static constexpr size_t HEADER = sizeof(Block);
    Block *last = nullptr;

    static Arena &of(void *self) { return *static_cast<Arena *>(self); }
    static void *allocate(size_t size, void *self) noexcept
    {
        Arena &arena = of(self);
        void *memory = ::operator new(HEADER + size, std::nothrow);
        if (memory == nullptr) return nullptr;
        Block *block = static_cast<Block *>(memory);
        *block = {arena.last, nullptr, size};
        if (arena.last) arena.last->next = block;
        arena.last = block;
        return static_cast<char *>(memory) + HEADER;
    }
    static void *allocate_zeroed(size_t count, size_t size, void *self) noexcept
    {
        if (size != 0 && count > (SIZE_MAX - HEADER) / size) return nullptr;
        void *memory = allocate(count * size, self);
        if (memory != nullptr) std::memset(memory, 0, count * size);
        return memory;
    }
    static void release(void *memory, void *self) noexcept
    {
        if (memory == nullptr) return;
        Arena &arena = of(self);
        Block *block = reinterpret_cast<Block *>(static_cast<char *>(memory) - HEADER);
        if (block->previous) block->previous->next = block->next;
        if (block->next) block->next->previous = block->previous;
        else arena.last = block->previous;
        ::operator delete(block);
    }
    static void *reallocate(void *memory, size_t old_size, size_t new_size, void *self) noexcept
    {
        void *grown = allocate(new_size, self);
        if (grown == nullptr || memory == nullptr) return grown;
        std::memcpy(grown, memory, std::min(old_size, new_size));
        release(memory, self);
        return grown;
    }
    void release_all() noexcept
    {
        while (last != nullptr) release(reinterpret_cast<char *>(last) + HEADER, this);
    }
};

struct Program::State {
    Arena arena;
    MIR_alloc alloc{&Arena::allocate, &Arena::allocate_zeroed, &Arena::reallocate, &Arena::release, &arena};
    MIR_context_t context = nullptr;
    bool generator = false;
    std::unordered_map<std::string, void *> functions;
    Place place;

    ~State()
    {
        finish();
        arena.release_all();
    }
    /* a context MIR is not in the middle of building: its own finish frees it, code memory included */
    void finish()
    {
        if (!context) return;
        const MIR_context_t finished = context;
        context = nullptr;
        if (generator) MIR_gen_finish(finished);
        MIR_finish(finished);
    }
    /* the context of a compile that failed inside MIR: its heap is freed here, MIR's state being unknown (the
       code memory it reserved, address space only until generation, is the one thing left) */
    void abandon()
    {
        context = nullptr;
        generator = false;
        arena.release_all();
    }
};

namespace {
/* The state of one compile. It lives outside the frame MIR may jump out of (`run`), so the frames between hold
   only trivial locals: a jump skips no destructor. */
struct Lowering {
    const IrModule &ir;
    Program::State &state;
    MIR_context_t ctx = nullptr;
    bool module_open = false, function_open = false;
    std::unordered_map<std::string, const IrFunction *> function_named;
    std::unordered_map<std::string, unsigned> levels;
    std::vector<MIR_item_t> protos, targets; /* by callee, made at its first call: its prototype and its import or forward */
    /* the function being lowered */
    MIR_item_t item = nullptr;
    MIR_reg_t c = 0, v = 0, session = 0, rpn = 0, output = 0; /* the pointers a kind has */
    std::vector<MIR_reg_t> arguments, temporaries;
    struct Loop {
        MIR_reg_t low = 0, high = 0, counter = 0;
        MIR_insn_t top = nullptr, done = nullptr;
    };
    std::vector<Loop> loops;
    struct Branch {
        MIR_insn_t otherwise = nullptr, done = nullptr;
    };
    std::vector<Branch> branches;
    std::vector<MIR_var_t> parameters; /* the function or prototype being declared, and the names it uses */
    std::vector<std::string> parameter_names;
    std::string name; /* the name being made for MIR, which copies it: a member, since a jump out of MIR skips a local's destructor */
    std::vector<MIR_op_t> operands; /* the call being assembled: a stack, a nested call's arguments above it */
    int registers = 0;
    std::string failure; /* why the module cannot be built, when it cannot: found by us, so MIR is finished normally */

    MIR_reg_t fresh(MIR_type_t type)
    {
        name = xpp::format("r{}", registers++);
        return MIR_new_func_reg(ctx, item->u.func, type, name.c_str());
    }
    MIR_op_t reg(MIR_reg_t r) { return MIR_new_reg_op(ctx, r); }
    MIR_op_t number(double x) { return MIR_new_double_op(ctx, x); }
    void emit(MIR_insn_t insn) { MIR_append_insn(ctx, item, insn); }
    void emit(MIR_insn_code_t code, MIR_op_t a, MIR_op_t b) { emit(MIR_new_insn(ctx, code, a, b)); }
    void emit(MIR_insn_code_t code, MIR_op_t a, MIR_op_t b, MIR_op_t d) { emit(MIR_new_insn(ctx, code, a, b, d)); }
    void jump(MIR_insn_code_t code, MIR_insn_t label, MIR_op_t a, MIR_op_t b) { emit(MIR_new_insn(ctx, code, MIR_new_label_op(ctx, label), a, b)); }
    MIR_op_t slot(MIR_reg_t base, int index) { return MIR_new_mem_op(ctx, MIR_T_D, static_cast<MIR_disp_t>(index) * DOUBLE_BYTES, base, 0, 1); }

    /* a double operand in a register: an immediate or a memory operand is moved into a new one */
    MIR_op_t in_register(MIR_op_t operand)
    {
        if (operand.mode == MIR_OP_REG) return operand;
        const MIR_reg_t r = fresh(MIR_T_D);
        emit(MIR_DMOV, reg(r), operand);
        return reg(r);
    }
    void store(MIR_op_t destination, MIR_op_t value) { emit(MIR_DMOV, destination, value.mode == MIR_OP_MEM ? in_register(value) : value); }

    /* a parameter of a prototype or function, named `name` or p0, p1, ...; the names are fixed by name_parameters
       once all are added (the storage may move while they are) */
    void parameter(MIR_type_t type, const char *fixed = nullptr)
    {
        parameter_names.push_back(fixed ? std::string(fixed) : xpp::format("p{}", parameters.size()));
        parameters.push_back({type, nullptr, 0});
    }
    void name_parameters()
    {
        for (size_t i = 0; i < parameters.size(); ++i) parameters[i].name = parameter_names[i].c_str();
    }

    /* the callee's prototype, and its import or forward declaration, made when first called: the module needs
       no item for a function it never calls */
    bool declare(int index)
    {
        const size_t k = static_cast<size_t>(index);
        if (protos[k]) return true;
        const IrCallee &target = ir.callees[k];
        parameters.clear();
        parameter_names.clear();
        const IrFunction *module_function = nullptr;
        if (target.address == nullptr) {
            const auto found = function_named.find(target.name);
            if (found == function_named.end()) {
                failure = "a call of an undefined function " + target.name;
                return false;
            }
            module_function = found->second;
            parameter(MIR_T_P);
            parameter(MIR_T_P);
            parameter(MIR_T_P);
            if (module_function->kind == IrFunction::Kind::Equation) parameter(MIR_T_P);
            for (int a = 0; a < module_function->doubles; ++a) parameter(MIR_T_D);
        } else {
            if (target.session) parameter(MIR_T_P);
            for (int a = 0; a < target.doubles; ++a) parameter(MIR_T_D);
            if (target.index) parameter(MIR_T_I32);
        }
        name_parameters();
        name = "proto_" + target.name;
        MIR_type_t result = MIR_T_D;
        protos[k] = MIR_new_proto_arr(ctx, name.c_str(), 1, &result, parameters.size(), parameters.data());
        targets[k] = module_function ? MIR_new_forward(ctx, target.name.c_str()) : MIR_new_import(ctx, target.name.c_str());
        return true;
    }

    /* The value of a node as an operand of an instruction, as c2mir's own code has them: a constant or a
       variable is a memory operand and a number an immediate, so MIR's generator folds the load into the
       instruction that uses it; the others are a register. The instructions that compute it are emitted here. */
    MIR_op_t operand(int node, int depth)
    {
        if (!failure.empty()) return number(0.0);
        if (depth > MAX_TREE_DEPTH) {
            failure = xpp::format("an expression nested deeper than {}", MAX_TREE_DEPTH);
            return number(0.0);
        }
        const IrNode &n = ir.nodes[static_cast<size_t>(node)];
        switch (n.op) {
        case IrOp::Number: return number(n.number);
        case IrOp::Constant: return slot(c, n.index);
        case IrOp::Variable: return slot(v, n.index);
        case IrOp::Temporary: return reg(temporaries[static_cast<size_t>(n.index)]);
        case IrOp::Argument: return reg(arguments[static_cast<size_t>(n.index)]);
        case IrOp::LoopCounter: {
            const MIR_reg_t r = fresh(MIR_T_D);
            emit(MIR_I2D, reg(r), reg(loops[static_cast<size_t>(n.index)].counter));
            return reg(r);
        }
        case IrOp::Operand: {
            const MIR_reg_t i = fresh(MIR_T_I64), r = fresh(MIR_T_D);
            emit(MIR_MOV, reg(i), MIR_new_mem_op(ctx, MIR_T_I32, static_cast<MIR_disp_t>(n.index) * INT_BYTES, rpn, 0, 1));
            emit(MIR_I2D, reg(r), reg(i));
            return reg(r);
        }
        case IrOp::Add: return arithmetic(MIR_DADD, n, depth);
        case IrOp::Subtract: return arithmetic(MIR_DSUB, n, depth);
        case IrOp::Multiply: return arithmetic(MIR_DMUL, n, depth);
        case IrOp::Divide: return arithmetic(MIR_DDIV, n, depth);
        case IrOp::GuardedDivide: {
            /* l / (r==0 ? zero_divisor : r): the divisor is chosen in a register of its own; a NaN divisor is not
               0, as in C (dbne) */
            const MIR_op_t left = operand(n.args[0], depth + 1), right = operand(n.args[1], depth + 1);
            const MIR_reg_t divisor = fresh(MIR_T_D), r = fresh(MIR_T_D);
            const MIR_insn_t keep = MIR_new_label(ctx);
            emit(MIR_DMOV, reg(divisor), right);
            jump(MIR_DBNE, keep, reg(divisor), number(0.0));
            emit(MIR_DMOV, reg(divisor), number(ir.zero_divisor));
            emit(keep);
            emit(MIR_DDIV, reg(r), left, reg(divisor));
            return reg(r);
        }
        case IrOp::Select: {
            /* args[0]==0 ? args[1] : args[2]; a NaN condition is not equal to 0, so it takes args[2] */
            const MIR_op_t cond = in_register(operand(n.args[0], depth + 1));
            const MIR_reg_t r = fresh(MIR_T_D);
            const MIR_insn_t otherwise = MIR_new_label(ctx), done = MIR_new_label(ctx);
            jump(MIR_DBNE, otherwise, cond, number(0.0));
            emit(MIR_DMOV, reg(r), operand(n.args[1], depth + 1));
            emit(MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, done)));
            emit(otherwise);
            emit(MIR_DMOV, reg(r), operand(n.args[2], depth + 1));
            emit(done);
            return reg(r);
        }
        case IrOp::Call: return call(n, depth);
        }
        return reg(c);
    }
    MIR_op_t arithmetic(MIR_insn_code_t code, const IrNode &n, int depth)
    {
        const MIR_op_t a = operand(n.args[0], depth + 1), b = operand(n.args[1], depth + 1);
        const MIR_reg_t r = fresh(MIR_T_D);
        emit(code, reg(r), a, b);
        return reg(r);
    }
    MIR_op_t call(const IrNode &n, int depth)
    {
        if (!declare(n.callee)) return number(0.0);
        const IrCallee &target = ir.callees[static_cast<size_t>(n.callee)];
        const size_t k = static_cast<size_t>(n.callee);
        const MIR_reg_t r = fresh(MIR_T_D);
        const size_t base = operands.size();
        operands.push_back(MIR_new_ref_op(ctx, protos[k]));
        operands.push_back(MIR_new_ref_op(ctx, targets[k]));
        operands.push_back(reg(r));
        if (target.address == nullptr) {
            operands.push_back(reg(c));
            operands.push_back(reg(v));
            operands.push_back(reg(session));
            if (function_named.at(target.name)->kind == IrFunction::Kind::Equation) operands.push_back(MIR_new_int_op(ctx, 0));
        } else if (target.session) {
            operands.push_back(reg(session));
        }
        for (int a : ir.arguments(n)) {
            const MIR_op_t value = in_register(operand(a, depth + 1));
            operands.push_back(value);
        }
        if (target.address != nullptr && target.index) operands.push_back(MIR_new_int_op(ctx, n.index));
        emit(MIR_new_insn_arr(ctx, MIR_CALL, operands.size() - base, operands.data() + base));
        operands.resize(base);
        return reg(r);
    }

    void statement(const IrStatement &s, MIR_insn_t finish)
    {
        switch (s.kind) {
        case IrStatement::Kind::Temporary: emit(MIR_DMOV, reg(temporaries[static_cast<size_t>(s.index)]), operand(s.value, 0)); break;
        case IrStatement::Kind::Variable: store(slot(v, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::Constant: store(slot(c, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::Output: store(slot(output, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::ReturnWithoutOutput: jump(MIR_BEQ, finish, reg(output), MIR_new_int_op(ctx, 0)); break;
        case IrStatement::Kind::LoopBegin: {
            /* the bounds are ints, as the interpreter's casts make them; the loop runs from low to high
               inclusive, and not at all when low > high */
            Loop &loop = loops[static_cast<size_t>(s.index)];
            loop.low = fresh(MIR_T_I64);
            loop.high = fresh(MIR_T_I64);
            loop.counter = fresh(MIR_T_I64);
            const MIR_op_t low = in_register(operand(s.value, 0));
            emit(MIR_D2I, reg(loop.low), low);
            emit(MIR_EXT32, reg(loop.low), reg(loop.low));
            const MIR_op_t high = in_register(operand(s.other, 0));
            emit(MIR_D2I, reg(loop.high), high);
            emit(MIR_EXT32, reg(loop.high), reg(loop.high));
            emit(MIR_DMOV, reg(temporaries[static_cast<size_t>(s.slot)]), number(0.0));
            loop.top = MIR_new_label(ctx);
            loop.done = MIR_new_label(ctx);
            jump(MIR_BGT, loop.done, reg(loop.low), reg(loop.high));
            emit(MIR_MOV, reg(loop.counter), reg(loop.low));
            emit(loop.top);
            break;
        }
        case IrStatement::Kind::LoopEnd: {
            const Loop &loop = loops[static_cast<size_t>(s.index)];
            const MIR_op_t total = reg(temporaries[static_cast<size_t>(s.slot)]);
            emit(MIR_DADD, total, total, operand(s.value, 0));
            jump(MIR_BEQ, loop.done, reg(loop.counter), reg(loop.high));
            emit(MIR_ADD, reg(loop.counter), reg(loop.counter), MIR_new_int_op(ctx, 1));
            emit(MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, loop.top)));
            emit(loop.done);
            break;
        }
        case IrStatement::Kind::IfBegin: {
            /* the then arm runs when the condition is 0; a NaN is not 0 */
            const MIR_op_t cond = in_register(operand(s.value, 0));
            branches.push_back({MIR_new_label(ctx), MIR_new_label(ctx)});
            jump(MIR_DBNE, branches.back().otherwise, cond, number(0.0));
            break;
        }
        case IrStatement::Kind::IfElse:
            emit(MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, branches.back().done)));
            emit(branches.back().otherwise);
            break;
        case IrStatement::Kind::IfEnd:
            emit(branches.back().done);
            branches.pop_back();
            break;
        }
    }

    /* one function: its parameters, its registers, its statements, its return */
    void lower(const IrFunction &f)
    {
        parameters.clear();
        parameter_names.clear();
        parameter(MIR_T_P, "c");
        parameter(MIR_T_P, "v");
        parameter(MIR_T_P, "S");
        if (f.kind == IrFunction::Kind::Equation) parameter(MIR_T_P, "R");
        if (f.kind == IrFunction::Kind::Rhs || f.kind == IrFunction::Kind::Jacobian) parameter(MIR_T_P, "o");
        for (int a = 0; a < f.doubles; ++a) {
            name = xpp::format("a{}", a);
            parameter(MIR_T_D, name.c_str());
        }
        name_parameters();
        const bool returns = f.kind == IrFunction::Kind::User || f.kind == IrFunction::Kind::Equation;
        MIR_type_t result = MIR_T_D;
        function_open = true; /* MIR makes the function current before it can refuse its name */
        item = MIR_new_func_arr(ctx, f.name.c_str(), returns ? 1 : 0, &result, parameters.size(), parameters.data());
        const MIR_func_t machine = item->u.func;
        c = MIR_reg(ctx, "c", machine);
        v = MIR_reg(ctx, "v", machine);
        session = MIR_reg(ctx, "S", machine);
        if (f.kind == IrFunction::Kind::Equation) rpn = MIR_reg(ctx, "R", machine);
        if (f.kind == IrFunction::Kind::Rhs || f.kind == IrFunction::Kind::Jacobian) output = MIR_reg(ctx, "o", machine);
        arguments.clear();
        for (int a = 0; a < f.doubles; ++a) {
            name = xpp::format("a{}", a);
            arguments.push_back(MIR_reg(ctx, name.c_str(), machine));
        }
        registers = 0;
        temporaries.clear();
        for (int t = 0; t < f.temporaries; ++t) {
            name = xpp::format("t{}", t);
            temporaries.push_back(MIR_new_func_reg(ctx, machine, MIR_T_D, name.c_str()));
        }
        loops.assign(static_cast<size_t>(f.loops), Loop{});
        branches.clear();
        const MIR_insn_t finish = MIR_new_label(ctx);
        for (const IrStatement &s : f.statements) statement(s, finish);
        emit(finish);
        if (returns) emit(MIR_new_ret_insn(ctx, 1, in_register(operand(f.result, 0))));
        else emit(MIR_new_ret_insn(ctx, 0));
        MIR_finish_func(ctx);
        function_open = false;
    }

    void build()
    {
        ctx = MIR_init2(&state.alloc, nullptr);
        state.context = ctx;
        MIR_set_error_func(ctx, raised);
        for (const IrFunction &f : ir.functions) {
            function_named.emplace(f.name, &f);
            levels.emplace(f.name, LEVEL[static_cast<size_t>(f.kind)]);
        }
        escape.levels = &levels;
        protos.assign(ir.callees.size(), nullptr);
        targets.assign(ir.callees.size(), nullptr);
        for (const IrCallee &callee : ir.callees)
            if (callee.address != nullptr) MIR_load_external(ctx, callee.name.c_str(), const_cast<void *>(callee.address));
        module_open = true;
        const MIR_module_t built = MIR_new_module(ctx, "model");
        for (const IrFunction &f : ir.functions) lower(f);
        MIR_finish_module(ctx);
        module_open = false;
        if (!failure.empty()) return;
        MIR_load_module(ctx, built);
        MIR_gen_init(ctx);
        state.generator = true;
        MIR_link(ctx, generate, nullptr);
        for (MIR_item_t it = DLIST_HEAD(MIR_item_t, built->items); it; it = DLIST_NEXT(MIR_item_t, it))
            if (it->item_type == MIR_func_item) state.functions.emplace(it->u.func->name, it->addr);
    }

    /* after a jump out of MIR: with no function or module open MIR is finished as usual (it was linking or
       generating); else its state is unknown and its heap is freed without it */
    void recover()
    {
        if (function_open || module_open) state.abandon();
        else state.finish();
    }
};

/* The calls into MIR, in a frame of their own with nothing but the reference: a jump out of MIR skips no
   destructor. Returns the text of what failed, or nullptr. */
const char *run(Lowering &lowering)
{
    if (setjmp(escape.exit)) return escape.text.data();
    lowering.build();
    return nullptr;
}
} // namespace

Program::Program(Place place) : state_(std::make_unique<State>())
{
    state_->place = std::move(place);
}

Program::~Program() = default;

void background_started()
{
    /* registered at the first one, after every static object was made, so it runs before any is destroyed */
    static const bool waits_at_exit = std::atexit(wait_for_background_compiles) == 0;
    if (!waits_at_exit) log(XPP_LOG_WARN, "the exit cannot wait for a compile on its thread\n");
    in_background.fetch_add(1);
}

void background_ended()
{
    in_background.fetch_sub(1);
    in_background.notify_all();
}

Result<std::unique_ptr<Program>> Program::compile(const IrModule &module, Place place)
{
    auto program = std::unique_ptr<Program>(new Program(std::move(place)));
    const std::lock_guard lock(compiling);
    Lowering lowering{module, *program->state_};
    if (const char *failed = run(lowering)) {
        const std::string message = failed;
        lowering.recover();
        return fail("MIR", message, program->state_->place);
    }
    if (!lowering.failure.empty()) return fail("MIR", lowering.failure, program->state_->place);
    return program;
}

Result<void *> Program::function(const std::string &name) const
{
    const auto found = state_->functions.find(name);
    if (found == state_->functions.end()) return fail("MIR", "function not found: " + name, state_->place);
    return found->second;
}

} // namespace xpp::mir
