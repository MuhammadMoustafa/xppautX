/* Light/dark theme: the user's choice (settings.json's page.theme, adopted from hello and saved by session.setPage) or the
   system's. The CSS reads `data-theme` on <html>. */
import {useEffect, useState} from 'preact/hooks';

const media = () => window.matchMedia('(prefers-color-scheme: dark)');

/** whether `theme` shows dark now, following the system when it is 'system' */
export function useDark(theme: 'system' | 'light' | 'dark'): boolean {
  const [systemDark, setSystemDark] = useState(() => media().matches);
  useEffect(() => {
    const m = media(), on = () => setSystemDark(m.matches);
    m.addEventListener('change', on);
    return () => m.removeEventListener('change', on);
  }, []);
  const dark = theme === 'dark' || (theme === 'system' && systemDark);
  /* set while the page renders, not in an effect: an effect runs after the charts' own effects, and a chart reads the
     theme when it draws (plot/colors.ts cssColor), so on a switch they would draw the old theme's colors */
  document.documentElement.dataset.theme = dark ? 'dark' : 'light';
  return dark;
}
