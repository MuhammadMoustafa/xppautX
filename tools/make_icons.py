#!/usr/bin/env python3
"""Make assets/icon.ico (16, 32, 48 and 256 px) from assets/icon.svg, the
icon's one source. Changing the icon is: replace assets/icon.svg, run this,
commit both, rebuild (the Windows exe carries the .ico through
assets/xppautx.rc).

Chrome (or Edge) headless rasterises the SVG at 256 px; Pillow scales that
down and writes the .ico. Needs Python 3 with Pillow (pip install pillow)
and a Chrome or Edge: found in the usual places, or named with --chrome.

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
SIZES = [16, 32, 48, 256]
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


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--chrome', help='Chrome or Edge to rasterise the SVG with')
    args = ap.parse_args()
    browser = find_browser(args.chrome)
    big = max(SIZES)
    with tempfile.TemporaryDirectory(prefix='xppicon') as tmp:
        tmp = pathlib.Path(tmp)
        shutil.copy(SVG, tmp / 'icon.svg')
        page = tmp / 'icon.html'
        page.write_text('<!doctype html><html><body style="margin:0;background:transparent">'
                        f'<img src="icon.svg" style="display:block;width:{big}px;height:{big}px"></body></html>',
                        encoding='utf-8')
        png = tmp / 'icon.png'
        subprocess.run([browser, '--headless=new', '--disable-gpu', '--hide-scrollbars',
                        '--default-background-color=00000000', f'--window-size={big},{big}',
                        f'--user-data-dir={tmp / "profile"}', f'--screenshot={png}', page.as_uri()],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
        img = Image.open(png).convert('RGBA').crop((0, 0, big, big))
        img.save(ICO, format='ICO', sizes=[(s, s) for s in SIZES])
    print(f'make_icons: wrote {ICO.relative_to(ROOT).as_posix()} ({", ".join(map(str, SIZES))} px)')


if __name__ == '__main__':
    main()
