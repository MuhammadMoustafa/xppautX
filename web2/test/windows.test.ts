import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {Command} from '../src/protocol/types';
import {HELLO} from './hello';

function connected() {
  const sent: Command[] = [];
  const session = new Session({send: command => sent.push(command), open: () => {}, close: () => {}});
  session.store.dispatch({type:'event',ev:HELLO});
  return {session,sent};
}

test('page window actions send registry commands and exact names', () => {
  const {session,sent}=connected();
  session.newWindow();
  session.renameWindow('graph_1','Phase "plane"');
  session.selectWindow('Phase "plane"');
  assert.deepEqual(sent.slice(0,2),[
    {cmd:'window.create'},
    {cmd:'window.rename',window:'graph_1',name:'Phase "plane"'},
  ]);
  assert.ok(sent.every(c=>c.cmd!=='click'));
});

test('closing AUTO sends a named close for every open diagram view', () => {
  const {session,sent}=connected();
  session.store.dispatch({type:'diagram',action:{type:'event',ev:{ev:'auto.diagram',op:'views',n:2,names:['Diagram A','Diagram B']}}});
  session.closeAuto();
  assert.deepEqual(sent,[{cmd:'window.destroy',window:'Diagram A'},{cmd:'window.destroy',window:'Diagram B'}]);
});

test('Hide AUTO and Show AUTO are window.hide and window.show of every view, and the windows event says what is shown', () => {
  const {session,sent}=connected();
  session.store.dispatch({type:'diagram',action:{type:'core',open:true}});
  session.store.dispatch({type:'diagram',action:{type:'event',ev:{ev:'auto.diagram',op:'views',n:2,names:['Diagram A','Diagram B']}}});
  session.showAuto(false);
  assert.deepEqual(sent,[{cmd:'window.hide',window:'Diagram A'},{cmd:'window.hide',window:'Diagram B'}]);
  const list = (shown: boolean) => ({ev:'window.list' as const,windows:[
    {name:'graph_1',kind:'graph' as const,shown:true},
    {name:'Diagram A',kind:'diagram' as const,shown},{name:'Diagram B',kind:'diagram' as const,shown}]});
  session.store.dispatch({type:'event',ev:list(false)});
  assert.equal(session.store.getState().diagram.shown,false);
  session.store.dispatch({type:'event',ev:list(true)});
  assert.equal(session.store.getState().diagram.shown,true);
  assert.equal(session.store.getState().windows.length,3);
});
