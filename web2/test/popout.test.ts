import {strict as assert} from 'node:assert';
import {test} from 'node:test';
import {popOutRequest, viewKeyText, viewLabel} from '../src/popout';
import {initialState, reduce} from '../src/store/state';

test('a pop-out address names its view: kind and name, in the fragment', () => {
  assert.deepEqual(popOutRequest('#popout=graph%3Agraph_1'), {kind: 'graph', name: 'graph_1'});
  assert.deepEqual(popOutRequest(`#popout=${encodeURIComponent('autox:phase plane: 2')}`), {kind: 'autox', name: 'phase plane: 2'});
  assert.deepEqual(popOutRequest('#popout=dock%3Atable'), {kind: 'dock', name: 'table'});
  assert.equal(popOutRequest(''), null);
  assert.equal(popOutRequest('#other'), null);
});

test('an address that names no view is an error, never a guess', () => {
  for (const bad of ['#popout=', '#popout=graph', '#popout=graph%3A', '#popout=window%3Ax', '#popout=dock%3Anowhere', '#popout=%E0%A4%A'])
    assert.equal(typeof popOutRequest(bad), 'string', bad);
});

test('a view is called by its name, a dock tab by its label', () => {
  assert.equal(viewLabel({kind: 'graph', name: 'g'}), 'g');
  assert.equal(viewLabel({kind: 'dock', name: 'text'}), 'Model');
  assert.equal(viewKeyText({kind: 'dock', name: 'text'}), 'dock:text');
});

test('views go out and come back, each once', () => {
  const out = reduce(initialState, {type: 'popout', key: 'graph:a', out: true});
  assert.deepEqual(out.popouts, ['graph:a']);
  assert.equal(reduce(out, {type: 'popout', key: 'graph:a', out: true}), out);
  assert.deepEqual(reduce(out, {type: 'popout', key: 'dock:table', out: true}).popouts, ['graph:a', 'dock:table']);
  assert.deepEqual(reduce(out, {type: 'popout', key: 'graph:a', out: false}).popouts, []);
  assert.equal(reduce(initialState, {type: 'popout', key: 'graph:a', out: false}), initialState);
});
