# The options file

You can have many options files. They are useful for initializing XPP. However, because you can now set options from within the ODE file, options files are probably obsolete. They have the following format:

    9x15    BIG_FONT_NAME   <-- menu fonts
    fixed   SMALL_FONT_NAME <-- IC, browser, etc fonts
    0       BACKGROUND (1=white,0=black)
    0   IXPLT <--  X-axis variable (0=time)
    1   IYPLT <--  Y-axis variable
    1   IZPLT <--  Z-axis variable
    0   AXES <-- type of axis (0-2d 5-3d)
    1   NJMP <-- nOutput
    40  NMESH <-- Nullcline mesh
    4   METHOD <-- Integration method
    1   TIMEPLOT <--- set to zero if one axis is not time
    8000    MAXSTOR <--- maximum rows stored
    20.0    TEND <-- total integration time
    .05 DT <--- time step
    0.0 T0 <-- start time
    0.0 TRANS <-- transient
    100.    BOUND <--- bounds
    .0001   HMIN <-- min step for GEAR
    1.0 HMAX <-- max ``   ``   ``
    .00001  TOLER <-- tolerance for GEAR
    0.0 DELAY <-- maximal delay
    0.0 XLO  <--- 2D window sizes
    20.0    XHI
    -2.0    YLO
    2.0 YHI

Within an ODE file, you can call up a different options file by typing

    option <filename>

where `<filename>` is the name of an options file. The full list of
option names (also usable as `@ name=value` lines in an ODE file) is in
[Quick reference](16-quick-reference.md#the-options-list).
The appearance keys above (`BIG_FONT_NAME`, `SMALL_FONT_NAME`,
`BACKGROUND`) are X11 window settings; the browser front end (web2) has
its own theme (light/dark/system, docs/ui-v2.md section 6) and ignores
them. Everything else — the numerics, the plot axes, the storage size —
still applies.
