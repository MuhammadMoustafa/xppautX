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
  session.renameWindow('main','Phase "plane"');
  session.selectWindow('Phase "plane"');
  assert.deepEqual(sent.slice(0,2),[
    {cmd:'window.new'},
    {cmd:'window.rename',window:'main',name:'Phase "plane"'},
  ]);
  assert.ok(sent.every(c=>c.cmd!=='click'));
});

test('closing autoX sends a named close for every open diagram view', () => {
  const {session,sent}=connected();
  session.store.dispatch({type:'diagram',action:{type:'event',ev:{ev:'autox.diagram',op:'views',n:2,names:['Diagram A','Diagram B']}}});
  session.closeAutox();
  assert.deepEqual(sent,[{cmd:'window.close',window:'Diagram A'},{cmd:'window.close',window:'Diagram B'}]);
});

test('Hide autoX and Show autoX are window.hide and window.show of every view, and the windows event says what is shown', () => {
  const {session,sent}=connected();
  session.store.dispatch({type:'diagram',action:{type:'core',open:true}});
  session.store.dispatch({type:'diagram',action:{type:'event',ev:{ev:'autox.diagram',op:'views',n:2,names:['Diagram A','Diagram B']}}});
  session.showAutox(false);
  assert.deepEqual(sent,[{cmd:'window.hide',window:'Diagram A'},{cmd:'window.hide',window:'Diagram B'}]);
  const list = (shown: boolean) => ({ev:'window.list' as const,windows:[
    {name:'main',kind:'graph' as const,shown:true,group:'first' as const},
    {name:'Diagram A',kind:'diagram' as const,shown,group:'first' as const},{name:'Diagram B',kind:'diagram' as const,shown,group:'first' as const}],
    split:{direction:'none' as const,share:0.5,shown:{first:'main',second:''},selected:'main'}});
  session.store.dispatch({type:'event',ev:list(false)});
  assert.equal(session.store.getState().diagram.shown,false);
  session.store.dispatch({type:'event',ev:list(true)});
  assert.equal(session.store.getState().diagram.shown,true);
  assert.equal(session.store.getState().windows.length,3);
  /* the split view's state is the same event's (W720) */
  const split={direction:'right' as const,share:0.3,shown:{first:'main',second:'voltage'},selected:'voltage'};
  session.store.dispatch({type:'event',ev:{ev:'window.list',windows:[{name:'main',kind:'graph',shown:true,group:'first'},{name:'voltage',kind:'graph',shown:true,group:'second'}],split}});
  assert.deepEqual(session.store.getState().split,split);
  assert.equal(session.store.getState().windows[1].group,'second');
});
