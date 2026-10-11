/* The decoder of a program's instructions (expr_decode.h, W276). */
#include "expr_decode.h"
#include "model.h"
#include "session.h"

namespace xpp::expr {

namespace {

/* what is open while a program is read: the program itself, a sum's body,
   or a conditional (base: the stack depth under its branches; else_pc and
   end_pc where its else part and the end of it are) */
struct Frame {
    enum class Kind { Program, Sum, If } kind;
    std::size_t base, limit; /* limit: where the program or sum body ends */
    std::size_t else_pc = 0, end_pc = 0;
    bool has_then = false;
};

}

std::expected<std::vector<Instr>, std::string> decode(const Session &s, std::span<const int> p, int arguments)
{
    const Model &m = s.model();
    std::vector<Instr> out;
    out.reserve(p.size());
    std::vector<Frame> frames{{Frame::Kind::Program, 0, p.size()}};
    std::size_t depth = 0;
    const auto refuse = [](const char *why) { return std::unexpected(std::string(why)); };
    const auto need = [&](std::size_t n) { return depth >= frames.back().base + n; };
    const auto in_range = [](int in, std::size_t bound) { return in >= 0 && static_cast<std::size_t>(in) < bound; };

    if (p.empty()) return refuse("an empty program");
    for (std::size_t pc = 0;;) {
        const std::size_t limit = frames.back().limit;
        if (pc >= limit) return refuse("an unterminated program");
        while (frames.back().kind == Frame::Kind::If && frames.back().has_then && frames.back().end_pc == pc) {
            if (depth != frames.back().base + 1) return refuse("inconsistent conditional stacks");
            frames.pop_back();
            out.push_back({JOIN, 0, 0, pc});
        }
        if (frames.back().kind == Frame::Kind::If && frames.back().has_then && pc > frames.back().end_pc)
            return refuse("an invalid conditional jump");
        const int op = p[pc], type = op / MAXTYPE, in = op % MAXTYPE;
        std::size_t next = pc + 1;
        Instr i{op, type, in, pc};
        if (op == ENDEXP || op == ENDSUM) {
            const bool sum = op == ENDSUM;
            const Frame &f = frames.back();
            if (f.kind == Frame::Kind::If) return refuse("an invalid result stack");
            if ((f.kind == Frame::Kind::Sum) != sum) return refuse("a misplaced end of program");
            if (depth != f.base + 1) return refuse("an invalid result stack");
            if (!sum) return out;
            if (next != f.limit) return refuse("a sum that does not end where it says");
            frames.pop_back();
            out.push_back(i);
            pc = next;
            continue;
        } else if (op == NUMSYM) {
            if (limit - pc < 3) return refuse("a truncated number");
            i.number = number_from_halves(p[pc + 2], p[pc + 1]);
            next += 2;
            ++depth;
        } else if (op == MYIF) {
            if (next >= limit || p[next] < 0 || !need(1)) return refuse("an invalid conditional jump");
            const std::size_t target = next + 1 + static_cast<std::size_t>(p[next]);
            if (target >= limit) return refuse("an out-of-range conditional jump");
            --depth;
            frames.push_back({Frame::Kind::If, depth, limit, target});
            ++next;
        } else if (op == MYTHEN) {
            Frame &f = frames.back();
            if (next >= limit || p[next] < 0 || f.kind != Frame::Kind::If || f.has_then) return refuse("an invalid conditional jump");
            const std::size_t end = next + 1 + static_cast<std::size_t>(p[next]);
            ++next;
            if (next != f.else_pc || end >= limit || end < next || depth != f.base + 1) return refuse("an invalid conditional jump");
            const Frame &outer = frames[frames.size() - 2];
            if (outer.kind == Frame::Kind::If && end > (outer.has_then ? outer.end_pc : outer.else_pc - 2))
                return refuse("an invalid conditional jump");
            --depth;
            f.end_pc = end;
            f.has_then = true;
        } else if (op == MYELSE) {
            /* the else part starts here */
            pc = next;
            continue;
        } else if (op == ENDFUN) {
            if (arguments < 0 || next >= limit || p[next] != arguments) return refuse("an invalid function end");
            pc = next + 1;
            continue;
        } else if (op == INDXCOM) {
            ++depth;
        } else if (op == SUMSYM) {
            if (next >= limit || p[next] <= 0 || !need(2)) return refuse("an invalid sum");
            const std::size_t end = next + 1 + static_cast<std::size_t>(p[next]);
            if (end > limit) return refuse("an out-of-range sum");
            depth -= 2;
            frames.push_back({Frame::Kind::Sum, depth, end});
            next += 1;
        } else if (op == ENDDELAY || op == ENDSHIFT || op == ENDISHIFT || op == RANDNORM) {
            if (!need(2)) return refuse("a stack underflow");
            --depth;
        } else if (op == ENDDELSHFT || op == ENDSET) {
            if (!need(3)) return refuse("a stack underflow");
            depth -= 2;
        } else if (op == RANDUNI || op == RANDPOI) {
            if (!need(1)) return refuse("a stack underflow");
        } else if (type == SVARTYPE || type == SCONTYPE || type == CONTYPE || type == VARTYPE) {
            /* a delay's, shift's or set's own variable or constant, or an operand */
            const bool constant = type == SCONTYPE || type == CONTYPE;
            if (!in_range(in, constant ? s.parser.constants.size() : s.parser.variables.size())) return refuse("an out-of-range operand");
            ++depth;
        } else if (type == USTACKTYPE) {
            if (arguments < 0 || in >= arguments || in < 0) return refuse("an out-of-range argument");
            ++depth;
        } else if (type == FUN1TYPE) {
            if (!in_range(in, FUN1_COUNT) || !fun1[static_cast<std::size_t>(in)]) return refuse("an invalid unary function");
            if (!need(1)) return refuse("a stack underflow");
        } else if (type == FUN2TYPE) {
            if (!in_range(in, FUN2_COUNT)) return refuse("an out-of-range binary function");
            if (in > DIVIDE && !fun2[static_cast<std::size_t>(in)]) return refuse("an invalid binary function");
            if (!need(2)) return refuse("a stack underflow");
            --depth;
        } else if (type == UFUNTYPE) {
            if (in < 0 || in >= m.nfun || next >= limit || p[next] != m.narg_fun[in]) return refuse("an invalid user function call");
            i.count = p[next++];
            if (i.count < 0 || !need(static_cast<std::size_t>(i.count))) return refuse("a stack underflow");
            depth = depth - static_cast<std::size_t>(i.count) + 1;
        } else if (type == TABTYPE || type == NETTYPE || type == VECTYPE) {
            const std::size_t bound = type == TABTYPE ? s.tables.size() : type == NETTYPE ? m.networks.size() : m.vectors.size();
            if (!in_range(in, bound)) return refuse("an out-of-range table, network or vectorizer");
            if (!need(1)) return refuse("a stack underflow");
        } else if (type == KERTYPE) {
            if (!in_range(in, m.kernels.size())) return refuse("an out-of-range kernel");
            ++depth;
        } else {
            return refuse("an unsupported instruction");
        }
        if (depth > EXPR_STACK) return refuse("an out-of-range stack");
        out.push_back(i);
        pc = next;
    }
}

}
