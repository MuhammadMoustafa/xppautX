# W881 The download toast does not cover the Values panel (#963)

A notice (the browser save's "downloaded" toast) now has a strip of its own between the
work area and the status bar. Before, it floated over the bottom of the Values panel and
the plot's axis names (at 768 px it hid the NUMERICS group's header and "Add slider").

Each picture is the model open after `session.save` (lecar.odex), with the notice shown:

- `notice-375-light.jpg`, `notice-375-dark.jpg`: 375 px (the Values panel is a sheet, closed)
- `notice-768-light.jpg`, `notice-768-dark.jpg`: 768 px (the Values panel is the dock's column)
- `notice-1280-light.jpg`, `notice-1280-dark.jpg`: 1280 px

At 375 px with the Values sheet open the notice waits beneath the sheet until it closes.
