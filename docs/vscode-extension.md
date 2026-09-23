# The VS Code extension and xppautX

How the [XPP-ODE extension](https://github.com/MuhammadMoustafa/XPP-ODE-Extension)
uses this program, and what this project promises it. The two are released
independently: the extension does not bundle `xppautX`, so a user can stay
on any release of either.

## What the extension does

**Open in XPP Interactive** runs

```
xppautX --web --no-open --port 0 model.ode
```

in the model's folder, reads the one line the program prints,

```
XPP: http://127.0.0.1:<port>/?t=<token>
```

and shows that address in a frame inside a VS Code webview. The page, the
script and the engine all come from the same binary, so there is nothing
to keep in step. Closing the panel kills the process; the process exits by
itself ten seconds after its last page disconnects.

Which `xppautX` runs: the `xpp-ode.xppautxPath` setting when it is set,
else the copy the extension's **Download xppautX** command fetched from the
latest GitHub release into VS Code's global storage. With neither, the
panel button is hidden and the command explains how to get the program.

Once a day the extension compares `xppautX --version` with the latest
release tag and offers the newer one (a setting turns this off).

## What xppautX promises

- `--web --no-open --port 0`: pick a free port, print the `XPP:` line
  above on stdout before anything else, then serve on 127.0.0.1 only.
- `--version`: print `xppautX <tag>` and exit. The Makefile sets the tag
  from `GITHUB_REF_NAME` in CI (the release tag, `v1.2.0`) or `git
  describe` locally.
- Release assets named `xppautX-<tag>-<platform>.tar.gz` (`.zip` on
  Windows) for `windows-x64`, `linux-x64`, `macos-arm64` and `macos-x64`,
  each holding the binary and `LICENSE` in one top-level folder
  (`.github/workflows/release.yml`, `tools/package_release.sh`). The
  extension downloads the one for the machine and extracts it with `tar`.
- The page keeps working when framed from another origin: it uses only
  relative addresses and the token in its query string.

The `--server` mode (JSON on stdin/stdout, `docs/protocol.md`) is not used
by the extension any more; it stays for tests and other embedders.

## Licensing

xppautX is GPL v2 (XPPAUT is Bard Ermentrout's). The extension is MIT and
ships no xppautX code; it downloads a release at the user's request and
records its origin in a `SOURCE.txt` next to the binary. The source of every
release is its `xppautX-<tag>-source.tar.gz` asset.
