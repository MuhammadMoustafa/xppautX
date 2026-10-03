/** Theme variables used by canvas plots must exist in the stylesheet. */
export function cssVar(name: string): string {
  const value = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  if (!value) throw new Error(`Missing theme variable ${name}`);
  return value;
}
