# UI review, 2026-11-07: mockups

The maintainer's UI review settled each design from mockups of the real page
(AGENTS.md: a UI decision is made by looking at it). Each mockup is the running
page (`xppautX --no-open`, examples/ode/lecar.odex, run once) with CSS and a
script injected through the DevTools protocol, so it shows the real data, fonts
and tokens. The page in these pictures predates the fix of the "Â·" hint.
Discussion #718 records the decisions.

| Pictures | Decision | Card |
|---|---|---|
| `B_*_now_vs_B.jpg` (390, 768, 1024x700, 1280, 1440, 1280 dark): today left, option B right | the plot first on every screen | W639 (#711) |
| `numbers_values_1024.jpg`, `names_and_copied.jpg` | numbers and names cut with …, full on hover, click copies with a green tick | W640 (#712), W639 (#711) |
| `dialogs_today.jpg` | the dialogs before W641: seven dialogs built three ways | W641 (#713) |
| `form_settings_1280.jpg`, `forms_desktop.jpg`, `forms_phone.jpg` | the form and dialog system | W641 (#713) |
| `panels_table_now_vs_dock.jpg`, `panels_autox_tabs.jpg` | the panels as one dock, the views as tabs | W644 (#716) |

Kept per AGENTS.md's mockup rule (2026-11-23): only pictures of the chosen designs, and of the page before them. The
pictures of rejected options (the copy tip's variants, scientific number format, the first dialog shell) and the scripts
and CSS that made the pictures (`mockup-sources/`) were deleted once W639-W644 merged. They are in git history before
that commit.
