import assert from 'node:assert/strict';
import {test} from 'node:test';
import {options} from 'preact';
import {Session} from '../src/session';
import {installTestHook} from '../src/testhook';

test('W166: rendering acknowledgement waits for nested renders and effects, including errors', () => {
  const originalWindow = Object.getOwnPropertyDescriptor(globalThis, 'window');
  const originalRender = options.debounceRendering;
  const originalEffect = options.requestAnimationFrame;
  const queuedRenders: (() => void)[] = [], queuedEffects: (() => void)[] = [];
  const page = {__xppCheckRendering: true, __xpp: undefined as unknown as {rendered: () => boolean}};
  Object.defineProperty(globalThis, 'window', {configurable: true, value: page});
  options.debounceRendering = fn => { queuedRenders.push(fn); };
  options.requestAnimationFrame = fn => { queuedEffects.push(fn); };
  try {
    const session = new Session({send: () => {}, close: () => {}, open: () => {}});
    installTestHook(session);
    assert.equal(page.__xpp.rendered(), true);
    options.debounceRendering!(() => {
      options.requestAnimationFrame!(() => {
        options.debounceRendering!(() => {});
      });
    });
    assert.equal(page.__xpp.rendered(), false);
    queuedRenders.shift()!();
    assert.equal(page.__xpp.rendered(), false, 'the effect is still queued');
    queuedEffects.shift()!();
    assert.equal(page.__xpp.rendered(), false, 'the effect queued another render');
    queuedRenders.shift()!();
    assert.equal(page.__xpp.rendered(), true);
    options.debounceRendering!(() => { throw new Error('render failed'); });
    assert.throws(queuedRenders.shift()!, /render failed/);
    assert.equal(page.__xpp.rendered(), true, 'a failed callback must release its pending count');
    page.__xppCheckRendering = false;
    const beforeRender = options.debounceRendering, beforeEffect = options.requestAnimationFrame;
    installTestHook(session);
    assert.equal(options.debounceRendering, beforeRender);
    assert.equal(options.requestAnimationFrame, beforeEffect);
    assert.equal(page.__xpp.rendered(), false, 'an ordinary page does not claim to observe rendering');
  } finally {
    options.debounceRendering = originalRender;
    options.requestAnimationFrame = originalEffect;
    if (originalWindow) Object.defineProperty(globalThis, 'window', originalWindow);
    else Reflect.deleteProperty(globalThis, 'window');
  }
});
