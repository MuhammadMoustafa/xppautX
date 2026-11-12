# MIR spike fixtures (W706)

Each model is `<name>.c`, the C text `core/expr_native.cpp` hands TinyCC for it (declarations and source: `p_rhs`,
the equations and the user functions, as compiled at a load), and `<name>.manifest`: `node`, `nvar` (the parser's
variable count), the constants `c`, the literal table `N`, the external function indices `f1`, `f2` and Session
helper names, and 40 `row`s (the time and the states) spread over the model's own `--silent` run. Numbers are C99
hex floats, so they are exact. `make mirspike` compiles each text with TinyCC and with MIR, calls `p_rhs` on every
row with both and compares every output (`ydot` and the fixed variables) bit for bit.

Made once from models in `examples/ode` (mu26, ma23), W246's PY_S1Bf_bench and Izhikevich's 2003 regular-spiking
neuron, by a scratch build whose `Generator::build` wrote the text and manifest before compiling it (not
committed): a fixture records what the generator wrote on 2026-11-11, so a later change to the generator does not
change it. A model that needs a Session helper `p_rhs` calls (networks, tables) has no fixture: the spike has no
Session.

`mu26_rhs` is mu26's text with only `p_rhs` kept (the equations and user functions cut out).
