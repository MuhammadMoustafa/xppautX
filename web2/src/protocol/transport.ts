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

/** the waits before a POST that failed with no answer is sent again (W124):
    a connection the browser or the network dropped is usually back at once,
    and a command is said lost only after about 4 s of failures */
const RESEND_MS: readonly number[] = [100, 300, 1000, 3000];

/* this page's id among the pages that send to one xppautX: 128 random bits */
function pageId(): string {
  return Array.from(crypto.getRandomValues(new Uint8Array(16)), b => b.toString(16).padStart(2, '0')).join('');
}

const wait = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

export class HttpTransport implements Transport {
  private source: EventSource | null = null;
  /* the last command's POST: the next one goes out once it was answered */
  private sending: Promise<void> = Promise.resolve();
  /* the commands sent so far; the next one's number is one more */
  private sent = 0;
  private readonly page = pageId();
  /* open's onFailed: a command that fails before open is a bug, thrown
     (below, out of the chain of POSTs, which goes on) */
  private failed: (text: string) => void = text => {
    throw new Error(text);
  };

  constructor(private readonly token: string = location.search, private readonly base = '/',
    private readonly resendMs: readonly number[] = RESEND_MS) {}

  /** commands reach the core in the order they were sent, each once: each
      POST waits for the one before it (xppautX answers each connection on a
      thread of its own, so two in flight at once could be taken in either
      order). One that failed with no answer (the browser or the network
      lost the connection) may have arrived or not: it is sent again, with
      the same number (p= this page, n= its place), which the server takes
      only once (core/xpp_http.cpp serve_cmd). A command still failing after
      RESEND_MS, or one the server refused (403 a bad token, 413 too long,
      400 cut short), is said (onFailed) and does not hold up the next. */
  send(cmd: Command): void {
    const body = JSON.stringify(cmd);
    const url = `${this.base}cmd${this.token}${this.token.includes('?') ? '&' : '?'}p=${this.page}&n=${++this.sent}`;
    this.sending = this.sending.then(async () => {
      let refused = '';
      for (let tries = 0; ; tries++) {
        try {
          const r = await fetch(url, {method: 'POST', body});
          if (!r.ok) {
            const reason = (await r.text().catch((e: unknown) => String(e))).trim() || r.statusText;
            refused = `The command ${cmd.cmd} was refused (${r.status}${reason ? ': ' + reason : ''})`;
          }
          break;
        } catch (e) {
          if (tries < this.resendMs.length) {
            await wait(this.resendMs[tries]);
            continue;
          }
          refused = `The command ${cmd.cmd} did not reach xppautX (${e instanceof Error ? e.message : String(e)})`;
          break;
        }
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
