# W305: Audit of the core's mode-dependent decisions

Stage 1 study for card [W305 / #364](https://github.com/MuhammadMoustafa/xppautX/issues/364) (2026-10-11):
"A decision is made once in the core; the window, `--server` and `--silent` (and `--convert`, `--check`) differ only in how it is shown."

## 1. Search patterns and candidate counts

The audit inspected all references to mode-determining symbols and CLI flags defined in [core/xpp_batch.h](../../core/xpp_batch.h), [core/xpp_globals.h](../../core/xpp_globals.h), and [core/comline.h](../../core/comline.h). Searches across `core/` used read-only `git grep -n` for the following patterns:

- `program.interactive` (global interactive front-end flag)
- `batch_options.` (`enabled`, `yes`, `check`, `range`, `equilibria`, `out_file`, `user_out_file`, `use_intern_sets`, `intern_set_use`, `intern_sets_used`, `uses_intern_set`)
- `session.silent` and `session.generated` (headless silent run and generated silent script flags in protocol/UI session)
- `silent` (parameter/local flag in [adj2.cpp](../../core/adj2.cpp), [numerics.cpp](../../core/numerics.cpp), [ui_json.cpp](../../core/ui_json.cpp), [xppautx_main.cpp](../../core/xppautx_main.cpp))
- `MODE_` (`MODE_WINDOW`, `MODE_BROWSER`, `MODE_SERVER` in [xppautx_main.cpp](../../core/xppautx_main.cpp))
- `convert` and `check` CLI mode flags and functions in [xppautx_main.cpp](../../core/xppautx_main.cpp) and [odex_convert.cpp](../../core/odex_convert.cpp)
- `batch` (parameter/local flag in [xpp_batch.cpp](../../core/xpp_batch.cpp) and [xppautx_main.cpp](../../core/xppautx_main.cpp))
- `dryrun`, `querysets`, `querypars`, `queryics`, `newseed` (CLI flags in [comline.cpp](../../core/comline.cpp) and [json_silent.cpp](../../core/json_silent.cpp))
- `xpp::http::active()` (embedded server event routing in [json_io.cpp](../../core/json_io.cpp) and [json_player.cpp](../../core/json_player.cpp))

### Candidate occurrences per file

| File | Candidates |
|---|---:|
| [core/adj2.cpp](../../core/adj2.cpp) | 3 |
| [core/axes2.cpp](../../core/axes2.cpp) | 2 |
| [core/comline.cpp](../../core/comline.cpp) | 14 |
| [core/gear.cpp](../../core/gear.cpp) | 2 |
| [core/graphics.cpp](../../core/graphics.cpp) | 2 |
| [core/integrate.cpp](../../core/integrate.cpp) | 13 |
| [core/json_io.cpp](../../core/json_io.cpp) | 2 |
| [core/json_model.cpp](../../core/json_model.cpp) | 1 |
| [core/json_player.cpp](../../core/json_player.cpp) | 15 |
| [core/json_prompts.cpp](../../core/json_prompts.cpp) | 6 |
| [core/json_silent.cpp](../../core/json_silent.cpp) | 11 |
| [core/lunch-new.cpp](../../core/lunch-new.cpp) | 1 |
| [core/model_options.cpp](../../core/model_options.cpp) | 2 |
| [core/my_ps.cpp](../../core/my_ps.cpp) | 1 |
| [core/my_svg.cpp](../../core/my_svg.cpp) | 1 |
| [core/nullcline.cpp](../../core/nullcline.cpp) | 2 |
| [core/numerics.cpp](../../core/numerics.cpp) | 1 |
| [core/odex_convert.cpp](../../core/odex_convert.cpp) | 8 |
| [core/setx_file.cpp](../../core/setx_file.cpp) | 1 |
| [core/ui_json.cpp](../../core/ui_json.cpp) | 14 |
| [core/xpp_batch.cpp](../../core/xpp_batch.cpp) | 12 |
| [core/xpp_ui.cpp](../../core/xpp_ui.cpp) | 2 |
| [core/xpp_util.cpp](../../core/xpp_util.cpp) | 3 |
| [core/xppautx_main.cpp](../../core/xppautx_main.cpp) | 19 |
| **Total** | **128** |

*(Note: In the audit table below, contiguous variable references or shared statements in the same function are recorded in 111 distinct audit sites).*

---

## 2. Audit table of mode-dependent places

Classes:
- **presentation**: only how a result is shown or where output goes (stays; noted whether behind `xpp_ui` or front-end specific).
- **behaviour split**: a different computation, file, default, value or error in one mode (to be merged into one path).
- **W304**: who answers a question (settled under W304 / #363).
- **unsure**: requires further architectural clarification.

| Place | Code test | Difference between modes | Class |
|---|---|---|---|
| [adj2.cpp:201](../../core/adj2.cpp#L201) | `void new_h_fun(Session &s, int silent)` | Parameter suppresses formula ask in `auto_adjoint` | **W304** |
| [adj2.cpp:235](../../core/adj2.cpp#L235) | `make_h(..., int silent)` | Threaded `silent` flag bypasses coupling input | **W304** |
| [adj2.cpp:242](../../core/adj2.cpp#L242) | `if(silent==0){` | Prompts `new_string_of` if 0; uses default if 1 | **W304** |
| [axes2.cpp:99](../../core/axes2.cpp#L99) | `if(program.interactive){ re_title(s);` | Re-titles window and sets font in GUI; skipped in silent | **presentation** (stays; window UI) |
| [axes2.cpp:111](../../core/axes2.cpp#L111) | `if(program.interactive)SmallBase();` | Sets base font size in GUI; skipped in silent | **presentation** (stays; window UI) |
| [comline.cpp:123](../../core/comline.cpp#L123) | `for (name : {"--server", ...})` | Rejects single-dash spellings for front-end switches | **presentation** (stays; CLI validation) |
| [comline.cpp:172](../../core/comline.cpp#L172) | `batch_options.out_file=argv[i+1];` | Records batch output file path from `--outfile` | **presentation** (stays; batch option destination) |
| [comline.cpp:183](../../core/comline.cpp#L183) | `batch_options.use_intern_sets=...` | Sets all-vs-chosen internal sets flag for batch mode | **behaviour split** (internal set filtering) |
| [comline.cpp:248](../../core/comline.cpp#L248) | `batch_options.equilibria=...` | Sets `--equil` mode flag (0 or 1) for batch run | **behaviour split** (batch equilibrium computation) |
| [comline.cpp:290](../../core/comline.cpp#L290) | `batch_options.intern_set_use` | Configures per-set selection vector for batch run | **behaviour split** (internal set filtering) |
| [comline.cpp:376](../../core/comline.cpp#L376) | `case SILENT: batch_options.enabled=1;` | Sets batch mode flag from `--silent` switch | **presentation** (stays; mode flag setup) |
| [comline.cpp:424](../../core/comline.cpp#L424) | `case QSETS: batch_options.enabled=1;` | Forces batch mode and dryrun for `--qsets` query | **behaviour split** (query dryrun mode coupling) |
| [comline.cpp:429](../../core/comline.cpp#L429) | `case QPARS: batch_options.enabled=1;` | Forces batch mode and dryrun for `--qpars` query | **behaviour split** (query dryrun mode coupling) |
| [comline.cpp:434](../../core/comline.cpp#L434) | `case QICS: batch_options.enabled=1;` | Forces batch mode and dryrun for `--qics` query | **behaviour split** (query dryrun mode coupling) |
| [gear.cpp:297](../../core/gear.cpp#L297) | `void save_batch_shoot(Session &s)` | Integrates manifolds and writes files via `ask_output_writer` | **presentation** (stays behind `xpp_ui`) |
| [gear.cpp:542](../../core/gear.cpp#L542) | `if(program.interactive){ ch=my_abort();` | Polls keyboard abort during root find; skipped in silent | **presentation** (stays; interactive abort) |
| [graphics.cpp:287](../../core/graphics.cpp#L287) | `if(program.interactive){ create_a_pop` | Populates secondary multi-window graphs only in GUI | **behaviour split** (multi-window graph setup) |
| [graphics.cpp:988](../../core/graphics.cpp#L988) | `if(!program.interactive)return;` | Skips symbol drawing and `marks_data_equilibrium` in silent | **behaviour split** (equilibrium mark data omitted) |
| [integrate.cpp:210](../../core/integrate.cpp#L210) | `if(!program.interactive){` | Bypasses "Range Integrate" dialog; uses options directly | **presentation** (must move dialog behind `xpp_ui`) |
| [integrate.cpp:258](../../core/integrate.cpp#L258) | `if(!program.interactive){` | Bypasses 2D range dialog; uses options directly | **presentation** (must move dialog behind `xpp_ui`) |
| [integrate.cpp:693](../../core/integrate.cpp#L693) | `if(program.interactive){ show_progress` | Emits progress text in interactive mode; skipped in silent | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:747](../../core/integrate.cpp#L747) | `if(batch_options.range==1){` | Reset storage writes data only if `batch_options.range==1` | **behaviour split** (range reset storage writing) |
| [integrate.cpp:812](../../core/integrate.cpp#L812) | `if(shoot) save_batch_shoot(s);` | Writes manifolds for equilibria when requested | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:925](../../core/integrate.cpp#L925) | `if(do_range(...)!=0&&!program.interactive)` | Logs warning on range integration error in silent mode | **presentation** (stays; headless error log) |
| [integrate.cpp:1073](../../core/integrate.cpp#L1073) | `if(usual_integrate(...)!=0&&!program.interactive)` | Logs warning on incomplete integration in silent mode | **presentation** (stays; headless error log) |
| [integrate.cpp:1125](../../core/integrate.cpp#L1125) | `if(program.interactive){ auto_freeze` | Freezes curve and redraws IC inputs in interactive mode | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:1551](../../core/integrate.cpp#L1551) | `if(program.interactive) cwidth=...` | Fetches command width for progress bar in interactive | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:1700](../../core/integrate.cpp#L1700) | `if(program.interactive){ plot_command` | Draws progress bar and polls abort key in interactive | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:1823](../../core/integrate.cpp#L1823) | `if(...&&program.interactive&&OnTheFly)` | Draws on-the-fly trajectory on screen during integration | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:2052](../../core/integrate.cpp#L2052) | `if (program.interactive){set_color` | Sets active drawing color in interactive mode | **presentation** (stays behind `xpp_ui`) |
| [integrate.cpp:2098](../../core/integrate.cpp#L2098) | `if(!program.interactive){ return(0); }` | Silent halts on full storage; interactive asks `TwoChoice` | **behaviour split** (storage full handling) |
| [json_io.cpp:88](../../core/json_io.cpp#L88) | `if (xpp::http::active()) {` | Routes protocol event: HTTP SSE vs stdout stream | **presentation** (stays; event sink routing) |
| [json_io.cpp:92](../../core/json_io.cpp#L92) | `if (!proto) return;` | Suppresses protocol event emission in silent mode | **presentation** (stays; event sink routing) |
| [json_model.cpp:67](../../core/json_model.cpp#L67) | `program.interactive = 1;` | Sets interactive flag when starting session model | **presentation** (stays; front-end state) |
| [json_player.cpp:283](../../core/json_player.cpp#L283) | `if (session.silent ...` | Pacing delay is 0 in silent; pauses for UI speed in GUI | **presentation** (stays; replay timing) |
| [json_player.cpp:332](../../core/json_player.cpp#L332) | `if (session.silent) quit_session();` | Quits immediately on diverged replay in silent mode | **presentation** (stays; replay lifecycle) |
| [json_player.cpp:543](../../core/json_player.cpp#L543) | `session.silent ? output_write : nullptr` | Restricts file writes to launch folder during silent replay | **behaviour split** (silent replay write sandboxing) |
| [json_player.cpp:595](../../core/json_player.cpp#L595) | `player.wait_for_page = ...` | Replay waits for browser page connect in HTTP mode | **presentation** (stays; front-end readiness) |
| [json_player.cpp:596](../../core/json_player.cpp#L596) | `load(s, 0, play || session.silent);` | Begins playback immediately upon load in silent mode | **presentation** (stays; autoplay trigger) |
| [json_player.cpp:700](../../core/json_player.cpp#L700) | `if (player.open && !session.silent)` | Pauses replay on user command in interactive mode | **presentation** (stays; replay pause) |
| [json_player.cpp:746](../../core/json_player.cpp#L746) | `json::session.silent ? output_write` | Restricts recording launch file writes in silent mode | **behaviour split** (silent replay write sandboxing) |
| [json_player.cpp:748](../../core/json_player.cpp#L748) | `json::session.silent ? output_folder : temp` | Working directory is launch folder in silent, temp in GUI | **behaviour split** (recording working directory) |
| [json_player.cpp:787](../../core/json_player.cpp#L787) | `session.silent ? output_write : nullptr` | Installs write sandboxing guard during step in silent | **behaviour split** (silent replay write sandboxing) |
| [json_player.cpp:828](../../core/json_player.cpp#L828) | `if (session.silent) report_automatic` | Logs automatic save decision to stderr in silent mode | **presentation** (stays; headless reporting) |
| [json_player.cpp:850](../../core/json_player.cpp#L850) | `if (session.silent) report_automatic` | Logs automatic key answer to stderr in silent mode | **presentation** (stays; headless reporting) |
| [json_player.cpp:857](../../core/json_player.cpp#L857) | `if (session.silent) report_automatic` | Logs automatic value answer to stderr in silent mode | **presentation** (stays; headless reporting) |
| [json_player.cpp:898](../../core/json_player.cpp#L898) | `if (session.silent) xpp::inbox::close` | Closes inbox to exit at end of recording in silent | **presentation** (stays; replay lifecycle) |
| [json_player.cpp:911](../../core/json_player.cpp#L911) | `if (session.silent) xpp::inbox::close` | Closes inbox on recording load failure in silent | **presentation** (stays; replay lifecycle) |
| [json_player.cpp:918](../../core/json_player.cpp#L918) | `if (session.silent && empty()) close` | Closes inbox if recording has no steps in silent | **presentation** (stays; replay lifecycle) |
| [json_prompts.cpp:52](../../core/json_prompts.cpp#L52) | `if (session.generated) {` | Captures open prompt for silent script diagnostic trace | **presentation** (stays; diagnostic reporting) |
| [json_prompts.cpp:60](../../core/json_prompts.cpp#L60) | `if (session.silent && !ask_user)` | Rejects GUI-only ask (e.g. pixels) in silent mode | **presentation** (stays; headless ask rejection) |
| [json_prompts.cpp:66](../../core/json_prompts.cpp#L66) | `if (session.generated) advance()` | Advances generated script queue on question prompt | **presentation** (stays; script input pump) |
| [json_prompts.cpp:108](../../core/json_prompts.cpp#L108) | `if (session.generated) silent_fail` | Stops process if generated script mismatches dialogue | **presentation** (stays; script dialogue integrity) |
| [json_prompts.cpp:111](../../core/json_prompts.cpp#L111) | `if (!session.generated ...)` | Allows setting commands during ask in interactive mode | **presentation** (stays; interactive command queue) |
| [json_prompts.cpp:134](../../core/json_prompts.cpp#L134) | `if (session.silent) { log(...); return; }` | Writes error to log in silent; emits event in GUI | **presentation** (stays behind `xpp_ui`) |
| [json_silent.cpp:112](../../core/json_silent.cpp#L112) | `ran = !dryrun && uses_intern_set(...)` | Integrates only for selected internal sets or non-dryrun | **behaviour split** (silent internal set execution plan) |
| [json_silent.cpp:117](../../core/json_silent.cpp#L117) | `batch_options.out_file = name + ".dat"` | Derives output file name per internal set if no `--outfile` | **behaviour split** (silent per-set output naming) |
| [json_silent.cpp:122](../../core/json_silent.cpp#L122) | `if (!dryrun) return; command("query")` | Emits query command in dry-run mode | **behaviour split** (query dryrun execution plan) |
| [json_silent.cpp:128](../../core/json_silent.cpp#L128) | `batch_options.range == 1` | Generates range run command if batch range enabled | **behaviour split** (silent range command selection) |
| [json_silent.cpp:140](../../core/json_silent.cpp#L140) | `if (!batch_options.range || reset == 0)` | Writes output file unless range reset storage already wrote | **behaviour split** (silent output write condition) |
| [json_silent.cpp:148](../../core/json_silent.cpp#L148) | `command("browser", "write", ...)` | Generates protocol command to save output data | **presentation** (stays; silent script generation) |
| [json_silent.cpp:168](../../core/json_silent.cpp#L168) | `if (sets == 0 || used == 0) add_run(-1)` | Runs base model when no internal sets selected | **behaviour split** (silent execution plan) |
| [json_silent.cpp:172](../../core/json_silent.cpp#L172) | `if (nc_batch != 2 ...)` | Generates script commands to compute/save nullclines | **behaviour split** (silent batch nullcline generation) |
| [json_silent.cpp:181](../../core/json_silent.cpp#L181) | `if ((df != 4 && df != 5) ...)` | Generates script commands to compute/save direction field | **behaviour split** (silent batch dfield generation) |
| [json_silent.cpp:189](../../core/json_silent.cpp#L189) | `if (equilibria < 0) return;` | Generates script command to compute/save equilibria | **behaviour split** (silent batch equilibrium generation) |
| [lunch-new.cpp:629](../../core/lunch-new.cpp#L629) | `batch_options.uses_intern_set(i)` | Outputs 0 or 1 for internal set use in values query | **presentation** (stays; query output formatting) |
| [model_options.cpp:672](../../core/model_options.cpp#L672) | `whole = &batch_options.range;` | `@ range=` option bound to global `batch_options` struct | **behaviour split** (batch option bound to global) |
| [model_options.cpp:679](../../core/model_options.cpp#L679) | `batch_options.out_file = path;` | `@ output=` option mutates global `batch_options` struct | **behaviour split** (batch option bound to global) |
| [my_ps.cpp:201](../../core/my_ps.cpp#L201) | `if(program.interactive)init_x11(s);` | Reinitializes X11 context after PS export; skipped in silent | **presentation** (stays; window drawing context) |
| [my_svg.cpp:309](../../core/my_svg.cpp#L309) | `if(program.interactive)init_x11(s);` | Reinitializes X11 context after SVG export; skipped in silent | **presentation** (stays; window drawing context) |
| [nullcline.cpp:498](../../core/nullcline.cpp#L498) | `if(!batch_options.enabled)return;` | Skips batch nullcline draw in plot restore if not batch | **behaviour split** (plot restore nullclines) |
| [nullcline.cpp:513](../../core/nullcline.cpp#L513) | `if(!batch_options.enabled)return;` | Skips batch direction field draw in plot restore if not batch | **behaviour split** (plot restore direction fields) |
| [numerics.cpp:278](../../core/numerics.cpp#L278) | `new_h_fun(s,1);` | Passes `silent=1` to bypass coupling ask in `auto_adjoint` | **W304** |
| [odex_convert.cpp:627](../../core/odex_convert.cpp#L627) | `if (declined_ == DeclinedRename::refuse)` | Accepts rename suggestion in check; refuses in convert | **W304** |
| [odex_convert.cpp:644](../../core/odex_convert.cpp#L644) | `refuse(... nobody can answer ...)` | Refuses conversion if rename question is unanswered | **W304** |
| [odex_convert.cpp:1728](../../core/odex_convert.cpp#L1728) | `int check_file(const string &file)` | CLI entry point for `--check` mode | **presentation** (stays; CLI mode entry) |
| [odex_convert.cpp:1739](../../core/odex_convert.cpp#L1739) | `convert_text(..., DeclinedRename::report)` | Runs conversion inspection without saving file | **W304** |
| [odex_convert.cpp:1765](../../core/odex_convert.cpp#L1765) | `/* This command owns stdout ... */` | Writes check diagnostics directly to stdout | **presentation** (stays; CLI output) |
| [odex_convert.cpp:1872](../../core/odex_convert.cpp#L1872) | `choice = file_replace_choice(true);` | Prompts file replace choice when opening `.ode` | **W304** |
| [odex_convert.cpp:1889](../../core/odex_convert.cpp#L1889) | `int convert_file(const string &ode)` | CLI entry point for `--convert` mode | **presentation** (stays; CLI mode entry) |
| [odex_convert.cpp:1898](../../core/odex_convert.cpp#L1898) | `if (... && !confirm(... replace it))` | Prompts confirm replace in standalone `--convert` mode | **W304** |
| [setx_file.cpp:638](../../core/setx_file.cpp#L638) | `if (redraw && program.interactive)` | Redraws UI inputs after applying `.setx` in GUI | **presentation** (stays behind `xpp_ui`) |
| [ui_json.cpp:68](../../core/ui_json.cpp#L68) | `session.silent ? xpp::log_exit_code() : 0` | Silent returns error count on exit; GUI returns 0 | **presentation** (stays; process exit code) |
| [ui_json.cpp:88](../../core/ui_json.cpp#L88) | `if (session.silent ... !player_finished)` | Fails exit if replay did not finish steps in silent | **presentation** (stays; replay verification) |
| [ui_json.cpp:342](../../core/ui_json.cpp#L342) | `answer_policy(session.silent, ...)` | Delegates UI question answering policy | **W304** |
| [ui_json.cpp:354](../../core/ui_json.cpp#L354) | `session.silent && !session.generated ...` | Defaults unrecorded save questions to replace in silent replay | **behaviour split** (silent replay save replacement) |
| [ui_json.cpp:515](../../core/ui_json.cpp#L515) | `if (session.silent) { show_error; return; }` | Silent logs error and suppresses `file` protocol event | **presentation** (stays behind `xpp_ui`) |
| [ui_json.cpp:589](../../core/ui_json.cpp#L589) | `if (session.generated) silent_fail` | Mismatched answer command terminates silent script | **presentation** (stays; script dialogue integrity) |
| [ui_json.cpp:869](../../core/ui_json.cpp#L869) | `if (session.silent && !session.generated)` | Preserves launch output folder on model switch in silent | **behaviour split** (silent replay output folder redirection) |
| [ui_json.cpp:905](../../core/ui_json.cpp#L905) | `if (session.generated) advance()` | Advances generated script queue after command | **presentation** (stays; script input pump) |
| [ui_json.cpp:951](../../core/ui_json.cpp#L951) | `else if (session.generated) advance()` | Advances generated script queue after abort | **presentation** (stays; script input pump) |
| [ui_json.cpp:977](../../core/ui_json.cpp#L977) | `if (!silent && !http::active())` | Opens stdout protocol and stdin reader for `--server` | **presentation** (stays; protocol channel setup) |
| [ui_json.cpp:996](../../core/ui_json.cpp#L996) | `if (silent) { ui.err_msg = nullptr; ... }` | Clears UI table hooks so headless defaults handle output | **presentation** (stays behind `xpp_ui`) |
| [ui_json.cpp:1009](../../core/ui_json.cpp#L1009) | `session.silent = silent;` | Sets silent session flag and records working dir | **presentation** (stays; front-end installation) |
| [ui_json.cpp:1022](../../core/ui_json.cpp#L1022) | `session.generated = true; silent = true;` | Configures session flags for `json_ui_silent` | **presentation** (stays; front-end mode entry) |
| [ui_json.cpp:1033](../../core/ui_json.cpp#L1033) | `program.interactive = 1;` | Sets interactive flag for GUI start screen | **presentation** (stays; front-end mode entry) |
| [xpp_batch.cpp:107](../../core/xpp_batch.cpp#L107) | `program.interactive = 0;` | Clears interactive flag during model load setup | **presentation** (stays; load setup state) |
| [xpp_batch.cpp:108](../../core/xpp_batch.cpp#L108) | `batch_options.out_file = "output.dat";` | Resets default batch output file name on load | **behaviour split** (global batch option default) |
| [xpp_batch.cpp:112](../../core/xpp_batch.cpp#L112) | `if (batch) batch_options.enabled = 1;` | Forces `batch_options.enabled` if batch argument set | **behaviour split** (global batch mode enablement) |
| [xpp_batch.cpp:163](../../core/xpp_batch.cpp#L163) | `batch_before = batch_options;` | Restores global batch options on load failure/rollback | **behaviour split** (global batch state management) |
| [xpp_batch.cpp:203](../../core/xpp_batch.cpp#L203) | `load_in(..., int batch, ...)` | Threads `batch` flag through model loader | **behaviour split** (batch argument thread-through) |
| [xpp_batch.cpp:224](../../core/xpp_batch.cpp#L224) | `load_model(..., int batch, ...)` | Entry argument forcing batch mode for silent load | **behaviour split** (batch argument thread-through) |
| [xpp_batch.cpp:237](../../core/xpp_batch.cpp#L237) | `if (batch) batch_options.enabled = 1;` | Sets batch mode in prepared load phase | **behaviour split** (global batch mode enablement) |
| [xpp_batch.cpp:276](../../core/xpp_batch.cpp#L276) | `if (batch || !is_no_model(...))` | Silent logs missing model as error; GUI shows start screen | **presentation** (stays; error log routing) |
| [xpp_batch.cpp:280](../../core/xpp_batch.cpp#L280) | `if (batch) log(...) else bottom_msg(...)` | Silent logs conversion to stderr; GUI shows `bottom_msg` | **presentation** (stays behind `xpp_ui`) |
| [xpp_ui.cpp:72](../../core/xpp_ui.cpp#L72) | `if (batch_options.check) return None;` | `--check` sets question policy to leave unanswered | **W304** |
| [xpp_ui.cpp:74](../../core/xpp_ui.cpp#L74) | `if (batch_options.yes) return Suggested;` | `-y`/`--yes` sets question policy to auto-suggest | **W304** |
| [xpp_util.cpp:329](../../core/xpp_util.cpp#L329) | `if(program.interactive){ redraw_dfield` | PS export draws nullclines/dfield only in interactive | **behaviour split** (PS plot content discrepancy) |
| [xpp_util.cpp:342](../../core/xpp_util.cpp#L342) | `if(program.interactive){ draw_label` | PS export draws labels and frozen curves only in interactive | **behaviour split** (PS plot content discrepancy) |
| [xpp_util.cpp:356](../../core/xpp_util.cpp#L356) | `if(program.interactive){ draw_label` | SVG export draws labels and frozen curves only in interactive | **behaviour split** (SVG plot content discrepancy) |
| [xppautx_main.cpp:154](../../core/xppautx_main.cpp#L154) | `bool silent = false;` | Parses `--silent` for session runner UI installation | **presentation** (stays; front-end setup) |
| [xppautx_main.cpp:189](../../core/xppautx_main.cpp#L189) | `if (silent) std::erase(args, "--silent");` | Strips `--silent` before passing args to model loader | **presentation** (stays; CLI arg cleanup) |
| [xppautx_main.cpp:214](../../core/xppautx_main.cpp#L214) | `load_model(..., 0, ...)` | Passes `batch=0` in interactive/server runner | **presentation** (stays; front-end setup) |
| [xppautx_main.cpp:216](../../core/xppautx_main.cpp#L216) | `if (!loaded && !silent && is_no_model)` | Opens start screen in GUI; fails in silent | **presentation** (stays; GUI start screen) |
| [xppautx_main.cpp:222](../../core/xppautx_main.cpp#L222) | `if (!silent) json_ui_load_error(...)` | Emits load error event in GUI; logs in silent | **presentation** (stays behind `xpp_ui`) |
| [xppautx_main.cpp:227](../../core/xppautx_main.cpp#L227) | `if (!silent) { recent::note(...) }` | Records model in recent files list in GUI; skipped in silent | **presentation** (stays; GUI recent list) |
| [xppautx_main.cpp:274](../../core/xppautx_main.cpp#L274) | `mode = MODE_BROWSER / MODE_SERVER` | Parses `--browser`, `--web`, `--server` | **presentation** (stays; main mode selection) |
| [xppautx_main.cpp:289](../../core/xppautx_main.cpp#L289) | `if (strcmp(argv[i], "--silent") == 0) batch=1;` | Parses `--silent` in main argument loop | **presentation** (stays; main mode selection) |
| [xppautx_main.cpp:295](../../core/xppautx_main.cpp#L295) | `if (check && !convert ...)` | Dispatches to `odex::check_file` | **presentation** (stays; main mode dispatch) |
| [xppautx_main.cpp:307](../../core/xppautx_main.cpp#L307) | `xpp::batch_options.yes = yes != 0;` | Sets global `yes` flag from `-y` / `--yes` | **W304** |
| [xppautx_main.cpp:309](../../core/xppautx_main.cpp#L309) | `if (convert) return convert_file(...)` | Dispatches to `odex::convert_file` | **presentation** (stays; main mode dispatch) |
| [xppautx_main.cpp:318](../../core/xppautx_main.cpp#L318) | `recording_batch = batch != 0;` | Detects batch replay of `.recx` file | **presentation** (stays; main mode selection) |
| [xppautx_main.cpp:319](../../core/xppautx_main.cpp#L319) | `if (batch && !recording_batch)` | Dispatches to `json_ui_silent` | **presentation** (stays; main mode dispatch) |
| [xppautx_main.cpp:331](../../core/xppautx_main.cpp#L331) | `if (mode == MODE_WINDOW ...)` | Falls back to browser if window unsupported or `--no-open` | **presentation** (stays; GUI display fallback) |
| [xppautx_main.cpp:333](../../core/xppautx_main.cpp#L333) | `if (recording_batch) mode = MODE_SERVER;` | Runs batch recording replay under server protocol mode | **presentation** (stays; replay mode dispatch) |
| [xppautx_main.cpp:334](../../core/xppautx_main.cpp#L334) | `if (mode != MODE_SERVER) { http::start }` | Starts embedded HTTP daemon for window/browser mode | **presentation** (stays; daemon startup) |
| [xppautx_main.cpp:343](../../core/xppautx_main.cpp#L343) | `if (mode == MODE_WINDOW ...)` | Launches native window or shows browser page | **presentation** (stays; GUI launcher) |

---

## 3. Behaviour splits and proposed follow-up cards

The 34 behaviour split sites fall into 7 logical groups. Each group represents a core operational difference between modes that should be unified under a single code path.

### Proposed Card 1: Unify vector plot export contents across all modes (PS and SVG)
- **User operation**: Exporting PostScript or SVG via `--mkplot` (`--silent`), protocol command (`--server`), or File > Export (GUI).
- **Places**: [core/xpp_util.cpp:329](../../core/xpp_util.cpp#L329), [core/xpp_util.cpp:342](../../core/xpp_util.cpp#L342), [core/xpp_util.cpp:356](../../core/xpp_util.cpp#L356), [core/nullcline.cpp:498](../../core/nullcline.cpp#L498), [core/nullcline.cpp:513](../../core/nullcline.cpp#L513).
- **Split**: Labels and frozen curves are omitted in `--silent`; batch nullclines and direction fields are rendered only if `batch_options.enabled`.
- **Target**: Render identical plot elements (curves, labels, nullclines, direction fields) across `--silent`, `--server`, and window modes.
- **Enforcing check**: `goldencheck.py` and `servercheck.py` (comparing PS and SVG byte output across modes).

### Proposed Card 2: Unify storage-full limit handling via `xpp_ui`
- **User operation**: Integrating a model whose trajectory exceeds the allocated row storage limit.
- **Places**: [core/integrate.cpp:2098](../../core/integrate.cpp#L2098).
- **Split**: In `--silent`, integration stops unconditionally with a warning; in interactive and `--server` mode, it asks `TwoChoice` ("Storage full: Overwrite?").
- **Target**: Remove the mode branch in `stor_full`. Route the overwrite decision through `TwoChoice` in all modes; `--silent`'s answering policy (under W304) will decline or overwrite automatically according to `-y`/`--yes` or default script policy.
- **Enforcing check**: `servercheck.py` (verifying identical overflow behavior in server and silent modes) and `examples_check.sh`.

### Proposed Card 3: Decouple multi-window plot graphs and equilibrium mark data from interactive display
- **User operation**: Loading models with multi-window plot configurations (`multi_win != 0`) and finding equilibria in headless runs.
- **Places**: [core/graphics.cpp:287](../../core/graphics.cpp#L287), [core/graphics.cpp:988](../../core/graphics.cpp#L988).
- **Split**: Secondary graph windows in `plot_windows.graph` are only created and initialized if `program.interactive` is true; `marks_data_equilibrium` records data points only if `program.interactive` is true.
- **Target**: Initialize graph settings structures in `Session::plot_windows` and store equilibrium mark data unconditionally, leaving only window surface allocation to the GUI front end.
- **Enforcing check**: `servercheck.py` (querying graph definitions and mark points in headless mode).

### Proposed Card 4: Move `XppBatchOptions` into `Session` and eliminate global batch option state
- **User operation**: Setting `@ range=`, `@ output=`, `--outfile`, and `--internset`.
- **Places**: [core/comline.cpp:172-183](../../core/comline.cpp#L172-L183), [core/comline.cpp:248-297](../../core/comline.cpp#L248-L297), [core/model_options.cpp:672](../../core/model_options.cpp#L672), [core/model_options.cpp:679](../../core/model_options.cpp#L679), [core/xpp_batch.cpp:108-112](../../core/xpp_batch.cpp#L108-L112), [core/xpp_batch.cpp:163-195](../../core/xpp_batch.cpp#L163-L195), [core/xpp_batch.cpp:203-237](../../core/xpp_batch.cpp#L203-L237).
- **Split**: `batch_options` is a global variable mutated by model options and command-line parsing, requiring manual rollback in `in_model`.
- **Target**: Place batch execution settings inside `Session` (e.g. `Session::batch`), adhering to AGENTS.md's "No global state" rule. Eliminate `batch` integer parameters across `load_model` and `load_in`.
- **Enforcing check**: `sourcecheck.sh` (`globalcheck.py`), `servercheck.py`.

### Proposed Card 5: Unify range integration storage-reset data export
- **User operation**: Running range integration with storage reset (`res == 1`).
- **Places**: [core/integrate.cpp:747](../../core/integrate.cpp#L747), [core/json_silent.cpp:128](../../core/json_silent.cpp#L128), [core/json_silent.cpp:140](../../core/json_silent.cpp#L140).
- **Split**: Intermediate range sweeps with reset storage write per-run output files (`write_this_run`) only if `batch_options.range == 1`. In `--server` or GUI, intermediate runs are discarded without writing unless `@ range=1` was declared in the ODE file.
- **Target**: Provide a uniform setting on `RangeVars` that governs whether each reset sweep writes output files in all modes.
- **Enforcing check**: `servercheck.py` (range sweep execution with reset storage across modes).

### Proposed Card 6: Harmonize recording replay output sandbox policy
- **User operation**: Replaying a `.recx` recording via `--silent` vs via GUI / `--server`.
- **Places**: [core/ui_json.cpp:354](../../core/ui_json.cpp#L354), [core/ui_json.cpp:869](../../core/ui_json.cpp#L869), [core/json_player.cpp:543](../../core/json_player.cpp#L543), [core/json_player.cpp:746](../../core/json_player.cpp#L746), [core/json_player.cpp:748](../../core/json_player.cpp#L748), [core/json_player.cpp:787](../../core/json_player.cpp#L787).
- **Split**: Replaying a recording in `--silent` intercepts writes with `output_write`, forces the working folder to the launch folder across model switches, and defaults unrecorded save decisions to `SAVE_REPLACE`. Interactive replay isolates writes in a temporary directory and defaults unrecorded saves to `SAVE_ASK`.
- **Target**: Define one shared execution policy for replay sandboxing and save decisions regardless of whether an interface is attached.
- **Enforcing check**: `servercheck.py` (verifying `.recx` output files and model switches between `--silent` and `--server`).

### Proposed Card 7: Promote batch queries and auxiliary data output to first-class core operations
- **User operation**: Command-line queries (`--qsets`, `--qpars`, `--qics`) and auxiliary batch data generation (`--equil`, `@ ncdraw=2`, `@ dfdraw=4|5`).
- **Places**: [core/comline.cpp:424-436](../../core/comline.cpp#L424-L436), [core/json_silent.cpp:112-124](../../core/json_silent.cpp#L112-L124), [core/json_silent.cpp:168-190](../../core/json_silent.cpp#L168-L190).
- **Split**: Auxiliary batch tasks and queries are implemented as bespoke synthetic JSON commands in `json_silent.cpp` tied to `dryrun` and `batch_options` flags.
- **Target**: Unify query and auxiliary export routines in the core so that `--silent`, `--server`, and the web front end share the exact same dispatch and output generation.
- **Enforcing check**: `servercheck.py` (`check_outside_paths` and values query tests).

---

## 4. Verification of the two known splits

### Split 1: W273's `@ OUTPUT` read as the number 0 in `--silent` only
- **Status**: **Fixed** in commit `682d39bd` (W273 / [#330](https://github.com/MuhammadMoustafa/xppautX/issues/330), 2026-10-11).
- **Where it was**: In [core/model_options.cpp](../../core/model_options.cpp), `@ OUTPUT=filename` was previously registered as an integer/numerical option, parsing the filename as 0. In `--silent`, the data writer subsequently wrote to a file named `0`.
- **Where it is now**: In [core/model_options.cpp:673](../../core/model_options.cpp#L673), `OUTPUT` is registered as `OptionKind::path` in `option_group::output`. It parses the file via `own_file(s, v, path)` and `xpp::files::path_within`, ensuring the filename is inside the model folder and setting `batch_options.out_file`. The bug is no longer present.

### Split 2: W286/W297's outside path asked twice
- **Status**: **Fixed** in commit `2d2f02dd` (W297 / [#355](https://github.com/MuhammadMoustafa/xppautX/issues/355), 2026-10-11).
- **Where it was**: When opening a `.ode` file containing an outside path (e.g. `@ output=../data.dat` or an outside table), [core/xpp_batch.cpp](../../core/xpp_batch.cpp) first converted the `.ode` via `odex::open_ode`, prompting the user for approval. Immediately afterward, `load_model` loaded the generated `.odex` file, prompting the user a second time for the same path.
- **Where it is now**: In [core/odex_convert.cpp:1882](../../core/odex_convert.cpp#L1882) and [core/xpp_batch.cpp:250,271](../../core/xpp_batch.cpp#L250), `odex::open_ode` collects approved `outside_paths` and transfers them directly into `load_in`, populating `Model::outside_paths`. Consequently, the `.odex` load recognizes the prior approval and does not prompt a second time. Verified by `servercheck.py:check_outside_paths` ("W297: .ode outside output is asked exactly once").

## 5. Review (coordinator, 2026-10-14)

Six rows were checked against the code (xpp_util.cpp:329, graphics.cpp:287 and
:988, integrate.cpp:2098, nullcline.cpp:498, json_player.cpp:748): each quotes
the right line. The classification of the proposed cards is corrected as follows.

- **Behaviour splits that stand**: cards 1 (a PS or SVG export holds different
  elements by mode), 2 (a full storage stops in --silent, asks elsewhere), 3
  (multi-window graphs and equilibrium marks exist only with a front end) and
  5 (a range run with reset writes each run's file only under `@ range=1`, a
  setting that is the run's, not the mode's). Each becomes a card.
- **Card 4 is not a mode split** but a "No global state" finding: `@ range`
  and `@ output` are a load's values written into the process-wide
  `batch_options`, with a manual rollback on a failed load. It becomes its own
  card (the model's options in the Model), not part of W305.
- **Card 6 is presentation and W304**: where a replay writes (the launch
  folder in --silent, the player's temp folder with a front end) is where output
  goes; the replace default for an unrecorded save in --silent is who answers
  (W304). No card.
- **Card 7 is the --silent front end's own**: json_silent.cpp is that front
  end's built-in script, and `--qsets`/`--qpars`/`--qics`/`--equil` are its
  command-line options. Whether the window should offer the same queries is a
  feature request, not a split. No card.
