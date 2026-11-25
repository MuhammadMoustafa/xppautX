# W875 The start screen follows the dark theme (#957)

The start screen (no model open) now draws in the theme in force, the page's own choice
included, and has the title bar's theme button (light, dark, or follow the system).

Before: the start screen never set the page's theme itself; it drew whatever the first
render had left, so a light choice made in the page drew dark when the system was dark.

- `start-375-light.jpg`, `start-375-dark.jpg`: a phone width (375 px), the system's scheme
- `start-768-light.jpg`, `start-768-dark.jpg`: 768 px
- `start-1280-light.jpg`, `start-1280-dark.jpg`: 1280 px

The examples folder of this worktree holds no .odex models, so each picture shows its
"no examples" line with the worktree's path.
