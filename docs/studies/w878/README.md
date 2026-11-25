# W878: autoX's Grab picks on the diagram at once

Pictures of the autoX view after Grab (ab.odex, the steady run done), the page as the web2check `shots` section takes them (`XPP_SHOTS=folder node tools/web2check.mjs --only shots`, `grab-<width>-<theme>`). Grab opens no form: the pick bar sits over the view, and the grab's cursor is on the diagram.

- `grab-375-light.jpg`, `grab-375-dark.jpg`: 375 px. The pick bar's text, Take and Cancel, and the cursor on the diagram.
- `grab-768-light.jpg`, `grab-768-dark.jpg`: 768 px, the same.
- `grab-1280-light.jpg`, `grab-1280-dark.jpg`: 1280 px, the same.

The before state (an empty Grab form before the pick) is not kept: the form is gone, and the check `G grabs in the view` (section autox) asserts no form.
