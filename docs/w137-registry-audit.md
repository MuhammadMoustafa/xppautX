# W137 registry audit — issue [#189](https://github.com/MuhammadMoustafa/xppautX/issues/189)

Searched the common owners (`xpp_io`, `xpp_files`, `xpp_log`, `xpp_error`,
`xpp_util`, `xpp_mem`, `xpp_ui`, `xpp_math`, Model and Session), then all
of `core/`, for table reading/writing, CSV quoting, colour/group drawing,
pixel encoding, movie completion and save commit/abort. Reused `DataFormat`,
`DataTable`, `append_csv_field`, `data_number`, `Tokens`, `LineReader`,
`ImageFormat`, the PS/SVG/GIF encoders, and W129's
`open_writer_asking`/`commit_save`/`abort_save`. No second save path.

## What moved

- `json_windows.cpp`: still GIF/PPM encoders and palette reduction moved to
  `image_format.cpp`'s GIF/PPM rows. Kinescope, array pictures and range movies
  call the rows' pixel/movie hooks. `json_ani.cpp` uses the same hooks.
- `arrayplot.cpp`: PostScript printing calls the PS row's array hook;
  `array_print.cpp` remains the array PS encoder, registered once.
- `integrate.cpp`: both SVG group branches and the literal-1 PS/SVG colour
  branch use registry hooks. `nullcline.cpp`'s five SVG branches use the
  group hook, including direction-field state.
- `axes2.cpp`: SVG axis-label dispatch moved to the row's label hook.
  `graf_par.cpp`: PS key direction and PS/SVG filename/title dispatch moved
  to row metadata/hooks. `graphics.cpp`'s format-mode switch moved into
  the registry's mode lookup.
- The complete grep also found `json_silent.cpp`'s PS/SVG batch branch;
  it now uses row menu keys and parameter values. `my_ps.cpp` owns one
  parameter-values builder used by its dialog and the batch script.
- AUTO `csv_export.cpp` now builds mixed-field DataTables, preserving
  double precision, and uses the CSV row's binary writer and one quoter.
  Numeric-only data formats refuse mixed-field tables. Fit and diagram
  import use `read_data_table`, which selects `data_format_of_file`.
- Range movie cleanup aborts the Writer after any incomplete sweep,
  integration failure, cancelled capture or Stop; only completion commits.

## Remaining format-name and extension matches

The case-insensitive scan below covers format names, PSFMT/SVGFMT,
quoted extensions and dotted extensions. All matches in `my_ps.*`,
`my_svg.*`, `array_print.*` and `scrngif.*` belong to encoders; format
behaviour dispatch is in `image_format.*`. Other matches are registry
indices selected by menus, protocol/menu names, the batch default,
format-specific settings, or explanatory comments; none is a per-format
writing/drawing branch outside the registry.

| File | Matching lines |
| --- | --- |
| `core/aniparse.cpp` | 1060 |
| `core/aniparse.h` | 31 |
| `core/array_print.cpp` | 3, 4, 68 |
| `core/array_print.h` | 7 |
| `core/arrayplot.h` | 48 |
| `core/auto_nox.cpp` | 314 |
| `core/auto_nox.h` | 129 |
| `core/colormap.cpp` | 1 |
| `core/diagram.cpp` | 203, 394, 396, 397 |
| `core/diagram.h` | 72 |
| `core/graf_par.cpp` | 731, 732 |
| `core/graf_par.h` | 18, 19, 127, 129 |
| `core/graphics.h` | 20, 22 |
| `core/image_format.cpp` | 3, 6, 7, 27, 29, 34, 50, 67, 69, 70, 72, 73, 76 |
| `core/image_format.h` | 20, 22, 26, 28, 29, 33, 34, 50, 77 |
| `core/integrate.cpp` | 1871 |
| `core/json_ani.cpp` | 95, 102, 103, 106, 112, 115, 118, 122, 126, 127, 141, 142, 143 |
| `core/json_silent.cpp` | 27 |
| `core/json_windows.cpp` | 2, 277, 505, 510, 582 |
| `core/lunch-new.h` | 120 |
| `core/menus.cpp` | 189, 197, 198, 200, 580, 592, 645, 646, 659 |
| `core/my_ps.cpp` | 2, 5, 116, 385 |
| `core/my_ps.h` | 17, 18, 26, 29, 36, 43, 59 |
| `core/my_svg.cpp` | 2, 5, 39, 43, 234, 271, 280, 296, 304 |
| `core/my_svg.h` | 16 |
| `core/nullcline.cpp` | 574 |
| `core/nullcline.h` | 20, 79 |
| `core/scrngif.cpp` | 1, 2, 3, 21, 22, 44, 82, 89, 402, 403, 404, 411, 415, 421 |
| `core/scrngif.h` | 12, 14 |
| `core/xpp_batch.cpp` | 104 |
| `core/xpp_files.cpp` | 466, 467 |
| `core/xpp_files.h` | 106 |
| `core/xpp_ui.h` | 262 |
| `core/xppautx_main.cpp` | 124 |

## Security and review scope

Readers parse the whole table before fit/import applies it, reject ragged
or malformed text rows, and retain source lines for errors. Fit validates
column indices and finite values; import checks integer bounds before casts.
Pixel allocation keeps the existing 8192 dimension cap and rejects short
payloads. Saves retain W129's permission owner and temp/rename Writer.
Text readers retain their existing whole-file/table allocation behaviour;
a genuinely large input can still exhaust memory. This existing weakness
is unchanged; the card adds no allocation from a claimed file size.

No `.ode` grammar changes, so `odex-quirks.md` is unaffected. CSV exports
and registry completion are xppautX features; no new claim about a bug in
XPPAUT's source is made in `xppaut-findings.md`.
Reviewer web2check sections: `phase`, `marks`, `aplot`, `ani`, `kinescope`,
`files`, `auto`, `runs`, `help`. The agent does not run web2check.

## Gate results after the W166 rebase

Native `WERROR=1` build with `-j2`: zero warnings; 29 unit groups,
21,326 checks, zero failures. Servercheck: 658 passed; autocheck: 187
passed. Goldencheck (`--bin`, its supported binary option): six files
byte-identical, no baseline updates. Examples with `JOBS=1`: 184 models,
183 unchanged output MD5s and one model with no output by design.
Web2 build/typecheck and sleepcheck passed. The intentional byte change
is AUTO CSV's LF line endings on Windows, checked in autocheck; it
requires no golden baseline change. Native gates used a worktree-local
TEMP/TMP directory to avoid shared temporary-directory interference.
