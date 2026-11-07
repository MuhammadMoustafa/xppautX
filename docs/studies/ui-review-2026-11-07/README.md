# UI review, 2026-11-07: mockups

The maintainer's UI review settled each design from mockups of the real page
(AGENTS.md: a UI decision is made by looking at it). Each mockup is the running
page (`xppautX --no-open`, examples/ode/lecar.odex, run once) with CSS and a
script injected through the DevTools protocol, so it shows the real data, fonts
and tokens. The page in these pictures predates the fix of the "Â·" hint.

| Pictures | Decision | Card |
|---|---|---|
| `B_*_now_vs_B.jpg` (390, 768, 1024x700, 1280, 1440, 1280 dark): today left, option B right | the plot first on every screen | W639 (#711) |
| `numbers_values_1024.jpg`, `copy_tip_variants.jpg` (B chosen), `number_format_auto_vs_sci.jpg` (automatic chosen), `names_and_copied.jpg` | numbers and names cut with …, full on hover, click copies with a green tick | W640 (#712), W639 (#711) |
| `dialogs_today.jpg`, `dialogs_today_vs_one_shell.jpg` | the dialogs today, and a first shell | W641 (#713) |
| `form_settings_1280.jpg`, `forms_desktop.jpg`, `forms_phone.jpg` | the form and dialog system | W641 (#713) |
| `panels_table_now_vs_dock.jpg`, `panels_autox_tabs.jpg` | the panels as one dock, the views as tabs | W644 |

`mockup-sources/` holds what made them, for the cards' agents to read (not
code to copy: the cards build each part in its owner):
- `mock.mjs` shoots the page at each width, before and after a CSS and script
  (`node mock.mjs ./xppautX WORKDIR CSS JS`, the model copied into WORKDIR;
  `SIZES='[[w,h],...]'` narrows the widths); `b.css`/`b.js` are option B,
  `n.css`, `m.css` and `nm_copied.js` the number and name display;
- `dialogs.mjs` opens each dialog and shoots it (`d.css`/`d.js`: the first shell);
- `forms.mjs` with `f.css`/`f.js` is the form system;
- `panels.mjs` with `p.css`/`p.js` (over `b.css`/`b.js`) is the dock and the view tabs.
