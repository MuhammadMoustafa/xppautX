/* The page's per-viewer preferences (the theme, the side panels, the folded Values sections),
   kept as JSON in this browser's localStorage. Storage may be blocked or hold a value that is
   not ours: a read then gives `fallback`, and a write lasts only for this page. */

/** the value stored under `key` if `parse` accepts it (returns non-null), else `fallback` */
export function readPref<T>(key: string, parse: (v: unknown) => T | null, fallback: T): T {
  try {
    const raw = localStorage.getItem(key);
    return raw === null ? fallback : parse(JSON.parse(raw)) ?? fallback;
  } catch {
    return fallback;
  }
}

/** stores `value` under `key`; undefined removes it */
export function writePref(key: string, value: unknown): void {
  try {
    if (value === undefined) localStorage.removeItem(key);
    else localStorage.setItem(key, JSON.stringify(value));
  } catch {
    /* storage blocked: the choice lasts for this page */
  }
}
