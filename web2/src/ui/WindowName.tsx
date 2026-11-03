import {useRef, useState} from 'preact/hooks';
import {useSession} from './context';

/** Only presentation lives here: the core validates names and uniqueness. */
export function WindowName({name,disabled=false}:{name:string;disabled?:boolean}) {
  const session=useSession();
  const [editing,setEditing]=useState(false);
  const input=useRef<HTMLInputElement>(null);
  if(!editing)return <button class="window-name small" disabled={disabled} onDblClick={()=>setEditing(true)}
    onClick={()=>setEditing(true)} title="Rename window">{name}</button>;
  return <form onSubmit={e=>{e.preventDefault();session.renameWindow(name,input.current!.value);setEditing(false);}}>
    <input ref={input} autoFocus aria-label="Window name" defaultValue={name} onKeyDown={e=>{e.stopPropagation();if(e.key==='Escape')setEditing(false);}}/>
    <button type="submit">Rename</button><button type="button" onClick={()=>setEditing(false)}>Cancel</button>
  </form>;
}
