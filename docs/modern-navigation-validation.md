# Modern navigation implementation and validation

2026-10-04 · branch `codex/modern-navigation`, based on `0971ac0c`.

## Delivered

The approved design in [modern-navigation.md](modern-navigation.md) is
implemented for W186–W191. The six published task cards are
[#239](https://github.com/MuhammadMoustafa/xppautX/issues/239),
[#240](https://github.com/MuhammadMoustafa/xppautX/issues/240),
[#241](https://github.com/MuhammadMoustafa/xppautX/issues/241),
[#242](https://github.com/MuhammadMoustafa/xppautX/issues/242),
[#243](https://github.com/MuhammadMoustafa/xppautX/issues/243) and
[#244](https://github.com/MuhammadMoustafa/xppautX/issues/244).
They remain open while the implementation is local and unmerged.

Commands are grouped into searchable Files, Run, Analysis, Plot and Tools.
The sidebar stays stable when legacy letter shortcuts change modes.
The core owns plain labels, identities, shortcut keys and action kinds;
the page only groups the identities. The header identifies the model;
the W192 follow-up puts common run actions in one toolbar above the plot
and session save in Files. Specialized workspace tools use a disclosure.

Native Windows/GTK/macOS File menu code exposes session open/save through
the existing prompt/picker route. Ctrl/Cmd+O and Ctrl/Cmd+S use the same
identified commands. Existing picker cancellation and overwrite logic is
reused. Browser mode retains browser uploads/downloads. Save explicitly
means Save session as; persistent save-in-place and recent files are outside
this design.

Ctrl/Cmd+K searches commands; arrows/Home/End navigate results. F6 and
Shift+F6 cycle visible work areas. Dialogs retain their focus handling.
Numerics names the selected solver and folds its unused fields with an
explanation. Working-value checkpoints copy authoritative parameter/IC
values, restore them in one validated command without running, and clear
on a model/session hello or reconnect. They are temporary; they do not
save solver settings or replace durable session files.

## Validation

| Gate | Result and scope |
|---|---|
| UI typecheck and build | Passed; final application bundles regenerated |
| UI unit tests | 314 passed, including exhaustive command reachability, search, action kinds, malformed identities and checkpoint copying/model boundaries |
| Strict Windows UCRT build | Passed with `WERROR=1` |
| Native unit gate | All passed; initial file-test failures were missing `cmd.exe` on PATH, resolved by adding Windows System32 to the runner PATH |
| Protocol suite | 717 checks passed, including existing scientific/session/recording regressions and 10 stable-navigation checks |
| Browser workflow sections | 402 checks passed across navigation, desktop, layout, keys, record, player, busy, files, native-binding fixtures and values; the 20-check player section was rerun after correcting its old label/keypress assertions |
| Visual inspection | Final desktop, 390×844 plot and command drawer reviewed; long labels wrap, eliminating the sidebar's horizontal scrolling |
| Windows WebView2 | 18 native-binding fixture checks passed in the actual desktop WebView2 host; picker results are still mocked |

The browser checks compare plotted numbers and data rows against the same
core's `output.dat`; checkpoint restore also compares every parameter and
IC exactly and verifies that no new series was generated. These establish
display/state consistency for the tested models, not independent
mathematical certification of all solvers.

Initial failures in new assertions were corrected to match the established
`message.error` event and legitimate search matches. Existing recording
assertions were updated for the plain labels and stable initiating command;
playback's actual command and answer-key sequence is still asserted.

Local logs are in `build/final-build.log`, `build/final-native-check.log`,
`build/ui-unit-final.log`, `build/final-server-check.log`,
`build/final-browser-check.log`, `build/player-final-check.log` and
`build/native-window-check.log`.
The broad browser log retains its three superseded player assertion failures;
the final player log passes all 20 checks. Build logs are not versioned.

## Review and limitations

Stable command IDs are validated against core metadata; unknown identities
and combinations with `key` or `win` are refused. Client and core kind
classification agree, preserving busy-state restrictions. Native actions
queue fixed normal commands and grant no new filesystem authority.
Recordings retain the stable command and its subsequent prompt answers;
legacy typed commands and recordings continue through their existing path.

Windows native code was compiled. Native-binding fixtures cover chosen
paths, cancellation, picker failure and avoiding duplicate downloads.
They are mocked bindings, not proof of an actual OS dialog interaction.
GTK and macOS menu additions have not been compiled or exercised on their
platforms. The prior review's live Windows picker evidence predates these
menu additions. Platform runtime acceptance remains a release check.

The existing palette, plotting engine and numerical engines are unchanged.
Some specialized popup menus still retain legacy terminology; this change
modernizes the main navigation and common workflow, not every scientific
dialog. No push or merge was performed.

## Visual evidence

### Permanent run toolbar (W192)

Common actions now have one permanent home above the plot: Run from initial
(I/G), Run from current (I/L), Continue (C), and Stop. The misleading former
Use current state tooltip said it copied without running; the core and the
existing runs regression prove Last copies the last state into Initial and
starts a new trajectory. The new label and manual describe that behavior.
Continue asks for an end time and appends to the existing trajectory.
Current/Continue require a prior state; computation actions are guarded
while busy. Activity and the active plot's last stored time are visible;
the time is sampled data, not a solver clock with double precision.

Removed duplicate run controls from the header, empty 2D/3D plots and States.
The default sidebar keeps More run options; parameter and solver editing
lives in Values, session save in Files, and search retains every original
command. Numerics prefix mode still exposes its applicable commands.

Searched/reused Session.menuAction, command kinds/useMayMain, existing Last
and Continue implementations in core/integrate.cpp, abort, inspectNumber,
and the active plot series. No run algorithm or protocol was added. Removed
the now-unused useCurrentState wrapper. No new filesystem or external
service access is involved in the toolbar.

Typecheck, 318 UI unit tests and the strict UCRT build pass. Combined final
section results total 170 browser checks for navigation, keys, runs, busy,
layout, recording, playback and 3D. Run-from-current checks exact starting
ICs; Continue checks appended rows and end time; busy tests guard all three
run buttons and stop through the toolbar. Recording/playback retain the
existing stable identities. The layout matrix includes widths 600–2000
at heights 560 and 900, plus the existing tall-window cases.

Early layout runs found that the toolbar squeezed short stacked desktops.
Compact buttons and a smaller Values height cap address this. The original
grid rule used invalid minmax(0, fit-content(...)) syntax, so the cap did not
apply; it is now a valid fit-content track. The final layout-only rerun
passes all 23 checks. Short stacked Values may scroll; side-by-side reference
States/Parameters still fit. Browser visual checks at 1280×720 and 390×844
confirm the controls and no narrow horizontal overflow. These screenshots
are browser evidence. Desktop native pickers are unchanged in this card.

Logs: `build/run-toolbar-unit.log`, `build/run-toolbar-build-final.log`,
`build/run-toolbar-browser.log` (keys/runs/record/player pass; superseded
layout failures), `build/run-toolbar-browser-final.log` (3D pass),
`build/run-toolbar-acceptance.log` (navigation/busy pass; superseded layout
failures), and `build/run-toolbar-layout-verified.log` (final layout pass).

![Permanent desktop run toolbar](modern-navigation/run-toolbar.png)

![Narrow run toolbar](modern-navigation/run-toolbar-narrow.png)

### States and parameters follow-up

The user's follow-up prioritizes Run and scientific inspection. Run is now
the first navigation group. The former large grey checkpoint box is a
folded Recovery disclosure below the primary values. States appears before
Parameters, which uses two columns on wide desktops; Numerics starts folded.
The reference model's two states and twelve parameters fit without scrolling
at the tested 1280×860 desktop size and were visually confirmed at 1280×720.
Large models and narrow screens still require scrolling.

Values show ten significant digits, using compact scientific notation at
extremes. Focused edits retain the full core double. Final Current values
come from the core; live trajectory samples and tail diagnostics are
float32. Each state's Tail rate is the maximum sampled |Δstate/Δt| over up
to the last ten stored intervals. It is per-variable, in state/time units,
with no automatic threshold or steady-state claim. A sampled quiet tail can
miss motion or round small changes to zero; extend the run and inspect the
trajectory to assess settling. Invalid/missing samples show a dash.

The final follow-up passes typecheck, 317 UI unit tests, the strict UCRT
asset build, and 103 browser checks across navigation, layout, runs and
values. These cover all reference value fields on screen, inspection
precision, current-state updates, parameter edits, checkpoint recovery,
and sliders after widening the values column. Slider cards now fit the
actual plot width and the drag test scrolls the track into view before use.
The earlier run's hidden-upload-input geometry assertion was corrected to
check visible value fields. No numerical engine or native core code changed.
Logs: `build/values-inspection-unit.log`, `build/values-inspection-build.log`
and `build/values-inspection-browser-final.log`.

![States and parameters together](modern-navigation/states-parameters.png)

![Narrow state inspection](modern-navigation/states-parameters-narrow.png)

### Shortcut context follow-up

The user's unfinished I/G example identified the value of visible command
state. File and Numerics now have a sticky context box, active-command
highlighting, an explicit Main commands button, and status-bar feedback.
Shortcut hints reflect the active layer (R in File; Esc, I to return to Main
before I). Submenu dialogs explain that letters belong to their displayed
menu and Escape cancels. Keyboard routing is unchanged: an unfinished I
must be cancelled before typing F, R for the settings-file picker.

Validation passes typecheck, 318 UI unit tests, the final strict UCRT asset
build, and 62 browser checks: 35 navigation/keyboard checks plus 27 busy
checks. The first combined browser run had two busy-edit failures because
the test focused a hidden Numerics input. The helper now unfolds the section
and scrolls the field into view before editing; the unchanged busy-value
assertions pass on rerun. Logs: `build/shortcut-context-unit.log`,
`build/shortcut-context-browser-final.log` (includes the superseded failures),
`build/shortcut-context-busy.log`, and `build/shortcut-context-build-final.log`.
Browser screenshots at 1280×720 confirm the visible File context and
unfinished Integrate menu. These are browser evidence, not native OS dialog
evidence. No numerical core or native menu code changed in this follow-up.

![Visible File shortcut context](modern-navigation/shortcut-context.png)

![Unfinished Integrate menu](modern-navigation/shortcut-submenu.png)

### Initial implementation screenshots

![Desktop workspace](modern-navigation/desktop.png)

![Narrow workspace](modern-navigation/narrow.png)

![Narrow command drawer](modern-navigation/drawer.png)
