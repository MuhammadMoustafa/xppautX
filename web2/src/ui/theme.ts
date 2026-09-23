/* Light/dark theme: the user's choice (remembered in this browser) or the
   system's. The CSS reads `data-theme` on <html>. */
import {useEffect, useState} from 'preact/hooks';
import type {Theme} from '../store/state';

const KEY = 'xppTheme';
const media = () => window.matchMedia('(prefers-color-scheme: dark)');

export function savedTheme(): Theme {
  try {
    const t = localStorage.getItem(KEY);
    return t === 'light' || t === 'dark' ? t : 'system';
  } catch {
    return 'system';
  }
}

export function saveTheme(t: Theme): void {
  try {
    if (t === 'system') localStorage.removeItem(KEY);
    else localStorage.setItem(KEY, t);
  } catch {
    /* storage blocked: the choice lasts for this page */
  }
}

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
