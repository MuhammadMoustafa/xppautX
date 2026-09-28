/* base64 encoding for the `pixels` ask's answer (docs/protocol.md): a
   frame or window's RGB pixels, or a whole file the core wrote and the
   page read back. The GIF encoder that used to live here was removed at
   W66 (docs/roadmap.md): the core itself writes every GIF now
   (core/json_windows.cpp), asking `pixels` for each frame instead. */

/** base64 of any byte array, chunked so a large one never blows the call
    stack on `String.fromCharCode(...)` */
export function bytesToBase64(bytes: Uint8Array | Uint8ClampedArray): string {
  let s = '';
  const CHUNK = 0x2000;
  for (let i = 0; i < bytes.length; i += CHUNK) s += String.fromCharCode(...bytes.subarray(i, i + CHUNK));
  return btoa(s);
}
