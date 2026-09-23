/* The top bar: the menu drawer's button (narrow screens), the model, the
   most used command, the theme, and the way back to the classic interface. */
import type {Theme} from '../store/state';
import {useSession, useStore} from './context';
import {saveTheme} from './theme';

const NEXT_THEME: Record<Theme, Theme> = {system: 'light', light: 'dark', dark: 'system'};
const THEME_NAME: Record<Theme, string> = {system: 'Auto', light: 'Light', dark: 'Dark'};

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
        title="Initialconds / Go (I, G)">Integrate</button>
      <button onClick={setTheme} title="Theme: light, dark, or as the system">
        <span class="wide-only">Theme:</span> {THEME_NAME[theme]}
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
      <a class="button classic" href={`/${location.search}`} title="The classic interface, with every window">Classic</a>
    </header>
  );
}
