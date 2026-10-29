# Izhikevich models: XPPAUT 8.0 against xppautX (W416, #478)

`izh_*.ode` are the 28 models of the card W416 (Izhikevich 2004 Fig. 1 A-T, Izhikevich 2003 Fig. 2) as XPPAUT
reads them, with the text `xppautX --convert` turned into the committed `examples/ode/izh_*.odex` (branch
`task/W416-izhikevich`, commit message "W416 The Izhikevich model's firing behaviours ..."; the hash is the merged
commit's). Two of them (Q, T) are maps (`meth=discrete`), the rest ODEs with a `global` event.

`compare.sh XPPAUT XPPAUTX [ODEX_FOLDER]` (WSL) runs each model in a folder of its own with

    xppaut izh_X.ode -silent        # XPPAUT 8.0
    xppautX izh_X.ode --silent      # xppautX; the first open also writes izh_X.odex

and requires output.dat (CRs removed) identical byte for byte, and with ODEX_FOLDER (the repository's
`examples/ode`) the converted `.odex` identical to the committed one.

XPPAUT 8.0 is `reference/xppaut-8.0` (commit c021b511eee95a43576227656c6530e9af78ca27 of the fork,
`git archive`) built as `paper/benchmarks/five-model-screen/build.py` builds its `legacy` tree (gcc,
`-O2 -ffp-contract=off -DAUTO -DCVODE_YES`, X11 linked, no X server needed for `-silent`).
Result at W416: output 28 of 28 identical, converted .odex 28 of 28 identical.
