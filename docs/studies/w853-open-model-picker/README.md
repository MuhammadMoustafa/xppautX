# W853 File > Open model opens the picker at once (#934)

Each width (1280, 768, 375 px) in light and dark, as the page rendered them in the browser pane (the theme is the page's own switch).

- `menu-<width>-<theme>.jpg`: File > Open model in the File menu (desktop) or the menu drawer (768, 375), the item "Open model" (Ctrl+O) with its description.
- `picker-<width>-<theme>.jpg`: the state the click leads to: the "Open model" dialog with its "Choose file..." button and no form. The old form with the Example and Recent fields is gone; recent models and examples stay on the start screen.

Reviewed when captured: the text is not cut at any width; the dialog and drawer fit the frame.
