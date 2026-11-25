# W873 Every open, save and load opens the file picker at once (#955)

Each flow at 1280, 768 and 375 px, light and dark, as the page rendered them in a headless browser (theme by prefers-color-scheme).
A browser has no OS dialog, so a save shows its result (a download) and an open shows the page's own prompt; the desktop window opens the
operating system's dialog in the same place.

- `setload-picker-<width>-<theme>.jpg`: File > Sets > Load a named part of a file: the picker's prompt at once, with no form (scope and source are no longer asked). Its title is the core's name for the listing command ("List file contents").
- `setload-parts-<width>-<theme>.jpg`: after the pick, the picked file's parts (its defaults and each named set) are listed to choose from.
- `saveas-<width>-<theme>.jpg`: File > Save session as: no form (the data table is kept, the value in force); the browser's save is a download.
- `export-<width>-<theme>.jpg`: File > Export > Graph as > pdf: no form; the format is the menu item's, and a bare Graph export takes it from the extension of the name picked (pdf, svg).

Reviewed when captured: no cut text, the prompt and the parts list fit every frame, dark text and borders stay readable.
