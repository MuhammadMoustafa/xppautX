/* W711: p_rhs from the Generator's trees (core/expr_ir.h) through MIR's own API: no C text, no c2mir. Each
   tree becomes the same IEEE operations in the same order as the C text, one double register per value
   (MIR's generator allocates them). Everything the code calls is an import named in the trees' callee table,
   resolved only through it. */
#include "xpp_mir_state.h"
#include <cstdio>

namespace xpp::mir {

namespace {
/* a tree deeper than this is refused: the lowering recurses on it (a frame is about 100 bytes) */
constexpr int MAX_TREE_DEPTH = 4096;
constexpr int DOUBLE_BYTES = 8;
/* the names of a call's parameters, MIR wants each one named */
constexpr const char *PARAMETER[] = {"S", "a0", "a1", "a2", "i"};

/* The state of one lowering. It lives outside the frame MIR may jump out of (xpp_mir.cpp's build), so the
   frames between hold only trivial locals. */
struct Lowering {
    const RhsIr &ir;
    MIR_context_t ctx = nullptr;
    MIR_item_t function = nullptr;
    MIR_reg_t c = 0, v = 0, session = 0, o = 0;
    std::vector<MIR_reg_t> temporaries;
    std::vector<MIR_item_t> protos, imports; /* by callee */
    std::vector<MIR_var_t> parameters;       /* the callee being declared */
    int registers = 0;
    const char *error = nullptr;

    MIR_reg_t fresh(MIR_type_t type)
    {
        char name[24];
        std::snprintf(name, sizeof name, "r%d", registers++);
        return MIR_new_func_reg(ctx, function->u.func, type, name);
    }
    MIR_op_t reg(MIR_reg_t r) { return MIR_new_reg_op(ctx, r); }
    void emit(MIR_insn_t insn) { MIR_append_insn(ctx, function, insn); }
    void emit(MIR_insn_code_t code, MIR_op_t a, MIR_op_t b) { emit(MIR_new_insn(ctx, code, a, b)); }
    void emit(MIR_insn_code_t code, MIR_op_t a, MIR_op_t b, MIR_op_t c3) { emit(MIR_new_insn(ctx, code, a, b, c3)); }
    MIR_op_t slot(MIR_reg_t base, int index) { return MIR_new_mem_op(ctx, MIR_T_D, static_cast<MIR_disp_t>(index) * DOUBLE_BYTES, base, 0, 1); }

    /* a double operand in a register: an immediate or a memory operand is moved into a new one */
    MIR_op_t in_register(MIR_op_t operand)
    {
        if (operand.mode == MIR_OP_REG) return operand;
        const MIR_reg_t r = fresh(MIR_T_D);
        emit(MIR_DMOV, reg(r), operand);
        return reg(r);
    }

    /* The value of a node as an operand of an instruction, as c2mir's own code has them: a constant or a
       variable is a memory operand and a number an immediate, so MIR's generator folds the load into the
       instruction that uses it; the others are a register. The instructions that compute it are emitted here. */
    MIR_op_t operand(int node, int depth)
    {
        if (depth > MAX_TREE_DEPTH) {
            error = "an expression nested too deep";
            return reg(c);
        }
        const IrNode &n = ir.nodes[static_cast<size_t>(node)];
        switch (n.op) {
        case IrOp::Number: return MIR_new_double_op(ctx, n.number);
        case IrOp::Constant: return slot(c, n.index);
        case IrOp::Variable: return slot(v, n.index);
        case IrOp::Temporary: return reg(temporaries[static_cast<size_t>(n.index)]);
        case IrOp::Argument: error = "an argument outside an inlined body"; return reg(c);
        case IrOp::Add: return arithmetic(MIR_DADD, n, depth);
        case IrOp::Subtract: return arithmetic(MIR_DSUB, n, depth);
        case IrOp::Multiply: return arithmetic(MIR_DMUL, n, depth);
        case IrOp::Divide: return arithmetic(MIR_DDIV, n, depth);
        case IrOp::GuardedDivide: {
            /* l / (r==0 ? zero_divisor : r): the divisor is chosen in a register of its own */
            const MIR_op_t left = operand(n.args[0], depth + 1), right = operand(n.args[1], depth + 1);
            const MIR_reg_t divisor = fresh(MIR_T_D), r = fresh(MIR_T_D);
            const MIR_insn_t keep = MIR_new_label(ctx);
            emit(MIR_DMOV, reg(divisor), right);
            emit(MIR_new_insn(ctx, MIR_DBNE, MIR_new_label_op(ctx, keep), reg(divisor), MIR_new_double_op(ctx, 0.0)));
            emit(MIR_DMOV, reg(divisor), MIR_new_double_op(ctx, ir.zero_divisor));
            emit(keep);
            emit(MIR_DDIV, reg(r), left, reg(divisor));
            return reg(r);
        }
        case IrOp::Select: {
            /* args[0]==0 ? args[1] : args[2]; a NaN condition is not equal to 0, so it takes args[2] */
            const MIR_op_t cond = in_register(operand(n.args[0], depth + 1));
            const MIR_reg_t r = fresh(MIR_T_D);
            const MIR_insn_t otherwise = MIR_new_label(ctx), done = MIR_new_label(ctx);
            emit(MIR_new_insn(ctx, MIR_DBNE, MIR_new_label_op(ctx, otherwise), cond, MIR_new_double_op(ctx, 0.0)));
            emit(MIR_DMOV, reg(r), operand(n.args[1], depth + 1));
            emit(MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, done)));
            emit(otherwise);
            emit(MIR_DMOV, reg(r), operand(n.args[2], depth + 1));
            emit(done);
            return reg(r);
        }
        case IrOp::Call: {
            const IrCallee &callee = ir.callees[static_cast<size_t>(n.callee)];
            const MIR_reg_t r = fresh(MIR_T_D);
            MIR_op_t ops[8];
            size_t count = 0;
            ops[count++] = MIR_new_ref_op(ctx, protos[static_cast<size_t>(n.callee)]);
            ops[count++] = MIR_new_ref_op(ctx, imports[static_cast<size_t>(n.callee)]);
            ops[count++] = reg(r);
            if (callee.session) ops[count++] = reg(session);
            for (int a = 0; a < callee.doubles; ++a) ops[count++] = in_register(operand(n.args[static_cast<size_t>(a)], depth + 1));
            if (callee.index) ops[count++] = MIR_new_int_op(ctx, n.index);
            emit(MIR_new_insn_arr(ctx, MIR_CALL, count, ops));
            return reg(r);
        }
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

    void statement(const IrStatement &s, MIR_insn_t finish)
    {
        switch (s.kind) {
        case IrStatement::Kind::Temporary: emit(MIR_DMOV, reg(temporaries[static_cast<size_t>(s.index)]), operand(s.value, 0)); break;
        case IrStatement::Kind::Variable: emit(MIR_DMOV, slot(v, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::Output: emit(MIR_DMOV, slot(o, s.index), operand(s.value, 0)); break;
        case IrStatement::Kind::ReturnWithoutOutput:
            emit(MIR_new_insn(ctx, MIR_BEQ, MIR_new_label_op(ctx, finish), reg(o), MIR_new_int_op(ctx, 0)));
            break;
        }
    }

    /* the module: an import and a prototype for each callee, then p_rhs */
    void build_module()
    {
        module_item = MIR_new_module(ctx, "model");
        for (const IrCallee &callee : ir.callees) {
            parameters.clear();
            if (callee.session) parameters.push_back({MIR_T_P, PARAMETER[0], 0});
            for (int a = 0; a < callee.doubles; ++a) parameters.push_back({MIR_T_D, PARAMETER[1 + a], 0});
            if (callee.index) parameters.push_back({MIR_T_I32, PARAMETER[4], 0});
            MIR_type_t result = MIR_T_D;
            protos.push_back(MIR_new_proto_arr(ctx, ("proto_" + callee.name).c_str(), 1, &result, parameters.size(), parameters.data()));
            imports.push_back(MIR_new_import(ctx, callee.name.c_str()));
        }
        MIR_var_t arguments[] = {{MIR_T_P, "c", 0}, {MIR_T_P, "v", 0}, {MIR_T_P, "S", 0}, {MIR_T_P, "o", 0}};
        function = MIR_new_func_arr(ctx, "p_rhs", 0, nullptr, 4, arguments);
        const MIR_func_t f = function->u.func;
        c = MIR_reg(ctx, "c", f);
        v = MIR_reg(ctx, "v", f);
        session = MIR_reg(ctx, "S", f);
        o = MIR_reg(ctx, "o", f);
        for (int t = 0; t < ir.temporaries; ++t) {
            char name[24];
            std::snprintf(name, sizeof name, "t%d", t);
            temporaries.push_back(MIR_new_func_reg(ctx, f, MIR_T_D, name));
        }
        const MIR_insn_t finish = MIR_new_label(ctx);
        for (const IrStatement &s : ir.statements) statement(s, finish);
        emit(finish);
        emit(MIR_new_ret_insn(ctx, 0));
        MIR_finish_func(ctx);
        MIR_finish_module(ctx);
    }
    MIR_module_t module_item = nullptr;
};

const char *front(Program::State &state, const void *input)
{
    auto &lowering = *static_cast<Lowering *>(const_cast<void *>(input));
    lowering.ctx = state.context;
    lowering.build_module();
    state.module = lowering.module_item;
    return lowering.error;
}
} // namespace

Result<std::unique_ptr<Program>> Program::compile(const RhsIr &ir, Place place, unsigned level)
{
    if (!ir.unsupported.empty()) return fail("MIR", "not lowered: " + ir.unsupported, place);
    Lowering lowering{ir};
    std::vector<Symbol> symbols;
    for (const IrCallee &callee : ir.callees) symbols.push_back({callee.name.c_str(), callee.address});
    return assemble(std::move(place), symbols, level, front, &lowering);
}

} // namespace xpp::mir
