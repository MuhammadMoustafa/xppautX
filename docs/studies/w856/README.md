# W856 Forms and menus show user text, never the protocol's (#937)

Each width (1280, 768, 375 px) in light and dark, as the page rendered them in the browser pane.

- `autox-run-form-<width>-<theme>.jpg`: autoX > Run's form. Its description is user text only (no `hello`, `autoxinfo`, `autox_run`, no command ids).
- `options-form-<width>-<theme>.jpg`: an options form (autoX > Numerics, the Continuation group). The labels read as "Steps", "Parameter min/max", "Norm min/max": the group heading, then the option's own name, never a dotted key such as "Limits.parameter min".

Reviewed when captured: no text cut or overlapping at any width. In the 1280 and 768 captures the "Follow all branches" switch shows on in some captures and off in others: the value was not changed by these captures, the switch's stored state differs between the sessions the captures were taken in.
