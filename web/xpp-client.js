/* XPP front end for the xppcore-server protocol (docs/protocol.md).

   Plain browser JavaScript, no dependencies and no editor API: the host
   creates `new XppClient(rootElement, sendFunction)` and feeds every event
   object from the server to `client.receive(ev)`. web/serve.js hosts it in
   a browser; the VS Code extension hosts it in a webview. */
(function (global) {
  'use strict';

  const DASHES = [[], [1, 6], [], [4, 2], [1, 3], [4, 4], [1, 5], [4, 4, 4, 1], [4, 2], [1, 3]];
  const GREEK = {a: 'α', b: 'β', c: 'χ', d: 'δ', e: 'ε', f: 'φ', g: 'γ', h: 'η', i: 'ι', j: 'ϕ', k: 'κ',
    l: 'λ', m: 'μ', n: 'ν', o: 'ο', p: 'π', q: 'θ', r: 'ρ', s: 'σ', t: 'τ', u: 'υ', v: 'ϖ', w: 'ω',
    x: 'ξ', y: 'ψ', z: 'ζ', A: 'Α', B: 'Β', C: 'Χ', D: 'Δ', E: 'Ε', F: 'Φ', G: 'Γ', H: 'Η', I: 'Ι',
    K: 'Κ', L: 'Λ', M: 'Μ', N: 'Ν', O: 'Ο', P: 'Π', Q: 'Θ', R: 'Ρ', S: 'Σ', T: 'Τ', U: 'Υ', W: 'Ω',
    X: 'Ξ', Y: 'Ψ', Z: 'Ζ'};
  const TEXT_SIZES = [8, 10, 12, 14, 18]; /* xppaut's five font sizes */
  const AUTO_BUTTONS = [['Parameter', 'param'], ['Axes', 'axes'], ['Numerics', 'numerics'], ['Run', 'run'],
    ['Grab', 'grab'], ['Usr period', 'usr'], ['Clear', 'clear'], ['reDraw', 'redraw'], ['File', 'file']];

  function el(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined) e.textContent = text;
    return e;
  }

  /* xppaut key names from a DOM keyboard event, or null */
  function keyName(e) {
    if (e.ctrlKey || e.metaKey || e.altKey) return null;
    if (e.key.length === 1) return e.key;
    const named = ['Escape', 'Enter', 'Tab', 'Backspace', 'Delete', 'Home', 'End', 'ArrowLeft',
      'ArrowRight', 'ArrowUp', 'ArrowDown', 'PageUp', 'PageDown'];
    return named.includes(e.key) ? e.key : null;
  }

  /* one drawing surface: a canvas that replays draw ops */
  class Surface {
    constructor(client, id, w, h) {
      this.client = client;
      this.id = id;
      this.canvas = el('canvas', 'xpp-canvas');
      this.canvas.tabIndex = 0;
      this.ctx = this.canvas.getContext('2d');
      this.overlay = null;
      this.resize(w, h);
      this.color = 0;
      this.lineWidth = 1;
      this.dash = 0;
      this.font = {size: 1, symbol: false, color: 0};
    }
    resize(w, h) {
      if (this.canvas.width === w && this.canvas.height === h) return;
      const old = this.canvas.width ? this.ctx.getImageData(0, 0, this.canvas.width, this.canvas.height) : null;
      this.canvas.width = w;
      this.canvas.height = h;
      this.clear();
      if (old) this.ctx.putImageData(old, 0, 0);
    }
    pen(i) {
      return this.client.colorOf(i);
    }
    apply() {
      const c = this.ctx;
      c.strokeStyle = c.fillStyle = this.pen(this.color);
      c.lineWidth = Math.max(1, this.lineWidth);
      c.setLineDash(DASHES[this.dash] || []);
    }
    clear() {
      const c = this.ctx;
      c.save();
      c.fillStyle = this.client.colorOf(-1);
      c.fillRect(0, 0, this.canvas.width, this.canvas.height);
      c.restore();
    }
    textFont(size, symbol) {
      const px = TEXT_SIZES[Math.max(0, Math.min(4, size))];
      return (symbol ? 'italic ' : '') + px + 'px "DejaVu Sans Mono", Consolas, monospace';
    }
    run(ops) {
      const c = this.ctx;
      for (const o of ops) {
        switch (o[0]) {
          case 'clear': this.clear(); break;
          case 'color': this.color = o[1]; break;
          case 'lw': this.lineWidth = o[1]; break;
          case 'dash': this.dash = o[1]; break;
          case 'ls': break;
          case 'line':
            this.apply();
            c.beginPath();
            c.moveTo(o[1] + 0.5, o[2] + 0.5);
            c.lineTo(o[3] + 0.5, o[4] + 0.5);
            c.stroke();
            break;
          case 'point':
            this.apply();
            if (!o[3]) c.fillRect(o[1], o[2], 1, 1);
            else {
              const r = Math.round(o[3] / 1.41421356 + 0.5);
              c.beginPath();
              c.arc(o[1], o[2], r, 0, 2 * Math.PI);
              c.fill();
            }
            break;
          case 'bead':
            this.apply();
            c.beginPath();
            c.arc(o[1], o[2], 2, 0, 2 * Math.PI);
            c.fill();
            break;
          case 'frect': this.apply(); c.fillRect(o[1], o[2], o[3], o[4]); break;
          case 'rect': this.apply(); c.strokeRect(o[1] + 0.5, o[2] + 0.5, o[3], o[4]); break;
          case 'circle':
          case 'fcircle':
            this.apply();
            c.beginPath();
            c.arc(o[1], o[2], o[3], 0, 2 * Math.PI);
            o[0] === 'circle' ? c.stroke() : c.fill();
            break;
          case 'ellipse':
          case 'fellipse':
            this.apply();
            c.beginPath();
            c.ellipse(o[1] + o[3] / 2, o[2] + o[4] / 2, Math.abs(o[3] / 2), Math.abs(o[4] / 2), 0, 0, 2 * Math.PI);
            o[0] === 'ellipse' ? c.stroke() : c.fill();
            break;
          case 'cross':
            c.save();
            c.globalCompositeOperation = 'difference';
            c.strokeStyle = '#ffffff';
            c.lineWidth = 2;
            c.setLineDash([]);
            c.beginPath();
            c.moveTo(o[1] - 8, o[2]); c.lineTo(o[1] + 8, o[2]);
            c.moveTo(o[1], o[2] - 8); c.lineTo(o[1], o[2] + 8);
            c.stroke();
            c.restore();
            break;
          case 'font': this.font = {size: o[1], symbol: o[2] === 1, color: o[3]}; this.color = o[3]; break;
          case 'text':
            c.fillStyle = this.pen(0);
            c.font = this.textFont(1, false);
            c.fillText(o[3], o[1], o[2]);
            break;
          case 'rtext':
            this.apply();
            c.font = this.textFont(this.font.size, this.font.symbol);
            c.fillText(this.font.symbol ? greek(o[3]) : o[3], o[1], o[2]);
            break;
          case 'stext': this.richText(o[1], o[2], o[3], o[4]); break;
        }
      }
    }
    /* XPP rich text: \1 symbol, \0 roman, \s sub, \S super, \n normal */
    richText(x, y, s, size) {
      const c = this.ctx;
      let cx = x, cy = y, cs = size, symbol = false, buf = '';
      const sup = TEXT_SIZES[size] * 0.8, sub = sup / 2;
      const flush = () => {
        if (!buf) return;
        c.font = this.textFont(cs, false);
        c.fillStyle = this.pen(0);
        const t = symbol ? greek(buf) : buf;
        c.fillText(t, cx, cy);
        cx += c.measureText(t).width;
        buf = '';
      };
      for (let i = 0; i < s.length; i++) {
        if (s[i] === '\\' && i + 1 < s.length) {
          flush();
          const k = s[++i];
          if (k === '0') symbol = false;
          else if (k === '1') symbol = true;
          else if (k === 'n') { cy = y; cs = size; }
          else if (k === 's') { cy += sub; if (size > 0) cs = size - 1; }
          else if (k === 'S') { cy -= sup; if (size > 0) cs = size - 1; }
        } else buf += s[i];
      }
      flush();
    }
    /* canvas coordinates of a mouse event */
    at(e) {
      const r = this.canvas.getBoundingClientRect();
      return [Math.round((e.clientX - r.left) * this.canvas.width / r.width),
        Math.round((e.clientY - r.top) * this.canvas.height / r.height)];
    }
  }

  function greek(s) {
    return Array.from(s, ch => GREEK[ch] || ch).join('');
  }

  class XppClient {
    constructor(root, send) {
      this.root = root;
      this.send = send;
      this.palette = [];
      this.surfaces = new Map();
      this.menus = null;
      this.menuWhich = 0;
      this.state = null;
      this.pendingAsk = null;
      this.typeahead = [];
      this.busy = false;
      this.build();
    }

    colorOf(i) {
      if (i === -1 || i === undefined) return '#ffffff';
      if (i === 0) return '#000000';
      return this.palette[i] || '#000000';
    }

    /* ---- layout ---------------------------------------------------------------- */

    build() {
      const r = this.root;
      r.classList.add('xpp');
      r.innerHTML = '';
      this.titleBar = el('div', 'xpp-title', 'XPP');
      this.menuPanel = el('div', 'xpp-menu');
      this.plotArea = el('div', 'xpp-plots');
      this.mainHost = el('div', 'xpp-main-plot');
      this.plotArea.appendChild(this.mainHost);
      this.sidePanel = el('div', 'xpp-side');
      this.status = el('div', 'xpp-status');
      this.hint = el('span', 'xpp-hint');
      this.progress = el('span', 'xpp-progress');
      this.status.append(this.hint, this.progress);
      this.extraWindows = el('div', 'xpp-extra');
      this.errorBar = el('div', 'xpp-errors');
      this.logBox = el('details', 'xpp-log');
      this.logSummary = el('summary', '', 'Messages');
      this.logText = el('pre', 'xpp-log-text');
      this.logBox.append(this.logSummary, this.logText);
      this.logLines = 0;
      const middle = el('div', 'xpp-middle');
      middle.append(this.menuPanel, this.plotArea, this.sidePanel);
      r.append(this.titleBar, this.errorBar, middle, this.extraWindows, this.logBox, this.status);
      this.dialogLayer = el('div', 'xpp-dialogs');
      r.appendChild(this.dialogLayer);

      r.tabIndex = 0;
      r.addEventListener('keydown', e => this.onKey(e));
      new ResizeObserver(() => this.sendMainSize()).observe(this.mainHost);
      /* side by side needs room; a narrow panel stacks menus, plot and values */
      new ResizeObserver(() => r.classList.toggle('xpp-narrow', r.clientWidth < 760)).observe(r);
    }

    /* The error the user must see. One at a time: a newer one replaces it,
       and starting the next command clears it (the history stays under
       Messages). A load failure or crash is sticky: nothing else will run. */
    showError(text, detail, sticky) {
      if (this.errorSticky && !sticky) return;
      this.errorBar.innerHTML = '';
      this.errorSticky = !!sticky;
      const item = el('div', 'xpp-error-item');
      const msg = el('div', 'xpp-error-text', text);
      const close = el('button', 'xpp-close', '\u00d7');
      close.title = 'Dismiss';
      close.addEventListener('click', () => item.remove());
      item.append(msg, close);
      if (detail) {
        const pre = el('pre', 'xpp-error-detail', detail);
        item.appendChild(pre);
      }
      this.errorBar.appendChild(item);
    }

    clearError() {
      if (!this.errorSticky) this.errorBar.innerHTML = '';
    }

    /* text the program printed (what xppaut writes to its terminal) */
    log(text, scan = true) {
      const lines = String(text).replace(/\r/g, '').split('\n');
      if (lines[lines.length - 1] === '') lines.pop();
      for (const line of lines) {
        this.logText.textContent += line + '\n';
        this.logLines++;
        /* xppaut reports model and file problems only as printed text */
        const known = this.lastError && line.includes(this.lastError);
        if (scan && !known && /error|illegal|not found|can't|cannot|unable|bad |undefined|failed/i.test(line)) {
          this.recentErrors = (this.recentErrors || []).concat(line.trim()).slice(-10);
          clearTimeout(this.errorTimer);
          this.errorTimer = setTimeout(() => {
            if (this.recentErrors.length) this.showError('XPP reported a problem', this.recentErrors.join('\n'));
            this.recentErrors = [];
          }, 200);
        }
      }
      const extra = this.logText.textContent.length - 200000;
      if (extra > 0) this.logText.textContent = this.logText.textContent.slice(extra);
      this.logSummary.textContent = `Messages (${this.logLines})`;
      this.logText.scrollTop = this.logText.scrollHeight;
    }

    /* the server process ended */
    exited(code) {
      /* the summary below already shows what was printed */
      clearTimeout(this.errorTimer);
      this.recentErrors = [];
      this.busy = true;
      if (this.pendingAsk && this.pendingAsk.close) this.pendingAsk.close();
      this.pendingAsk = null;
      const tail = this.logText.textContent.trimEnd().split('\n').slice(-25).join('\n');
      if (!this.menus) {
        this.showError('XPP could not load this file. What it printed:', tail || '(nothing)', true);
      } else if (code) {
        this.showError(`XPP stopped unexpectedly (exit code ${code}). Last output:`, tail || '(nothing)', true);
      } else {
        this.hint.textContent = 'XPP has exited.';
      }
      this.logBox.open = !this.menus || !!code;
    }

    sendMainSize() {
      const s = this.surfaces.get(1);
      if (!s) return;
      const w = Math.max(200, Math.floor(this.mainHost.clientWidth));
      const h = Math.max(150, Math.floor(this.mainHost.clientHeight));
      if (w === s.canvas.width && h === s.canvas.height) return;
      clearTimeout(this.sizeTimer);
      this.sizeTimer = setTimeout(() => {
        s.resize(w, h);
        this.send({cmd: 'size', win: 1, w, h});
      }, 150);
    }

    renderMenu() {
      const m = this.menus;
      if (!m) return;
      const which = ['main', 'file', 'num'][this.menuWhich] || 'main';
      const items = m[which], keys = m[which + '_keys'], hints = m[which + '_hints'];
      this.menuPanel.innerHTML = '';
      items.forEach((label, i) => {
        const b = el('button', 'xpp-menu-item', label);
        b.title = (hints && hints[i]) || '';
        const key = keys[i];
        b.addEventListener('click', () => this.key(key === '' ? 'Escape' : key));
        b.addEventListener('mouseenter', () => { this.hint.textContent = b.title; });
        this.menuPanel.appendChild(b);
      });
    }

    renderState() {
      const st = this.state;
      if (!st) return;
      if (!this.sideBuilt) {
        this.sidePanel.innerHTML = '';
        this.parTable = this.valueTable('Parameters', 'par');
        this.icTable = this.valueTable('Initial conditions', 'ic');
        const go = el('button', 'xpp-go', 'Integrate');
        go.title = 'Initialconds / Go (i g)';
        go.addEventListener('click', () => this.keys(['i', 'g']));
        this.sidePanel.append(go, this.icTable.box, this.parTable.box);
        this.sideBuilt = true;
      }
      this.fillTable(this.parTable, st.pars);
      this.fillTable(this.icTable, st.ics);
      this.menuWhich = st.menu;
    }

    valueTable(title, kind) {
      const box = el('div', 'xpp-values');
      box.appendChild(el('div', 'xpp-values-title', title));
      const list = el('div', 'xpp-values-list');
      box.appendChild(list);
      return {box, list, kind, inputs: new Map()};
    }

    fillTable(t, pairs) {
      for (const [name, value] of pairs) {
        let input = t.inputs.get(name);
        if (!input) {
          const row = el('label', 'xpp-value');
          row.appendChild(el('span', 'xpp-value-name', name));
          input = el('input', 'xpp-value-input');
          input.spellcheck = false;
          input.addEventListener('keydown', e => {
            e.stopPropagation();
            if (e.key === 'Enter') {
              const v = Number(input.value);
              if (Number.isFinite(v)) this.send({cmd: 'set', kind: t.kind, name, value: v});
              input.blur();
            } else if (e.key === 'Escape') {
              input.value = input.dataset.value;
              input.blur();
            }
          });
          row.appendChild(input);
          t.list.appendChild(row);
          t.inputs.set(name, input);
        }
        const s = String(value);
        input.dataset.value = s;
        if (document.activeElement !== input) input.value = s;
      }
    }

    /* ---- input --------------------------------------------------------------------- */

    key(k) {
      if (this.pendingAsk && this.answerByKey(k)) return;
      if (this.busy) {
        if (k === 'Escape') this.send({cmd: 'key', key: k});
        else this.typeahead.push(k);
        return;
      }
      this.busy = true;
      this.clearError();
      this.send({cmd: 'key', key: k});
    }

    keys(list) {
      list.forEach(k => this.key(k));
    }

    onKey(e) {
      if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) return;
      const k = keyName(e);
      if (!k) return;
      e.preventDefault();
      this.key(k);
    }

    /* ---- events -------------------------------------------------------------------- */

    receive(ev) {
      switch (ev.ev) {
        case 'hello':
          this.menus = ev.menus;
          this.titleBar.textContent = ev.title;
          this.charCell = ev.char;
          this.renderMenu();
          break;
        case 'palette': this.palette = ev.colors; break;
        case 'window': this.onWindow(ev); break;
        case 'draw': {
          const s = this.surfaces.get(ev.win);
          if (s) s.run(ev.ops);
          break;
        }
        case 'state': this.state = ev; this.renderState(); this.renderMenu(); break;
        case 'menu': this.menuWhich = ev.which; this.renderMenu(); break;
        case 'title': this.titleBar.textContent = ev.text; break;
        case 'message': this.onMessage(ev); break;
        case 'progress': this.progress.textContent = ev.of ? `${ev.n}/${ev.of}` : ''; break;
        case 'idle':
          this.busy = false;
          this.progress.textContent = '';
          if (this.typeahead.length) this.key(this.typeahead.shift());
          break;
        case 'ask': this.onAsk(ev); break;
        case 'equilibrium': this.showEquilibrium(ev); break;
        case 'source': this.showSource(ev); break;
        case 'ping': this.flash(); break;
        case 'bye': this.hint.textContent = 'XPP has exited.'; break;
        /* sent by the host, not the server */
        case 'log': this.log(ev.text); break;
        case 'exit': this.exited(ev.code); break;
      }
    }

    onMessage(ev) {
      if (ev.error !== undefined) {
        clearTimeout(this.errorTimer); /* the printed copy of this message */
        this.recentErrors = [];
        this.lastError = ev.error.trim();
        this.showError(ev.error);
        this.log('error: ' + ev.error, false);
      } else if (ev.box !== undefined) {
        this.boxHint = ev.box;
        this.hint.textContent = ev.box;
      } else {
        const text = ev.bottom ?? ev.xy ?? ev.auto ?? ev.calc ?? '';
        if (ev.auto !== undefined && this.autoHint) this.autoHint.textContent = text;
        else this.hint.textContent = text;
      }
    }

    flash() {
      this.root.classList.add('xpp-ping');
      setTimeout(() => this.root.classList.remove('xpp-ping'), 150);
    }

    onWindow(ev) {
      if (ev.op === 'create') {
        let s = this.surfaces.get(ev.win);
        if (!s) {
          s = new Surface(this, ev.win, ev.w, ev.h);
          this.surfaces.set(ev.win, s);
          this.placeSurface(s, ev);
        } else s.resize(ev.w, ev.h);
        if (ev.win === 1) this.sendMainSize();
      } else if (ev.op === 'destroy') {
        const s = this.surfaces.get(ev.win);
        if (s) {
          (s.frame || s.canvas).remove();
          this.surfaces.delete(ev.win);
        }
      } else if (ev.op === 'select') {
        for (const s of this.surfaces.values()) s.canvas.classList.toggle('xpp-active', s.id === ev.win);
      }
    }

    placeSurface(s, ev) {
      s.canvas.addEventListener('mousedown', e => this.onCanvasDown(s, e));
      if (ev.win === 1) {
        this.mainHost.appendChild(s.canvas);
        return;
      }
      const frame = el('div', 'xpp-window');
      const bar = el('div', 'xpp-window-title', ev.title || (ev.win <= 10 ? `Window ${ev.win}` : ''));
      frame.append(bar);
      s.frame = frame;
      if (ev.win === 101) {
        /* AUTO: buttons, the diagram, stability circle, info strip, hint */
        bar.textContent = ev.title || 'AUTO';
        const body = el('div', 'xpp-auto');
        const buttons = el('div', 'xpp-auto-buttons');
        for (const [label, opName] of AUTO_BUTTONS) {
          const b = el('button', '', label);
          b.addEventListener('click', () => this.command({cmd: 'auto', op: opName}));
          buttons.appendChild(b);
        }
        const abort = el('button', 'xpp-abort', 'ABORT');
        abort.addEventListener('click', () => this.send({cmd: 'abort'}));
        buttons.appendChild(abort);
        const stab = new Surface(this, 102, 108, 108);
        this.surfaces.set(102, stab);
        buttons.appendChild(stab.canvas);
        const right = el('div', 'xpp-auto-right');
        const info = new Surface(this, 103, ev.w, 45);
        this.surfaces.set(103, info);
        this.autoHint = el('div', 'xpp-auto-hint');
        s.canvas.addEventListener('mousemove', () => {});
        right.append(s.canvas, info.canvas, this.autoHint);
        body.append(buttons, right);
        frame.appendChild(body);
      } else if (ev.win === 104) {
        bar.textContent = 'Animation';
        const buttons = el('div', 'xpp-ani-buttons');
        const add = (label, cmd) => {
          const b = el('button', '', label);
          b.addEventListener('click', () => this.command(cmd));
          buttons.appendChild(b);
        };
        add('File', {cmd: 'ani', op: 'file'});
        add('Reset', {cmd: 'ani', op: 'reset'});
        add('<<<<', {cmd: 'ani', op: 'step', n: -1});
        add('>>>>', {cmd: 'ani', op: 'step', n: 1});
        add('>> x10', {cmd: 'ani', op: 'step', n: 10});
        frame.append(buttons, s.canvas);
      } else {
        frame.appendChild(s.canvas);
        s.canvas.addEventListener('focus', () => this.send({cmd: 'click', win: s.id}));
      }
      this.extraWindows.appendChild(frame);
    }

    command(cmd) {
      if (this.busy) return;
      this.busy = true;
      this.clearError();
      this.send(cmd);
    }

    onCanvasDown(s, e) {
      const a = this.pendingAsk;
      if (!a || (a.win !== undefined && a.win !== s.id && !(a.win === 101 && s.id === 101))) {
        if (s.id >= 2 && s.id <= 10 && !this.busy) this.send({cmd: 'click', win: s.id});
        return;
      }
      const [x, y] = s.at(e);
      if (a.kind === 'mouse') this.answer({x, y});
      else if (a.kind === 'grab') this.answer({x, y});
      else if (a.kind === 'rubber') this.rubber(s, a, x, y);
    }

    rubber(s, a, x0, y0) {
      const snap = s.ctx.getImageData(0, 0, s.canvas.width, s.canvas.height);
      let x1 = x0, y1 = y0;
      const move = e => {
        [x1, y1] = s.at(e);
        s.ctx.putImageData(snap, 0, 0);
        s.ctx.save();
        s.ctx.strokeStyle = '#808080';
        s.ctx.setLineDash([3, 3]);
        s.ctx.beginPath();
        if (a.flag === 1) {
          s.ctx.moveTo(x0, y0);
          s.ctx.lineTo(x1, y1);
        } else s.ctx.rect(Math.min(x0, x1), Math.min(y0, y1), Math.abs(x1 - x0), Math.abs(y1 - y0));
        s.ctx.stroke();
        s.ctx.restore();
      };
      const up = e => {
        window.removeEventListener('mousemove', move);
        window.removeEventListener('mouseup', up);
        [x1, y1] = s.at(e);
        s.ctx.putImageData(snap, 0, 0);
        this.answer({x: x0, y: y0, x2: x1, y2: y1});
      };
      window.addEventListener('mousemove', move);
      window.addEventListener('mouseup', up);
    }

    /* ---- asks ------------------------------------------------------------------------ */

    answer(fields) {
      const a = this.pendingAsk;
      if (!a) return;
      this.pendingAsk = null;
      if (a.close) a.close();
      this.root.classList.remove('xpp-picking');
      this.send(Object.assign({cmd: 'answer', id: a.id}, fields));
      this.root.focus();
    }

    cancel() {
      this.answer({ok: 0});
    }

    /* a key typed while something is being asked */
    answerByKey(k) {
      const a = this.pendingAsk;
      if (a.kind === 'menu' || a.kind === 'choice') {
        if (k === 'Escape') { this.cancel(); return true; }
        const key = k.toLowerCase();
        if (a.keys.includes(key)) { this.answer({key}); return true; }
        if (k === 'Enter' && a.kind === 'menu' && a.def >= 0) { this.answer({key: a.keys[a.def]}); return true; }
        return true;
      }
      if (a.kind === 'mouse' || a.kind === 'rubber' || a.kind === 'alert') {
        if (k === 'Escape' || (a.kind === 'alert' && k === 'Enter')) {
          a.kind === 'alert' ? this.answer({}) : this.cancel();
        }
        return true;
      }
      if (a.kind === 'grab') {
        this.answer({key: k});
        return true;
      }
      return false; /* forms take their keys through their inputs */
    }

    onAsk(a) {
      this.pendingAsk = a;
      this.busy = true;
      switch (a.kind) {
        case 'menu': this.askMenu(a); break;
        case 'choice': this.askChoice(a); break;
        case 'string': this.askForm(a, [a.name], [a.value], a.title || a.name, a.ok, a.cancel); break;
        case 'form': this.askForm(a, a.names, a.values, a.title); break;
        case 'file': this.askForm(a, ['File'], [a.file], `${a.title}  (${a.wild}, in ${a.dir})`); break;
        case 'checklist': this.askChecklist(a); break;
        case 'alert': this.askAlert(a); break;
        case 'mouse':
        case 'rubber':
          this.root.classList.add('xpp-picking');
          this.hint.textContent = this.boxHint || (a.kind === 'mouse' ? 'Click in the plot (Esc cancels)'
            : a.flag === 1 ? 'Drag a line in the plot (Esc cancels)' : 'Drag a box in the plot (Esc cancels)');
          break;
        case 'grab':
          this.hint.textContent = 'Arrows/Tab move, Enter grabs, Esc quits, s/e mark a branch';
          break;
      }
      if (this.typeahead.length && (a.kind === 'menu' || a.kind === 'choice')) {
        const k = this.typeahead.shift();
        this.answerByKey(k);
      }
    }

    dialog(title) {
      const box = el('div', 'xpp-dialog');
      if (title) box.appendChild(el('div', 'xpp-dialog-title', title));
      this.dialogLayer.appendChild(box);
      this.pendingAsk.close = () => box.remove();
      return box;
    }

    askMenu(a) {
      const box = this.dialog(a.title);
      box.classList.add('xpp-popup');
      a.items.forEach((label, i) => {
        const b = el('button', 'xpp-menu-item' + (i === a.def ? ' xpp-default' : ''), label);
        if (a.hints) b.title = a.hints[i] || '';
        b.addEventListener('click', () => this.answer({key: a.keys[i]}));
        box.appendChild(b);
      });
      const c = el('button', 'xpp-cancel', 'Cancel');
      c.addEventListener('click', () => this.cancel());
      box.appendChild(c);
    }

    askChoice(a) {
      const box = this.dialog(a.title);
      box.appendChild(el('div', 'xpp-question', a.question));
      const row = el('div', 'xpp-buttons');
      a.choices.forEach((label, i) => {
        const b = el('button', '', label);
        b.addEventListener('click', () => this.answer({key: a.keys[i]}));
        row.appendChild(b);
      });
      box.appendChild(row);
      row.firstChild.focus();
    }

    askAlert(a) {
      const box = this.dialog('');
      box.appendChild(el('div', 'xpp-question', a.message));
      const b = el('button', '', a.button || 'Ok');
      b.addEventListener('click', () => this.answer({}));
      box.appendChild(b);
      b.focus();
    }

    askForm(a, names, values, title, okLabel, cancelLabel) {
      const box = this.dialog(title);
      const form = el('form', 'xpp-form');
      const inputs = names.map((n, i) => {
        const label = el('label', 'xpp-field');
        const shown = n.replace(/^\*\d/, '');
        label.appendChild(el('span', '', shown));
        const input = el('input');
        input.value = values[i];
        input.spellcheck = false;
        if (a.max) input.maxLength = a.max;
        if (/^\*0/.test(n) && this.state) {
          const list = el('datalist');
          list.id = `xpp-vars-${a.id}-${i}`;
          ['T'].concat(this.state.ics.map(p => p[0])).forEach(v => list.appendChild(el('option', '', v)))
            ;
          label.appendChild(list);
          input.setAttribute('list', list.id);
        }
        label.appendChild(input);
        form.appendChild(label);
        return input;
      });
      const row = el('div', 'xpp-buttons');
      const ok = el('button', '', okLabel || 'Ok');
      ok.type = 'submit';
      const cancel = el('button', '', cancelLabel || 'Cancel');
      cancel.type = 'button';
      cancel.addEventListener('click', () => this.cancel());
      row.append(ok, cancel);
      form.appendChild(row);
      form.addEventListener('submit', e => {
        e.preventDefault();
        const v = inputs.map(i => i.value);
        if (a.kind === 'form') this.answer({values: v});
        else if (a.kind === 'file') this.answer({file: v[0]});
        else this.answer({value: v[0]});
      });
      form.addEventListener('keydown', e => {
        e.stopPropagation();
        if (e.key === 'Escape') this.cancel();
        if (e.key === 'Enter') {
          /* also when a datalist suggestion popup has the key */
          e.preventDefault();
          form.requestSubmit();
        }
      });
      box.appendChild(form);
      inputs[0].focus();
      inputs[0].select();
    }

    askChecklist(a) {
      const box = this.dialog(a.title);
      const boxes = a.names.map((n, i) => {
        const label = el('label', 'xpp-check');
        const c = el('input');
        c.type = 'checkbox';
        c.checked = !!a.flags[i];
        label.append(c, document.createTextNode(' ' + n));
        box.appendChild(label);
        return c;
      });
      const row = el('div', 'xpp-buttons');
      const ok = el('button', '', 'Done');
      ok.addEventListener('click', () => this.answer({flags: boxes.map(c => (c.checked ? 1 : 0))}));
      const cancel = el('button', '', 'Cancel');
      cancel.addEventListener('click', () => this.cancel());
      row.append(ok, cancel);
      box.appendChild(row);
    }

    showEquilibrium(ev) {
      let w = this.eqPanel;
      if (!w) {
        w = this.eqPanel = el('div', 'xpp-window xpp-eq');
        this.extraWindows.appendChild(w);
      }
      w.innerHTML = '';
      const bar = el('div', 'xpp-window-title', `Equilibrium: ${ev.type}`);
      const close = el('button', 'xpp-close', '×');
      close.addEventListener('click', () => { w.remove(); this.eqPanel = null; });
      bar.appendChild(close);
      w.appendChild(bar);
      w.appendChild(el('div', 'xpp-eq-counts',
        `c+ = ${ev.cplus}  c- = ${ev.cminus}  im = ${ev.im}  r+ = ${ev.rplus}  r- = ${ev.rminus}`));
      for (const [name, v] of ev.values) w.appendChild(el('div', 'xpp-eq-value', `${name} = ${Number(v).toPrecision(5)}`));
    }

    showSource(ev) {
      const w = el('div', 'xpp-window xpp-source');
      const bar = el('div', 'xpp-window-title', 'Source');
      const close = el('button', 'xpp-close', '×');
      close.addEventListener('click', () => w.remove());
      bar.appendChild(close);
      const pre = el('pre', '', ev.lines.join('\n'));
      w.append(bar, pre);
      this.extraWindows.appendChild(w);
    }
  }

  global.XppClient = XppClient;
})(typeof window !== 'undefined' ? window : globalThis);
