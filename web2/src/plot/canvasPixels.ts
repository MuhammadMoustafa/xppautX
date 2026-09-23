/* A canvas's picture as the core's `pixels` answer wants it (docs/protocol.md
   `pixels`): its size and the RGB bytes, alpha dropped. */
export function canvasPixels(canvas: HTMLCanvasElement): {w: number; h: number; rgb: Uint8ClampedArray} | null {
  const w = canvas.width, h = canvas.height, ctx = canvas.getContext('2d');
  if (!w || !h || !ctx) return null;
  const rgba = ctx.getImageData(0, 0, w, h).data;
  const rgb = new Uint8ClampedArray(w * h * 3);
  for (let i = 0, j = 0; j < rgb.length; i += 4, j += 3) {
    rgb[j] = rgba[i];
    rgb[j + 1] = rgba[i + 1];
    rgb[j + 2] = rgba[i + 2];
  }
  return {w, h, rgb};
}
