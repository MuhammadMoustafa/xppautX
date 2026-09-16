# Shipping the panel in the VS Code extension

What the extension repository (XPP-ODE-Extension) has to do to ship the
interactive panel. Nothing here belongs in xppautX; this file only says what
this project provides and what the extension must add.

## What xppautX provides

Each tagged release of xppautX attaches, per platform
(`.github/workflows/release.yml`):

| Asset | Contents |
|---|---|
| `xppautX-<tag>-linux-x64.tar.gz` | `xppautX`, LICENSE, CITATION.cff, README.txt |
| `xppautX-<tag>-macos-arm64.tar.gz`, `...-macos-x64.tar.gz` | the same, for macOS |
| `xppautX-<tag>-windows-x64.zip` | the same with `.exe` |
| `xppautX-<tag>-source.tar.gz` | the source of those binaries (GPL v2) |

The extension runs `xppautX --server` (it renders the page itself in a
webview). The same binary without `--server` is what people without VS Code
run to get the front end in a browser.

The protocol is `docs/protocol.md`; `hello.protocol` is its version number
(1 today). The front end script is `web/xpp-client.js` and
`web/xpp-client.css`, which the extension copies into `media/`.

## What the extension repository has to add

### 1. Get the binary in, one package per platform

VS Code supports platform-specific extension packages: build one `.vsix`
per target with only that platform's binary inside, and the Marketplace
gives each user the right one.

- Add a workflow that, for each target (`win32-x64`, `linux-x64`,
  `darwin-arm64`, `darwin-x64`), downloads the matching xppautX release
  asset, unpacks `xppautX` into `bin/`, marks it executable on
  Linux and macOS, and runs `vsce package --target <target>`.
  `gh release download <tag> --repo MuhammadMoustafa/xppautX --pattern '...'`
  does the download; pin the xppautX tag in a variable so the extension
  controls when it moves.
- Publish all packages from one job: `vsce publish --packagePath *.vsix`.
- Add `bin/` to `.gitignore` and to `.vscodeignore` exceptions (the binary
  must be *in* the package but not in git).
- Building locally for a test: download the assets by hand into `bin/` and
  run `vsce package --target win32-x64`. The workflow is the normal route;
  local packaging is only for trying it out.

### 2. Find the binary at run time

Order: the `xpp-ode.serverCommand` setting if the user set one, else the
bundled `bin/xppautX[.exe]`, else `xppautX` on PATH; the extension appends
`--server` itself. Say which
one failed when it cannot start; the panel already shows what the program
printed.

### 3. Refuse a server that speaks another protocol

The first event is `hello` with `protocol`. If it is not the version the
bundled `media/xpp-client.js` expects, show a message asking the user to
update the extension or their own `xppautX`, and do not open the
panel.

### 4. Keep the front end in step

`media/xpp-client.js` and `media/xpp-client.css` are copies of xppautX
`web/`. Copy them whenever the pinned xppautX tag changes (a script or a
workflow step: `curl` them from the tag). They must match the protocol
version of the bundled server.

### 5. Licensing

The extension is MIT; `xppautX` is GPL v2 (XPPAUT is Bard
Ermentrout's). Shipping them together is fine as long as each release:

- includes the GPL v2 text (`LICENSE` from the asset) next to the binary,
- states in the README and the Marketplace page that the panel runs
  `xppautX` under GPL v2, with a link to the exact
  source (`xppautX-<tag>-source.tar.gz` of the pinned tag).

Have someone check this before the first publish; it is the one part that
cannot be fixed afterwards by a patch release.

### 6. Tests worth having

- Start the bundled binary, wait for `hello`, send a key, expect an `ask`
  (`test/serverProtocol.test.ts` already does the protocol part).
- One test per platform package in CI: unpack the built `.vsix` and check
  that the binary is there and runs `--version`.

## Suggested order

1. Tag xppautX, let its release workflow produce the assets.
2. Add the packaging workflow with that tag pinned, and the run-time lookup.
3. Try the package locally on Windows, then publish a pre-release version.
4. Ask a few users; then publish properly.
