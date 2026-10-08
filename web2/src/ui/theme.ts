/* Light/dark theme: the user's choice (remembered in this browser) or the
   system's. The CSS reads `data-theme` on <html>. */
import {useEffect, useState} from 'preact/hooks';
import type {Theme} from '../store/state';
import {readPref, writePref} from '../store/prefs';

const KEY = 'xppTheme';
const media = () => window.matchMedia('(prefers-color-scheme: dark)');

export const savedTheme = (): Theme =>
  readPref<Theme>(KEY, t => (t === 'light' || t === 'dark' ? t : null), 'system');

/** 'system' is no stored choice */
export const saveTheme = (t: Theme): void => writePref(KEY, t === 'system' ? undefined : t);

/** whether `theme` shows dark now, following the system when it is 'system' */
export function useDark(theme: Theme): boolean {
  const [systemDark, setSystemDark] = useState(() => media().matches);
  useEffect(() => {
    const m = media(), on = () => setSystemDark(m.matches);
    m.addEventListener('change', on);
    return () => m.removeEventListener('change', on);
  }, []);
  const dark = theme === 'dark' || (theme === 'system' && systemDark);
  useEffect(() => {
    document.documentElement.dataset.theme = dark ? 'dark' : 'light';
  }, [dark]);
  return dark;
}
