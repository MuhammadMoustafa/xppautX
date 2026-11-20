/* The model's native code, from the Generator's trees (expr_ir.h) through MIR's own API: each tree becomes the
   same IEEE operations in the same order as the interpreter does them, one double register per value (MIR's
   generator allocates them), nothing fused or reassociated (MIR has no such pattern; the build keeps
   -ffp-contract=off). Everything the code calls is an import named in the module's callee table, loaded into
   MIR's environment by us and found only there. */
#include "xpp_mir.h"
#include <array>
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
#include <mir-alloc.h>
#include <mir-dlist.h>
#include <span>
#include "xpp_text.h"
#include "mir.h"
#include "mir-gen.h"
#include "expr_ir.h"
#include "ir_shape.h"
#include "native_functions.h"

namespace xpp::mir {

namespace {
constexpr int DOUBLE_BYTES = 8;
constexpr int INT_BYTES = 4;
/* what one diagnostic is cut to */
constexpr size_t MESSAGE_MAX = 512;

/* The optimization level of MIR's generator, for every function (W817, measured with one pinned CPU on the comparison's
   models): 1, register allocation without GVN, copy propagation, dead store elimination and LICM. Level 2 compiled
   lecdiff1600's 45k-node right-hand side in about 1.6 s of CPU (perf), most of it in dse's memory liveness
   (make_live_from_mem and update_call_mem_live); level 1 takes about 0.4 s. A run of the models' code is
   0-6% longer at level 1 (mu26's 400 time units: 2742 M instructions with its compile, 2593 M at level 2, TinyCC's
   2756 M), and a run that compiles at all is faster at level 1 for the compile it no longer pays (lecdiff1600 43 G to
   10 G instructions, screen 1254 M to 731 M). */
constexpr unsigned OPTIMIZE_LEVEL = 1;
/* the statements of a chunk: about 50 equations of a right-hand side (an equation is a statement or two) */
constexpr size_t CHUNK_STATEMENTS = 100;

/* MIR reports an error through a function that must not return and has no user data: `raised` (ours
   and MIR's both) records the text and jumps back to the compile that called MIR (`run`): the thread's own, since
   a compile runs on the model's build thread (W715) or on the core thread (the exact Jacobian). */
struct Escape {
    std::jmp_buf exit{};
    std::array<char, MESSAGE_MAX> text{};
};
constinit thread_local Escape escape{};
/* One compile at a time: MIR's generator sorts its static pattern table at each MIR_gen_init
   (mir-gen-x86_64.c patterns_init), so two contexts may not generate at once. Never destroyed: a short run's
   exit comes before the compile on its thread (W715) ends, and static destruction would destroy the mutex under
   it (macOS CI 43443622373: SIGABRT at the exit of a short run); the thread ends with the process instead, like
   any thread running at exit, having touched nothing else that is destroyed (W717: an exit that waited for the
   compile made every short run pay the whole compile, mu26's one step 21 ms for 7). */
std::mutex &compiling = *new std::mutex;

/* MIR's error function; ours report through it too, with the kind of MIR's closest to what is wrong */
void MIR_NO_RETURN raised(MIR_error_type_t, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vsnprintf(escape.text.data(), escape.text.size(), format, args);
    va_end(args);
    std::longjmp(escape.exit, 1);
}

/* MIR_link's per-function step: the optimization level, then MIR's own generation */
void generate(MIR_context_t ctx, MIR_item_t item)
{
    if (item != nullptr && item->item_type == MIR_func_item) MIR_gen_set_optimize_level(ctx, OPTIMIZE_LEVEL);
    MIR_set_gen_interface(ctx, item);
}
} // namespace

/* MIR's heap, kept in lists so that a compile MIR failed inside (its state unknown, so it cannot be finished)
   frees every block it holds without MIR's help. MIR allocates and frees millions of small blocks (a compile is
   mostly that: malloc and free were a fifth of its instructions, W817), so a block of up to SMALL_MAX bytes comes from
   a chunk, with a free list for each size, never to malloc; a larger one is its own block in a list. Each block
   has a header ending in its size, so that a free finds the way it came; both are aligned as malloc's are.
   MIR_alloc's callbacks must not throw: an allocation that fails is nullptr, which MIR reports as its own error. */
struct Arena {
    struct alignas(16) Small {
        size_t unused;
        size_t size;
    };
    struct alignas(16) Large {
        Large *previous, *next;
        size_t unused;
        size_t size;
    };
    struct alignas(16) Chunk {
        Chunk *next;
    };
    static constexpr size_t GRAIN = 16;                      /* the size classes' step, and MIR's alignment */
    static constexpr size_t SMALL_MAX = 1024;                /* the largest block of a class: MIR's tables and instructions are smaller */
    static constexpr size_t CHUNK_BYTES = 256 * 1024;        /* what small blocks are cut from: few mallocs, and a fit for the cache */
    static constexpr size_t CLASSES = SMALL_MAX / GRAIN;
    static_assert(sizeof(Small) == GRAIN && sizeof(Chunk) == GRAIN);
    struct Free {
        Free *next;
    };
    std::array<Free *, CLASSES> free_small{};
    Chunk *chunks = nullptr;
    char *cut = nullptr, *cut_end = nullptr;
    Large *last = nullptr;

    static Arena &of(void *self) { return *static_cast<Arena *>(self); }
    static size_t round(size_t size) { return std::max(GRAIN, (size + GRAIN - 1) / GRAIN * GRAIN); }
    static size_t &size_of(void *memory) { return *reinterpret_cast<size_t *>(static_cast<char *>(memory) - sizeof(size_t)); }
    static void *allocate(size_t size, void *self) noexcept
    {
        Arena &arena = of(self);
        if (size > SMALL_MAX) return arena.allocate_large(size);
        const size_t bytes = round(size), cls = bytes / GRAIN - 1;
        if (Free *reused = arena.free_small[cls]) {
            arena.free_small[cls] = reused->next;
            return reused;
        }
        if (static_cast<size_t>(arena.cut_end - arena.cut) < sizeof(Small) + bytes && !arena.new_chunk()) return nullptr;
        Small *block = reinterpret_cast<Small *>(arena.cut);
        block->size = bytes;
        arena.cut += sizeof(Small) + bytes;
        return block + 1;
    }
    bool new_chunk() noexcept
    {
        void *memory = ::operator new(CHUNK_BYTES, std::nothrow);
        if (memory == nullptr) return false;
        Chunk *chunk = static_cast<Chunk *>(memory);
        chunk->next = chunks;
        chunks = chunk;
        cut = reinterpret_cast<char *>(chunk + 1);
        cut_end = static_cast<char *>(memory) + CHUNK_BYTES;
        return true;
    }
    void *allocate_large(size_t size) noexcept
    {
        if (size > SIZE_MAX - sizeof(Large)) return nullptr;
        void *memory = ::operator new(sizeof(Large) + size, std::nothrow);
        if (memory == nullptr) return nullptr;
        Large *block = static_cast<Large *>(memory);
        *block = {last, nullptr, 0, size};
        if (last) last->next = block;
        last = block;
        return block + 1;
    }
    static void *allocate_zeroed(size_t count, size_t size, void *self) noexcept
    {
        if (size != 0 && count > SIZE_MAX / size) return nullptr;
        void *memory = allocate(count * size, self);
        if (memory != nullptr) std::memset(memory, 0, count * size);
        return memory;
    }
    static void release(void *memory, void *self) noexcept
    {
        if (memory == nullptr) return;
        Arena &arena = of(self);
        const size_t size = size_of(memory);
        if (size <= SMALL_MAX) {
            Free *freed = static_cast<Free *>(memory);
            freed->next = arena.free_small[size / GRAIN - 1];
            arena.free_small[size / GRAIN - 1] = freed;
            return;
        }
        Large *block = static_cast<Large *>(memory) - 1;
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
        while (last != nullptr) release(last + 1, this);
        while (chunks != nullptr) {
            Chunk *next = chunks->next;
            ::operator delete(chunks);
            chunks = next;
        }
        free_small.fill(nullptr);
        cut = cut_end = nullptr;
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
    std::vector<MIR_item_t> protos, targets; /* by callee, made at its first call: its prototype and its import or forward */
    /* the function being lowered */
    MIR_item_t item = nullptr;
    MIR_reg_t c = 0, v = 0, parser = 0, rpn = 0, output = 0; /* the pointers a kind has */
    std::vector<MIR_reg_t> arguments, temporaries; /* the temporaries a function uses are made at their first use: 0 is none yet */
    MIR_item_t chunk_proto = nullptr;              /* the prototype of a chunk's call, once a function needs one */
    struct Loop {
        MIR_reg_t low = 0, high = 0, counter = 0;
        MIR_insn_t top = nullptr, done = nullptr;
    };
    std::vector<Loop> loops;
    /* the Repeat the function is in: its counter (an int register), the instruction its pass starts at, and its count; none outside one */
    MIR_reg_t repeat_counter = 0;
    MIR_insn_t repeat_top = nullptr;
    int repeat_count = 0;
    bool repeating = false;
    struct Branch {
        MIR_insn_t otherwise = nullptr, done = nullptr;
    };
    std::vector<Branch> branches;
    std::vector<MIR_var_t> parameters; /* the function or prototype being declared, and the names it uses */
    std::vector<std::string> parameter_names;
    std::string name; /* the name being made for MIR, which copies it: a member, since a jump out of MIR skips a local's destructor */
    std::vector<MIR_op_t> operands; /* the call being assembled: a stack, a nested call's arguments above it */
    int registers = 0;
    std::vector<MIR_reg_t> spare;   /* the double registers made by fresh whose value was used */
    std::vector<char> scratch;      /* by register: made by fresh (the named temporaries and arguments are not) */
    std::string failure; /* why the module cannot be built, when it cannot: found by us, so MIR is finished normally */

    MIR_reg_t fresh(MIR_type_t type)
    {
        if (type == MIR_T_D && !spare.empty()) {
            const MIR_reg_t r = spare.back();
            spare.pop_back();
            return r;
        }
        name = xpp::format("r{}", registers++);
        const MIR_reg_t r = MIR_new_func_reg(ctx, item->u.func, type, name.c_str());
        if (type == MIR_T_D) {
            if (scratch.size() <= r) scratch.resize(r + 1, 0);
            scratch[r] = 1;
        }
        return r;
    }
    /* a value's register once its one use is emitted: a later value takes it, so a function holds the registers
       its deepest expression needs, not one for each node (MIR's cost grows with the registers it tracks) */
    bool is_scratch(MIR_op_t operand) const { return operand.mode == MIR_OP_REG && operand.u.reg < scratch.size() && scratch[operand.u.reg]; }
    void done_with(MIR_op_t operand)
    {
        if (is_scratch(operand)) spare.push_back(operand.u.reg);
    }
    MIR_op_t reg(MIR_reg_t r) { return MIR_new_reg_op(ctx, r); }
    MIR_reg_t temporary(int index)
    {
        MIR_reg_t &r = temporaries[static_cast<size_t>(index)];
        if (r == 0) {
            name = xpp::format("t{}", index);
            r = MIR_new_func_reg(ctx, item->u.func, MIR_T_D, name.c_str());
        }
        return r;
    }
    MIR_op_t number(double x) { return MIR_new_double_op(ctx, x); }
    void emit(MIR_insn_t insn) { MIR_append_insn(ctx, item, insn); }
    void emit(MIR_insn_code_t code, MIR_op_t a, MIR_op_t b) { emit(MIR_new_insn(ctx, code, a, b)); }
    void emit(MIR_insn_code_t code, MIR_op_t a, MIR_op_t b, MIR_op_t d) { emit(MIR_new_insn(ctx, code, a, b, d)); }
    void jump(MIR_insn_code_t code, MIR_insn_t label, MIR_op_t a, MIR_op_t b) { emit(MIR_new_insn(ctx, code, MIR_new_label_op(ctx, label), a, b)); }
    /* element `index` of the array at `base`, or in a Repeat with a stride the element `index` + stride * the counter
       (the counter itself for a stride of 1; any other stride multiplies it, at the use, so a branch inside the Repeat
       never leaves a register unset) */
    MIR_op_t slot(MIR_reg_t base, int index, int stride = 0)
    {
        const MIR_disp_t disp = static_cast<MIR_disp_t>(index) * DOUBLE_BYTES;
        if (stride == 0) return MIR_new_mem_op(ctx, MIR_T_D, disp, base, 0, 1);
        if (stride < 0 || !repeating) {
            failure = "an element stride outside a repeat or negative";
            return MIR_new_mem_op(ctx, MIR_T_D, disp, base, 0, 1);
        }
        if (stride == 1) return MIR_new_mem_op(ctx, MIR_T_D, disp, base, repeat_counter, DOUBLE_BYTES);
        const MIR_reg_t scaled = fresh(MIR_T_I64);
        emit(MIR_MUL, reg(scaled), reg(repeat_counter), MIR_new_int_op(ctx, stride));
        return MIR_new_mem_op(ctx, MIR_T_D, disp, base, scaled, DOUBLE_BYTES);
    }

    /* a double operand in a register: an immediate or a memory operand is moved into a new one */
    MIR_op_t in_register(MIR_op_t operand)
    {
        if (operand.mode == MIR_OP_REG) return operand;
        const MIR_reg_t r = fresh(MIR_T_D);
        emit(MIR_DMOV, reg(r), operand);
        return reg(r);
    }
    void store(MIR_op_t destination, MIR_op_t value)
    {
        const MIR_op_t source = value.mode == MIR_OP_MEM ? in_register(value) : value;
        emit(MIR_DMOV, destination, source);
        done_with(source);
    }

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
        }
        const ir::CallShape shape = ir::call_shape(target, module_function);
        for (size_t p = 0; p < shape.before.size(); ++p) parameter(MIR_T_P);
        for (int a = 0; a < shape.doubles; ++a) parameter(MIR_T_D);
        if (shape.index) parameter(MIR_T_I32);
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
        if (depth > ir::MAX_TREE_DEPTH) {
            failure = xpp::format("an expression nested deeper than {}", ir::MAX_TREE_DEPTH);
            return number(0.0);
        }
        const IrNode &n = ir.nodes[static_cast<size_t>(node)];
        switch (n.op) {
        case IrOp::Number: return number(n.number);
        case IrOp::Constant: return slot(c, n.index, n.stride);
        case IrOp::Variable: return slot(v, n.index, n.stride);
        case IrOp::Temporary: return reg(temporary(n.index));
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
            const MIR_reg_t divisor = fresh(MIR_T_D);
            const MIR_insn_t keep = MIR_new_label(ctx);
            emit(MIR_DMOV, reg(divisor), right);
            done_with(right);
            jump(MIR_DBNE, keep, reg(divisor), number(0.0));
            emit(MIR_DMOV, reg(divisor), number(ir.zero_divisor));
            emit(keep);
            const MIR_reg_t r = fresh(MIR_T_D);
            emit(MIR_DDIV, reg(r), left, reg(divisor));
            done_with(left);
            done_with(reg(divisor));
            return reg(r);
        }
        case IrOp::Select: {
            /* args[0]==0 ? args[1] : args[2]; a NaN condition is not equal to 0, so it takes args[2] */
            const MIR_op_t cond = in_register(operand(n.args[0], depth + 1));
            const MIR_reg_t r = fresh(MIR_T_D);
            const MIR_insn_t otherwise = MIR_new_label(ctx), done = MIR_new_label(ctx);
            jump(MIR_DBNE, otherwise, cond, number(0.0));
            done_with(cond);
            const MIR_op_t then_value = operand(n.args[1], depth + 1);
            emit(MIR_DMOV, reg(r), then_value);
            done_with(then_value);
            emit(MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, done)));
            emit(otherwise);
            const MIR_op_t else_value = operand(n.args[2], depth + 1);
            emit(MIR_DMOV, reg(r), else_value);
            done_with(else_value);
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
        /* the result goes into an operand's register that is not used again */
        MIR_reg_t r;
        if (is_scratch(a)) {
            r = a.u.reg;
            done_with(b);
        } else if (is_scratch(b)) {
            r = b.u.reg;
        } else {
            r = fresh(MIR_T_D);
        }
        emit(code, reg(r), a, b);
        return reg(r);
    }
    /* a hidden parameter's operand in a call: the caller's own pointers, and null for the RPN and the output */
    MIR_op_t hidden_operand(ir::Hidden h)
    {
        switch (h) {
        case ir::Hidden::Constants: return reg(c);
        case ir::Hidden::Variables: return reg(v);
        case ir::Hidden::Parser: return reg(parser);
        case ir::Hidden::Rpn:
        case ir::Hidden::Output: return MIR_new_int_op(ctx, 0);
        }
        return MIR_new_int_op(ctx, 0);
    }
    MIR_op_t call(const IrNode &n, int depth)
    {
        const IrCallee &target = ir.callees[static_cast<size_t>(n.callee)];
        if (target.negation) {
            const MIR_op_t value = operand(ir.arguments(n)[0], depth + 1);
            const MIR_reg_t negated = fresh(MIR_T_D);
            emit(MIR_DNEG, reg(negated), value);
            done_with(value);
            return reg(negated);
        }
        if (!declare(n.callee)) return number(0.0);
        const size_t k = static_cast<size_t>(n.callee);
        const MIR_reg_t r = fresh(MIR_T_D);
        const size_t base = operands.size();
        operands.push_back(MIR_new_ref_op(ctx, protos[k]));
        operands.push_back(MIR_new_ref_op(ctx, targets[k]));
        operands.push_back(reg(r));
        const ir::CallShape shape = ir::call_shape(target, target.address == nullptr ? function_named.at(target.name) : nullptr);
        for (ir::Hidden h : shape.before) operands.push_back(hidden_operand(h));
        for (int a : ir.arguments(n)) {
            const MIR_op_t value = in_register(operand(a, depth + 1));
            operands.push_back(value);
        }
        if (shape.index) operands.push_back(MIR_new_int_op(ctx, n.index));
        emit(MIR_new_insn_arr(ctx, MIR_CALL, operands.size() - base, operands.data() + base));
        for (size_t i = base + 3; i < operands.size(); ++i) done_with(operands[i]); /* after the prototype, the callee and the result */
        operands.resize(base);
        return reg(r);
    }

    void statement(const IrStatement &s, MIR_insn_t finish)
    {
        switch (s.kind) {
        case IrStatement::Kind::Temporary: {
            const MIR_op_t value = operand(s.value, 0);
            emit(MIR_DMOV, reg(temporary(s.index)), value);
            done_with(value);
            break;
        }
        case IrStatement::Kind::Variable: store(slot(v, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::Constant: store(slot(c, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::Output: store(slot(output, s.index, s.stride), operand(s.value, 0)); break;
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
            done_with(low);
            emit(MIR_EXT32, reg(loop.low), reg(loop.low));
            const MIR_op_t high = in_register(operand(s.other, 0));
            emit(MIR_D2I, reg(loop.high), high);
            done_with(high);
            emit(MIR_EXT32, reg(loop.high), reg(loop.high));
            emit(MIR_DMOV, reg(temporary(s.slot)), number(0.0));
            loop.top = MIR_new_label(ctx);
            loop.done = MIR_new_label(ctx);
            jump(MIR_BGT, loop.done, reg(loop.low), reg(loop.high));
            emit(MIR_MOV, reg(loop.counter), reg(loop.low));
            emit(loop.top);
            break;
        }
        case IrStatement::Kind::LoopEnd: {
            const Loop &loop = loops[static_cast<size_t>(s.index)];
            const MIR_op_t total = reg(temporary(s.slot));
            const MIR_op_t value = operand(s.value, 0);
            emit(MIR_DADD, total, total, value);
            done_with(value);
            jump(MIR_BEQ, loop.done, reg(loop.counter), reg(loop.high));
            emit(MIR_ADD, reg(loop.counter), reg(loop.counter), MIR_new_int_op(ctx, 1));
            emit(MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, loop.top)));
            emit(loop.done);
            break;
        }
        case IrStatement::Kind::RepeatBegin:
            if (repeating || s.index < 1) {
                failure = "a repeat inside a repeat or of no pass";
                break;
            }
            repeating = true;
            repeat_count = s.index;
            repeat_counter = fresh(MIR_T_I64);
            repeat_top = MIR_new_label(ctx);
            emit(MIR_MOV, reg(repeat_counter), MIR_new_int_op(ctx, 0));
            emit(repeat_top);
            break;
        case IrStatement::Kind::RepeatEnd:
            if (!repeating) {
                failure = "the end of a repeat that did not begin";
                break;
            }
            repeating = false;
            emit(MIR_ADD, reg(repeat_counter), reg(repeat_counter), MIR_new_int_op(ctx, 1));
            jump(MIR_BLT, repeat_top, reg(repeat_counter), MIR_new_int_op(ctx, repeat_count));
            break;
        case IrStatement::Kind::IfBegin: {
            /* the then arm runs when the condition is 0; a NaN is not 0 */
            const MIR_op_t cond = in_register(operand(s.value, 0));
            branches.push_back({MIR_new_label(ctx), MIR_new_label(ctx)});
            jump(MIR_DBNE, branches.back().otherwise, cond, number(0.0));
            done_with(cond);
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

    /* where a long function (the right-hand side of thousands of equations, its exact Jacobian) is cut: into
       runs of about CHUNK_STATEMENTS statements, each its own MIR function, called in order by the function itself.
       MIR's cost per instruction grows with the instructions and registers of one function (its liveness sets are
       bitmaps over all of them, and a function's working set leaves the cache): lecdiff's right-hand side of 200,
       400, 800, 1600 and 3200 cells compiled in 38, 112, 278, 681 and 1544 M cycles as one function (2.3 to 2.9 times
       for each doubling) and in 33, 88, 196, 432 and 885 M cut into chunks of 100 statements (2.05 at the large end:
       linear), 1.7 times faster at 3200 (W817, one pinned CPU). A cut falls only between statements outside a loop
       or a conditional, with no temporary used on both sides and not at a ReturnWithoutOutput, which stays in the
       function with the runs shorter than a chunk. Empty when the function is short. */
    std::vector<std::pair<size_t, size_t>> chunks_of(const IrFunction &f)
    {
        std::vector<std::pair<size_t, size_t>> chunks;
        const size_t count = f.statements.size();
        if (count <= 2 * CHUNK_STATEMENTS || f.kind == IrFunction::Kind::User || f.kind == IrFunction::Kind::Equation) return chunks;
        /* by temporary, the first and the last statement that uses it: a cut inside is refused */
        std::vector<size_t> first(static_cast<size_t>(f.temporaries), count), last(static_cast<size_t>(f.temporaries), 0);
        std::vector<int> pending;
        std::vector<size_t> seen(ir.nodes.size(), count); /* the statement a node was last walked for */
        const auto use = [&](int temporary, size_t at) {
            const size_t t = static_cast<size_t>(temporary);
            first[t] = std::min(first[t], at);
            last[t] = std::max(last[t], at);
        };
        for (size_t i = 0; i < count; ++i) {
            const IrStatement &st = f.statements[i];
            if (st.kind == IrStatement::Kind::Temporary) use(st.index, i);
            if (st.slot >= 0) use(st.slot, i);
            for (int root : {st.value, st.other}) {
                if (root >= 0) pending.push_back(root);
            }
            while (!pending.empty()) { /* a walk with its own stack: a deep tree cannot exhaust ours */
                const int node = pending.back();
                pending.pop_back();
                if (seen[static_cast<size_t>(node)] == i) continue;
                seen[static_cast<size_t>(node)] = i;
                const IrNode &n = ir.nodes[static_cast<size_t>(node)];
                if (n.op == IrOp::Temporary) use(n.index, i);
                if (n.op == IrOp::Call) {
                    for (int a : ir.arguments(n)) pending.push_back(a);
                } else if (n.op == IrOp::Add || n.op == IrOp::Subtract || n.op == IrOp::Multiply || n.op == IrOp::Divide ||
                           n.op == IrOp::GuardedDivide || n.op == IrOp::Select) {
                    for (int a : n.args) {
                        if (a >= 0) pending.push_back(a);
                    }
                }
            }
        }
        /* by position i (between statement i - 1 and i), how many temporaries span it: a running sum of their ends */
        std::vector<int> spanning(count + 2, 0);
        for (size_t t = 0; t < first.size(); ++t) {
            if (first[t] >= last[t]) continue;
            ++spanning[first[t] + 1];
            --spanning[last[t] + 1];
        }
        std::vector<char> blocked(count + 1, 0);
        int open = 0;
        for (size_t i = 0; i <= count; ++i) {
            open += spanning[i];
            blocked[i] = open != 0;
        }
        size_t start = 0;
        int depth = 0;
        for (size_t i = 0; i < count; ++i) {
            const IrStatement::Kind kind = f.statements[i].kind;
            if (kind == IrStatement::Kind::ReturnWithoutOutput) {
                if (depth != 0) return {}; /* never written by the Generator inside a nest: not cut at all */
                if (i - start >= CHUNK_STATEMENTS) chunks.emplace_back(start, i);
                start = i + 1;
                continue;
            }
            if (kind == IrStatement::Kind::LoopBegin || kind == IrStatement::Kind::IfBegin || kind == IrStatement::Kind::RepeatBegin) ++depth;
            if (kind == IrStatement::Kind::LoopEnd || kind == IrStatement::Kind::IfEnd || kind == IrStatement::Kind::RepeatEnd) --depth;
            if (depth == 0 && i + 1 - start >= CHUNK_STATEMENTS && !blocked[i + 1]) {
                chunks.emplace_back(start, i + 1);
                start = i + 1;
            }
        }
        if (count - start >= CHUNK_STATEMENTS) chunks.emplace_back(start, count);
        return chunks;
    }

    /* the function f's statements from `first` to `last`, named `fname`: its parameters, its registers, its
       statements, its return. A function that was cut calls its chunks (cut, their functions) where they were. */
    MIR_item_t lower(const IrFunction &f, size_t first_statement, size_t last_statement, const std::string &fname,
                     const std::vector<std::pair<size_t, size_t>> &cut, const std::vector<MIR_item_t> &chunk_items)
    {
        parameters.clear();
        parameter_names.clear();
        for (ir::Hidden h : ir::own_hidden(f)) parameter(MIR_T_P, ir::hidden_name(h));
        for (int a = 0; a < f.doubles; ++a) {
            name = xpp::format("a{}", a);
            parameter(MIR_T_D, name.c_str());
        }
        name_parameters();
        const bool returns = f.kind == IrFunction::Kind::User || f.kind == IrFunction::Kind::Equation;
        MIR_type_t result = MIR_T_D;
        function_open = true; /* MIR makes the function current before it can refuse its name */
        item = MIR_new_func_arr(ctx, fname.c_str(), returns ? 1 : 0, &result, parameters.size(), parameters.data());
        const MIR_func_t machine = item->u.func;
        c = MIR_reg(ctx, "c", machine);
        v = MIR_reg(ctx, "v", machine);
        parser = MIR_reg(ctx, "S", machine);
        if (f.kind == IrFunction::Kind::Equation) rpn = MIR_reg(ctx, "R", machine);
        if (f.kind == IrFunction::Kind::Rhs || f.kind == IrFunction::Kind::Jacobian) output = MIR_reg(ctx, "o", machine);
        arguments.clear();
        for (int a = 0; a < f.doubles; ++a) {
            name = xpp::format("a{}", a);
            arguments.push_back(MIR_reg(ctx, name.c_str(), machine));
        }
        registers = 0;
        spare.clear();
        scratch.clear();
        temporaries.assign(static_cast<size_t>(f.temporaries), 0);
        loops.assign(static_cast<size_t>(f.loops), Loop{});
        branches.clear();
        repeating = false;
        const MIR_insn_t finish = MIR_new_label(ctx);
        size_t next_chunk = 0;
        for (size_t i = first_statement; i < last_statement;) {
            if (next_chunk < cut.size() && i == cut[next_chunk].first) {
                call_chunk(chunk_items[next_chunk]);
                i = cut[next_chunk++].second;
            } else {
                statement(f.statements[i++], finish);
            }
        }
        emit(finish);
        if (returns) emit(MIR_new_ret_insn(ctx, 1, in_register(operand(f.result, 0))));
        else emit(MIR_new_ret_insn(ctx, 0));
        MIR_finish_func(ctx);
        function_open = false;
        return item;
    }

    /* the call of a chunk of the function being lowered: the same four pointers */
    void call_chunk(MIR_item_t chunk)
    {
        if (chunk_proto == nullptr) {
            parameters.clear();
            parameter_names.clear();
            for (int p = 0; p < 4; ++p) parameter(MIR_T_P);
            name_parameters();
            chunk_proto = MIR_new_proto_arr(ctx, "proto_chunk", 0, nullptr, parameters.size(), parameters.data());
        }
        std::array<MIR_op_t, 6> operands = {MIR_new_ref_op(ctx, chunk_proto), MIR_new_ref_op(ctx, chunk), reg(c), reg(v), reg(parser), reg(output)};
        emit(MIR_new_insn_arr(ctx, MIR_CALL, operands.size(), operands.data()));
    }

    /* one function of the module, or the chunks it is cut into and the function that calls them */
    void lower(const IrFunction &f)
    {
        const auto cut = chunks_of(f);
        std::vector<MIR_item_t> chunk_items;
        for (size_t k = 0; k < cut.size(); ++k) {
            name = f.name + "." + std::to_string(k);
            chunk_items.push_back(lower(f, cut[k].first, cut[k].second, std::string(name), {}, {}));
        }
        lower(f, 0, f.statements.size(), f.name, cut, chunk_items);
    }

    void build()
    {
        ctx = MIR_init2(&state.alloc, nullptr);
        state.context = ctx;
        MIR_set_error_func(ctx, raised);
        for (const IrFunction &f : ir.functions) function_named.emplace(f.name, &f);
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
    return native::function_at(state_->functions, name, state_->place, "MIR");
}

} // namespace xpp::mir
