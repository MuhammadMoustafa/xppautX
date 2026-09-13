# xppautX — notes for Claude Code

Fork of XPPAUT 8.x being modernized. See README.md for the plan and layout.

## Build (from Windows this repo builds only under WSL)

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && make -j8"

Headless smoke test (writes output.dat in cwd, expect 601 rows for lecar.ode):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppaut examples/ode/lecar.ode -silent && wc -l output.dat"

Run the GUI (WSLg shows the X11 window on the Windows desktop):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppaut examples/ode/lecar.ode"

`sudo` inside WSL needs the user's password; apt installs must be run by the user.
The Windows-side gcc at C:\Strawberry\c\bin is Perl's MinGW without X11 headers — do not use it.

## Conventions

- Keep upstream function and file names so upstream patches stay mergeable.
- `core/fftn.c` does `#include __FILE__`; the Makefile's `-I.` is required for it.
- `core/sbml2xpp.c` needs libsbml and is not built, same as upstream.
- Commit messages: imperative subject, body explains why.
