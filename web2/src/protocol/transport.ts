/* The wire: events by Server-Sent Events, commands by POST, on the same
   endpoints core/xpp_http.cpp serves. xppautX puts a token in the page's
   address; both URLs need it. */
import type {Command, XppEvent} from './types';

export interface Transport {
  send(cmd: Command): void;
  /** starts the event stream; `onEvent` gets every event, `onStatus` the connection's state,
      `onFailed` what went wrong with a command that did not reach the core (refused, or never sent) */
  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void, onFailed: (text: string) => void): void;
  close(): void;
}

export class HttpTransport implements Transport {
  private source: EventSource | null = null;
  /* the last command's POST: the next one goes out once it was answered */
  private sending: Promise<void> = Promise.resolve();
  /* open's onFailed: a command that fails before open is a bug, thrown
     (below, out of the chain of POSTs, which goes on) */
  private failed: (text: string) => void = text => {
    throw new Error(text);
  };

  constructor(private readonly token: string = location.search, private readonly base = '/') {}

  /** commands reach the core in the order they were sent: each POST waits
      for the one before it (xppautX answers each connection on a thread of
      its own, so two in flight at once could be taken in either order); a
      failed one does not hold up the next, and is said (onFailed): one the
      server refused (core/xpp_http.cpp: 403 a bad token, 413 too long, 400
      cut short) with its status and reason, one that never got there with
      why */
  send(cmd: Command): void {
    const body = JSON.stringify(cmd);
    this.sending = this.sending.then(async () => {
      let refused = '';
      try {
        const r = await fetch(`${this.base}cmd${this.token}`, {method: 'POST', body});
        if (!r.ok) {
          const reason = (await r.text().catch((e: unknown) => String(e))).trim() || r.statusText;
          refused = `The command ${cmd.cmd} was refused (${r.status}${reason ? ': ' + reason : ''})`;
        }
      } catch (e) {
        refused = `The command ${cmd.cmd} did not reach xppautX (${e instanceof Error ? e.message : String(e)})`;
      }
      if (refused) {
        try {
          this.failed(refused);
        } catch (e) {
          setTimeout(() => {
            throw e;
          });
        }
      }
    });
  }

  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void, onFailed: (text: string) => void): void {
    this.failed = onFailed;
    const source = new EventSource(`${this.base}events${this.token}`);
    source.onopen = () => onStatus(true);
    source.onerror = () => onStatus(false); /* EventSource reconnects by itself */
    source.onmessage = m => {
      onEvent(JSON.parse(m.data) as XppEvent);
    };
    this.source = source;
  }

  close(): void {
    this.source?.close();
    this.source = null;
  }
}
