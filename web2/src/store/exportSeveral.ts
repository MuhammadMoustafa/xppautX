/* The File > Export > Several window's own state (W774): which formats are ticked. The ticks are not kept between
   uses: opening the window clears them. The batch it starts is the runner's (store/batch.ts, session.ts batchRun), shared with the pins. */

export interface ExportSeveralState {
  open: boolean;
  /** the ticked formats, each `exportKey`: the command and its format (exportKey in exportSeveral.ts) */
  ticks: string[];
}

export const initialExportSeveral: ExportSeveralState = {open: false, ticks: []};

export type ExportSeveralAction =
  | {type: 'open'; open: boolean}
  | {type: 'tick'; key: string; on: boolean};

export function reduceExportSeveral(state: ExportSeveralState, action: ExportSeveralAction): ExportSeveralState {
  switch (action.type) {
    case 'open':
      return action.open ? {...initialExportSeveral, open: true} : {...state, open: false};
    case 'tick': {
      const ticks = state.ticks.filter(key => key !== action.key);
      return {...state, ticks: action.on ? [...ticks, action.key] : ticks};
    }
  }
}
