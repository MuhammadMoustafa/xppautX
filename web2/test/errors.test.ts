import assert from 'node:assert/strict';
import {test} from 'node:test';
import {errorPlace, errorText, placeWords, unreadFile} from '../src/protocol/errors';

test("an error's text reads as the core's (xpp::Error::text): file:line:col: what, what is unknown left out (W140)", () => {
  assert.equal(errorText({error: 'bad', file: 'm.ode', line: 3, col: 7, source: ''}), 'm.ode:3:7: bad');
  assert.equal(errorText({error: 'bad', file: 'm.ode', line: 3, col: 0, source: ''}), 'm.ode:3: bad');
  assert.equal(errorText({error: 'bad', file: 'm.ode', line: 0, col: 0, source: ''}), 'm.ode: bad');
  assert.equal(errorText({error: 'bad', file: '', line: 5, col: 0, source: ''}), 'line 5: bad');
  assert.equal(errorText({error: 'bad'}), 'bad');
});

test("an error's place: in words, none when it names nothing, a file with no line is one to add", () => {
  assert.equal(errorPlace({error: 'x', file: '', line: 0}), undefined);
  assert.equal(placeWords({file: 'm.ode', line: 3, col: 2, source: ''}), 'm.ode, line 3, column 2');
  assert.equal(placeWords({file: 'm.ode', line: 0, col: 0, source: ''}), 'm.ode');
  assert.equal(placeWords({file: '', line: 4, col: 0, source: ''}), 'line 4');
  assert.equal(unreadFile({file: '/a/b/gone.set', line: 0}), 'gone.set');
  assert.equal(unreadFile({file: 'gone.set', line: 2}), undefined);
  assert.equal(unreadFile({file: ''}), undefined);
});
