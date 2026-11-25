# W866: boundary-value and homoclinic branches drawn as not applicable (#947)

The page with W866 at each width (1280, 768, 375), light and dark: a boundary-value branch (`bvp` run of a
one-parameter model) drawn as a thin solid line in the theme's foreground, its legend naming the kind
("Boundary value solutions", "Branch 1 · boundary value") rather than "stable" or "unstable".

Decision (maintainer, 2026-11-24): name the kind, not "not applicable steady".
The review found the line stayed dark after a switch to the dark theme (the chart read the foreground
colour once, when it was built); fixed in the same branch, these pictures taken after the fix.
