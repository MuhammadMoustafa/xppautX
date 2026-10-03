/* The WebAssembly core's client, shared by the page (page.ts) and node's
   check (tools/wasmcheck.mjs): web2's own state reducer fed by the core's
   events, which a Transport brings. web2's pages use HttpTransport; here
   the transport is a LinePort, the lines of a Web Worker or of the module
   itself, so what the page holds of a run (its plots, series, appends) is
   web2's code, not a second reading of docs/protocol.md. */
import type {AskEvent, Command, XppEvent} from '../../web2/src/protocol/types';
import type {Transport} from '../../web2/src/protocol/transport';
import {initialState, reduce, type AppState} from '../../web2/src/store/state';

/** the two ends of the line stream: the host starts the core, hands each
    event line to `onLine`, and takes command lines through send */
export interface LinePort {
  send(line: string): void;
  close(): void;
}

export class LineTransport implements Transport {
  private port: LinePort | null = null;

  /** `connect` starts the core and returns its port; it may deliver lines at once */
  constructor(private readonly connect: (onLine: (line: string) => void) => LinePort) {}

  send(cmd: Command): void {
    if (!this.port) throw new Error('the core is not started');
    this.port.send(JSON.stringify(cmd));
  }

  open(onEvent: (ev: XppEvent) => void, onStatus: (open: boolean) => void): void {
    this.port = this.connect(line => onEvent(JSON.parse(line) as XppEvent));
    onStatus(true);
  }

  close(): void {
    this.port?.close();
    this.port = null;
  }
}

/** a safety limit for a wait, never a pass mark (docs: a check waits for a condition) */
const WAIT_LIMIT_MS = 120_000;

export class WasmClient {
  /** what the page shows: web2's reduced state of every event so far */
  state: AppState = initialState;
  /** events not yet taken by collect */
  private pending: XppEvent[] = [];
  private wake: (() => void) | null = null;
  /** called after each event (the page redraws) */
  onChange: (() => void) | null = null;

  constructor(private readonly transport: Transport) {}

  start(): void {
    this.transport.open(ev => {
      this.state = reduce(this.state, {type: 'event', ev});
      this.pending.push(ev);
      this.wake?.();
      this.onChange?.();
    }, open => {
      this.state = reduce(this.state, {type: 'connection', open});
    }, text => {
      throw new Error(text);
    });
  }

  send(cmd: Command): void {
    this.transport.send(cmd);
  }

  close(): void {
    this.transport.close();
  }

  /** the events up to and including the first that `until` accepts, taken from those not collected yet */
  async collect(until: (ev: XppEvent) => boolean): Promise<XppEvent[]> {
    const taken: XppEvent[] = [];
    const deadline = Date.now() + WAIT_LIMIT_MS;
    for (;;) {
      while (this.pending.length) {
        const ev = this.pending.shift()!;
        taken.push(ev);
        if (until(ev)) return taken;
      }
      if (Date.now() > deadline) throw new Error(`no matching event after ${taken.length} events (the last: ${taken.at(-1)?.ev})`);
      await new Promise<void>(resolve => {
        const timer = setTimeout(resolve, 1000);
        this.wake = () => {
          clearTimeout(timer);
          resolve();
        };
      });
      this.wake = null;
    }
  }

  /** a command that ends with `idle` (docs/protocol.md): its events, the idle last */
  async run(cmd: Command): Promise<XppEvent[]> {
    this.send(cmd);
    return this.collect(isIdle);
  }

  /** the first events: the model's hello and its first idle */
  async started(): Promise<XppEvent[]> {
    return this.collect(isIdle);
  }

  /** Initialconds, Go (keys i and g, as docs/protocol.md's menus take them), up to the answer: the run is on */
  async beginIntegration(): Promise<void> {
    this.send({cmd: 'key', key: 'i'});
    const ask = (await this.collect(ev => ev.ev === 'ask')).at(-1) as AskEvent;
    this.send({cmd: 'answer', id: ask.id, key: 'g'});
  }

  /** the same, to the run's end: every event of the run */
  async integrate(): Promise<XppEvent[]> {
    await this.beginIntegration();
    return this.collect(isIdle);
  }

  /** Numerics' Total (keys u and t), then its Escape back to the main menu */
  async setTotal(total: string): Promise<void> {
    await this.run({cmd: 'key', key: 'u'});
    this.send({cmd: 'key', key: 't'});
    const ask = (await this.collect(ev => ev.ev === 'ask')).at(-1) as AskEvent;
    this.send({cmd: 'answer', id: ask.id, ok: 1, value: total});
    await this.collect(isIdle);
    await this.run({cmd: 'key', key: 'Escape'});
  }
}

export function isIdle(ev: XppEvent): boolean {
  return ev.ev === 'idle';
}
