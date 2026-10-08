# Commands, hotkeys and quick access: design draft

Status: defaults approved by the maintainer, 2026-10-07. Built: the command table (W207), the key dispatcher
(W208), the keymap file and its protocol command (W211), and the keymap editor and the pinned quick-access
toolbar (W212), the value undo/redo (W210), the one Continue (W213) and Save a copy of the session (W218). All of the
cards below are built.
Decided in conversation: the XPPAUT one-letter sequences stop shaping the
interface; Ctrl+S saves the session and Ctrl+Shift+S is Save as; Ctrl+Z
undoes Values edits; users can rebind keys and choose quick-access commands.

## Principles

1. **A command is data.** One table owns every command: a stable `id`, a
   `category`, a plain `label`, a one-line `description`, a `kind` (what it
   needs: a model, a run, AUTO ...), its default keys and whether it may be
   pinned. The sidebar, command search (Ctrl+K), toolbar, native menus, the
   manual's key table and recordings are all generated from it. No second
   list of labels or keys exists anywhere (single source).
2. **Keys are a layer over commands.** A key never defines a command, and a
   command never depends on which menu is open. Recordings store the id.
3. **XPPAUT keys are an optional keymap**, named "XPPAUT sequences", off by
   default: `F` then `S` is a chord in that keymap, not a mode of the menu.
   No sticky "File shortcuts" state; chords show a pending-key hint and
   cancel with Esc or on a timeout-free focus loss.
4. **Reserved keys are not ours:** Alt+F4, Ctrl+W, Ctrl+Q, F11, F12, the
   browser's and OS's. They cannot be bound (the editor says so) and the
   page never calls `preventDefault` on them.
5. **Everything the user changes is visible and reversible:** each binding
   shows whether it is default, changed or added by the user; Reset exists
   per binding and for all.

## What other programs do (to take the good parts)

- **VS Code:** a Keyboard Shortcuts editor listing every command with its
  key, source (Default/User) and `when` context; search by command name or
  by *pressing the keys* ("Record Keys"); right-click to Change, Remove,
  Reset; conflicts shown by listing every command on the same key; chords
  (Ctrl+K Ctrl+S); the underlying file `keybindings.json` for power users,
  where `-command` removes a default binding.
- **GNOME / KDE settings:** click a row, press the new combination (Esc
  cancels, Backspace clears), a conflict names the command that already has
  it and offers Replace; Reset all; shortcuts grouped by category.
- **Blender / Emacs:** whole keymap presets; a keymap is data, not code.

Taken: the command list with search and record-keys; click-row-press-keys
with Esc/Backspace; the conflict names the other command and offers
Replace or Cancel; default/user layers with per-binding Reset; chords for
the XPPAUT preset. Not taken: `when` expressions (our context is only "has a
model / has run data / dialog open", fixed per command `kind`); hand-edited
JSON as the primary path.

## Defaults (changeable unless marked fixed)

| Command | Keys |
|---|---|
| Open model | Ctrl+O |
| Save session | Ctrl+S |
| Save session as | Ctrl+Shift+S |
| Reload model | Ctrl+R |
| Command search | Ctrl+K (fixed: the way back to everything) |
| Next / previous pane (move focus between the sidebar, toolbar, plot and side panels; W208 lists them from the page's landmark regions) | F6 / Shift+F6 |
| Undo / redo value edit | Ctrl+Z / Ctrl+Shift+Z, Ctrl+Y |
| Run from initial | Ctrl+Enter |
| Run from last state | Ctrl+Shift+Enter |
| Continue | Alt+Enter |
| Run to steady state | Alt+S |
| Stop | Esc while running |
| Help | F1 |

Ctrl+S needs a session file path remembered by the Session (a new
`session_file`); the first save, or a model with no session, opens Save as.
The title shows an unsaved-changes dot; "Save this session first?" at quit
stays.

## Quick access

The toolbar above the plot has a fixed left part (Run from initial, Run
from last state, Continue, Stop) and a **user-pinned** part: any command can
be pinned from its row in command search or the keymap editor, dragged to
reorder, unpinned from the toolbar's context menu. A command that does not
fit moves into an overflow menu, never disappears. Pinned commands show
their label (icon-only is not used: labels are the discoverability fix).

## Settings file

`keymap.json` is the user's, in the per-user config folder, owned by one
module in the core (`xpp_keymap`), served to the page in `hello`. It stores
only the user's differences: `bindings` (command id → keys, `[]` = removed),
`pinned` (ordered command ids), `preset` (`"default"` or `"xppaut"`).

- Loaded all or nothing (W125): unknown command id, malformed key string,
  duplicate key within a context, a reserved key, an over-long or too-deep
  value: the file, line and value are shown and nothing is applied.
- Written through `xpp::Writer` (temp, then rename). Never executable, never
  read from a path the file names; the only path is the fixed config path.
- Browser mode and the desktop window read and write the same file through
  one protocol command, so the two never disagree.
- Not a fallback target: a missing file means "all defaults", a bad file is
  an error, never silently ignored.

Settled while building it (W211, docs/protocol.md "Keymap" has the whole
contract): a key has one spelling (`Ctrl+Alt+Shift+Meta+` in that order, a
capital letter, `F1`..`F24` or a DOM name such as `Enter`), a chord is at most
two parts, and a key that starts another command's chord clashes with it as
two commands on one key do. While the file is bad the page is shown the table's
defaults marked `ok: false` with the error (never the user's part of it, and no
second error event); `set` replaces a bad file whole. The reserved list is
core/xpp_keymap.h's `RESERVED_KEYS`, sent in `hello.keymap.reserved`. The
three shortcut-layer rows take no keys.

## Keymap editor (a dialog in the page)

Rows: command label, category, keys, source. Search matches label,
description, id or pressed keys. Click a key cell, press the combination
(Esc cancels, Backspace clears); on conflict: "Ctrl+B is Run from last
state. Replace / Cancel". Context menu: Reset binding, Add second binding,
Pin to toolbar. Buttons: Reset all, Preset: Default / XPPAUT sequences.
All of it reachable by keyboard and announced for screen readers.

## Cards (after W203-W205 merge; issues #257-#267)

| Card | Work |
|---|---|
| W207 | Command table in the core (id, category, label, description, kind, default keys); `hello` sends it; sidebar, search and native menus generated from it; recordings keep working by id |
| W208 | Page keyboard layer: one dispatcher from key to command id, chords, reserved keys, pending-chord hint; remove the sticky shortcut modes; XPPAUT sequences as a preset |
| W209 | Ctrl+S / Save as: Session remembers its file; unsaved dot; Ctrl+Shift+S; native menus use the same ids |
| W210 | Value undo/redo stack (bounded snapshots of parameters, initial conditions and numerics) replacing the checkpoint; Run from last state pushes the old values |
| W211 | `keymap.json` owner module, all-or-nothing load, protocol command, security checks |
| W212 | Keymap editor dialog and pinned quick-access toolbar |
| W213 | One Continue (honours the output stride, remove the legacy `C` prompt), docs and servercheck (done: Alt+Enter is `continue`'s default key) |

Decided 2026-10-06: `nout` is renamed first (W206 before W207), so the format
change is tested early and every later card builds on the new name. The UI
label is "Store every N steps"; the key is renamed in `.odex`, `.snapx`,
`.recx` and the protocol with no old-file support (no fallbacks): the
converter maps the `.ode` word `nout` once, our own formats take only the
new name, and the examples and tests are converted in the same card.
