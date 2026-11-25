# Colors

The colors used in individual curves take on numbers from 0 to 10. Here is the meaning of the numbers:

**0-Black/White; 1-Red; 2-Red Orange; 3-Orange; 4-Yellow Orange; 5-Yellow; 6-Yellow Green; 7-Green; 8-Blue Green; 9-Blue; 10-Purple.**

**In web2** these eleven indices (0 the foreground color, then red through purple) map to a palette per theme, keeping the hue named above; see [Using the interface](04-using-the-interface.md) for the light/dark/system theme.

**On the dark theme** a color too dark to read against the plot's background is drawn lighter: one whose contrast against `--surface` is under 3:1 (WCAG's ratio for a graphic) has its lightness raised, its hue, saturation and alpha kept, until it reaches 3:1. Pure blue (`#0000ff`, the unstable periodic orbits' default) is drawn as a lighter blue, and black as a grey. Nothing else changes: the core keeps the color as set, a color picker shows and gives back that color, and the light theme draws every color as it is. A color that already reads on the dark theme, and `foreground`, are drawn as they are.
