#!/usr/bin/env python3
"""Make the icon files from assets/icon.svg, their one source: changing the
icon is replace assets/icon.svg, run this, commit everything it writes,
rebuild (Windows carries assets/icon.ico through assets/xppautx.rc; Linux
embeds assets/icons/hicolor/256x256/apps/xppautx.png in the GTK window when
built with the window, W13b; `make app` carries assets/icon.icns into the
macOS bundle).

Chrome (or Edge) headless rasterises the SVG at 1024 px; Pillow scales that
down to write everything else: .ico (16/32/48/256), .icns (macOS, resized
internally by Pillow -- no iconutil needed), and a Linux hicolor icon theme
tree (16/32/48/64/128/256/512, assets/icons/hicolor/SIZExSIZE/apps/xppautx.png,
what tools/associate/install-linux.sh installs). Needs Python 3 with Pillow
(pip install pillow) and a Chrome or Edge: found in the usual places, or
named with --chrome.

    python3 tools/make_icons.py [--chrome PATH]
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
SVG = ROOT / 'assets' / 'icon.svg'
ICO = ROOT / 'assets' / 'icon.ico'
ICNS = ROOT / 'assets' / 'icon.icns'
HICOLOR = ROOT / 'assets' / 'icons' / 'hicolor'
ICO_SIZES = [16, 32, 48, 256]
HICOLOR_SIZES = [16, 32, 48, 64, 128, 256, 512]
RENDER_SIZE = 1024  # the one rasterisation everything else is scaled from
BROWSERS = [
    'C:/Program Files/Google/Chrome/Application/chrome.exe',
    'C:/Program Files (x86)/Google/Chrome/Application/chrome.exe',
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    'google-chrome', 'chromium', 'chromium-browser', 'microsoft-edge',
]


def find_browser(given):
    for b in ([given] if given else BROWSERS):
        path = b if os.path.isfile(b) else shutil.which(b)
        if path:
            return path
    sys.exit('make_icons: no Chrome or Edge found; name one with --chrome PATH')


def render_svg(browser, size, tmp):
    """size px PNG of SVG, rasterised by headless Chrome/Edge (no other
    dependency draws an SVG at a chosen size portably)."""
    shutil.copy(SVG, tmp / 'icon.svg')
    page = tmp / 'icon.html'
    page.write_text('<!doctype html><html><body style="margin:0;background:transparent">'
                    f'<img src="icon.svg" style="display:block;width:{size}px;height:{size}px"></body></html>',
                    encoding='utf-8')
    png = tmp / 'icon.png'
    subprocess.run([browser, '--headless=new', '--disable-gpu', '--hide-scrollbars',
                    '--default-background-color=00000000', f'--window-size={size},{size}',
                    f'--user-data-dir={tmp / "profile"}', f'--screenshot={png}', page.as_uri()],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    return Image.open(png).convert('RGBA').crop((0, 0, size, size))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--chrome', help='Chrome or Edge to rasterise the SVG with')
    args = ap.parse_args()
    browser = find_browser(args.chrome)
    with tempfile.TemporaryDirectory(prefix='xppicon') as tmp:
        img = render_svg(browser, RENDER_SIZE, pathlib.Path(tmp))

        img.save(ICO, format='ICO', sizes=[(s, s) for s in ICO_SIZES])
        # Pillow's ICNS writer resizes this one image down to each of its own
        # sizes (up to 1024) itself; nothing macOS-specific is needed.
        img.save(ICNS, format='ICNS')

        written = [ICO.relative_to(ROOT), ICNS.relative_to(ROOT)]
        for size in HICOLOR_SIZES:
            out_dir = HICOLOR / f'{size}x{size}' / 'apps'
            out_dir.mkdir(parents=True, exist_ok=True)
            out = out_dir / 'xppautx.png'
            img.resize((size, size), Image.LANCZOS).save(out, format='PNG')
            written.append(out.relative_to(ROOT))
    for path in written:
        print(f'make_icons: wrote {path.as_posix()}')


if __name__ == '__main__':
    main()
