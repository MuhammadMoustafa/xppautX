/* The File > Export > Several window's own state (W774): which formats are ticked and the folder shown. The ticks are not kept between
   uses: opening the window clears them. The batch it starts is the runner's (store/batch.ts, session.ts batchRun), shared with the pins. */

export interface ExportSeveralState {
  open: boolean;
  /** the ticked formats, each `exportKey`: the command and its format (exportKey in exportSeveral.ts) */
  ticks: string[];
  /** the folder the files go to: its full path on the desktop, a browser folder's name; null before one is picked */
  folder: string | null;
}

export const initialExportSeveral: ExportSeveralState = {open: false, ticks: [], folder: null};

export type ExportSeveralAction =
  | {type: 'open'; open: boolean}
  | {type: 'tick'; key: string; on: boolean}
  | {type: 'folder'; folder: string | null};

export function reduceExportSeveral(state: ExportSeveralState, action: ExportSeveralAction): ExportSeveralState {
  switch (action.type) {
    case 'open':
      return action.open ? {...initialExportSeveral, open: true} : {...state, open: false};
    case 'tick': {
      const ticks = state.ticks.filter(key => key !== action.key);
      return {...state, ticks: action.on ? [...ticks, action.key] : ticks};
    }
    case 'folder':
      return {...state, folder: action.folder};
  }
}
