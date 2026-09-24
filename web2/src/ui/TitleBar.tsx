/* The top bar: the menu drawer's button (narrow screens), the model, the
   most used command, the theme, and the panels' toggles. */
import type {Theme} from '../store/state';
import {BUSY_TITLE, useSession, useStore} from './context';
import {saveTheme} from './theme';

const NEXT_THEME: Record<Theme, Theme> = {system: 'light', light: 'dark', dark: 'system'};
/* the theme is an icon, not a word: "Auto" beside the feature buttons read as AUTO (T21) */
const THEME_NAME: Record<Theme, string> = {system: 'follow system', light: 'light', dark: 'dark'};

function ThemeIcon({theme}: {theme: Theme}) {
  const common = {width: 18, height: 18, viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', 'stroke-width': 2,
    'stroke-linecap': 'round' as const, 'stroke-linejoin': 'round' as const, 'aria-hidden': 'true' as const, class: 'icon'};
  if (theme === 'light') {
    return (
      <svg {...common}>
        <circle cx="12" cy="12" r="4" />
        <path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4" />
      </svg>
    );
  }
  if (theme === 'dark') return <svg {...common}><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z" /></svg>;
  /* follow the system: a half-filled circle */
  return (
    <svg {...common}>
      <circle cx="12" cy="12" r="9" />
      <path d="M12 3a9 9 0 0 1 0 18z" fill="currentColor" />
    </svg>
  );
}

export function TitleBar() {
  const session = useSession();
  const title = useStore(s => s.title);
  const file = useStore(s => s.hello?.file ?? '');
  const theme = useStore(s => s.theme);
  const drawer = useStore(s => s.drawerOpen);
  const valuesOpen = useStore(s => s.valuesOpen);
  const tableOpen = useStore(s => s.table.open);
  const textOpen = useStore(s => s.text.open);
  const aplotOpen = useStore(s => s.aplot.open);
  const aniOpen = useStore(s => s.ani.open);
  const helpOpen = useStore(s => s.help.open);
  const busy = useStore(s => s.busy);
  const setTheme = () => {
    const t = NEXT_THEME[theme];
    saveTheme(t);
    session.store.dispatch({type: 'theme', theme: t});
  };
  return (
    <header class="title-bar">
      <button class="menu-toggle" aria-controls="command-menu" aria-expanded={drawer}
        onClick={() => session.store.dispatch({type: 'drawer', open: !drawer})}>Menu</button>
      <h1>{title || 'XPP'}</h1>
      <span class="muted file">{file}</span>
      <span class="spacer" />
      <button class="primary" disabled={busy} onClick={() => session.keys('i', 'g')}
        title={busy ? BUSY_TITLE : 'Initialconds / Go (I, G)'}>Integrate</button>
      <button class="theme-toggle icon-button" onClick={setTheme} data-theme-choice={theme}
        aria-label={`Theme: ${THEME_NAME[theme]}`} title={`Theme: ${THEME_NAME[theme]} (click for ${THEME_NAME[NEXT_THEME[theme]]})`}>
        <ThemeIcon theme={theme} />
      </button>
      <button class="values-toggle" aria-controls="values-panel" aria-expanded={valuesOpen}
        onClick={() => session.store.dispatch({type: 'valuesPanel', open: !valuesOpen})}>Values</button>
      <button class="table-toggle" aria-controls="table-panel" aria-expanded={tableOpen}
        onClick={() => (tableOpen ? session.closeTable() : session.openTable())}>Data</button>
      <button class="text-toggle" aria-controls="text-panel" aria-expanded={textOpen}
        onClick={() => (textOpen ? session.closeText() : session.openText())}
        title="Equations, source and the last equilibrium">Text</button>
      <button class="aplot-toggle" aria-controls="aplot-panel" aria-expanded={aplotOpen}
        onClick={() => (aplotOpen ? session.closeAplot() : session.openAplot())}
        title="The array plot: XPP's grid of a range of columns and rows, coloured by value">Array</button>
      <button class="ani-toggle" aria-controls="ani-panel" aria-expanded={aniOpen}
        onClick={() => (aniOpen ? session.closeAni() : session.openAni())}
        title="The animation (Viewaxes/Toon): play, step and seek its frames">Animation</button>
      <button class="help-toggle" aria-controls="help-panel" aria-expanded={helpOpen}
        onClick={() => session.store.dispatch({type: 'help', action: {type: 'open'}})}
        title="The manual (F1)">Help</button>
    </header>
  );
}
