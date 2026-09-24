/*! xppautX web2, GPL-2.0. Bundles Preact, Copyright (c) 2015-present Jason Miller, and uPlot, Copyright (c) 2022 Leon Sorokin, both under the MIT License. */
"use strict";(()=>{var Mi,Ae,_l,ef,vn,Pl,Ml,Cl,oa,Ti,zo,Dl,sa,ia,ra,Ll,Ei={},Ai=[],tf=/acit|ex(?:s|g|n|p|$)|rph|grid|ows|mnc|ntw|ine[ch]|zoo|^ord|itera/i,Ci=Array.isArray;function cn(e,t){for(var n in t)e[n]=t[n];return e}function la(e){e&&e.parentNode&&e.parentNode.removeChild(e)}function nf(e,t,n){var o,i,r,a={};for(r in t)r=="key"?o=t[r]:r=="ref"?i=t[r]:a[r]=t[r];if(arguments.length>2&&(a.children=arguments.length>3?Mi.call(arguments,2):n),typeof e=="function"&&e.defaultProps!=null)for(r in e.defaultProps)a[r]===void 0&&(a[r]=e.defaultProps[r]);return Si(e,a,o,i,null)}function Si(e,t,n,o,i){var r={type:e,props:t,key:n,ref:o,__k:null,__:null,__b:0,__e:null,__c:null,constructor:void 0,__v:i??++_l,__i:-1,__u:0};return i==null&&Ae.vnode!=null&&Ae.vnode(r),r}function ve(e){return e.children}function Pi(e,t){this.props=e,this.context=t}function Rn(e,t){if(t==null)return e.__?Rn(e.__,e.__i+1):null;for(var n;t<e.__k.length;t++)if((n=e.__k[t])!=null&&n.__e!=null)return n.__e;return typeof e.type=="function"?Rn(e):null}function of(e){if(e.__P&&e.__d){var t=e.__v,n=t.__e,o=[],i=[],r=cn({},t);r.__v=t.__v+1,Ae.vnode&&Ae.vnode(r),ca(e.__P,r,t,e.__n,e.__P.namespaceURI,32&t.__u?[n]:null,o,n??Rn(t),!!(32&t.__u),i),r.__v=t.__v,r.__.__k[r.__i]=r,Rl(o,r,i),t.__e=t.__=null,r.__e!=n&&Ol(r)}}function Ol(e){if((e=e.__)!=null&&e.__c!=null)return e.__e=e.__c.base=null,e.__k.some(function(t){if(t!=null&&t.__e!=null)return e.__e=e.__c.base=t.__e}),Ol(e)}function aa(e){(!e.__d&&(e.__d=!0)&&vn.push(e)&&!_i.__r++||Pl!=Ae.debounceRendering)&&((Pl=Ae.debounceRendering)||Ml)(_i)}function _i(){try{for(var e,t=1;vn.length;)vn.length>t&&vn.sort(Cl),e=vn.shift(),t=vn.length,of(e)}finally{vn.length=_i.__r=0}}function Il(e,t,n,o,i,r,a,l,d,c,p){var m,u,f,w,k,A,C=o&&o.__k||Ai,P=t.length;for(d=rf(n,t,C,d,P),m=0;m<P;m++)(f=n.__k[m])!=null&&(u=f.__i!=-1&&C[f.__i]||Ei,f.__i=m,A=ca(e,f,u,i,r,a,l,d,c,p),w=f.__e,f.ref&&u.ref!=f.ref&&(u.ref&&da(u.ref,null,f),p.push(f.ref,f.__c||w,f)),k==null&&w!=null&&(k=w),4&f.__u?(d=$l(f,d,e),u.__e&&(u.__e=null)):typeof f.type=="function"&&A!==void 0?d=A:w&&(d=w.nextSibling),f.__u&=-7);return n.__e=k,d}function rf(e,t,n,o,i){var r,a,l,d,c,p=n.length,m=p,u=0;for(e.__k=new Array(i),r=0;r<i;r++)(a=t[r])!=null&&typeof a!="boolean"&&typeof a!="function"?(typeof a=="string"||typeof a=="number"||typeof a=="bigint"||a.constructor==String?a=e.__k[r]=Si(null,a,null,null,null):Ci(a)?a=e.__k[r]=Si(ve,{children:a},null,null,null):a.constructor===void 0&&a.__b>0?a=e.__k[r]=Si(a.type,a.props,a.key,a.ref?a.ref:null,a.__v):e.__k[r]=a,d=r+u,a.__=e,a.__b=e.__b+1,l=null,(c=a.__i=af(a,n,d,m))!=-1&&(m--,(l=n[c])&&(l.__u|=2)),l==null||l.__v==null?(c==-1&&(i>p?u--:i<p&&u++),typeof a.type!="function"&&(a.__u|=4)):c!=d&&(c==d-1?u--:c==d+1?u++:(c>d?u--:u++,a.__u|=4))):e.__k[r]=null;if(m)for(r=0;r<p;r++)(l=n[r])!=null&&(2&l.__u)==0&&(l.__e==o&&(o=Rn(l)),ql(l,l));return o}function $l(e,t,n){var o,i;if(typeof e.type=="function"){for(o=e.__k,i=0;o&&i<o.length;i++)o[i]&&(o[i].__=e,t=$l(o[i],t,n));return t}e.__e!=t&&(t&&e.type&&!t.parentNode&&(t=Rn(e)),t=n.insertBefore(e.__e,t||null));do t=t&&t.nextSibling;while(t!=null&&t.nodeType==8);return t}function af(e,t,n,o){var i,r,a,l=e.key,d=e.type,c=t[n],p=c!=null&&(2&c.__u)==0;if(c===null&&l==null||p&&l==c.key&&d==c.type)return n;if(o>(p?1:0)){for(i=n-1,r=n+1;i>=0||r<t.length;)if((c=t[a=i>=0?i--:r++])!=null&&(2&c.__u)==0&&l==c.key&&d==c.type)return a}return-1}function El(e,t,n){t[0]=="-"?e.setProperty(t,n??""):e[t]=n==null?"":typeof n!="number"||tf.test(t)?n:n+"px"}function ki(e,t,n,o,i){var r,a;e:if(t=="style")if(typeof n=="string")e.style.cssText=n;else{if(typeof o=="string"&&(e.style.cssText=o=""),o)for(t in o)n&&t in n||El(e.style,t,"");if(n)for(t in n)o&&n[t]==o[t]||El(e.style,t,n[t])}else if(t[0]=="o"&&t[1]=="n")r=t!=(t=t.replace(Dl,"$1")),a=t.toLowerCase(),t=a in e||t=="onFocusOut"||t=="onFocusIn"?a.slice(2):t.slice(2),e.l||(e.l={}),e.l[t+r]=n,n?o?n[zo]=o[zo]:(n[zo]=sa,e.addEventListener(t,r?ra:ia,r)):e.removeEventListener(t,r?ra:ia,r);else{if(i=="http://www.w3.org/2000/svg")t=t.replace(/xlink(H|:h)/,"h").replace(/sName$/,"s");else if(t!="width"&&t!="height"&&t!="href"&&t!="list"&&t!="form"&&t!="tabIndex"&&t!="download"&&t!="rowSpan"&&t!="colSpan"&&t!="role"&&t!="popover"&&t in e)try{e[t]=n??"";break e}catch{}typeof n=="function"||(n==null||n===!1&&t[4]!="-"?e.removeAttribute(t):e.setAttribute(t,t=="popover"&&n==1?"":n))}}function Al(e){return function(t){if(this.l){var n=this.l[t.type+e];if(t[Ti]==null)t[Ti]=sa++;else if(t[Ti]<n[zo])return;return n(Ae.event?Ae.event(t):t)}}}function ca(e,t,n,o,i,r,a,l,d,c){var p,m,u,f,w,k,A,C,P,$,E,U,R,N,x,v,T=t.type;if(t.constructor!==void 0)return null;128&n.__u&&(d=!!(32&n.__u),r=[l=t.__e=n.__e]),(p=Ae.__b)&&p(t);e:if(typeof T=="function"){m=a.length;try{if(P=t.props,$=T.prototype&&T.prototype.render,E=(p=T.contextType)&&o[p.__c],U=p?E?E.props.value:p.__:o,n.__c?C=(u=t.__c=n.__c).__=u.__E:($?t.__c=u=new T(P,U):(t.__c=u=new Pi(P,U),u.constructor=T,u.render=lf),E&&E.sub(u),u.state||(u.state={}),u.__n=o,f=u.__d=!0,u.__h=[],u._sb=[]),$&&u.__s==null&&(u.__s=u.state),$&&T.getDerivedStateFromProps!=null&&(u.__s==u.state&&(u.__s=cn({},u.__s)),cn(u.__s,T.getDerivedStateFromProps(P,u.__s))),w=u.props,k=u.state,u.__v=t,f)$&&T.getDerivedStateFromProps==null&&u.componentWillMount!=null&&u.componentWillMount(),$&&u.componentDidMount!=null&&u.__h.push(u.componentDidMount);else{if($&&T.getDerivedStateFromProps==null&&P!==w&&u.componentWillReceiveProps!=null&&u.componentWillReceiveProps(P,U),t.__v==n.__v||!u.__e&&u.shouldComponentUpdate!=null&&u.shouldComponentUpdate(P,u.__s,U)===!1){t.__v!=n.__v&&(u.props=P,u.state=u.__s,u.__d=!1),t.__e=n.__e,t.__k=n.__k,t.__k.some(function(L){L&&(L.__=t)}),Ai.push.apply(u.__h,u._sb),u._sb=[],u.__h.length&&a.push(u),l=Rn(n);break e}u.componentWillUpdate!=null&&u.componentWillUpdate(P,u.__s,U),$&&u.componentDidUpdate!=null&&u.__h.push(function(){u.componentDidUpdate(w,k,A)})}if(u.context=U,u.props=P,u.__P=e,u.__e=!1,R=Ae.__r,N=0,$)u.state=u.__s,u.__d=!1,R&&R(t),p=u.render(u.props,u.state,u.context),Ai.push.apply(u.__h,u._sb),u._sb=[];else do u.__d=!1,R&&R(t),p=u.render(u.props,u.state,u.context),u.state=u.__s;while(u.__d&&++N<25);u.state=u.__s,u.getChildContext!=null&&(o=cn(cn({},o),u.getChildContext())),$&&!f&&u.getSnapshotBeforeUpdate!=null&&(A=u.getSnapshotBeforeUpdate(w,k)),x=p!=null&&p.type===ve&&p.key==null?Fl(p.props.children):p,l=Il(e,Ci(x)?x:[x],t,n,o,i,r,a,l,d,c),u.base=t.__e,t.__u&=-161,u.__h.length&&a.push(u),C&&(u.__E=u.__=null)}catch(L){if(a.length=m,t.__v=null,d||r!=null){if(L.then){for(t.__u|=d?160:128;l&&l.nodeType==8&&l.nextSibling;)l=l.nextSibling;r!=null&&(r[r.indexOf(l)]=null),t.__e=l}else if(r!=null)for(v=r.length;v--;)la(r[v])}else t.__e=n.__e;t.__k==null&&(t.__k=n.__k||[]),L.then||Nl(t),Ae.__e(L,t,n)}}else r==null&&t.__v==n.__v?(t.__k=n.__k,t.__e=n.__e):l=t.__e=sf(n.__e,t,n,o,i,r,a,d,c);return(p=Ae.diffed)&&p(t),128&t.__u?void 0:l}function Nl(e){e&&(e.__c&&(e.__c.__e=!0),e.__k&&e.__k.some(Nl))}function Rl(e,t,n){for(var o=0;o<n.length;o++)da(n[o],n[++o],n[++o]);Ae.__c&&Ae.__c(t,e),e.some(function(i){try{e=i.__h,i.__h=[],e.some(function(r){r.call(i)})}catch(r){Ae.__e(r,i.__v)}})}function Fl(e){return typeof e!="object"||e==null||e.__b>0?e:Ci(e)?e.map(Fl):e.constructor!==void 0?null:cn({},e)}function sf(e,t,n,o,i,r,a,l,d){var c,p,m,u,f,w,k,A=n.props||Ei,C=t.props,P=t.type;if(P=="svg"?i="http://www.w3.org/2000/svg":P=="math"?i="http://www.w3.org/1998/Math/MathML":i||(i="http://www.w3.org/1999/xhtml"),r!=null){for(c=0;c<r.length;c++)if((f=r[c])&&"setAttribute"in f==!!P&&(P?f.localName==P:f.nodeType==3)){e=f,r[c]=null;break}}if(e==null){if(P==null)return document.createTextNode(C);e=document.createElementNS(i,P,C.is&&C),l&&(Ae.__m&&Ae.__m(t,r),l=!1),r=null}if(P==null)A===C||l&&e.data==C||(e.data=C);else{if(r=P=="textarea"&&C.defaultValue!=null?null:r&&Mi.call(e.childNodes),!l&&r!=null)for(A={},c=0;c<e.attributes.length;c++)A[(f=e.attributes[c]).name]=f.value;for(c in A)f=A[c],c=="dangerouslySetInnerHTML"?m=f:c=="children"||c in C||c=="value"&&"defaultValue"in C||c=="checked"&&"defaultChecked"in C||ki(e,c,null,f,i);for(c in C)f=C[c],c=="children"?u=f:c=="dangerouslySetInnerHTML"?p=f:c=="value"?w=f:c=="checked"?k=f:l&&typeof f!="function"||A[c]===f||ki(e,c,f,A[c],i);if(p)l||m&&(p.__html==m.__html||p.__html==e.innerHTML)||(e.innerHTML=p.__html),t.__k=[];else if(m&&(e.innerHTML=""),Il(t.type=="template"?e.content:e,Ci(u)?u:[u],t,n,o,P=="foreignObject"?"http://www.w3.org/1999/xhtml":i,r,a,r?r[0]:n.__k&&Rn(n,0),l,d),r!=null)for(c=r.length;c--;)la(r[c]);l&&P!="textarea"||(c="value",P=="progress"&&w==null?e.removeAttribute("value"):w!=null&&(w!==e[c]||P=="progress"&&!w||P=="option"&&w!=A[c])&&ki(e,c,w,A[c],i),c="checked",k!=null&&k!=e[c]&&ki(e,c,k,A[c],i))}return e}function da(e,t,n){try{if(typeof e=="function"){var o=typeof e.__u=="function";o&&e.__u(),o&&t==null||(e.__u=e(t))}else e.current=t}catch(i){Ae.__e(i,n)}}function ql(e,t,n){var o,i;if(Ae.unmount&&Ae.unmount(e),(o=e.ref)&&(o.current&&o.current!=e.__e||da(o,null,t)),(o=e.__c)!=null){if(o.componentWillUnmount)try{o.componentWillUnmount()}catch(r){Ae.__e(r,t)}o.base=o.__P=o.__n=null}if(o=e.__k)for(i=0;i<o.length;i++)o[i]&&ql(o[i],t,n||typeof e.type!="function");n||la(e.__e),e.__c=e.__=e.__e=void 0}function lf(e,t,n){return this.constructor(e,n)}function Hl(e,t,n){var o,i,r,a;t==document&&(t=document.documentElement),Ae.__&&Ae.__(e,t),i=(o=typeof n=="function")?null:n&&n.__k||t.__k,r=[],a=[],ca(t,e=(!o&&n||t).__k=nf(ve,null,[e]),i||Ei,Ei,t.namespaceURI,!o&&n?[n]:i?null:t.firstChild?Mi.call(t.childNodes):null,r,!o&&n?n:i?i.__e:t.firstChild,o,a),Rl(r,e,a),e.props.children=null}function zl(e){function t(n){var o,i;return this.getChildContext||(o=new Set,(i={})[t.__c]=this,this.getChildContext=function(){return i},this.componentWillUnmount=function(){o=null},this.shouldComponentUpdate=function(r){this.props.value!=r.value&&o.forEach(function(a){a.__e=!0,aa(a)})},this.sub=function(r){o.add(r);var a=r.componentWillUnmount;r.componentWillUnmount=function(){o&&o.delete(r),a&&a.call(r)}}),n.children}return t.__c="__cC"+Ll++,t.__=e,t.Provider=t.__l=(t.Consumer=function(n,o){return n.children(o)}).contextType=t,t}Mi=Ai.slice,Ae={__e:function(e,t,n,o){for(var i,r,a;t=t.__;)if((i=t.__c)&&!i.__)try{if((r=i.constructor)&&r.getDerivedStateFromError!=null&&(i.setState(r.getDerivedStateFromError(e)),a=i.__d),i.componentDidCatch!=null&&(i.componentDidCatch(e,o||{}),a=i.__d),a)return i.__E=i}catch(l){e=l}throw e}},_l=0,ef=function(e){return e!=null&&e.constructor===void 0},Pi.prototype.setState=function(e,t){var n;n=this.__s!=null&&this.__s!=this.state?this.__s:this.__s=cn({},this.state),typeof e=="function"&&(e=e(cn({},n),this.props)),e&&cn(n,e),e!=null&&this.__v&&(t&&this._sb.push(t),aa(this))},Pi.prototype.forceUpdate=function(e){this.__v&&(this.__e=!0,e&&this.__h.push(e),aa(this))},Pi.prototype.render=ve,vn=[],Ml=typeof Promise=="function"?Promise.prototype.then.bind(Promise.resolve()):setTimeout,Cl=function(e,t){return e.__v.__b-t.__v.__b},_i.__r=0,oa=Math.random().toString(8),Ti="__d"+oa,zo="__a"+oa,Dl=/(PointerCapture)$|Capture$/i,sa=0,ia=Al(!1),ra=Al(!0),Ll=0;var Di=class{constructor(t=location.search,n="/"){this.token=t;this.base=n}url(t){return`${this.base}files${t===void 0?"":"/"+encodeURIComponent(t)}${this.token}`}async list(){let t=await fetch(this.url(),{cache:"no-store"});if(!t.ok)throw new Error(`listing the model's folder: ${t.status} ${await t.text()}`);return(await t.json()).files}async get(t){let n=await fetch(this.url(t),{cache:"no-store"});if(n.status===404)return null;if(!n.ok)throw new Error(`reading ${t}: ${n.status} ${await n.text()}`);return n.blob()}async put(t,n){let o=await fetch(this.url(t),{method:"PUT",body:n});if(!o.ok)throw new Error(`copying ${t} into the model's folder: ${await o.text()}`);return o.json()}};async function ua(e){let t=await crypto.subtle.digest("SHA-256",await e.arrayBuffer());return[...new Uint8Array(t)].map(n=>n.toString(16).padStart(2,"0")).join("")}var Li=class{constructor(t=location.search,n="/"){this.token=t;this.base=n;this.source=null}send(t){fetch(`${this.base}cmd${this.token}`,{method:"POST",body:JSON.stringify(t)})}open(t,n){let o=new EventSource(`${this.base}events${this.token}`);o.onopen=()=>n(!0),o.onerror=()=>n(!1),o.onmessage=i=>{t(JSON.parse(i.data))},this.source=o}close(){this.source?.close(),this.source=null}};function Bt(e,t){let n=document.createElement("a");n.href=t,n.download=e,document.body.appendChild(n),n.click(),n.remove()}function pa(e){return Number.isFinite(e)?String(Number(e.toPrecision(9))):String(e)}function cf(e){let t=[];return e.curves.forEach((n,o)=>{o&&t.push(""),t.push(`# ${n.label}`),t.push(e.t?"row,T,x,y":"row,x,y");for(let i=0;i<n.xs.length;i++){let r=n.row0+i,a=`${pa(n.xs[i])},${pa(n.ys[i])}`;t.push(e.t?`${r},${pa(e.t[r])},${a}`:`${r},${a}`)}}),t.join(`
`)+`
`}function Xl(e,t){let n=URL.createObjectURL(new Blob([cf(t)],{type:"text/csv"}));Bt(e,n),setTimeout(()=>URL.revokeObjectURL(n),1e3)}var Oi=()=>window,Ii=()=>typeof Oi().showOpenFilePicker=="function",Ul=()=>typeof Oi().showSaveFilePicker=="function",Vl=e=>e instanceof DOMException&&e.name==="AbortError";async function $i(e){try{let t=await Oi().showOpenFilePicker({multiple:e});return await Promise.all(t.map(n=>n.getFile()))}catch(t){if(Vl(t))return null;throw t}}async function Kl(e){try{return await Oi().showSaveFilePicker({suggestedName:e})}catch(t){if(Vl(t))return null;throw t}}async function Bl(e,t){let n=await e.createWritable();await n.write(t),await n.close()}function Ni(e,t){let n=URL.createObjectURL(t);Bt(e,n),setTimeout(()=>URL.revokeObjectURL(n),1e4)}function jl(e){return-e||0}function Wl(e,t){return t-e}function Yl(e,t){let n=Math.max(1,t);switch(e){case"ArrowUp":return 1;case"ArrowDown":return-1;case"PageUp":return n;case"PageDown":return-n;default:return null}}function Gl(e,t,n){let o=e+t;return n?{send:0,pending:o}:{send:o,pending:0}}function Ri(e){let t="";for(let o=0;o<e.length;o+=8192)t+=String.fromCharCode(...e.subarray(o,o+8192));return btoa(t)}var ha=[0,51,102,153,204,255],df=216,Zl=8;function uf(){let e=new Uint8Array(768);for(let t=0;t<df;t++){let n=Math.floor(t/36),o=Math.floor(t%36/6),i=t%6;e[t*3]=ha[n],e[t*3+1]=ha[o],e[t*3+2]=ha[i]}return e}function pf(e){let t=e.length/3,n=new Uint8Array(t);for(let o=0,i=0;o<t;o++,i+=3){let r=Math.min(5,Math.round(e[i]/51)),a=Math.min(5,Math.round(e[i+1]/51)),l=Math.min(5,Math.round(e[i+2]/51));n[o]=r*36+a*6+l}return n}var ma=class{constructor(){this.bytes=[];this.buf=0;this.nbits=0}writeCode(t,n){for(this.buf|=t<<this.nbits,this.nbits+=n;this.nbits>=8;)this.bytes.push(this.buf&255),this.buf>>=8,this.nbits-=8}finish(){return this.nbits>0&&this.bytes.push(this.buf&255),this.buf=0,this.nbits=0,this.bytes}};function hf(e){let t=[];for(let n=0;n<e.length;n+=255){let o=e.length-n<255?e.length-n:255;t.push(o);for(let i=0;i<o;i++)t.push(e[n+i])}return t.push(0),t}function mf(e,t){let n=1<<t,o=n+1,i=new ma,r=[],a=0,l=0,d=0,c=()=>{r=new Array(4096);for(let m=0;m<n;m++)r[m]=new Map;d=o+1,a=t+1,l=(1<<a)-1,i.writeCode(n,a)};c();let p=-1;for(let m=0;m<e.length;m++){let u=e[m];if(p===-1){p=u;continue}let f=r[p].get(u);if(f!==void 0){p=f;continue}i.writeCode(p,a),d===4096?c():(r[p].set(u,d),r[d]=new Map,d++,d>l&&a<12&&(a++,l=(1<<a)-1)),p=u}return p!==-1&&i.writeCode(p,a),i.writeCode(o,a),hf(i.finish())}function Xo(e){return[e&255,e>>8&255]}function dn(e,t){for(let n=0;n<t.length;n++)e.push(t[n])}function Jl(e,t=100,n=!0){if(!e.length)throw new Error("encodeGif: no frames");let{w:o,h:i}=e[0];if(e.some(l=>l.w!==o||l.h!==i))throw new Error("encodeGif: all clips must be the same size");let r=[];dn(r,[71,73,70,56,57,97]),dn(r,[...Xo(o),...Xo(i),247,0,0]),dn(r,uf()),n&&(dn(r,[33,255,11]),dn(r,[78,69,84,83,67,65,80,69,50,46,48]),dn(r,[3,1,0,0,0]));let a=Math.max(1,Math.round(t/10));for(let l of e)dn(r,[33,249,4,4,...Xo(a),0,0]),dn(r,[44,0,0,0,0,...Xo(l.w),...Xo(l.h),0]),r.push(Zl),dn(r,mf(pf(l.rgb),Zl));return r.push(59),new Uint8Array(r)}var ff=new Uint8Array(new Uint16Array([1]).buffer)[0]===1;function Ql(e){if(typeof e!="string")return e.length;let t=e.length,n=t>0&&e[t-1]==="="?t>1&&e[t-2]==="="?2:1:0;return Math.floor((t/4*3-n)/4)}function ec(e){let t=Uint8Array.fromBase64;if(t)return t(e);let n=atob(e),o=new Uint8Array(n.length);for(let i=0;i<n.length;i++)o[i]=n.charCodeAt(i);return o}function fa(e,t,n){if(typeof e!="string"){for(let i=0;i<e.length;i++)t[n+i]=e[i]??NaN;return e.length}let o=tc(ec(e));return t.set(o,n),o.length}function tc(e){let t=e.length>>2;if(ff&&e.byteOffset%4===0)return new Float32Array(e.buffer,e.byteOffset,t);let n=new DataView(e.buffer,e.byteOffset,e.byteLength),o=new Float32Array(t);for(let i=0;i<t;i++)o[i]=n.getFloat32(4*i,!0);return o}function ft(e){if(typeof e=="string")return tc(ec(e));let t=new Float32Array(e.length);return fa(e,t,0),t}function nc(e){let t=new Map,n=new Map;for(let o of e.columns)t.set(o.col,ft(o.data)),n.set(o.col,o.name);return{win:e.win,rows:e.rows,three:e.three!==0,labels:{x:e.xlabel,y:e.ylabel,z:e.zlabel},curves:e.curves,shift:e.shift,columns:t,names:n,buffers:new Map(t),version:e.version??null}}var gf=1024;function oc(e,t){let n=t.from,o=t.rows;if(t.win!==e.win||!(n>=0&&n<=e.rows)||t.columns.length!==e.columns.size||!t.columns.every(a=>e.buffers.has(a.col)&&Ql(a.data)===o-n))return null;let i=new Map,r=new Map;for(let a of t.columns){let l=e.buffers.get(a.col);if(n<e.rows||o>l.length){let d=new Float32Array(Math.max(gf,o,n<e.rows?2*o:2*l.length));d.set(l.subarray(0,n)),l=d}fa(a.data,l,n),r.set(a.col,l),i.set(a.col,l.subarray(0,o))}return{...e,rows:o,columns:i,buffers:r}}function Fn(e,t){return e.names.get(t)??(t===0?"T":`#${t}`)}function Fi(e,t){return`${Fn(e,t.y)} vs ${Fn(e,t.x)}`}var Uo=new Float32Array(0);function bf(e){for(let t=1;t<e.length;t++)if(!(e[t]>=e[t-1]))return!1;return!0}function Vo(e,t=!1){let[n,o]=e.shift,i=Math.max(n,o,0),r=e.curves.map(m=>{let u=e.columns.get(m.x)??Uo,f=e.columns.get(m.y)??Uo,w=Math.max(0,Math.min(u.length,f.length)-i);return{label:Fi(e,m),xName:Fn(e,m.x),yName:Fn(e,m.y),color:m.color,line:m.line>0,radius:m.line>0?0:Math.max(1,-m.line),xs:t?Uo:u.subarray(i-n,i-n+w),ys:t?Uo:f.subarray(i-o,i-o+w),row0:i}}),a=e.curves[0],d=!!a&&i===0&&e.curves.every(m=>m.x===a.x)&&r.length>0&&bf(t?e.columns.get(a.x)??Uo:r[0].xs)?1:2,c=e.labels.x||(a?Fn(e,a.x):""),p=e.labels.y||(e.curves.length===1&&a?Fn(e,a.y):"");return{mode:d,curves:r,xLabel:c,yLabel:p,t:e.columns.get(0)??null}}var yf="uplot",vf="u-hz",wf="u-vt",xf="u-title",kf="u-wrap",Tf="u-under",Sf="u-over",Pf="u-axis",zn="u-off",Ef="u-select",Af="u-cursor-x",_f="u-cursor-y",Mf="u-cursor-pt",Cf="u-legend",Df="u-live",Lf="u-inline",Of="u-series",If="u-marker",ic="u-label",$f="u-value",Bo="width",jo="height";var rc="bottom",lo="left",ga="right",Oa="#000",ac=Oa+"0",ba="mousemove",sc="mousedown",ya="mouseup",lc="mouseenter",cc="mouseleave",dc="dblclick",Nf="resize",Rf="scroll",uc="change",Ui="dppxchange",Ia="--",go=typeof window<"u",Ta=go?document:null,uo=go?window:null,Ff=go?navigator:null,Te,qi;function Sa(){let e=devicePixelRatio;Te!=e&&(Te=e,qi&&Ea(uc,qi,Sa),qi=matchMedia(`(min-resolution: ${Te-.001}dppx) and (max-resolution: ${Te+.001}dppx)`),Xn(uc,qi,Sa),uo.dispatchEvent(new CustomEvent(Ui)))}function Ct(e,t){if(t!=null){let n=e.classList;!n.contains(t)&&n.add(t)}}function Pa(e,t){let n=e.classList;n.contains(t)&&n.remove(t)}function $e(e,t,n){e.style[t]=n+"px"}function jt(e,t,n,o){let i=Ta.createElement(e);return t!=null&&Ct(i,t),n?.insertBefore(i,o),i}function qt(e,t){return jt("div",e,t)}var pc=new WeakMap;function on(e,t,n,o,i){let r="translate("+t+"px,"+n+"px)",a=pc.get(e);r!=a&&(e.style.transform=r,pc.set(e,r),t<0||n<0||t>o||n>i?Ct(e,zn):Pa(e,zn))}var hc=new WeakMap;function mc(e,t,n){let o=t+n,i=hc.get(e);o!=i&&(hc.set(e,o),e.style.background=t,e.style.borderColor=n)}var fc=new WeakMap;function gc(e,t,n,o){let i=t+""+n,r=fc.get(e);i!=r&&(fc.set(e,i),e.style.height=n+"px",e.style.width=t+"px",e.style.marginLeft=o?-t/2+"px":0,e.style.marginTop=o?-n/2+"px":0)}var $a={passive:!0},qf={...$a,capture:!0};function Xn(e,t,n,o){t.addEventListener(e,n,o?qf:$a)}function Ea(e,t,n,o){t.removeEventListener(e,n,$a)}go&&Sa();function Wt(e,t,n,o){let i;n=n||0,o=o||t.length-1;let r=o<=2147483647;for(;o-n>1;)i=r?n+o>>1:Dt((n+o)/2),t[i]<e?n=i:o=i;return e-t[n]<=t[o]-e?n:o}function Vc(e){return(n,o,i)=>{let r=-1,a=-1;for(let l=o;l<=i;l++)if(e(n[l])){r=l;break}for(let l=i;l>=o;l--)if(e(n[l])){a=l;break}return[r,a]}}var Kc=e=>e!=null,Bc=e=>e!=null&&e>0,Bi=Vc(Kc),Hf=Vc(Bc);function zf(e,t,n,o=0,i=!1){let r=i?Hf:Bi,a=i?Bc:Kc;[t,n]=r(e,t,n);let l=e[t],d=e[t];if(t>-1)if(o==1)l=e[t],d=e[n];else if(o==-1)l=e[n],d=e[t];else for(let c=t;c<=n;c++){let p=e[c];a(p)&&(p<l?l=p:p>d&&(d=p))}return[l??Me,d??-Me]}function ji(e,t,n,o){let i=vc(e),r=vc(t);e==t&&(i==-1?(e*=n,t/=n):(e/=n,t*=n));let a=n==10?un:jc,l=i==1?Dt:Ht,d=r==1?Ht:Dt,c=l(a(Qe(e))),p=d(a(Qe(t))),m=po(n,c),u=po(n,p);return n==10&&(c<0&&(m=Ce(m,-c)),p<0&&(u=Ce(u,-p))),o||n==2?(e=m*i,t=u*r):(e=Zc(e,m),t=Wi(t,u)),[e,t]}function Na(e,t,n,o){let i=ji(e,t,n,o);return e==0&&(i[0]=0),t==0&&(i[1]=0),i}var Ra=.1,bc={mode:3,pad:Ra},Yo={pad:0,soft:null,mode:0},Xf={min:Yo,max:Yo};function Vi(e,t,n,o){return Yi(n)?yc(e,t,n):(Yo.pad=n,Yo.soft=o?0:null,Yo.mode=o?3:0,yc(e,t,Xf))}function xe(e,t){return e??t}function Uf(e,t,n){for(t=xe(t,0),n=xe(n,e.length-1);t<=n;){if(e[t]!=null)return!0;t++}return!1}function yc(e,t,n){let o=n.min,i=n.max,r=xe(o.pad,0),a=xe(i.pad,0),l=xe(o.hard,-Me),d=xe(i.hard,Me),c=xe(o.soft,Me),p=xe(i.soft,-Me),m=xe(o.mode,0),u=xe(i.mode,0),f=t-e,w=un(f),k=vt(Qe(e),Qe(t)),A=un(k),C=Qe(A-w);(f<1e-24||C>10)&&(f=0,(e==0||t==0)&&(f=1e-24,m==2&&c!=Me&&(r=0),u==2&&p!=-Me&&(a=0)));let P=f||k||1e3,$=un(P),E=po(10,Dt($)),U=P*(f==0?e==0?.1:1:r),R=Ce(Zc(e-U,E/10),24),N=e>=c&&(m==1||m==3&&R<=c||m==2&&R>=c)?c:Me,x=vt(l,R<N&&e>=N?N:Yt(N,R)),v=P*(f==0?t==0?.1:1:a),T=Ce(Wi(t+v,E/10),24),L=t<=p&&(u==1||u==3&&T>=p||u==2&&T<=p)?p:-Me,H=Yt(d,T>L&&t<=L?L:vt(L,T));return x==H&&x==0&&(H=100),[x,H]}var Vf=new Intl.NumberFormat(go?Ff.language:"en-US"),Fa=e=>Vf.format(e),Lt=Math,Xi=Lt.PI,Qe=Lt.abs,Dt=Lt.floor,Je=Lt.round,Ht=Lt.ceil,Yt=Lt.min,vt=Lt.max,po=Lt.pow,vc=Lt.sign,un=Lt.log10,jc=Lt.log2,Kf=(e,t=1)=>Lt.sinh(e)*t,va=(e,t=1)=>Lt.asinh(e/t),Me=1/0;function wc(e){return(un((e^e>>31)-(e>>31))|0)+1}function Aa(e,t,n){return Yt(vt(e,t),n)}function Wc(e){return typeof e=="function"}function ye(e){return Wc(e)?e:()=>e}var Bf=()=>{},Yc=e=>e,Gc=(e,t)=>t,jf=e=>null,xc=e=>!0,kc=(e,t)=>e==t,Wf=/\.\d*?(?=9{6,}|0{6,})/gm,Un=e=>{if(Qc(e)||xn.has(e))return e;let t=`${e}`,n=t.match(Wf);if(n==null)return e;let o=n[0].length-1;if(t.indexOf("e-")!=-1){let[i,r]=t.split("e");return+`${Un(i)}e${r}`}return Ce(e,o)};function qn(e,t){return Un(Ce(Un(e/t))*t)}function Wi(e,t){return Un(Ht(Un(e/t))*t)}function Zc(e,t){return Un(Dt(Un(e/t))*t)}function Ce(e,t=0){if(Qc(e))return e;let n=10**t,o=e*n*(1+Number.EPSILON);return Je(o)/n}var xn=new Map;function Jc(e){return((""+e).split(".")[1]||"").length}function Zo(e,t,n,o){let i=[],r=o.map(Jc);for(let a=t;a<n;a++){let l=Qe(a),d=Ce(po(e,a),l);for(let c=0;c<o.length;c++){let p=e==10?+`${o[c]}e${a}`:o[c]*d,m=(a>=0?0:l)+(a>=r[c]?0:r[c]),u=e==10?p:Ce(p,m);i.push(u),xn.set(u,m)}}return i}var Go={},qa=[],ho=[null,null],wn=Array.isArray,Qc=Number.isInteger,Yf=e=>e===void 0;function Tc(e){return typeof e=="string"}function Yi(e){let t=!1;if(e!=null){let n=e.constructor;t=n==null||n==Object}return t}function Gf(e){return e!=null&&typeof e=="object"}var Zf=Object.getPrototypeOf(Uint8Array),ed="__proto__";function mo(e,t=Yi){let n;if(wn(e)){let o=e.find(i=>i!=null);if(wn(o)||t(o)){n=Array(e.length);for(let i=0;i<e.length;i++)n[i]=mo(e[i],t)}else n=e.slice()}else if(e instanceof Zf)n=e.slice();else if(t(e)){n={};for(let o in e)o!=ed&&(n[o]=mo(e[o],t))}else n=e;return n}function Ke(e){let t=arguments;for(let n=1;n<t.length;n++){let o=t[n];for(let i in o)i!=ed&&(Yi(e[i])?Ke(e[i],mo(o[i])):e[i]=mo(o[i]))}return e}var Jf=0,Qf=1,eg=2;function tg(e,t,n){for(let o=0,i,r=-1;o<t.length;o++){let a=t[o];if(a>r){for(i=a-1;i>=0&&e[i]==null;)e[i--]=null;for(i=a+1;i<n&&e[i]==null;)e[r=i++]=null}}}function ng(e,t){if(rg(e)){let a=e[0].slice();for(let l=1;l<e.length;l++)a.push(...e[l].slice(1));return ag(a[0])||(a=ig(a)),a}let n=new Set;for(let a=0;a<e.length;a++){let d=e[a][0],c=d.length;for(let p=0;p<c;p++)n.add(d[p])}let o=[Array.from(n).sort((a,l)=>a-l)],i=o[0].length,r=new Map;for(let a=0;a<i;a++)r.set(o[0][a],a);for(let a=0;a<e.length;a++){let l=e[a],d=l[0];for(let c=1;c<l.length;c++){let p=l[c],m=Array(i).fill(void 0),u=t?t[a][c]:Qf,f=[];for(let w=0;w<p.length;w++){let k=p[w],A=r.get(d[w]);k===null?u!=Jf&&(m[A]=k,u==eg&&f.push(A)):m[A]=k}tg(m,f,i),o.push(m)}}return o}var og=typeof queueMicrotask>"u"?e=>Promise.resolve().then(e):queueMicrotask;function ig(e){let t=e[0],n=t.length,o=Array(n);for(let r=0;r<o.length;r++)o[r]=r;o.sort((r,a)=>t[r]-t[a]);let i=[];for(let r=0;r<e.length;r++){let a=e[r],l=Array(n);for(let d=0;d<n;d++)l[d]=a[o[d]];i.push(l)}return i}function rg(e){let t=e[0][0],n=t.length;for(let o=1;o<e.length;o++){let i=e[o][0];if(i.length!=n)return!1;if(i!=t){for(let r=0;r<n;r++)if(i[r]!=t[r])return!1}}return!0}function ag(e,t=100){let n=e.length;if(n<=1)return!0;let o=0,i=n-1;for(;o<=i&&e[o]==null;)o++;for(;i>=o&&e[i]==null;)i--;if(i<=o)return!0;let r=vt(1,Dt((i-o+1)/t));for(let a=e[o],l=o+r;l<=i;l+=r){let d=e[l];if(d!=null){if(d<=a)return!1;a=d}}return!0}var td=["January","February","March","April","May","June","July","August","September","October","November","December"],nd=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];function od(e){return e.slice(0,3)}var sg=nd.map(od),lg=td.map(od),cg={MMMM:td,MMM:lg,WWWW:nd,WWW:sg};function Ko(e){return(e<10?"0":"")+e}function dg(e){return(e<10?"00":e<100?"0":"")+e}var ug={YYYY:e=>e.getFullYear(),YY:e=>(e.getFullYear()+"").slice(2),MMMM:(e,t)=>t.MMMM[e.getMonth()],MMM:(e,t)=>t.MMM[e.getMonth()],MM:e=>Ko(e.getMonth()+1),M:e=>e.getMonth()+1,DD:e=>Ko(e.getDate()),D:e=>e.getDate(),WWWW:(e,t)=>t.WWWW[e.getDay()],WWW:(e,t)=>t.WWW[e.getDay()],HH:e=>Ko(e.getHours()),H:e=>e.getHours(),h:e=>{let t=e.getHours();return t==0?12:t>12?t-12:t},AA:e=>e.getHours()>=12?"PM":"AM",aa:e=>e.getHours()>=12?"pm":"am",a:e=>e.getHours()>=12?"p":"a",mm:e=>Ko(e.getMinutes()),m:e=>e.getMinutes(),ss:e=>Ko(e.getSeconds()),s:e=>e.getSeconds(),fff:e=>dg(e.getMilliseconds())};function Ha(e,t){t=t||cg;let n=[],o=/\{([a-z]+)\}|[^{]+/gi,i;for(;i=o.exec(e);)n.push(i[0][0]=="{"?ug[i[1]]:i[0]);return r=>{let a="";for(let l=0;l<n.length;l++)a+=typeof n[l]=="string"?n[l]:n[l](r,t);return a}}var pg=new Intl.DateTimeFormat().resolvedOptions().timeZone;function hg(e,t){let n;return t=="UTC"||t=="Etc/UTC"?n=new Date(+e+e.getTimezoneOffset()*6e4):t==pg?n=e:(n=new Date(e.toLocaleString("en-US",{timeZone:t})),n.setMilliseconds(e.getMilliseconds())),n}var id=e=>e%1==0,Ki=[1,2,2.5,5],mg=Zo(10,-32,0,Ki),rd=Zo(10,0,32,Ki),fg=rd.filter(id),Hn=mg.concat(rd),za=`
`,ad="{YYYY}",Sc=za+ad,sd="{M}/{D}",Wo=za+sd,Hi=Wo+"/{YY}",ld="{aa}",gg="{h}:{mm}",co=gg+ld,Pc=za+co,Ec=":{ss}",Pe=null;function cd(e){let t=e*1e3,n=t*60,o=n*60,i=o*24,r=i*30,a=i*365,d=(e==1?Zo(10,0,3,Ki).filter(id):Zo(10,-3,0,Ki)).concat([t,t*5,t*10,t*15,t*30,n,n*5,n*10,n*15,n*30,o,o*2,o*3,o*4,o*6,o*8,o*12,i,i*2,i*3,i*4,i*5,i*6,i*7,i*8,i*9,i*10,i*15,r,r*2,r*3,r*4,r*6,a,a*2,a*5,a*10,a*25,a*50,a*100]),c=[[a,ad,Pe,Pe,Pe,Pe,Pe,Pe,1],[i*28,"{MMM}",Sc,Pe,Pe,Pe,Pe,Pe,1],[i,sd,Sc,Pe,Pe,Pe,Pe,Pe,1],[o,"{h}"+ld,Hi,Pe,Wo,Pe,Pe,Pe,1],[n,co,Hi,Pe,Wo,Pe,Pe,Pe,1],[t,Ec,Hi+" "+co,Pe,Wo+" "+co,Pe,Pc,Pe,1],[e,Ec+".{fff}",Hi+" "+co,Pe,Wo+" "+co,Pe,Pc,Pe,1]];function p(m){return(u,f,w,k,A,C)=>{let P=[],$=A>=a,E=A>=r&&A<a,U=m(w),R=Ce(U*e,3),N=wa(U.getFullYear(),$?0:U.getMonth(),E||$?1:U.getDate()),x=Ce(N*e,3);if(E||$){let v=E?A/r:0,T=$?A/a:0,L=R==x?R:Ce(wa(N.getFullYear()+T,N.getMonth()+v,1)*e,3),H=new Date(Je(L/e)),O=H.getFullYear(),X=H.getMonth();for(let _=0;L<=k;_++){let j=wa(O+T*_,X+v*_,1),W=j-m(Ce(j*e,3));L=Ce((+j+W)*e,3),L<=k&&P.push(L)}}else{let v=A>=i?i:A,T=Dt(w)-Dt(R),L=x+T+Wi(R-x,v);P.push(L);let H=m(L),O=H.getHours()+H.getMinutes()/n+H.getSeconds()/o,X=A/o,_=u.axes[f]._space,j=C/_;for(;L=Ce(L+A,e==1?0:3),!(L>k);)if(X>1){let W=Dt(Ce(O+X,6))%24,se=m(L).getHours()-W;se>1&&(se=-1),L-=se*o,O=(O+X)%24;let le=P[P.length-1];Ce((L-le)/A,3)*j>=.7&&P.push(L)}else P.push(L)}return P}}return[d,c,p]}var[bg,yg,vg]=cd(1),[wg,xg,kg]=cd(.001);Zo(2,-53,53,[1]);function Ac(e,t){return e.map(n=>n.map((o,i)=>i==0||i==8||o==null?o:t(i==1||n[8]==0?o:n[1]+o)))}function _c(e,t){return(n,o,i,r,a)=>{let l=t.find(w=>a>=w[0])||t[t.length-1],d,c,p,m,u,f;return o.map(w=>{let k=e(w),A=k.getFullYear(),C=k.getMonth(),P=k.getDate(),$=k.getHours(),E=k.getMinutes(),U=k.getSeconds(),R=A!=d&&l[2]||C!=c&&l[3]||P!=p&&l[4]||$!=m&&l[5]||E!=u&&l[6]||U!=f&&l[7]||l[1];return d=A,c=C,p=P,m=$,u=E,f=U,R(k)})}}function Tg(e,t){let n=Ha(t);return(o,i,r,a,l)=>i.map(d=>n(e(d)))}function wa(e,t,n){return new Date(e,t,n)}function Mc(e,t){return t(e)}var Sg="{YYYY}-{MM}-{DD} {h}:{mm}{aa}";function Cc(e,t){return(n,o,i,r)=>r==null?Ia:t(e(o))}function Pg(e,t){let n=e.series[t];return n.width?n.stroke(e,t):n.points.width?n.points.stroke(e,t):null}function Eg(e,t){return e.series[t].fill(e,t)}var Ag={show:!0,live:!0,isolate:!1,mount:Bf,markers:{show:!0,width:2,stroke:Pg,fill:Eg,dash:"solid"},idx:null,idxs:null,values:[]};function _g(e,t){let n=e.cursor.points,o=qt(),i=n.size(e,t);$e(o,Bo,i),$e(o,jo,i);let r=i/-2;$e(o,"marginLeft",r),$e(o,"marginTop",r);let a=n.width(e,t,i);return a&&$e(o,"borderWidth",a),o}function Mg(e,t){let n=e.series[t].points;return n._fill||n._stroke}function Cg(e,t){let n=e.series[t].points;return n._stroke||n._fill}function Dg(e,t){return e.series[t].points.size}var xa=[0,0];function Lg(e,t,n){return xa[0]=t,xa[1]=n,xa}function zi(e,t,n,o=!0){return i=>{i.button==0&&(!o||i.target==t)&&n(i)}}function ka(e,t,n,o=!0){return i=>{(!o||i.target==t)&&n(i)}}var Og={show:!0,x:!0,y:!0,lock:!1,move:Lg,points:{one:!1,show:_g,size:Dg,width:0,stroke:Cg,fill:Mg},bind:{mousedown:zi,mouseup:zi,click:zi,dblclick:zi,mousemove:ka,mouseleave:ka,mouseenter:ka},drag:{setScale:!0,x:!0,y:!1,dist:0,uni:null,click:(e,t)=>{t.stopPropagation(),t.stopImmediatePropagation()},_x:!1,_y:!1},focus:{dist:(e,t,n,o,i)=>o-i,prox:-1,bias:0},hover:{skip:[void 0],prox:null,bias:0},left:-10,top:-10,idx:null,dataIdx:null,idxs:null,event:null},dd={show:!0,stroke:"rgba(0,0,0,0.07)",width:2},Xa=Ke({},dd,{filter:Gc}),ud=Ke({},Xa,{size:10}),pd=Ke({},dd,{show:!1}),Ua='12px system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial, "Noto Sans", sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol", "Noto Color Emoji"',hd="bold "+Ua,md=1.5,Dc={show:!0,scale:"x",stroke:Oa,space:50,gap:5,alignTo:1,size:50,labelGap:0,labelSize:30,labelFont:hd,side:2,grid:Xa,ticks:ud,border:pd,font:Ua,lineGap:md,rotate:0},Ig="Value",$g="Time",Lc={show:!0,scale:"x",auto:!1,sorted:1,min:Me,max:-Me,idxs:[]};function Ng(e,t,n,o,i){return t.map(r=>r==null?"":Fa(r))}function Rg(e,t,n,o,i,r,a){let l=[],d=xn.get(i)||0;n=a?n:Ce(Wi(n,i),d);for(let c=n;c<=o;c=Ce(c+i,d))l.push(Object.is(c,-0)?0:c);return l}function _a(e,t,n,o,i,r,a){let l=[],d=e.scales[e.axes[t].scale].log,c=d==10?un:jc,p=Dt(c(n));i=po(d,p),d==10&&(i=Hn[Wt(i,Hn)]);let m=n,u=i*d;d==10&&(u=Hn[Wt(u,Hn)]);do l.push(m),m=m+i,d==10&&!xn.has(m)&&(m=Ce(m,xn.get(i))),m>=u&&(i=m,u=i*d,d==10&&(u=Hn[Wt(u,Hn)]));while(m<=o);return l}function Fg(e,t,n,o,i,r,a){let d=e.scales[e.axes[t].scale].asinh,c=o>d?_a(e,t,vt(d,n),o,i):[d],p=o>=0&&n<=0?[0]:[];return(n<-d?_a(e,t,vt(d,-o),-n,i):[d]).reverse().map(u=>-u).concat(p,c)}var fd=/./,qg=/[12357]/,Hg=/[125]/,Oc=/1/,Ma=(e,t,n,o)=>e.map((i,r)=>t==4&&i==0||r%o==0&&n.test(i.toExponential()[i<0?1:0])?i:null);function zg(e,t,n,o,i){let r=e.axes[n],a=r.scale,l=e.scales[a],d=e.valToPos,c=r._space,p=d(10,a),m=d(9,a)-p>=c?fd:d(7,a)-p>=c?qg:d(5,a)-p>=c?Hg:Oc;if(m==Oc){let u=Qe(d(1,a)-p);if(u<c)return Ma(t.slice().reverse(),l.distr,m,Ht(c/u)).reverse()}return Ma(t,l.distr,m,1)}function Xg(e,t,n,o,i){let r=e.axes[n],a=r.scale,l=r._space,d=e.valToPos,c=Qe(d(1,a)-d(2,a));return c<l?Ma(t.slice().reverse(),3,fd,Ht(l/c)).reverse():t}function Ug(e,t,n,o){return o==null?Ia:t==null?"":Fa(t)}var Ic={show:!0,scale:"y",stroke:Oa,space:30,gap:5,alignTo:1,size:50,labelGap:0,labelSize:30,labelFont:hd,side:3,grid:Xa,ticks:ud,border:pd,font:Ua,lineGap:md,rotate:0};function Vg(e,t){let n=3+(e||1)*2;return Ce(n*t,3)}function Kg(e,t){let{scale:n,idxs:o}=e.series[0],i=e._data[0],r=e.valToPos(i[o[0]],n,!0),a=e.valToPos(i[o[1]],n,!0),l=Qe(a-r),d=e.series[t],c=l/(d.points.space*Te);return o[1]-o[0]<=c}var $c={scale:null,auto:!0,sorted:0,min:Me,max:-Me},gd=(e,t,n,o,i)=>i,Nc={show:!0,auto:!0,sorted:0,gaps:gd,alpha:1,facets:[Ke({},$c,{scale:"x"}),Ke({},$c,{scale:"y"})]},Rc={scale:"y",auto:!0,sorted:0,show:!0,spanGaps:!1,gaps:gd,alpha:1,points:{show:Kg,filter:null},values:null,min:Me,max:-Me,idxs:[],path:null,clip:null};function Bg(e,t,n,o,i){return n/10}var bd={time:!0,auto:!0,distr:1,log:10,asinh:1,min:null,max:null,dir:1,ori:0},jg=Ke({},bd,{time:!1,ori:1}),Fc={};function yd(e,t){let n=Fc[e];return n||(n={key:e,plots:[],sub(o){n.plots.push(o)},unsub(o){n.plots=n.plots.filter(i=>i!=o)},pub(o,i,r,a,l,d,c){for(let p=0;p<n.plots.length;p++)n.plots[p]!=i&&n.plots[p].pub(o,i,r,a,l,d,c)}},e!=null&&(Fc[e]=n)),n}var fo=1,Ca=2;function Vn(e,t,n){let o=e.mode,i=e.series[t],r=o==2?e._data[t]:e._data,a=e.scales,l=e.bbox,d=r[0],c=o==2?r[1]:r[t],p=o==2?a[i.facets[0].scale]:a[e.series[0].scale],m=o==2?a[i.facets[1].scale]:a[i.scale],u=l.left,f=l.top,w=l.width,k=l.height,A=e.valToPosH,C=e.valToPosV;return p.ori==0?n(i,d,c,p,m,A,C,u,f,w,k,Zi,bo,Qi,wd,kd):n(i,d,c,p,m,C,A,f,u,k,w,Ji,yo,Ba,xd,Td)}function Va(e,t){let n=0,o=0,i=xe(e.bands,qa);for(let r=0;r<i.length;r++){let a=i[r];a.series[0]==t?n=a.dir:a.series[1]==t&&(a.dir==1?o|=1:o|=2)}return[n,o==1?-1:o==2?1:o==3?2:0]}function Wg(e,t,n,o,i){let r=e.mode,a=e.series[t],l=r==2?a.facets[1].scale:a.scale,d=e.scales[l];return i==-1?d.min:i==1?d.max:d.distr==3?d.dir==1?d.min:d.max:0}function pn(e,t,n,o,i,r){return Vn(e,t,(a,l,d,c,p,m,u,f,w,k,A)=>{let C=a.pxRound,P=c.dir*(c.ori==0?1:-1),$=c.ori==0?bo:yo,E,U;P==1?(E=n,U=o):(E=o,U=n);let R=C(m(l[E],c,k,f)),N=C(u(d[E],p,A,w)),x=C(m(l[U],c,k,f)),v=C(u(r==1?p.max:p.min,p,A,w)),T=new Path2D(i);return $(T,x,v),$(T,R,v),$(T,R,N),T})}function Gi(e,t,n,o,i,r){let a=null;if(e.length>0){a=new Path2D;let l=t==0?Qi:Ba,d=n;for(let m=0;m<e.length;m++){let u=e[m];if(u[1]>u[0]){let f=u[0]-d;f>0&&l(a,d,o,f,o+r),d=u[1]}}let c=n+i-d,p=10;c>0&&l(a,d,o-p/2,c,o+r+p)}return a}function Yg(e,t,n){let o=e[e.length-1];o&&o[0]==t?o[1]=n:e.push([t,n])}function Ka(e,t,n,o,i,r,a){let l=[],d=e.length;for(let c=i==1?n:o;c>=n&&c<=o;c+=i)if(t[c]===null){let m=c,u=c;if(i==1)for(;++c<=o&&t[c]===null;)u=c;else for(;--c>=n&&t[c]===null;)u=c;let f=r(e[m]),w=u==m?f:r(e[u]),k=m-i;f=a<=0&&k>=0&&k<d?r(e[k]):f;let C=u+i;w=a>=0&&C>=0&&C<d?r(e[C]):w,w>=f&&l.push([f,w])}return l}function qc(e){return e==0?Yc:e==1?Je:t=>qn(t,e)}function vd(e){let t=e==0?Zi:Ji,n=e==0?(i,r,a,l,d,c)=>{i.arcTo(r,a,l,d,c)}:(i,r,a,l,d,c)=>{i.arcTo(a,r,d,l,c)},o=e==0?(i,r,a,l,d)=>{i.rect(r,a,l,d)}:(i,r,a,l,d)=>{i.rect(a,r,d,l)};return(i,r,a,l,d,c=0,p=0)=>{c==0&&p==0?o(i,r,a,l,d):(c=Yt(c,l/2,d/2),p=Yt(p,l/2,d/2),t(i,r+c,a),n(i,r+l,a,r+l,a+d,c),n(i,r+l,a+d,r,a+d,p),n(i,r,a+d,r,a,p),n(i,r,a,r+l,a,c),i.closePath())}}var Zi=(e,t,n)=>{e.moveTo(t,n)},Ji=(e,t,n)=>{e.moveTo(n,t)},bo=(e,t,n)=>{e.lineTo(t,n)},yo=(e,t,n)=>{e.lineTo(n,t)},Qi=vd(0),Ba=vd(1),wd=(e,t,n,o,i,r)=>{e.arc(t,n,o,i,r)},xd=(e,t,n,o,i,r)=>{e.arc(n,t,o,i,r)},kd=(e,t,n,o,i,r,a)=>{e.bezierCurveTo(t,n,o,i,r,a)},Td=(e,t,n,o,i,r,a)=>{e.bezierCurveTo(n,t,i,o,a,r)};function Sd(e){return(t,n,o,i,r)=>Vn(t,n,(a,l,d,c,p,m,u,f,w,k,A)=>{let{pxRound:C,points:P}=a,$,E;c.ori==0?($=Zi,E=wd):($=Ji,E=xd);let U=Ce(P.width*Te,3),R=(P.size-P.width)/2*Te,N=Ce(R*2,3),x=new Path2D,v=new Path2D,{left:T,top:L,width:H,height:O}=t.bbox;Qi(v,T-N,L-N,H+N*2,O+N*2);let X=_=>{if(d[_]!=null){let j=C(m(l[_],c,k,f)),W=C(u(d[_],p,A,w));$(x,j+R,W),E(x,j,W,R,0,Xi*2)}};if(r)r.forEach(X);else for(let _=o;_<=i;_++)X(_);return{stroke:U>0?x:null,fill:x,clip:v,flags:fo|Ca}})}function Pd(e){return(t,n,o,i,r,a)=>{o!=i&&(r!=o&&a!=o&&e(t,n,o),r!=i&&a!=i&&e(t,n,i),e(t,n,a))}}var Gg=Pd(bo),Zg=Pd(yo);function Ed(e){let t=xe(e?.alignGaps,0);return(n,o,i,r)=>Vn(n,o,(a,l,d,c,p,m,u,f,w,k,A)=>{[i,r]=Bi(d,i,r);let C=a.pxRound,P=O=>C(m(O,c,k,f)),$=O=>C(u(O,p,A,w)),E,U;c.ori==0?(E=bo,U=Gg):(E=yo,U=Zg);let R=c.dir*(c.ori==0?1:-1),N={stroke:new Path2D,fill:null,clip:null,band:null,gaps:null,flags:fo},x=N.stroke,v=!1;if(r-i>=k*4){let O=V=>n.posToVal(V,c.key,!0),X=null,_=null,j,W,B,re=P(l[R==1?i:r]),se=P(l[i]),le=P(l[r]),q=O(R==1?se+1:le-1);for(let V=R==1?i:r;V>=i&&V<=r;V+=R){let ce=l[V],fe=(R==1?ce<q:ce>q)?re:P(ce),pe=d[V];fe==re?pe!=null?(W=pe,X==null?(E(x,fe,$(W)),j=X=_=W):W<X?X=W:W>_&&(_=W)):pe===null&&(v=!0):(X!=null&&U(x,re,$(X),$(_),$(j),$(W)),pe!=null?(W=pe,E(x,fe,$(W)),X=_=j=W):(X=_=null,pe===null&&(v=!0)),re=fe,q=O(re+R))}X!=null&&X!=_&&B!=re&&U(x,re,$(X),$(_),$(j),$(W))}else for(let O=R==1?i:r;O>=i&&O<=r;O+=R){let X=d[O];X===null?v=!0:X!=null&&E(x,P(l[O]),$(X))}let[L,H]=Va(n,o);if(a.fill!=null||L!=0){let O=N.fill=new Path2D(x),X=a.fillTo(n,o,a.min,a.max,L),_=$(X),j=P(l[i]),W=P(l[r]);R==-1&&([W,j]=[j,W]),E(O,W,_),E(O,j,_)}if(!a.spanGaps){let O=[];v&&O.push(...Ka(l,d,i,r,R,P,t)),N.gaps=O=a.gaps(n,o,i,r,O),N.clip=Gi(O,c.ori,f,w,k,A)}return H!=0&&(N.band=H==2?[pn(n,o,i,r,x,-1),pn(n,o,i,r,x,1)]:pn(n,o,i,r,x,H)),N})}function Jg(e){let t=xe(e.align,1),n=xe(e.ascDesc,!1),o=xe(e.alignGaps,0),i=xe(e.extend,!1);return(r,a,l,d)=>Vn(r,a,(c,p,m,u,f,w,k,A,C,P,$)=>{[l,d]=Bi(m,l,d);let E=c.pxRound,{left:U,width:R}=r.bbox,N=se=>E(w(se,u,P,A)),x=se=>E(k(se,f,$,C)),v=u.ori==0?bo:yo,T={stroke:new Path2D,fill:null,clip:null,band:null,gaps:null,flags:fo},L=T.stroke,H=u.dir*(u.ori==0?1:-1),O=x(m[H==1?l:d]),X=N(p[H==1?l:d]),_=X,j=X;i&&t==-1&&(j=U,v(L,j,O)),v(L,X,O);for(let se=H==1?l:d;se>=l&&se<=d;se+=H){let le=m[se];if(le==null)continue;let q=N(p[se]),V=x(le);t==1?v(L,q,O):v(L,_,V),v(L,q,V),O=V,_=q}let W=_;i&&t==1&&(W=U+R,v(L,W,O));let[B,re]=Va(r,a);if(c.fill!=null||B!=0){let se=T.fill=new Path2D(L),le=c.fillTo(r,a,c.min,c.max,B),q=x(le);v(se,W,q),v(se,j,q)}if(!c.spanGaps){let se=[];se.push(...Ka(p,m,l,d,H,N,o));let le=c.width*Te/2,q=n||t==1?le:-le,V=n||t==-1?-le:le;se.forEach(ce=>{ce[0]+=q,ce[1]+=V}),T.gaps=se=c.gaps(r,a,l,d,se),T.clip=Gi(se,u.ori,A,C,P,$)}return re!=0&&(T.band=re==2?[pn(r,a,l,d,L,-1),pn(r,a,l,d,L,1)]:pn(r,a,l,d,L,re)),T})}function Hc(e,t,n,o,i,r,a=Me){if(e.length>1){let l=null;for(let d=0,c=1/0;d<e.length;d++)if(t[d]!==void 0){if(l!=null){let p=Qe(e[d]-e[l]);p<c&&(c=p,a=Qe(n(e[d],o,i,r)-n(e[l],o,i,r)))}l=d}}return a}function Qg(e){e=e||Go;let t=xe(e.size,[.6,Me,1]),n=e.align||0,o=e.gap||0,i=e.radius;i=i==null?[0,0]:typeof i=="number"?[i,0]:i;let r=ye(i),a=1-t[0],l=xe(t[1],Me),d=xe(t[2],1),c=xe(e.disp,Go),p=xe(e.each,f=>{}),{fill:m,stroke:u}=c;return(f,w,k,A)=>Vn(f,w,(C,P,$,E,U,R,N,x,v,T,L)=>{let H=C.pxRound,O=n,X=o*Te,_=l*Te,j=d*Te,W,B;E.ori==0?[W,B]=r(f,w):[B,W]=r(f,w);let re=E.dir*(E.ori==0?1:-1),se=E.ori==0?Qi:Ba,le=E.ori==0?p:(Y,Ee,Ze,Bn,Mn,Zt,Cn)=>{p(Y,Ee,Ze,Mn,Bn,Cn,Zt)},q=xe(f.bands,qa).find(Y=>Y.series[0]==w),V=q!=null?q.dir:0,ce=C.fillTo(f,w,C.min,C.max,V),Ne=H(N(ce,U,L,v)),fe,pe,pt,Re=T,me=H(C.width*Te),lt=!1,wt=null,ct=null,I=null,oe=null;m!=null&&(me==0||u!=null)&&(lt=!0,wt=m.values(f,w,k,A),ct=new Map,new Set(wt).forEach(Y=>{Y!=null&&ct.set(Y,new Path2D)}),me>0&&(I=u.values(f,w,k,A),oe=new Map,new Set(I).forEach(Y=>{Y!=null&&oe.set(Y,new Path2D)})));let{x0:be,size:he}=c;if(be!=null&&he!=null){O=1,P=be.values(f,w,k,A),be.unit==2&&(P=P.map(Ze=>f.posToVal(x+Ze*T,E.key,!0)));let Y=he.values(f,w,k,A);he.unit==2?pe=Y[0]*T:pe=R(Y[0],E,T,x)-R(0,E,T,x),Re=Hc(P,$,R,E,T,x,Re),pt=Re-pe+X}else Re=Hc(P,$,R,E,T,x,Re),pt=Re*a+X,pe=Re-pt;pt<1&&(pt=0),me>=pe/2&&(me=0),pt<5&&(H=Yc);let He=pt>0,ht=Re-pt-(He?me:0);pe=H(Aa(ht,j,_)),fe=(O==0?pe/2:O==re?0:pe)-O*re*((O==0?X/2:0)+(He?me/2:0));let Se={stroke:null,fill:null,clip:null,band:null,gaps:null,flags:0},xt=lt?null:new Path2D,Ge=null;if(q!=null)Ge=f.data[q.series[1]];else{let{y0:Y,y1:Ee}=c;Y!=null&&Ee!=null&&($=Ee.values(f,w,k,A),Ge=Y.values(f,w,k,A))}let Pt=W*pe,de=B*pe;for(let Y=re==1?k:A;Y>=k&&Y<=A;Y+=re){let Ee=$[Y];if(Ee==null)continue;if(Ge!=null){let kt=Ge[Y]??0;if(Ee-kt==0)continue;Ne=N(kt,U,L,v)}let Ze=E.distr!=2||c!=null?P[Y]:Y,Bn=R(Ze,E,T,x),Mn=N(xe(Ee,ce),U,L,v),Zt=H(Bn-fe),Cn=H(vt(Mn,Ne)),Et=H(Yt(Mn,Ne)),It=Cn-Et;if(Ee!=null){let kt=Ee<0?de:Pt,Xt=Ee<0?Pt:de;lt?(me>0&&I[Y]!=null&&se(oe.get(I[Y]),Zt,Et+Dt(me/2),pe,vt(0,It-me),kt,Xt),wt[Y]!=null&&se(ct.get(wt[Y]),Zt,Et+Dt(me/2),pe,vt(0,It-me),kt,Xt)):se(xt,Zt,Et+Dt(me/2),pe,vt(0,It-me),kt,Xt),le(f,w,Y,Zt-me/2,Et,pe+me,It)}}return me>0?Se.stroke=lt?oe:xt:lt||(Se._fill=C.width==0?C._fill:C._stroke??C._fill,Se.width=0),Se.fill=lt?ct:xt,Se})}function eb(e,t){let n=xe(t?.alignGaps,0);return(o,i,r,a)=>Vn(o,i,(l,d,c,p,m,u,f,w,k,A,C)=>{[r,a]=Bi(c,r,a);let P=l.pxRound,$=W=>P(u(W,p,A,w)),E=W=>P(f(W,m,C,k)),U,R,N;p.ori==0?(U=Zi,N=bo,R=kd):(U=Ji,N=yo,R=Td);let x=p.dir*(p.ori==0?1:-1),v=$(d[x==1?r:a]),T=v,L=[],H=[];for(let W=x==1?r:a;W>=r&&W<=a;W+=x)if(c[W]!=null){let re=d[W],se=$(re);L.push(T=se),H.push(E(c[W]))}let O={stroke:e(L,H,U,N,R,P),fill:null,clip:null,band:null,gaps:null,flags:fo},X=O.stroke,[_,j]=Va(o,i);if(l.fill!=null||_!=0){let W=O.fill=new Path2D(X),B=l.fillTo(o,i,l.min,l.max,_),re=E(B);N(W,T,re),N(W,v,re)}if(!l.spanGaps){let W=[];W.push(...Ka(d,c,r,a,x,$,n)),O.gaps=W=l.gaps(o,i,r,a,W),O.clip=Gi(W,p.ori,w,k,A,C)}return j!=0&&(O.band=j==2?[pn(o,i,r,a,X,-1),pn(o,i,r,a,X,1)]:pn(o,i,r,a,X,j)),O})}function tb(e){return eb(nb,e)}function nb(e,t,n,o,i,r){let a=e.length;if(a<2)return null;let l=new Path2D;if(n(l,e[0],t[0]),a==2)o(l,e[1],t[1]);else{let d=Array(a),c=Array(a-1),p=Array(a-1),m=Array(a-1);for(let u=0;u<a-1;u++)p[u]=t[u+1]-t[u],m[u]=e[u+1]-e[u],c[u]=p[u]/m[u];d[0]=c[0];for(let u=1;u<a-1;u++)c[u]===0||c[u-1]===0||c[u-1]>0!=c[u]>0?d[u]=0:(d[u]=3*(m[u-1]+m[u])/((2*m[u]+m[u-1])/c[u-1]+(m[u]+2*m[u-1])/c[u]),isFinite(d[u])||(d[u]=0));d[a-1]=c[a-2];for(let u=0;u<a-1;u++)i(l,e[u]+m[u]/3,t[u]+d[u]*m[u]/3,e[u+1]-m[u]/3,t[u+1]-d[u+1]*m[u]/3,e[u+1],t[u+1])}return l}var Da=new Set;function zc(){for(let e of Da)e.syncRect(!0)}go&&(Xn(Nf,uo,zc),Xn(Rf,uo,zc,!0),Xn(Ui,uo,()=>{De.pxRatio=Te}));var ob=Ed(),ib=Sd();function Xc(e,t,n,o){return(o?[e[0],e[1]].concat(e.slice(2)):[e[0]].concat(e.slice(1))).map((r,a)=>La(r,a,t,n))}function rb(e,t){return e.map((n,o)=>o==0?{}:Ke({},t,n))}function La(e,t,n,o){return Ke({},t==0?n:o,e)}function Ad(e,t,n){return t==null?ho:[t,n]}var ab=Ad;function sb(e,t,n){return t==null?ho:Vi(t,n,Ra,!0)}function _d(e,t,n,o){return t==null?ho:ji(t,n,e.scales[o].log,!1)}var lb=_d;function Md(e,t,n,o){return t==null?ho:Na(t,n,e.scales[o].log,!1)}var cb=Md;function db(e,t,n,o,i){let r=vt(wc(e),wc(t)),a=t-e,l=Wt(i/o*a,n);do{let d=n[l],c=o*d/a;if(c>=i&&r+(d<5?xn.get(d):0)<=17)return[d,c]}while(++l<n.length);return[0,0]}function Uc(e){let t,n;return e=e.replace(/(\d+)px/,(o,i)=>(t=Je((n=+i)*Te))+"px"),[e,t,n]}function ub(e){e.show&&[e.font,e.labelFont].forEach(t=>{let n=Ce(t[2]*Te,1);t[0]=t[0].replace(/[0-9.]+px/,n+"px"),t[1]=n})}function De(e,t,n){let o={mode:xe(e.mode,1)},i=o.mode;function r(h,g,b,y){let S=g.valToPct(h);return y+b*(g.dir==-1?1-S:S)}function a(h,g,b,y){let S=g.valToPct(h);return y+b*(g.dir==-1?S:1-S)}function l(h,g,b,y){return g.ori==0?r(h,g,b,y):a(h,g,b,y)}o.valToPosH=r,o.valToPosV=a;let d=!1;o.status=0;let c=o.root=qt(yf);if(e.id!=null&&(c.id=e.id),Ct(c,e.class),e.title){let h=qt(xf,c);h.textContent=e.title}let p=jt("canvas"),m=o.ctx=p.getContext("2d"),u=qt(kf,c);Xn("click",u,h=>{h.target===w&&(Le!=oo||Fe!=io)&&ut.click(o,h)},!0);let f=o.under=qt(Tf,u);u.appendChild(p);let w=o.over=qt(Sf,u);e=mo(e);let k=+xe(e.pxAlign,1),A=qc(k);(e.plugins||[]).forEach(h=>{h.opts&&(e=h.opts(o,e)||e)});let C=e.ms||.001,P=o.series=i==1?Xc(e.series||[],Lc,Rc,!1):rb(e.series||[null],Nc),$=o.axes=Xc(e.axes||[],Dc,Ic,!0),E=o.scales={},U=o.bands=e.bands||[];U.forEach(h=>{h.fill=ye(h.fill||null),h.dir=xe(h.dir,-1)});let R=i==2?P[1].facets[0].scale:P[0].scale,N={axes:qm,series:Im},x=(e.drawOrder||["axes","series"]).map(h=>N[h]);function v(h){let g=h.distr==3?b=>un(b>0?b:h.clamp(o,b,h.min,h.max,h.key)):h.distr==4?b=>va(b,h.asinh):h.distr==100?b=>h.fwd(b):b=>b;return b=>{let y=g(b),{_min:S,_max:D}=h,F=D-S;return(y-S)/F}}function T(h){let g=E[h];if(g==null){let b=(e.scales||Go)[h]||Go;if(b.from!=null){T(b.from);let y=Ke({},E[b.from],b,{key:h});y.valToPct=v(y),E[h]=y}else{g=E[h]=Ke({},h==R?bd:jg,b),g.key=h;let y=g.time,S=g.range,D=wn(S);if((h!=R||i==2&&!y)&&(D&&(S[0]==null||S[1]==null)&&(S={min:S[0]==null?bc:{mode:1,hard:S[0],soft:S[0]},max:S[1]==null?bc:{mode:1,hard:S[1],soft:S[1]}},D=!1),!D&&Yi(S))){let F=S;S=(z,K,Z)=>K==null?ho:Vi(K,Z,F)}g.range=ye(S||(y?ab:h==R?g.distr==3?lb:g.distr==4?cb:Ad:g.distr==3?_d:g.distr==4?Md:sb)),g.auto=ye(D?!1:g.auto),g.clamp=ye(g.clamp||Bg),g._min=g._max=null,g.valToPct=v(g)}}}T("x"),T("y"),i==1&&P.forEach(h=>{T(h.scale)}),$.forEach(h=>{T(h.scale)});for(let h in e.scales)T(h);let L=E[R],H=L.distr,O,X;L.ori==0?(Ct(c,vf),O=r,X=a):(Ct(c,wf),O=a,X=r);let _={};for(let h in E){let g=E[h];(g.min!=null||g.max!=null)&&(_[h]={min:g.min,max:g.max},g.min=g.max=null)}let j=e.tzDate||(h=>new Date(Je(h/C))),W=e.fmtDate||Ha,B=C==1?vg(j):kg(j),re=_c(j,Ac(C==1?yg:xg,W)),se=Cc(j,Mc(Sg,W)),le=[],q=o.legend=Ke({},Ag,e.legend),V=o.cursor=Ke({},Og,{drag:{y:i==2}},e.cursor),ce=q.show,Ne=V.show,fe=q.markers;q.idxs=le,fe.width=ye(fe.width),fe.dash=ye(fe.dash),fe.stroke=ye(fe.stroke),fe.fill=ye(fe.fill);let pe,pt,Re,me=[],lt=[],wt,ct=!1,I={};if(q.live){let h=P[1]?P[1].values:null;ct=h!=null,wt=ct?h(o,1,0):{_:0};for(let g in wt)I[g]=Ia}if(ce)if(pe=jt("table",Cf,c),Re=jt("tbody",null,pe),q.mount(o,pe),ct){pt=jt("thead",null,pe,Re);let h=jt("tr",null,pt);jt("th",null,h);for(var oe in wt)jt("th",ic,h).textContent=oe}else Ct(pe,Lf),q.live&&Ct(pe,Df);let be={show:!0},he={show:!1};function He(h,g){if(g==0&&(ct||!q.live||i==2))return ho;let b=[],y=jt("tr",Of,Re,Re.childNodes[g]);Ct(y,h.class),h.show||Ct(y,zn);let S=jt("th",null,y);if(fe.show){let z=qt(If,S);if(g>0){let K=fe.width(o,g);K&&(z.style.border=K+"px "+fe.dash(o,g)+" "+fe.stroke(o,g)),z.style.background=fe.fill(o,g)}}let D=qt(ic,S);h.label instanceof HTMLElement?D.appendChild(h.label):D.textContent=h.label,g>0&&(fe.show||(D.style.color=h.width>0?fe.stroke(o,g):fe.fill(o,g)),Se("click",S,z=>{if(V._lock)return;Ln(z);let K=P.indexOf(h);if((z.ctrlKey||z.metaKey)!=q.isolate){let Z=P.some((Q,ee)=>ee>0&&ee!=K&&Q.show);P.forEach((Q,ee)=>{ee>0&&Qt(ee,Z?ee==K?be:he:be,!0,Ve.setSeries)})}else Qt(K,{show:!h.show},!0,Ve.setSeries)},!1),Wn&&Se(lc,S,z=>{V._lock||(Ln(z),Qt(P.indexOf(h),ao,!0,Ve.setSeries))},!1));for(var F in wt){let z=jt("td",$f,y);z.textContent="--",b.push(z)}return[y,b]}let ht=new Map;function Se(h,g,b,y=!0){let S=ht.get(g)||{},D=V.bind[h](o,g,b,y);D&&(Xn(h,g,S[h]=D),ht.set(g,S))}function xt(h,g,b){let y=ht.get(g)||{};for(let S in y)(h==null||S==h)&&(Ea(S,g,y[S]),delete y[S]);h==null&&ht.delete(g)}let Ge=0,Pt=0,de=0,Y=0,Ee=0,Ze=0,Bn=Ee,Mn=Ze,Zt=de,Cn=Y,Et=0,It=0,kt=0,Xt=0;o.bbox={};let Ir=!1,hi=!1,jn=!1,Dn=!1,mi=!1,$t=!1;function $r(h,g,b){(b||h!=o.width||g!=o.height)&&Ys(h,g),Qn(!1),jn=!0,hi=!0,eo()}function Ys(h,g){o.width=Ge=de=h,o.height=Pt=Y=g,Ee=Ze=0,Am(),_m();let b=o.bbox;Et=b.left=qn(Ee*Te,.5),It=b.top=qn(Ze*Te,.5),kt=b.width=qn(de*Te,.5),Xt=b.height=qn(Y*Te,.5)}let Sm=3;function Pm(){let h=!1,g=0;for(;!h;){g++;let b=Rm(g),y=Fm(g);h=g==Sm||b&&y,h||(Ys(o.width,o.height),hi=!0)}}function Em({width:h,height:g}){$r(h,g)}o.setSize=Em;function Am(){let h=!1,g=!1,b=!1,y=!1;$.forEach((S,D)=>{if(S.show&&S._show){let{side:F,_size:z}=S,K=F%2,Z=S.label!=null?S.labelSize:0,Q=z+Z;Q>0&&(K?(de-=Q,F==3?(Ee+=Q,y=!0):b=!0):(Y-=Q,F==0?(Ze+=Q,h=!0):g=!0))}}),On[0]=h,On[1]=b,On[2]=g,On[3]=y,de-=mn[1]+mn[3],Ee+=mn[3],Y-=mn[2]+mn[0],Ze+=mn[0]}function _m(){let h=Ee+de,g=Ze+Y,b=Ee,y=Ze;function S(D,F){switch(D){case 1:return h+=F,h-F;case 2:return g+=F,g-F;case 3:return b-=F,b+F;case 0:return y-=F,y+F}}$.forEach((D,F)=>{if(D.show&&D._show){let z=D.side;D._pos=S(z,D._size),D.label!=null&&(D._lpos=S(z,D.labelSize))}})}if(V.dataIdx==null){let h=V.hover,g=h.skip=new Set(h.skip??[]);g.add(void 0);let b=h.prox=ye(h.prox),y=h.bias??(h.bias=0);V.dataIdx=(S,D,F,z)=>{if(D==0)return F;let K=F,Z=b(S,D,F,z)??Me,Q=Z>=0&&Z<Me,ee=L.ori==0?de:Y,ue=V.left,ke=t[0],we=t[D];if(g.has(we[F])){K=null;let ge=null,ae=null,ne;if(y==0||y==-1)for(ne=F;ge==null&&ne-- >0;)g.has(we[ne])||(ge=ne);if(y==0||y==1)for(ne=F;ae==null&&ne++<we.length;)g.has(we[ne])||(ae=ne);if(ge!=null||ae!=null)if(Q){let Ie=ge==null?-1/0:O(ke[ge],L,ee,0),ze=ae==null?1/0:O(ke[ae],L,ee,0),at=ue-Ie,_e=ze-ue;at<=_e?at<=Z&&(K=ge):_e<=Z&&(K=ae)}else K=ae==null?ge:ge==null?ae:F-ge<=ae-F?ge:ae}else Q&&Qe(ue-O(ke[F],L,ee,0))>Z&&(K=null);return K}}let Ln=h=>{V.event=h};V.idxs=le,V._lock=!1;let mt=V.points;mt.show=ye(mt.show),mt.size=ye(mt.size),mt.stroke=ye(mt.stroke),mt.width=ye(mt.width),mt.fill=ye(mt.fill);let Jt=o.focus=Ke({},e.focus||{alpha:.3},V.focus),Wn=Jt.prox>=0,Yn=Wn&&mt.one,Nt=[],Gn=[],Zn=[];function Gs(h,g){let b=mt.show(o,g);if(b instanceof HTMLElement)return Ct(b,Mf),Ct(b,h.class),on(b,-10,-10,de,Y),w.insertBefore(b,Nt[g]),b}function Zs(h,g){if(i==1||g>0){let b=i==1&&E[h.scale].time,y=h.value;h.value=b?Tc(y)?Cc(j,Mc(y,W)):y||se:y||Ug,h.label=h.label||(b?$g:Ig)}if(Yn||g>0){h.width=h.width==null?1:h.width,h.paths=h.paths||ob||jf,h.fillTo=ye(h.fillTo||Wg),h.pxAlign=+xe(h.pxAlign,k),h.pxRound=qc(h.pxAlign),h.stroke=ye(h.stroke||null),h.fill=ye(h.fill||null),h._stroke=h._fill=h._paths=h._focus=null;let b=Vg(vt(1,h.width),1),y=h.points=Ke({},{size:b,width:vt(1,b*.2),stroke:h.stroke,space:b*2,paths:ib,_stroke:null,_fill:null},h.points);y.show=ye(y.show),y.filter=ye(y.filter),y.fill=ye(y.fill),y.stroke=ye(y.stroke),y.paths=ye(y.paths),y.pxAlign=h.pxAlign}if(ce){let b=He(h,g);me.splice(g,0,b[0]),lt.splice(g,0,b[1]),q.values.push(null)}if(Ne){le.splice(g,0,null);let b=null;Yn?g==0&&(b=Gs(h,g)):g>0&&(b=Gs(h,g)),Nt.splice(g,0,b),Gn.splice(g,0,0),Zn.splice(g,0,0)}rt("addSeries",g)}function Mm(h,g){g=g??P.length,h=i==1?La(h,g,Lc,Rc):La(h,g,{},Nc),P.splice(g,0,h),Zs(P[g],g)}o.addSeries=Mm;function Cm(h){if(P.splice(h,1),ce){q.values.splice(h,1),lt.splice(h,1);let g=me.splice(h,1)[0];xt(null,g.firstChild),g.remove()}Ne&&(le.splice(h,1),Nt.splice(h,1)[0].remove(),Gn.splice(h,1),Zn.splice(h,1)),rt("delSeries",h)}o.delSeries=Cm;let On=[!1,!1,!1,!1];function Dm(h,g){if(h._show=h.show,h.show){let b=h.side%2,y=E[h.scale];y==null&&(h.scale=b?P[1].scale:R,y=E[h.scale]);let S=y.time;h.size=ye(h.size),h.space=ye(h.space),h.rotate=ye(h.rotate),wn(h.incrs)&&h.incrs.forEach(F=>{!xn.has(F)&&xn.set(F,Jc(F))}),h.incrs=ye(h.incrs||(y.distr==2?fg:S?C==1?bg:wg:Hn)),h.splits=ye(h.splits||(S&&y.distr==1?B:y.distr==3?_a:y.distr==4?Fg:Rg)),h.stroke=ye(h.stroke),h.grid.stroke=ye(h.grid.stroke),h.ticks.stroke=ye(h.ticks.stroke),h.border.stroke=ye(h.border.stroke);let D=h.values;h.values=wn(D)&&!wn(D[0])?ye(D):S?wn(D)?_c(j,Ac(D,W)):Tc(D)?Tg(j,D):D||re:D||Ng,h.filter=ye(h.filter||(y.distr>=3&&y.log==10?zg:y.distr==3&&y.log==2?Xg:Gc)),h.font=Uc(h.font),h.labelFont=Uc(h.labelFont),h._size=h.size(o,null,g,0),h._space=h._rotate=h._incrs=h._found=h._splits=h._values=null,h._size>0&&(On[g]=!0,h._el=qt(Pf,u))}}function Io(h,g,b,y){let[S,D,F,z]=b,K=g%2,Z=0;return K==0&&(z||D)&&(Z=g==0&&!S||g==2&&!F?Je(Dc.size/3):0),K==1&&(S||F)&&(Z=g==1&&!D||g==3&&!z?Je(Ic.size/2):0),Z}let Js=o.padding=(e.padding||[Io,Io,Io,Io]).map(h=>ye(xe(h,Io))),mn=o._padding=Js.map((h,g)=>h(o,g,On,0)),dt,tt=null,nt=null,fi=i==1?P[0].idxs:null,Ut=null,$o=!1;function Qs(h,g){if(t=h??[],o.data=o._data=t,i==2){dt=0;for(let b=1;b<P.length;b++)dt+=t[b][0].length}else{t.length==0&&(o.data=o._data=t=[[]]),Ut=t[0],dt=Ut.length;let b=t;if(H==2){b=t.slice();let y=b[0]=Array(dt);for(let S=0;S<dt;S++)y[S]=S}o._data=t=b}if(Qn(!0),rt("setData"),H==2&&(jn=!0),g!==!1){let b=L;b.auto(o,$o)?Nr():gn(R,b.min,b.max),Dn=Dn||V.left>=0,$t=!0,eo()}}o.setData=Qs;function Nr(){$o=!0;let h,g;i==1&&(dt>0?(tt=fi[0]=0,nt=fi[1]=dt-1,h=t[0][tt],g=t[0][nt],H==2?(h=tt,g=nt):h==g&&(H==3?[h,g]=ji(h,h,L.log,!1):H==4?[h,g]=Na(h,h,L.log,!1):L.time?g=h+Je(86400/C):[h,g]=Vi(h,g,Ra,!0))):(tt=fi[0]=h=null,nt=fi[1]=g=null)),gn(R,h,g)}let gi,Jn,Rr,Fr,qr,Hr,zr,Xr,Ur,Tt;function el(h,g,b,y,S,D){h??(h=ac),b??(b=qa),y??(y="butt"),S??(S=ac),D??(D="round"),h!=gi&&(m.strokeStyle=gi=h),S!=Jn&&(m.fillStyle=Jn=S),g!=Rr&&(m.lineWidth=Rr=g),D!=qr&&(m.lineJoin=qr=D),y!=Hr&&(m.lineCap=Hr=y),b!=Fr&&m.setLineDash(Fr=b)}function tl(h,g,b,y){g!=Jn&&(m.fillStyle=Jn=g),h!=zr&&(m.font=zr=h),b!=Xr&&(m.textAlign=Xr=b),y!=Ur&&(m.textBaseline=Ur=y)}function Vr(h,g,b,y,S=0){if(y.length>0&&h.auto(o,$o)&&(g==null||g.min==null)){let D=xe(tt,0),F=xe(nt,y.length-1),z=b.min==null?zf(y,D,F,S,h.distr==3):[b.min,b.max];h.min=Yt(h.min,b.min=z[0]),h.max=vt(h.max,b.max=z[1])}}let nl={min:null,max:null};function Lm(){for(let y in E){let S=E[y];_[y]==null&&(S.min==null||_[R]!=null&&S.auto(o,$o))&&(_[y]=nl)}for(let y in E){let S=E[y];_[y]==null&&S.from!=null&&_[S.from]!=null&&(_[y]=nl)}_[R]!=null&&Qn(!0);let h={};for(let y in _){let S=_[y];if(S!=null){let D=h[y]=mo(E[y],Gf);if(S.min!=null)Ke(D,S);else if(y!=R||i==2)if(dt==0&&D.from==null){let F=D.range(o,null,null,y);D.min=F[0],D.max=F[1]}else D.min=Me,D.max=-Me}}if(dt>0){P.forEach((y,S)=>{if(i==1){let D=y.scale,F=_[D];if(F==null)return;let z=h[D];if(S==0){let K=z.range(o,z.min,z.max,D);z.min=K[0],z.max=K[1],tt=Wt(z.min,t[0]),nt=Wt(z.max,t[0]),nt-tt>1&&(t[0][tt]<z.min&&tt++,t[0][nt]>z.max&&nt--),y.min=Ut[tt],y.max=Ut[nt]}else y.show&&y.auto&&Vr(z,F,y,t[S],y.sorted);y.idxs[0]=tt,y.idxs[1]=nt}else if(S>0&&y.show&&y.auto){let[D,F]=y.facets,z=D.scale,K=F.scale,[Z,Q]=t[S],ee=h[z],ue=h[K];ee!=null&&Vr(ee,_[z],D,Z,D.sorted),ue!=null&&Vr(ue,_[K],F,Q,F.sorted),y.min=F.min,y.max=F.max}});for(let y in h){let S=h[y],D=_[y];if(S.from==null&&(D==null||D.min==null)){let F=S.range(o,S.min==Me?null:S.min,S.max==-Me?null:S.max,y);S.min=F[0],S.max=F[1]}}}for(let y in h){let S=h[y];if(S.from!=null){let D=h[S.from];if(D.min==null)S.min=S.max=null;else{let F=S.range(o,D.min,D.max,y);S.min=F[0],S.max=F[1]}}}let g={},b=!1;for(let y in h){let S=h[y],D=E[y];if(D.min!=S.min||D.max!=S.max){D.min=S.min,D.max=S.max;let F=D.distr;D._min=F==3?un(D.min):F==4?va(D.min,D.asinh):F==100?D.fwd(D.min):D.min,D._max=F==3?un(D.max):F==4?va(D.max,D.asinh):F==100?D.fwd(D.max):D.max,g[y]=b=!0}}if(b){P.forEach((y,S)=>{i==2?S>0&&g.y&&(y._paths=null):g[y.scale]&&(y._paths=null)});for(let y in g)jn=!0,rt("setScale",y);Ne&&V.left>=0&&(Dn=$t=!0)}for(let y in _)_[y]=null}function Om(h){let g=Aa(tt-1,0,dt-1),b=Aa(nt+1,0,dt-1);for(;h[g]==null&&g>0;)g--;for(;h[b]==null&&b<dt-1;)b++;return[g,b]}function Im(){if(dt>0){let h=P.some(g=>g._focus)&&Tt!=Jt.alpha;h&&(m.globalAlpha=Tt=Jt.alpha),P.forEach((g,b)=>{if(b>0&&g.show&&(ol(b,!1),ol(b,!0),g._paths==null)){let y=Tt;Tt!=g.alpha&&(m.globalAlpha=Tt=g.alpha);let S=i==2?[0,t[b][0].length-1]:Om(t[b]);g._paths=g.paths(o,b,S[0],S[1]),Tt!=y&&(m.globalAlpha=Tt=y)}}),P.forEach((g,b)=>{if(b>0&&g.show){let y=Tt;Tt!=g.alpha&&(m.globalAlpha=Tt=g.alpha),g._paths!=null&&il(b,!1);{let S=g._paths!=null?g._paths.gaps:null,D=g.points.show(o,b,tt,nt,S),F=g.points.filter(o,b,D,S);(D||F)&&(g.points._paths=g.points.paths(o,b,tt,nt,F),il(b,!0))}Tt!=y&&(m.globalAlpha=Tt=y),rt("drawSeries",b)}}),h&&(m.globalAlpha=Tt=1)}}function ol(h,g){let b=g?P[h].points:P[h];b._stroke=b.stroke(o,h),b._fill=b.fill(o,h)}function il(h,g){let b=g?P[h].points:P[h],{stroke:y,fill:S,clip:D,flags:F,_stroke:z=b._stroke,_fill:K=b._fill,_width:Z=b.width}=b._paths;Z=Ce(Z*Te,3);let Q=null,ee=Z%2/2;g&&K==null&&(K=Z>0?"#fff":z);let ue=b.pxAlign==1&&ee>0;if(ue&&m.translate(ee,ee),!g){let ke=Et-Z/2,we=It-Z/2,ge=kt+Z,ae=Xt+Z;Q=new Path2D,Q.rect(ke,we,ge,ae)}g?Kr(z,Z,b.dash,b.cap,K,y,S,F,D):$m(h,z,Z,b.dash,b.cap,K,y,S,F,Q,D),ue&&m.translate(-ee,-ee)}function $m(h,g,b,y,S,D,F,z,K,Z,Q){let ee=!1;K!=0&&U.forEach((ue,ke)=>{if(ue.series[0]==h){let we=P[ue.series[1]],ge=t[ue.series[1]],ae=(we._paths||Go).band;wn(ae)&&(ae=ue.dir==1?ae[0]:ae[1]);let ne,Ie=null;we.show&&ae&&Uf(ge,tt,nt)?(Ie=ue.fill(o,ke)||D,ne=we._paths.clip):ae=null,Kr(g,b,y,S,Ie,F,z,K,Z,Q,ne,ae),ee=!0}}),ee||Kr(g,b,y,S,D,F,z,K,Z,Q)}let rl=fo|Ca;function Kr(h,g,b,y,S,D,F,z,K,Z,Q,ee){el(h,g,b,y,S),(K||Z||ee)&&(m.save(),K&&m.clip(K),Z&&m.clip(Z)),ee?(z&rl)==rl?(m.clip(ee),Q&&m.clip(Q),yi(S,F),bi(h,D,g)):z&Ca?(yi(S,F),m.clip(ee),bi(h,D,g)):z&fo&&(m.save(),m.clip(ee),Q&&m.clip(Q),yi(S,F),m.restore(),bi(h,D,g)):(yi(S,F),bi(h,D,g)),(K||Z||ee)&&m.restore()}function bi(h,g,b){b>0&&(g instanceof Map?g.forEach((y,S)=>{m.strokeStyle=gi=S,m.stroke(y)}):g!=null&&h&&m.stroke(g))}function yi(h,g){g instanceof Map?g.forEach((b,y)=>{m.fillStyle=Jn=y,m.fill(b)}):g!=null&&h&&m.fill(g)}function Nm(h,g,b,y){let S=$[h],D;if(y<=0)D=[0,0];else{let F=S._space=S.space(o,h,g,b,y),z=S._incrs=S.incrs(o,h,g,b,y,F);D=db(g,b,z,y,F)}return S._found=D}function Br(h,g,b,y,S,D,F,z,K,Z){let Q=F%2/2;k==1&&m.translate(Q,Q),el(z,F,K,Z,z),m.beginPath();let ee,ue,ke,we,ge=S+(y==0||y==3?-D:D);b==0?(ue=S,we=ge):(ee=S,ke=ge);for(let ae=0;ae<h.length;ae++)g[ae]!=null&&(b==0?ee=ke=h[ae]:ue=we=h[ae],m.moveTo(ee,ue),m.lineTo(ke,we));m.stroke(),k==1&&m.translate(-Q,-Q)}function Rm(h){let g=!0;return $.forEach((b,y)=>{if(!b.show)return;let S=E[b.scale];if(S.min==null){b._show&&(g=!1,b._show=!1,Qn(!1));return}else b._show||(g=!1,b._show=!0,Qn(!1));let D=b.side,F=D%2,{min:z,max:K}=S,[Z,Q]=Nm(y,z,K,F==0?de:Y);if(Q==0)return;let ee=S.distr==2,ue=b._splits=b.splits(o,y,z,K,Z,Q,ee),ke=S.distr==2?ue.map(ne=>Ut[ne]):ue,we=S.distr==2?Ut[ue[1]]-Ut[ue[0]]:Z,ge=b._values=b.values(o,b.filter(o,ke,y,Q,we),y,Q,we);b._rotate=D==2?b.rotate(o,ge,y,Q):0;let ae=b._size;b._size=Ht(b.size(o,ge,y,h)),ae!=null&&b._size!=ae&&(g=!1)}),g}function Fm(h){let g=!0;return Js.forEach((b,y)=>{let S=b(o,y,On,h);S!=mn[y]&&(g=!1),mn[y]=S}),g}function qm(){for(let h=0;h<$.length;h++){let g=$[h];if(!g.show||!g._show)continue;let b=g.side,y=b%2,S,D,F=g.stroke(o,h),z=b==0||b==3?-1:1,[K,Z]=g._found;if(g.label!=null){let bt=g.labelGap*z,Mt=Je((g._lpos+bt)*Te);tl(g.labelFont[0],F,"center",b==2?"top":rc),m.save(),y==1?(S=D=0,m.translate(Mt,Je(It+Xt/2)),m.rotate((b==3?-Xi:Xi)/2)):(S=Je(Et+kt/2),D=Mt);let Nn=Wc(g.label)?g.label(o,h,K,Z):g.label;m.fillText(Nn,S,D),m.restore()}if(Z==0)continue;let Q=E[g.scale],ee=y==0?kt:Xt,ue=y==0?Et:It,ke=g._splits,we=Q.distr==2?ke.map(bt=>Ut[bt]):ke,ge=Q.distr==2?Ut[ke[1]]-Ut[ke[0]]:K,ae=g.ticks,ne=g.border,Ie=ae.show?ae.size:0,ze=Je(Ie*Te),at=Je((g.alignTo==2?g._size-Ie-g.gap:g.gap)*Te),_e=g._rotate*-Xi/180,Xe=A(g._pos*Te),At=(ze+at)*z,gt=Xe+At;D=y==0?gt:0,S=y==1?gt:0;let Rt=g.font[0],Vt=g.align==1?lo:g.align==2?ga:_e>0?lo:_e<0?ga:y==0?"center":b==3?ga:lo,tn=_e||y==1?"middle":b==2?"top":rc;tl(Rt,F,Vt,tn);let _t=g.font[1]*g.lineGap,Ft=ke.map(bt=>A(l(bt,Q,ee,ue))),Kt=g._values;for(let bt=0;bt<Kt.length;bt++){let Mt=Kt[bt];if(Mt!=null){y==0?S=Ft[bt]:D=Ft[bt],Mt=""+Mt;let Nn=Mt.indexOf(`
`)==-1?[Mt]:Mt.split(/\n/gm);for(let yt=0;yt<Nn.length;yt++){let Sl=Nn[yt];_e?(m.save(),m.translate(S,D+yt*_t),m.rotate(_e),m.fillText(Sl,0,0),m.restore()):m.fillText(Sl,S,D+yt*_t)}}}ae.show&&Br(Ft,ae.filter(o,we,h,Z,ge),y,b,Xe,ze,Ce(ae.width*Te,3),ae.stroke(o,h),ae.dash,ae.cap);let nn=g.grid;nn.show&&Br(Ft,nn.filter(o,we,h,Z,ge),y,y==0?2:1,y==0?It:Et,y==0?Xt:kt,Ce(nn.width*Te,3),nn.stroke(o,h),nn.dash,nn.cap),ne.show&&Br([Xe],[1],y==0?1:0,y==0?1:2,y==1?It:Et,y==1?Xt:kt,Ce(ne.width*Te,3),ne.stroke(o,h),ne.dash,ne.cap)}rt("drawAxes")}function Qn(h){P.forEach((g,b)=>{b>0&&(g._paths=null,h&&(i==1?(g.min=null,g.max=null):g.facets.forEach(y=>{y.min=null,y.max=null})))})}let vi=!1,jr=!1,No=[];function Hm(){jr=!1;for(let h=0;h<No.length;h++)rt(...No[h]);No.length=0}function eo(){vi||(og(al),vi=!0)}function zm(h,g=!1){vi=!0,jr=g,h(o),al(),g&&No.length>0&&queueMicrotask(Hm)}o.batch=zm;function al(){if(Ir&&(Lm(),Ir=!1),jn&&(Pm(),jn=!1),hi){if($e(f,lo,Ee),$e(f,"top",Ze),$e(f,Bo,de),$e(f,jo,Y),$e(w,lo,Ee),$e(w,"top",Ze),$e(w,Bo,de),$e(w,jo,Y),$e(u,Bo,Ge),$e(u,jo,Pt),p.width=Je(Ge*Te),p.height=Je(Pt*Te),$.forEach(({_el:h,_show:g,_size:b,_pos:y,side:S})=>{if(h!=null)if(g){let D=S===3||S===0?b:0,F=S%2==1;$e(h,F?"left":"top",y-D),$e(h,F?"width":"height",b),$e(h,F?"top":"left",F?Ze:Ee),$e(h,F?"height":"width",F?Y:de),Pa(h,zn)}else Ct(h,zn)}),gi=Jn=Rr=qr=Hr=zr=Xr=Ur=Fr=null,Tt=1,qo(!0),Ee!=Bn||Ze!=Mn||de!=Zt||Y!=Cn){Qn(!1);let h=de/Zt,g=Y/Cn;if(Ne&&!Dn&&V.left>=0){V.left*=h,V.top*=g,to&&on(to,Je(V.left),0,de,Y),no&&on(no,0,Je(V.top),de,Y);for(let b=0;b<Nt.length;b++){let y=Nt[b];y!=null&&(Gn[b]*=h,Zn[b]*=g,on(y,Ht(Gn[b]),Ht(Zn[b]),de,Y))}}if(Oe.show&&!mi&&Oe.left>=0&&Oe.width>0){Oe.left*=h,Oe.width*=h,Oe.top*=g,Oe.height*=g;for(let b in Qr)$e(ro,b,Oe[b])}Bn=Ee,Mn=Ze,Zt=de,Cn=Y}rt("setSize"),hi=!1}Ge>0&&Pt>0&&(m.clearRect(0,0,p.width,p.height),rt("drawClear"),x.forEach(h=>h()),rt("draw")),Oe.show&&mi&&(wi(Oe),mi=!1),Ne&&Dn&&($n(null,!0,!1),Dn=!1),q.show&&q.live&&$t&&(Zr(),$t=!1),d||(d=!0,o.status=1,rt("ready")),$o=!1,vi=!1}o.redraw=(h,g)=>{jn=g||!1,h!==!1?gn(R,L.min,L.max):eo()};function Wr(h,g){let b=E[h];if(b.from==null){if(dt==0){let y=b.range(o,g.min,g.max,h);g.min=y[0],g.max=y[1]}if(g.min>g.max){let y=g.min;g.min=g.max,g.max=y}if(dt>1&&g.min!=null&&g.max!=null&&g.max-g.min<1e-16)return;h==R&&b.distr==2&&dt>0&&(g.min=Wt(g.min,t[0]),g.max=Wt(g.max,t[0]),g.min==g.max&&g.max++),_[h]=g,Ir=!0,eo()}}o.setScale=Wr;let Yr,Gr,to,no,sl,ll,oo,io,cl,dl,Le,Fe,fn=!1,ut=V.drag,ot=ut.x,it=ut.y;Ne&&(V.x&&(Yr=qt(Af,w)),V.y&&(Gr=qt(_f,w)),L.ori==0?(to=Yr,no=Gr):(to=Gr,no=Yr),Le=V.left,Fe=V.top);let Oe=o.select=Ke({show:!0,over:!0,left:0,width:0,top:0,height:0},e.select),ro=Oe.show?qt(Ef,Oe.over?w:f):null;function wi(h,g){if(Oe.show){for(let b in h)Oe[b]=h[b],b in Qr&&$e(ro,b,h[b]);g!==!1&&rt("setSelect")}}o.setSelect=wi;function Xm(h){if(P[h].show)ce&&Pa(me[h],zn);else if(ce&&Ct(me[h],zn),Ne){let b=Yn?Nt[0]:Nt[h];b!=null&&on(b,-10,-10,de,Y)}}function gn(h,g,b){Wr(h,{min:g,max:b})}function Qt(h,g,b,y){g.focus!=null&&jm(h),g.show!=null&&P.forEach((S,D)=>{D>0&&(h==D||h==null)&&(S.show=g.show,Xm(D),i==2?(gn(S.facets[0].scale,null,null),gn(S.facets[1].scale,null,null)):gn(S.scale,null,null),eo())}),b!==!1&&rt("setSeries",h,g),y&&Ho("setSeries",o,h,g)}o.setSeries=Qt;function Um(h,g){Ke(U[h],g)}function Vm(h,g){h.fill=ye(h.fill||null),h.dir=xe(h.dir,-1),g=g??U.length,U.splice(g,0,h)}function Km(h){h==null?U.length=0:U.splice(h,1)}o.addBand=Vm,o.setBand=Um,o.delBand=Km;function Bm(h,g){P[h].alpha=g,Ne&&Nt[h]!=null&&(Nt[h].style.opacity=g),ce&&me[h]&&(me[h].style.opacity=g)}let ln,bn,In,ao={focus:!0};function jm(h){if(h!=In){let g=h==null,b=Jt.alpha!=1;P.forEach((y,S)=>{if(i==1||S>0){let D=g||S==0||S==h;y._focus=g?null:D,b&&Bm(S,D?1:Jt.alpha)}}),In=h,b&&eo()}}ce&&Wn&&Se(cc,pe,h=>{V._lock||(Ln(h),In!=null&&Qt(null,ao,!0,Ve.setSeries))});function en(h,g,b){let y=E[g];b&&(h=h/Te-(y.ori==1?Ze:Ee));let S=de;y.ori==1&&(S=Y,h=S-h),y.dir==-1&&(h=S-h);let D=y._min,F=y._max,z=h/S,K=D+(F-D)*z,Z=y.distr;return Z==3?po(10,K):Z==4?Kf(K,y.asinh):Z==100?y.bwd(K):K}function Wm(h,g){let b=en(h,R,g);return Wt(b,t[0],tt,nt)}o.valToIdx=h=>Wt(h,t[0]),o.posToIdx=Wm,o.posToVal=en,o.valToPos=(h,g,b)=>E[g].ori==0?r(h,E[g],b?kt:de,b?Et:0):a(h,E[g],b?Xt:Y,b?It:0),o.setCursor=(h,g,b)=>{Le=h.left,Fe=h.top,$n(null,g,b)};function ul(h,g){$e(ro,lo,Oe.left=h),$e(ro,Bo,Oe.width=g)}function pl(h,g){$e(ro,"top",Oe.top=h),$e(ro,jo,Oe.height=g)}let Ro=L.ori==0?ul:pl,Fo=L.ori==1?ul:pl;function Ym(){if(ce&&q.live)for(let h=i==2?1:0;h<P.length;h++){if(h==0&&ct)continue;let g=q.values[h],b=0;for(let y in g)lt[h][b++].firstChild.nodeValue=g[y]}}function Zr(h,g){if(h!=null&&(h.idxs?h.idxs.forEach((b,y)=>{le[y]=b}):Yf(h.idx)||le.fill(h.idx),q.idx=le[0]),ce&&q.live){for(let b=0;b<P.length;b++)(b>0||i==1&&!ct)&&Gm(b,le[b]);Ym()}$t=!1,g!==!1&&rt("setLegend")}o.setLegend=Zr;function Gm(h,g){let b=P[h],y=h==0&&H==2?Ut:t[h],S;ct?S=b.values(o,h,g)??I:(S=b.value(o,g==null?null:y[g],h,g),S=S==null?I:{_:S}),q.values[h]=S}function $n(h,g,b){cl=Le,dl=Fe,[Le,Fe]=V.move(o,Le,Fe),V.left=Le,V.top=Fe,Ne&&(to&&on(to,Je(Le),0,de,Y),no&&on(no,0,Je(Fe),de,Y));let y,S=tt>nt;ln=Me,bn=null;let D=L.ori==0?de:Y,F=L.ori==1?de:Y;if(Le<0||dt==0||S){y=V.idx=null;for(let z=0;z<P.length;z++){let K=Nt[z];K!=null&&on(K,-10,-10,de,Y)}Wn&&Qt(null,ao,!0,h==null&&Ve.setSeries),q.live&&(le.fill(y),$t=!0)}else{let z,K,Z;i==1&&(z=L.ori==0?Le:Fe,K=en(z,R),y=V.idx=Wt(K,t[0],tt,nt),Z=O(t[0][y],L,D,0));let Q=-10,ee=-10,ue=0,ke=0,we=!0,ge="",ae="";for(let ne=i==2?1:0;ne<P.length;ne++){let Ie=P[ne],ze=le[ne],at=ze==null?null:i==1?t[ne][ze]:t[ne][1][ze],_e=V.dataIdx(o,ne,y,K),Xe=_e==null?null:i==1?t[ne][_e]:t[ne][1][_e];if($t=$t||Xe!=at||_e!=ze,le[ne]=_e,ne>0&&Ie.show){let At=_e==null?-10:_e==y?Z:O(i==1?t[0][_e]:t[ne][0][_e],L,D,0),gt=Xe==null?-10:X(Xe,i==1?E[Ie.scale]:E[Ie.facets[1].scale],F,0);if(Wn&&Xe!=null){let Rt=L.ori==1?Le:Fe,Vt=Qe(Jt.dist(o,ne,_e,gt,Rt));if(Vt<ln){let tn=Jt.bias;if(tn!=0){let _t=en(Rt,Ie.scale),Ft=Xe>=0?1:-1,Kt=_t>=0?1:-1;Kt==Ft&&(Kt==1?tn==1?Xe>=_t:Xe<=_t:tn==1?Xe<=_t:Xe>=_t)&&(ln=Vt,bn=ne)}else ln=Vt,bn=ne}}if($t||Yn){let Rt,Vt;L.ori==0?(Rt=At,Vt=gt):(Rt=gt,Vt=At);let tn,_t,Ft,Kt,nn,bt,Mt=!0,Nn=mt.bbox;if(Nn!=null){Mt=!1;let yt=Nn(o,ne);Ft=yt.left,Kt=yt.top,tn=yt.width,_t=yt.height}else Ft=Rt,Kt=Vt,tn=_t=mt.size(o,ne);if(bt=mt.fill(o,ne),nn=mt.stroke(o,ne),Yn)ne==bn&&ln<=Jt.prox&&(Q=Ft,ee=Kt,ue=tn,ke=_t,we=Mt,ge=bt,ae=nn);else{let yt=Nt[ne];yt!=null&&(Gn[ne]=Ft,Zn[ne]=Kt,gc(yt,tn,_t,Mt),mc(yt,bt,nn),on(yt,Ht(Ft),Ht(Kt),de,Y))}}}}if(Yn){let ne=Jt.prox,Ie=In==null?ln<=ne:ln>ne||bn!=In;if($t||Ie){let ze=Nt[0];ze!=null&&(Gn[0]=Q,Zn[0]=ee,gc(ze,ue,ke,we),mc(ze,ge,ae),on(ze,Ht(Q),Ht(ee),de,Y))}}}if(Oe.show&&fn)if(h!=null){let[z,K]=Ve.scales,[Z,Q]=Ve.match,[ee,ue]=h.cursor.sync.scales,ke=h.cursor.drag;if(ot=ke._x,it=ke._y,ot||it){let{left:we,top:ge,width:ae,height:ne}=h.select,Ie=h.scales[ee].ori,ze=h.posToVal,at,_e,Xe,At,gt,Rt=z!=null&&Z(z,ee),Vt=K!=null&&Q(K,ue);Rt&&ot?(Ie==0?(at=we,_e=ae):(at=ge,_e=ne),Xe=E[z],At=O(ze(at,ee),Xe,D,0),gt=O(ze(at+_e,ee),Xe,D,0),Ro(Yt(At,gt),Qe(gt-At))):Ro(0,D),Vt&&it?(Ie==1?(at=we,_e=ae):(at=ge,_e=ne),Xe=E[K],At=X(ze(at,ue),Xe,F,0),gt=X(ze(at+_e,ue),Xe,F,0),Fo(Yt(At,gt),Qe(gt-At))):Fo(0,F)}else ea()}else{let z=Qe(cl-sl),K=Qe(dl-ll);if(L.ori==1){let ue=z;z=K,K=ue}ot=ut.x&&z>=ut.dist,it=ut.y&&K>=ut.dist;let Z=ut.uni;Z!=null?ot&&it&&(ot=z>=Z,it=K>=Z,!ot&&!it&&(K>z?it=!0:ot=!0)):ut.x&&ut.y&&(ot||it)&&(ot=it=!0);let Q,ee;ot&&(L.ori==0?(Q=oo,ee=Le):(Q=io,ee=Fe),Ro(Yt(Q,ee),Qe(ee-Q)),it||Fo(0,F)),it&&(L.ori==1?(Q=oo,ee=Le):(Q=io,ee=Fe),Fo(Yt(Q,ee),Qe(ee-Q)),ot||Ro(0,D)),!ot&&!it&&(Ro(0,0),Fo(0,0))}if(ut._x=ot,ut._y=it,h==null){if(b){if(Tl!=null){let[z,K]=Ve.scales;Ve.values[0]=z!=null?en(L.ori==0?Le:Fe,z):null,Ve.values[1]=K!=null?en(L.ori==1?Le:Fe,K):null}Ho(ba,o,Le,Fe,de,Y,y)}if(Wn){let z=b&&Ve.setSeries,K=Jt.prox;In==null?ln<=K&&Qt(bn,ao,!0,z):ln>K?Qt(null,ao,!0,z):bn!=In&&Qt(bn,ao,!0,z)}}$t&&(q.idx=y,Zr()),g!==!1&&rt("setCursor")}let yn=null;Object.defineProperty(o,"rect",{get(){return yn==null&&qo(!1),yn}});function qo(h=!1){h?yn=null:(yn=w.getBoundingClientRect(),rt("syncRect",yn))}function hl(h,g,b,y,S,D,F){V._lock||fn&&h!=null&&h.movementX==0&&h.movementY==0||(Jr(h,g,b,y,S,D,F,!1,h!=null),h!=null?$n(null,!0,!0):$n(g,!0,!1))}function Jr(h,g,b,y,S,D,F,z,K){if(yn==null&&qo(!1),Ln(h),h!=null)b=h.clientX-yn.left,y=h.clientY-yn.top;else{if(b<0||y<0){Le=-10,Fe=-10;return}let[Z,Q]=Ve.scales,ee=g.cursor.sync,[ue,ke]=ee.values,[we,ge]=ee.scales,[ae,ne]=Ve.match,Ie=g.axes[0].side%2==1,ze=L.ori==0?de:Y,at=L.ori==1?de:Y,_e=Ie?D:S,Xe=Ie?S:D,At=Ie?y:b,gt=Ie?b:y;if(we!=null?b=ae(Z,we)?l(ue,E[Z],ze,0):-10:b=ze*(At/_e),ge!=null?y=ne(Q,ge)?l(ke,E[Q],at,0):-10:y=at*(gt/Xe),L.ori==1){let Rt=b;b=y,y=Rt}}K&&(g==null||g.cursor.event.type==ba)&&((b<=1||b>=de-1)&&(b=qn(b,de)),(y<=1||y>=Y-1)&&(y=qn(y,Y))),z?(sl=b,ll=y,[oo,io]=V.move(o,b,y)):(Le=b,Fe=y)}let Qr={width:0,height:0,left:0,top:0};function ea(){wi(Qr,!1)}let ml,fl,gl,bl;function yl(h,g,b,y,S,D,F){fn=!0,ot=it=ut._x=ut._y=!1,Jr(h,g,b,y,S,D,F,!0,!1),h!=null&&(Se(ya,Ta,vl,!1),Ho(sc,o,oo,io,de,Y,null));let{left:z,top:K,width:Z,height:Q}=Oe;ml=z,fl=K,gl=Z,bl=Q}function vl(h,g,b,y,S,D,F){fn=ut._x=ut._y=!1,Jr(h,g,b,y,S,D,F,!1,!0);let{left:z,top:K,width:Z,height:Q}=Oe,ee=Z>0||Q>0,ue=ml!=z||fl!=K||gl!=Z||bl!=Q;if(ee&&ue&&wi(Oe),ut.setScale&&ee&&ue){let ke=z,we=Z,ge=K,ae=Q;if(L.ori==1&&(ke=K,we=Q,ge=z,ae=Z),ot&&gn(R,en(ke,R),en(ke+we,R)),it)for(let ne in E){let Ie=E[ne];ne!=R&&Ie.from==null&&Ie.min!=Me&&gn(ne,en(ge+ae,ne),en(ge,ne))}ea()}else V.lock&&(V._lock=!V._lock,$n(g,!0,h!=null));h!=null&&(xt(ya,Ta),Ho(ya,o,Le,Fe,de,Y,null))}function Zm(h,g,b,y,S,D,F){if(V._lock)return;Ln(h);let z=fn;if(fn){let K=!0,Z=!0,Q=10,ee,ue;L.ori==0?(ee=ot,ue=it):(ee=it,ue=ot),ee&&ue&&(K=Le<=Q||Le>=de-Q,Z=Fe<=Q||Fe>=Y-Q),ee&&K&&(Le=Le<oo?0:de),ue&&Z&&(Fe=Fe<io?0:Y),$n(null,!0,!0),fn=!1}Le=-10,Fe=-10,le.fill(null),$n(null,!0,!0),z&&(fn=z)}function wl(h,g,b,y,S,D,F){V._lock||(Ln(h),Nr(),ea(),h!=null&&Ho(dc,o,Le,Fe,de,Y,null))}function xl(){$.forEach(ub),$r(o.width,o.height,!0)}Xn(Ui,uo,xl);let so={};so.mousedown=yl,so.mousemove=hl,so.mouseup=vl,so.dblclick=wl,so.setSeries=(h,g,b,y)=>{let S=Ve.match[2];b=S(o,g,b),b!=-1&&Qt(b,y,!0,!1)},Ne&&(Se(sc,w,yl),Se(ba,w,hl),Se(lc,w,h=>{Ln(h),qo(!1)}),Se(cc,w,Zm),Se(dc,w,wl),Da.add(o),o.syncRect=qo);let xi=o.hooks=e.hooks||{};function rt(h,g,b){jr?No.push([h,g,b]):h in xi&&xi[h].forEach(y=>{y.call(null,o,g,b)})}(e.plugins||[]).forEach(h=>{for(let g in h.hooks)xi[g]=(xi[g]||[]).concat(h.hooks[g])});let kl=(h,g,b)=>b,Ve=Ke({key:null,setSeries:!1,filters:{pub:xc,sub:xc},scales:[R,P[1]?P[1].scale:null],match:[kc,kc,kl],values:[null,null]},V.sync);Ve.match.length==2&&Ve.match.push(kl),V.sync=Ve;let Tl=Ve.key,ta=yd(Tl);function Ho(h,g,b,y,S,D,F){Ve.filters.pub(h,g,b,y,S,D,F)&&ta.pub(h,g,b,y,S,D,F)}ta.sub(o);function Jm(h,g,b,y,S,D,F){Ve.filters.sub(h,g,b,y,S,D,F)&&so[h](null,g,b,y,S,D,F)}o.pub=Jm;function Qm(){ta.unsub(o),Da.delete(o),ht.clear(),Ea(Ui,uo,xl),c.remove(),pe?.remove(),rt("destroy")}o.destroy=Qm;function na(){rt("init",e,t),Qs(t||e.data,!1),_[R]?Wr(R,_[R]):Nr(),mi=Oe.show&&(Oe.width>0||Oe.height>0),Dn=$t=!0,$r(e.width,e.height)}return P.forEach(Zs),$.forEach(Dm),n?n instanceof HTMLElement?(n.appendChild(c),na()):n(o,na):na(),o}De.assign=Ke;De.fmtNum=Fa;De.rangeNum=Vi;De.rangeLog=ji;De.rangeAsinh=Na;De.orient=Vn;De.pxRatio=Te;De.join=ng;De.fmtDate=Ha,De.tzDate=hg;De.sync=yd;{De.addGap=Yg,De.clipGaps=Gi;let e=De.paths={points:Sd};e.linear=Ed,e.stepped=Jg,e.bars=Qg,e.spline=tb}function er(e){let t=e.width,n=e.height,o=e.getContext("2d");if(!t||!n||!o)return null;let i=o.getImageData(0,0,t,n).data,r=new Uint8ClampedArray(t*n*3);for(let a=0,l=0;l<r.length;a+=4,l+=3)r[l]=i[a],r[l+1]=i[a+1],r[l+2]=i[a+2];return{w:t,h:n,rgb:r}}var pb=["#1f2937","#d62839","#e8590c","#d97706","#b8860b","#a88400","#6b8e00","#2b9348","#0c8599","#1c7ed6","#7048e8"],hb=["#e5e7eb","#ff6b6b","#ff7a45","#ffa94d","#fcc419","#ffe066","#c0eb75","#69db7c","#3bc9db","#4dabf7","#b197fc"];function Be(e,t){let n=t?hb:pb;return n[e>=0&&e<n.length?e:0]}function ja(e,t){return e.xmin===t.xmin&&e.xmax===t.xmax&&e.ymin===t.ymin&&e.ymax===t.ymax&&e.left===t.left&&e.top===t.top&&e.width===t.width&&e.height===t.height}var nr=class{constructor(t,n=0){this.frame=t;this.out=new Int32Array(256);this.len=0;this.have=!1;this.pen=!1;this.px=0;this.py=0;this.next=n,this.W=Math.max(1,Math.ceil(t.width)),this.H=Math.max(1,Math.ceil(t.height)),this.stride=this.W+1,this.sx=t.width/(t.xmax-t.xmin),this.sy=t.height/(t.ymax-t.ymin);let o=Number.isFinite(this.sx)&&Number.isFinite(this.sy)?this.stride*(this.H+1):0;this.grid=new Uint32Array(o+31>>>5)}keep(t){if(this.len===this.out.length){let n=new Int32Array(2*this.out.length);n.set(this.out),this.out=n}this.out[this.len++]=t}run(t,n,o,i=1/0){let{W:r,H:a,stride:l,sx:d,sy:c,grid:p}=this,m=this.frame.xmin,u=this.frame.ymax;if(!p.length)return this.next=Math.max(this.next,o),!0;let f=Math.min(o,this.next+i),w=this.have,k=this.pen,A=this.px,C=this.py,P=this.next;for(;P<f;P++){let $=(t[P]-m)*d,E=(u-n[P])*c;if(!($-$===0&&E-E===0)){w=k=!1;continue}if(!w){w=!0,A=$,C=E;continue}let U=!1;if(!(A<0&&$<0||A>r&&$>r||C<0&&E<0||C>a&&E>a)){let R=A,N=C,x=$,v=E,T=!0;if(!(A>=0&&A<=r&&C>=0&&C<=a&&$>=0&&$<=r&&E>=0&&E<=a)){let L=$-A,H=E-C,O=0,X=1;for(let _=0;_<4&&T;_++){let j=_===0?-L:_===1?L:_===2?-H:H,W=_===0?A:_===1?r-A:_===2?C:a-C;if(j===0)W<0&&(T=!1);else{let B=W/j;j<0?B>X?T=!1:B>O&&(O=B):B<O?T=!1:B<X&&(X=B)}}R=A+O*L,N=C+O*H,x=A+X*L,v=C+X*H}if(T){let L=x-R,H=v-N,O=L<0?-L:L,X=H<0?-H:H,_=Math.ceil(O>X?O:X)|0,j=1/(_>0?_:1),W=L*j,B=H*j,re=R,se=N;for(let le=0;le<=_;le++){let q=(se|0)*l+(re|0),V=q>>>5,ce=1<<(q&31);(p[V]&ce)===0&&(p[V]|=ce,U=!0),re+=W,se+=B}}}U?(k||(this.keep(-1),this.keep(P-1)),this.keep(P),k=!0):k=!1,A=$,C=E}return this.have=w,this.pen=k,this.px=A,this.py=C,this.next=P,P>=o}draw(t,n,o,i){let r=o.width/(o.xmax-o.xmin),a=o.height/(o.ymax-o.ymin),l=Math.min(t.length,n.length),d=!0,c=0;for(let p=0;p<this.len;p++){let m=this.out[p];if(m<0||m>=l){d=!0;continue}let u=o.left+(t[m]-o.xmin)*r,f=o.top+(o.ymax-n[m])*a;d?i.moveTo(u,f):i.lineTo(u,f),d=!1,c++}return c}};var tr=new Uint32Array(0);function Cd(e,t,n,o,i,r,a){let l=Math.ceil(r)+1,d=Math.max(1,Math.ceil(i.width))+2*l,c=Math.max(1,Math.ceil(i.height))+2*l,p=i.width/(i.xmax-i.xmin),m=i.height/(i.ymax-i.ymin);if(!(Number.isFinite(p)&&Number.isFinite(m)))return 0;let u=d*c+31>>>5;tr.length<u?tr=new Uint32Array(u):tr.fill(0,0,u);let f=tr,w=0;for(let k=n;k<=o;k++){let A=(e[k]-i.xmin)*p,C=(i.ymax-t[k])*m,P=Math.floor(A)+l,$=Math.floor(C)+l;if(!(P>=0&&P<d&&$>=0&&$<c))continue;let E=$*d+P,U=E>>>5,R=1<<(E&31);f[U]&R||(f[U]|=R,a(i.left+A,i.top+C),w++)}return w}function Dd(e,t,n,o,i,r=1/0){let a=n.width/(n.xmax-n.xmin),l=n.height/(n.ymax-n.ymin),d=null,c=r*r;return e.forEach((p,m)=>{if(t[m])for(let u=0;u<p.xs.length;u++){let f=(p.xs[u]-n.xmin)*a-o,w=(n.ymax-p.ys[u])*l-i,k=f*f+w*w;k<c&&(c=k,d={curve:m,index:u,dist:0})}}),d&&(d.dist=Math.sqrt(c)),d}function Ld(e){return{xName:e.xname,yName:e.yname,xColor:e.xcolor,yColor:e.ycolor,x:ft(e.x),y:ft(e.y),frozen:e.frozen.map(t=>({x:ft(t.x),y:ft(t.y)}))}}function Od(e){return{n:e.n,du:e.du,dv:e.dv,scaled:e.scaled!==0,color:e.color,grid:ft(e.grid),speed:ft(e.speed),flows:e.flows.map(t=>({color:t.color,xs:ft(t.x),ys:ft(t.y)}))}}var Jo=e=>e.length>>2;function Id(e){if(!e.length)return 0;let t=1;for(let n=0;n<e.length;n++)e[n]!==e[n]&&t++;return t}function or(e,t){let n=[];if(e){let i=Jo(e.x)+e.frozen.reduce((a,l)=>a+Jo(l.x),0),r=Jo(e.y)+e.frozen.reduce((a,l)=>a+Jo(l.y),0);i&&n.push({key:"xnull",label:`${e.xName||"x"}-nullcline`,color:e.xColor,count:i}),r&&n.push({key:"ynull",label:`${e.yName||"y"}-nullcline`,color:e.yColor,count:r})}t&&t.n>0&&t.grid.length>=4&&n.push({key:"dfield",label:"Direction field",color:t.color,count:t.grid.length>>2});let o=t?.flows.filter(i=>i.xs.length>0)??[];return o.length&&n.push({key:"flow",label:"Flow",color:o[0].color,count:Id(o[0].xs)}),n}function kn(e){let t=e.width/(e.xmax-e.xmin),n=e.height/(e.ymax-e.ymin);return{x:o=>e.left+(o-e.xmin)*t,y:o=>e.top+(e.ymax-o)*n,sx:t,sy:n}}function Qo(e,t,n){let o=kn(t),i=e.length>>2;for(let r=0;r<i;r++){let a=4*r;n.moveTo(o.x(e[a]),o.y(e[a+1])),n.lineTo(o.x(e[a+2]),o.y(e[a+3]))}return i}function Nd(e,t,n,o){let i=kn(n),r=Math.min(e.length,t.length),a=!1,l=0;for(let d=0;d<r;d++){let c=e[d],p=t[d];if(c!==c||p!==p){a=!1;continue}a?o.lineTo(i.x(c),i.y(p)):o.moveTo(i.x(c),i.y(p)),a=!0,l++}return l}var mb=.6,fb=.9,$d=25*Math.PI/180;function Rd(e,t,n){let o=kn(t),i=e.grid.length>>2,r=Math.min(Math.abs(e.du*o.sx),Math.abs(e.dv*o.sy)),a=new Float64Array(3*i),l=0;for(let m=0;m<i;m++){let u=e.grid[4*m+2]*o.sx,f=-e.grid[4*m+3]*o.sy,w=Math.hypot(u,f);if(!(w>0))continue;let k=(e.speed[m]??0)*w;a[3*m]=u/w,a[3*m+1]=f/w,a[3*m+2]=k,k>l&&(l=k)}let d=new Float32Array(12*i),c=0,p=0;for(let m=0;m<i;m++){let u=a[3*m],f=a[3*m+1];if(u===0&&f===0)continue;let w=e.scaled?mb*r:l>0?fb*r*a[3*m+2]/l:0;if(!(w>=.5))continue;let k=o.x(e.grid[4*m]),A=o.y(e.grid[4*m+1]),C=k+u*w,P=A+f*w,$=Math.min(.35*w,n),E=Math.cos($d),U=Math.sin($d),R=-u,N=-f;d.set([k,A,C,P,C,P,C+$*(R*E-N*U),P+$*(R*U+N*E),C,P,C+$*(R*E+N*U),P+$*(-R*U+N*E)],c),c+=12,p++}return{segments:d.subarray(0,c),arrows:p}}function ir(e){if(!e)return[];let t=[];return e.equilibria.length&&t.push({key:"equilibria",label:"Equilibria",color:0,count:e.equilibria.length}),e.text.length&&t.push({key:"text",label:"Text",color:0,count:e.text.length}),e.arrows.length&&t.push({key:"arrows",label:"Arrows",color:e.arrows[0].color,count:e.arrows.length}),e.markers.length&&t.push({key:"markers",label:"Markers",color:e.markers[0].color,count:e.markers.length}),e.frozen.forEach((n,o)=>t.push({key:`frozen-${o}`,label:n.label,color:n.color,count:n.xs.length})),t}function ei(e,t,n,o,i,r){for(let a=0;a<=i;a++){let l=r+2*Math.PI*a/i,d=t+o*Math.cos(l),c=n-o*Math.sin(l);a?e.lineTo(d,c):e.moveTo(d,c)}}function Fd(e,t,n,o){let i=kn(t),r=i.x(e.x),a=i.y(e.y);return!Number.isFinite(r)||!Number.isFinite(a)?!1:(e.type==="stable"?ei(o,r,a,n,24,0):e.type==="saddle"?ei(o,r,a,1.4*n,3,Math.PI/2):ei(o,r,a,n*Math.SQRT2,4,Math.PI/4),!0)}var gb={box:[[[-6,-6],[6,-6],[6,6],[-6,6],[-6,-6]]],diamond:[[[8,0],[0,-8],[-8,0],[0,8],[8,0]]],triangle:[[[-6,-6],[6,-6],[0,6],[-6,-6]]],plus:[[[-6,0],[6,0]],[[0,-6],[0,6]]],cross:[[[-6,6],[6,-6]],[[-6,-6],[6,6]]],circle:[[]]},qd=4;function Hd(e,t,n,o){let i=kn(t),r=i.x(e.x),a=i.y(e.y),l=n*(e.size>0?e.size:1)/6;if(!Number.isFinite(r)||!Number.isFinite(a))return!1;if(e.shape==="circle")return ei(o,r,a,6*l,24,0),!0;for(let d of gb[e.shape])d.forEach(([c,p],m)=>m?o.lineTo(r+c*l,a-p*l):o.moveTo(r+c*l,a-p*l));return!0}function zd(e,t,n){let o=kn(t),i=o.x(e.x1),r=o.y(e.y1),a=o.x(e.x2),l=o.y(e.y2);if(![i,r,a,l].every(Number.isFinite))return!1;let d=a-i,c=l-r,p=i+e.size*d,m=r+e.size*c,u=-.5*e.size*c,f=.5*e.size*d;return e.pointer&&(n.moveTo(i,r),n.lineTo(a,l)),n.moveTo(p+u,m+f),n.lineTo(i,r),n.lineTo(p-u,m-f),!0}function Wa(e,t,n,o,i,r){let a=kn(o),l=Math.min(e.length,t.length),d=!1,c=0,p=NaN,m=NaN;for(let u=0;u<l;u++){let f=a.x(e[u]),w=a.y(t[u]);if(!Number.isFinite(f)||!Number.isFinite(w)){d=!1;continue}let k=Math.round(f),A=Math.round(w);d&&k===p&&A===m&&u<l-1||(p=k,m=A,c++,n?(d?r.lineTo(f,w):r.moveTo(f,w),d=!0):(ei(r,f,w,i,8,0),d=!0))}return c}var bb=[10,12,14,18,24];function Ya(e){return bb[Math.max(0,Math.min(4,Math.round(e)))]}var yb=100,vb=32768,wb=8,xb=16384;function Xd(e){let t=1/0,n=-1/0;for(let i of e)for(let r=0;r<i.length;r++){let a=i[r];a<t&&(t=a),a>n&&(n=a)}if(!(t<=n))return null;if(t===n)return{min:t-1,max:n+1};let o=(n-t)*.05;return{min:t-o,max:n+o}}function kb(e,t,n){let o=e.left+(t-e.xmin)*e.width/(e.xmax-e.xmin),i=e.top+(e.ymax-n)*e.height/(e.ymax-e.ymin);return Number.isFinite(o)&&Number.isFinite(i)?{x:o,y:i}:null}function ti(e){return getComputedStyle(document.documentElement).getPropertyValue(e).trim()}var vo=class{constructor(t,n){this.root=t;this.cb=n;this.u=null;this.model=null;this.visible=[];this.applying=!1;this.reportPending=!1;this.dark=!1;this.base={x:{min:0,max:1},y:{min:0,max:1}};this.drawStart=0;this.drawMs=[];this.draws=0;this.traces=[];this.traceTimer=null;this.traceStart=0;this.traceMs=null;this.vertices=[];this.nullclines=null;this.dfield=null;this.layers=[];this.marks=null;this.markLayers=[];this.hiddenLayers=new Set;this.runs=[];this.showRuns=!0;this.runsDrawn=0;this.runPaths=new WeakMap;this.layerDrawn=new Map;this.onArea=()=>{};this.linePath=(t,n,o,i)=>{var m,u;let r=this.model?.curves[n-1],a=new Path2D;if(!r)return{stroke:a,fill:null,clip:null,band:null,flags:1};let l=this.frame(t),d=Math.min(i,r.xs.length-1)+1,c=(m=this.traces)[u=n-1]??(m[u]={xs:null,ys:null,row0:0,current:null,done:!1,complete:null});(c.xs!==r.xs.buffer||c.ys!==r.ys.buffer||c.row0!==r.row0)&&(c.current&&c.done&&(c.complete=c.current),c.current=null,c.xs=r.xs.buffer,c.ys=r.ys.buffer,c.row0=r.row0),(!c.current||!ja(c.current.frame,l))&&(c.current&&c.done&&(c.complete=c.current),c.current=new nr(l,Math.max(0,o)),this.traceStart=performance.now());let p=0;return c.done=d-c.current.next<=vb&&c.current.run(r.xs,r.ys,d),c.done||(p+=c.complete?.draw(r.xs,r.ys,l,a)??0,this.traceLater()),this.vertices[n-1]=p+c.current.draw(r.xs,r.ys,l,a),{stroke:a,fill:null,clip:null,band:null,flags:1}}}set(t,n,o,i){let r=!this.u||!this.model||this.model.mode!==t.mode||this.model.curves.length!==t.curves.length||this.dark!==i||this.model.curves.some((a,l)=>a.color!==t.curves[l].color||a.line!==t.curves[l].line||a.label!==t.curves[l].label);r&&(this.visible=t.curves.map(()=>!0)),this.model=t,this.dark=i,this.base={x:n?.x??Xd(t.curves.map(a=>a.xs))??{min:0,max:1},y:n?.y??Xd(t.curves.map(a=>a.ys))??{min:0,max:1}},r?this.create():this.u.setData(this.data(),!1),this.applyViewport(o)}frame(t){return{xmin:t.scales.x.min,xmax:t.scales.x.max,ymin:t.scales.y.min,ymax:t.scales.y.max,left:t.bbox.left,top:t.bbox.top,width:t.bbox.width,height:t.bbox.height}}traceLater(){this.traceTimer===null&&(this.traceTimer=setTimeout(()=>this.traceRest(),0))}traceRest(){this.traceTimer=null;let t=this.model,n=this.u;if(!t||!n)return;let o=performance.now(),i;do i=!1,this.traces.forEach((r,a)=>{let l=t.curves[a],d=r.current;!l||!d||r.done||r.xs!==l.xs.buffer||r.ys!==l.ys.buffer||(r.done=d.run(l.xs,l.ys,l.xs.length,xb),r.done||(i=!0))});while(i&&performance.now()-o<wb);if(i){this.traceTimer=setTimeout(()=>this.traceRest(),0);return}this.traceMs=performance.now()-this.traceStart,this.applying=!0,n.batch(()=>n.setData(this.data(),!1)),this.applying=!1}pointPath(t){return(n,o,i,r)=>{let a=this.model?.curves[o-1],l=new Path2D,d=new Path2D,c=(t+.5)*De.pxRatio,p=n.bbox;return d.rect(p.left-2*c,p.top-2*c,p.width+4*c,p.height+4*c),a&&Cd(a.xs,a.ys,Math.max(0,i),Math.min(r,a.xs.length-1),this.frame(n),c,(m,u)=>{l.moveTo(m+c,u),l.arc(m,u,c,0,2*Math.PI)}),{stroke:null,fill:l,clip:d,flags:3}}}data(){let t=this.model;return t.mode===1?[t.curves[0]?.xs??new Float32Array(0),...t.curves.map(n=>n.ys)]:[null,...t.curves.map(n=>[n.xs,n.ys])]}create(){let t=this.model;this.u?.destroy(),this.traces=[],this.vertices=t.curves.map(()=>null);let n=()=>ti("--fg-muted")||"#666",o=()=>ti("--grid")||"#eee",i=ti("--plot-font"),r=p=>({label:p,stroke:n,font:i,labelFont:i,grid:{stroke:o,width:1},ticks:{stroke:o,width:1}}),a=t.curves.map((p,m)=>{let u=Be(p.color,this.dark),f={label:p.label,stroke:u,width:1.5,show:this.visible[m],points:{show:!1}};return p.line?t.mode===2&&(f.paths=this.linePath):(f.paths=this.pointPath(p.radius),f.fill=u,f.points={show:!1,size:2*p.radius+1,width:0,fill:u}),t.mode===2&&(f.facets=[{scale:"x",auto:!1},{scale:"y",auto:!1}]),f}),{width:l,height:d}=this.size(),c={mode:t.mode,width:l,height:d,legend:{show:!1},scales:{x:{time:!1,auto:!1},y:{auto:!1}},axes:[r(t.xLabel),r(t.yLabel)],series:[{},...a],cursor:{drag:{x:!0,y:!0,uni:20,setScale:!0},points:{show:!1},bind:{dblclick:()=>()=>(this.reset(),null)}},hooks:{setScale:[()=>this.scaleChanged()],drawClear:[()=>{this.drawStart=performance.now()}],drawAxes:[p=>this.drawPhase(p)],draw:[p=>{this.drawMarks(p),this.drawn()}]}};this.u=new De(c,this.data(),this.root),this.onArea(this.u.over)}setPhase(t,n){t===this.nullclines&&n===this.dfield||(this.nullclines=t,this.dfield=n,this.layers=or(t,n),this.u?.redraw(!1,!1))}setMarks(t){if(t===this.marks)return;this.marks=t,this.markLayers=ir(t),this.u?.redraw(!1,!1),t?.text.some(o=>/[\u0370-\u03ff]/.test(o.plain))&&typeof document<"u"&&document.fonts?.load&&document.fonts.load(`${Ya(2)}px Inter`,"\u03B1\u03B2").then(()=>this.u?.redraw(!1,!1),()=>{})}setRuns(t,n){t===this.runs&&n===this.showRuns||(this.runs=t,this.showRuns=n,this.u?.redraw(!1,!1))}setLayerVisible(t,n){n?this.hiddenLayers.delete(t):this.hiddenLayers.add(t),this.u?.redraw(!1,!1)}isLayerVisible(t){return!this.hiddenLayers.has(t)}drawPhase(t){if(this.layerDrawn.clear(),this.drawRuns(t),this.drawFrozen(t),!this.layers.length)return;let n=t.ctx,o=this.frame(t),i=De.pxRatio,r=this.nullclines,a=this.dfield,l=(d,c,p,m,u)=>{if(this.hiddenLayers.has(d))return;let f=new Path2D,w=u(f);this.layerDrawn.set(d,(this.layerDrawn.get(d)??0)+w),n.strokeStyle=Be(c,this.dark),n.lineWidth=p*i,n.setLineDash(m.map(k=>k*i)),n.stroke(f)};if(n.save(),n.beginPath(),n.rect(t.bbox.left,t.bbox.top,t.bbox.width,t.bbox.height),n.clip(),n.lineCap="round",n.lineJoin="round",a){for(let d of a.flows)l("flow",d.color,1,[],c=>Nd(d.xs,d.ys,o,c));a.n>0&&l("dfield",a.color,1,[],d=>{let{segments:c,arrows:p}=Rd(a,o,7*i);for(let m=0;m<c.length;m+=4)d.moveTo(c[m],c[m+1]),d.lineTo(c[m+2],c[m+3]);return p})}if(r){for(let d of r.frozen)l("xnull",r.xColor,1.25,[5,3],c=>Qo(d.x,o,c)),l("ynull",r.yColor,1.25,[5,3],c=>Qo(d.y,o,c));l("xnull",r.xColor,2,[],d=>Qo(r.x,o,d)),l("ynull",r.yColor,2,[],d=>Qo(r.y,o,d))}n.restore()}clipped(t,n){let o=t.ctx;o.save(),o.beginPath(),o.rect(t.bbox.left,t.bbox.top,t.bbox.width,t.bbox.height),o.clip(),o.lineCap="round",o.lineJoin="round",o.setLineDash([]),n(o),o.restore()}strokeLayer(t,n,o,i,r,a=!1){if(this.hiddenLayers.has(n))return;let l=new Path2D,d=r(l);this.layerDrawn.set(n,(this.layerDrawn.get(n)??0)+d),t.strokeStyle=o,t.lineWidth=i*De.pxRatio,t.stroke(l),a&&(t.fillStyle=o,t.fill(l))}drawRuns(t){if(this.runsDrawn=0,!this.runs.length||!this.showRuns)return;let n=this.frame(t),o=De.pxRatio;this.clipped(t,i=>{i.globalAlpha=.35,i.lineWidth=1.25*o;for(let r of this.runs){let a=this.runPaths.get(r);if(!a||a.dark!==this.dark||!ja(a.frame,n)){let l=0,d=r.curves.map(c=>{let p=new Path2D;return l+=Wa(c.xs,c.ys,c.line,n,Math.max(1,c.radius)*o,p),{p,css:Be(c.color,this.dark),fill:!c.line}});a={frame:n,dark:this.dark,drawn:l,paths:d},this.runPaths.set(r,a)}this.runsDrawn+=a.drawn;for(let{p:l,css:d,fill:c}of a.paths)i.strokeStyle=d,i.stroke(l),c&&(i.fillStyle=d,i.fill(l))}})}drawFrozen(t){let n=this.marks;if(!n?.frozen.length)return;let o=this.frame(t),i=De.pxRatio;this.clipped(t,r=>n.frozen.forEach((a,l)=>{let d=Be(a.color,this.dark);this.strokeLayer(r,`frozen-${l}`,d,1.5,c=>Wa(a.xs,a.ys,a.line,o,1.5*i,c),!a.line)}))}drawMarks(t){let n=this.marks;if(!n)return;let o=this.frame(t),i=De.pxRatio,r=ti("--fg")||"#1c2330",a=ti("--surface")||"#fff";this.clipped(t,l=>{for(let d of n.arrows)this.strokeLayer(l,"arrows",Be(d.color,this.dark),1.5,c=>zd(d,o,c)?1:0);for(let d of n.markers)this.strokeLayer(l,"markers",Be(d.color,this.dark),1.5,c=>Hd(d,o,qd*i,c)?1:0);for(let d of n.equilibria)this.strokeLayer(l,"equilibria",r,1.5,c=>Fd(d,o,5*i,c)?1:0,d.type==="stable");if(n.text.length&&!this.hiddenLayers.has("text")){l.textAlign="left",l.textBaseline="alphabetic",l.lineWidth=3*i,l.strokeStyle=a,l.fillStyle=r;let d=0;for(let c of n.text){let p=Ya(c.size)*i,m=kb(o,c.x,c.y);if(!m)continue;let u=m.x;for(let f of c.runs){l.font=`${f.small?Math.round(p*.75):p}px Inter, system-ui, sans-serif`;let w=m.y-f.rise*.45*p;l.strokeText(f.text,u,w),l.fillText(f.text,u,w),u+=l.measureText(f.text).width}d++}this.layerDrawn.set("text",d)}})}drawn(){this.drawMs.push(performance.now()-this.drawStart),this.drawMs.length>yb&&this.drawMs.shift(),this.draws++}size(){let t=this.root.getBoundingClientRect();return{width:Math.max(120,Math.floor(t.width)),height:Math.max(120,Math.floor(t.height))}}resize(){this.u&&this.u.setSize(this.size())}applyViewport(t){let n=this.u;n&&(this.applying=!0,n.batch(()=>{n.setScale("x",t.x??this.base.x),n.setScale("y",t.y??this.base.y)}),this.applying=!1)}setView(t,n){this.cb.onViewport({x:t.x,y:t.y},n)}reset(){this.cb.onViewport({x:null,y:null},!0)}scaleChanged(){!this.u||this.applying||this.reportPending||(this.reportPending=!0,queueMicrotask(()=>{this.reportPending=!1;let t=this.u;if(!t)return;let n=t.scales.x,o=t.scales.y;n.min==null||n.max==null||o.min==null||o.max==null||this.cb.onViewport({x:{min:n.min,max:n.max},y:{min:o.min,max:o.max}},!0)}))}ranges(){let t=this.u;return{x:{min:t.scales.x.min,max:t.scales.x.max},y:{min:t.scales.y.min,max:t.scales.y.max}}}hit(t,n,o){let i=this.u,r=this.model;if(!i||!r)return null;let{x:a,y:l}=this.ranges();return Dd(r.curves,this.visible,{xmin:a.min,xmax:a.max,ymin:l.min,ymax:l.max,width:i.over.clientWidth,height:i.over.clientHeight},t,n,o)}position(t,n){let o=this.u,i=this.model?.curves[t];if(!o||!i||n<0||n>=i.xs.length)return null;let r=o.over.getBoundingClientRect(),a=this.root.getBoundingClientRect();return{left:r.left-a.left+o.valToPos(i.xs[n],"x"),top:r.top-a.top+o.valToPos(i.ys[n],"y")}}areaBox(){let t=this.u;if(!t)return null;let n=t.over.getBoundingClientRect(),o=this.root.getBoundingClientRect();return{left:n.left-o.left,top:n.top-o.top,width:n.width,height:n.height}}setVisible(t,n){this.visible[t]=n,this.u?.setSeries(t+1,{show:n})}isVisible(t){return this.visible[t]??!1}png(){return this.u?this.u.ctx.canvas.toDataURL("image/png"):null}pixels(){return this.u?er(this.u.ctx.canvas):null}info(){let t=this.u,n=this.model;return!t||!n?null:{mode:n.mode,curves:n.curves.map((o,i)=>({label:o.label,points:o.xs.length,visible:this.visible[i],color:Be(o.color,this.dark)})),...this.ranges(),width:t.over.clientWidth,height:t.over.clientHeight,drawMs:this.drawMs.slice(),draws:this.draws,tracing:this.traceTimer!==null,traceMs:this.traceMs,vertices:this.vertices.slice(),runs:{count:this.runs.length,shown:this.showRuns,drawn:this.runsDrawn},layers:[...this.layers,...this.markLayers].map(o=>({...o,visible:this.isLayerVisible(o.key),drawn:this.layerDrawn.get(o.key)??0,css:Be(o.color,this.dark)}))}}destroy(){this.traceTimer!==null&&clearTimeout(this.traceTimer),this.traceTimer=null,this.u?.destroy(),this.u=null}};var Tb=480,Sb=360,rr=null,ar=null;function Pb(){return ar||(rr=document.createElement("div"),rr.style.cssText=`position:fixed; left:-10000px; top:0; width:${Tb}px; height:${Sb}px; pointer-events:none;`,document.body.appendChild(rr),ar=new vo(rr,{onViewport:()=>{}}),ar)}function Eb(e){let t=e.info;return!t||t.three||!(t.xlo<t.xhi&&t.ylo<t.yhi)?null:{x:{min:t.xlo,max:t.xhi},y:{min:t.ylo,max:t.yhi}}}function sr(e,t=!1){let n=e.series?Vo(e.series):null;if(!n)return null;let o=Pb();return o.set(n,Eb(e),e.viewport,t),o.setPhase(e.nullclines,e.dfield),o.setMarks(e.marks),o.pixels()}var Ga={fx:.5,fy:.5},Ab=.02,_b=.1;function Kd(e,t,n,o=!1){let i=Number(e.win);if(!(n&&i>=1&&i<=10&&!!t&&t.win===i&&!t.three)&&!(o&&i===101))return null;switch(e.kind){case"mouse":return"point";case"rubber":return e.flag===1?"line":"box";case"drag":return"drag";default:return null}}function Bd(e,t,n){let o=e&&e.mode===n&&e.win===Number(t.win);return{mode:n,win:Number(t.win),ask:t.id,cursor:o?e.cursor:Ga,anchor:null,waiting:!1}}var Ud=e=>Math.max(0,Math.min(1,e));function Mb(e,t){let n=t?_b:Ab;switch(e){case"ArrowLeft":return{dx:-n,dy:0};case"ArrowRight":return{dx:n,dy:0};case"ArrowUp":return{dx:0,dy:-n};case"ArrowDown":return{dx:0,dy:n};default:return null}}function Vd(e,t,n){return{fx:Ud(e.fx+t),fy:Ud(e.fy+n)}}function lr(e,t,n){if(t==="Escape")return{cancel:!0};let o=Mb(t,n);if(e.mode==="drag"){if(t==="Enter")return{cancel:!0};if(!o)return null;let i=Vd(Ga,-o.dx,-o.dy);return{drag:[{what:"down",at:Ga},{what:"move",at:i},{what:"up",at:i}]}}return o?{pick:{...e,cursor:Vd(e.cursor,o.dx,o.dy)}}:t!=="Enter"?null:e.mode==="point"||e.anchor?{confirm:e}:{pick:{...e,anchor:e.cursor}}}function Tn(e,t){return{x:e.x.min+t.fx*(e.x.max-e.x.min),y:e.y.max-t.fy*(e.y.max-e.y.min)}}function jd(e,t){let n=Tn(t,e.cursor);if(e.mode==="point"||e.mode==="drag")return{xd:n.x,yd:n.y};let o=Tn(t,e.anchor??e.cursor);return{xd:o.x,yd:o.y,xd2:n.x,yd2:n.y}}function Wd(e,t){let n=t?"Tap":"Click";switch(e.mode){case"point":return`${n} a point in the plot, or move the crosshair with the arrow keys and press Enter.`;case"box":case"line":return`Drag ${e.mode==="box"?"a box":"a line"} in the plot, or move the crosshair with the arrow keys and press Enter at each corner.`;case"drag":return"Drag the plot to scroll it, or use the arrow keys. Done (or Escape) ends."}}var Za=new Map;function wo(e,t){t?Za.set(e,t):Za.delete(e)}function cr(e){return Za.get(e)??null}var Yd={listing:null,confirm:null,uploads:[],offered:null,run:null,runFailed:!1,nextMenu:null};function Gd(e,t){switch(t.type){case"listing":return{...e,listing:t.files};case"confirm":return{...e,confirm:t.confirm};case"uploaded":return{...e,uploads:t.uploads};case"offered":return{...e,offered:t.offered}}}var Cb=new Set(["key","session","browser","auto","ani","aplot","plotvars","userbut","action","eqimport"]);function Zd(e,t,n,o){if(t.cmd==="answer"){if(!e.run||!n)return e;let{cmd:l,id:d,...c}=t,p={kind:n.kind,fields:c};return typeof n.mode=="string"&&(p.mode=n.mode),{...e,run:{...e.run,answers:[...e.run.answers,p]}}}if(!Cb.has(t.cmd)||t.cmd==="browser"&&"from"in t)return e;let i=e.nextMenu??o,r=t.cmd==="key"?String(t.key):"";return{...e,run:{menu:i,cmd:t,answers:[]},runFailed:!1,nextMenu:i===0&&r==="f"?1:i===0&&r==="u"?2:null}}function dr(e){return e.slice(Math.max(e.lastIndexOf("/"),e.lastIndexOf("\\"))+1)}function Kn(e){if(!e||e.length>255||e.startsWith(".")||e.startsWith(" ")||e.includes("..")||new TextEncoder().encode(e).length>255||/[\u0000-\u001f\u007f/\\:<>"|?*]/.test(e)||/[. ]$/.test(e))return!1;let t=e.split(".")[0].replace(/ +$/,"").toLowerCase();return!/^(con|prn|aux|nul|conin\$|conout\$|com[0-9]|lpt[0-9])$/.test(t)}function Jd(e,t){let n=new Set(t),o=e.lastIndexOf("."),i=o>0?e.slice(0,o):e,r=o>0?e.slice(o):"";for(let a=2;;a++){let l=`${i}-${a}${r}`;if(!n.has(l))return l}}function Db(e,t){return!t||t==="*"?!0:new RegExp("^"+t.split("*").map(o=>o.replace(/[.+?^${}()|[\]\\]/g,"\\$&")).join(".*")+"$","i").test(e)}function Qd(e,t){return e.find(n=>Db(n,t))??e[0]}function eu(e,t,n){let o=n.find(i=>i.name===e);return o?o.sha256===t?"same":"confirm":"copy"}var Lb=/(can'?t|cannot|could ?n'?o?t|unable to) (open|read|find)|not found/i;function tu(e,t){let n=/<([^<>]+)>/.exec(e)??/\bopen\s+(\S+\.\w+)/i.exec(e);if(n){let r=dr(n[1].trim());if(Kn(r))return r}if(!Lb.test(e)||!t)return null;let o=[...t.answers].reverse().find(r=>r.kind==="file"&&r.mode==="read"&&typeof r.fields.file=="string"),i=o?dr(o.fields.file):"";return i&&Kn(i)?i:null}function nu(e,t){return e===t?[]:e!==0?null:t===1?["f"]:t===2?["u"]:null}function ou(e,t){let n=t,o=new Set;return{getState:()=>n,dispatch(i){let r=e(n,i);if(r!==n){n=r;for(let a of[...o])a()}},subscribe(i){return o.add(i),()=>o.delete(i)}}}var Ob={a:"\u03B1",b:"\u03B2",c:"\u03C7",d:"\u03B4",e:"\u03B5",f:"\u03C6",g:"\u03B3",h:"\u03B7",i:"\u03B9",j:"\u03D5",k:"\u03BA",l:"\u03BB",m:"\u03BC",n:"\u03BD",o:"\u03BF",p:"\u03C0",q:"\u03B8",r:"\u03C1",s:"\u03C3",t:"\u03C4",u:"\u03C5",v:"\u03D6",w:"\u03C9",x:"\u03BE",y:"\u03C8",z:"\u03B6",A:"\u0391",B:"\u0392",C:"\u03A7",D:"\u0394",E:"\u0395",F:"\u03A6",G:"\u0393",H:"\u0397",I:"\u0399",J:"\u03D1",K:"\u039A",L:"\u039B",M:"\u039C",N:"\u039D",O:"\u039F",P:"\u03A0",Q:"\u0398",R:"\u03A1",S:"\u03A3",T:"\u03A4",U:"\u03A5",V:"\u03C2",W:"\u03A9",X:"\u039E",Y:"\u03A8",Z:"\u0396"};function Ib(e){let t="";for(let n of e)t+=Ob[n]??n;return t}function iu(e,t=!1){let n=[],o=t,i=0,r=!1,a="",l=()=>{a&&n.push({text:o?Ib(a):a,rise:i,small:r}),a=""};for(let d=0;d<e.length;d++){let c=e[d];if(c!=="\\"){a+=c;continue}l();let p=e[++d];p==="0"?o=!1:p==="1"?o=!0:p==="n"?(i=0,r=!1):p==="s"?(i-=.5,r=!0):p==="S"&&(i+=1,r=!0)}return l(),n}var $b={stable:"stable",unstable:"unstable",saddle:"saddle"},Nb=["box","diamond","triangle","plus","cross","circle"];function ru(e){return{equilibria:e.equilibria.map(t=>({x:t.x,y:t.y,type:$b[t.type]??"unstable"})),text:e.text.map(t=>{let n=iu(t.text,t.font===1);return{x:t.x,y:t.y,raw:t.text,runs:n,plain:n.map(o=>o.text).join(""),size:t.size}}),arrows:e.arrows.map(t=>({pointer:t.kind==="pointer",x1:t.x1,y1:t.y1,x2:t.x2,y2:t.y2,size:t.size,color:t.color})),markers:e.markers.map(t=>({x:t.x,y:t.y,shape:Nb.includes(t.shape)?t.shape:"box",size:t.size,color:t.color})),frozen:e.frozen.map((t,n)=>({label:t.key||t.name||`Frozen ${n+1}`,color:t.color,line:t.line!==0,xs:ft(t.x),ys:ft(t.y)}))}}var Ja={runs:[],erased:!1,live:!1};function Rb(e,t){return e.three===t.three&&JSON.stringify(e.curves)===JSON.stringify(t.curves)&&e.shift.every((n,o)=>n===t.shift[o])}function Fb(e){let t=new Map;for(let[n,o]of e.columns)t.set(n,o.buffer.byteLength===o.byteLength?o:o.slice());return{...e,columns:t,buffers:t}}function au(e,t){if(t.rows<=0)return e;let n=[...e,Fb(t)],o=n.reduce((r,a)=>r+a.rows,0),i=Math.max(0,n.length-50);for(let r=0;r<i;r++)o-=n[r].rows;for(;i<n.length-1&&o>4e6;)o-=n[i++].rows;return i?n.slice(i):n}function su(e,t,n){return!t||!Rb(t,n)?e.runs.length||e.erased||e.live?Ja:e:e.erased||e.live?{runs:e.runs,erased:!1,live:!1}:(n.version===null||t.version===null||n.version!==t.version)&&t.rows>0?{...e,runs:au(e.runs,t)}:e}function lu(e,t,n){return n<t.rows?{runs:e.erased?e.runs:au(e.runs,t),erased:!1,live:!0}:e.live&&!e.erased?e:{...e,erased:!1,live:!0}}function cu(){return{runs:[],erased:!0,live:!1}}function du(e){return e.runs.length||e.erased?{...e,runs:[],erased:!1}:e}var xo={x:null,y:null},uu={windows:[],active:1},qb=50;function pu(e){return{win:e,info:null,series:null,nullclines:null,dfield:null,marks:null,viewport:xo,viewportHistory:[],view3d:null,history:Ja,showRuns:!0}}function et(e,t){return e.windows.find(n=>n.win===t)}function Gt(e,t,n){let o=et(e,t),i=n(o??pu(t));if(i===o)return e;let r=o?e.windows.map(a=>a===o?i:a):[...e.windows,i].sort((a,l)=>a.win-l.win);return{...e,windows:r}}function Hb(e,t){return!!e&&JSON.stringify(e.curves)===JSON.stringify(t.curves)}function hu(e,t){return{windows:t.windows.map(o=>{let i=et(e,o.win)??pu(o.win);return{...i,info:o,view3d:i.view3d??(o.three?{theta:o.theta,phi:o.phi}:null)}}),active:t.active}}function mu(e,t,n,o){return Gt(e,t,i=>({...i,view3d:{theta:n,phi:o}}))}function fu(e,t){let n=nc(t);return Gt(e,t.win,o=>{let i=Hb(o.series,n),r=su(o.history,o.series,n);return{...o,series:n,history:r,viewport:i?o.viewport:xo,viewportHistory:i?o.viewportHistory:[]}})}function gu(e,t){return et(e,t)?Gt(e,t,n=>({...n,history:cu()})):e}function bu(e,t){let n=et(e,t);if(!n)return e;let o=du(n.history);return o===n.history?e:Gt(e,t,i=>({...i,history:o}))}function yu(e,t,n){let o=et(e,t);return!o||o.showRuns===n?e:Gt(e,t,i=>({...i,showRuns:n}))}function vu(e,t){let n=Ld(t);return Gt(e,t.win,o=>({...o,nullclines:n}))}function wu(e,t){let n=Od(t);return Gt(e,t.win,o=>({...o,dfield:n}))}function xu(e,t){let n=ru(t);return Gt(e,t.win,o=>({...o,marks:n}))}function ku(e,t){let n=et(e,t.win),o=n?.series&&oc(n.series,t);if(!n||!o)return null;let i=lu(n.history,n.series,t.from);return Gt(e,t.win,r=>({...r,series:o,history:i}))}function ur(e,t){return e.active===t?e:{...e,active:t}}function zb(e,t){return JSON.stringify(e)===JSON.stringify(t)}function Qa(e,t,n,o=!1){let i=et(e,t);if(!i||zb(n,i.viewport))return e;let r=o?[...i.viewportHistory,i.viewport].slice(-qb):i.viewportHistory;return Gt(e,t,a=>({...a,viewport:n,viewportHistory:r}))}function Tu(e,t){let n=et(e,t);return!n||!n.viewportHistory.length?e:Gt(e,t,o=>({...o,viewport:o.viewportHistory[o.viewportHistory.length-1],viewportHistory:o.viewportHistory.slice(0,-1)}))}function Su(e,t){let n=et(e,t);return!n||n.viewport.x===null&&n.viewport.y===null?e:Qa(e,t,xo,!0)}var Pu={event:null,values:new Float32Array(0),windowOpen:!1,open:!1,colorMap:"viridis",hover:null};function ko(e,t){switch(t.type){case"event":return{...e,event:t.ev,values:ft(t.ev.values),hover:null};case"window":return t.open===e.windowOpen?e:t.open?{...e,windowOpen:!0}:{...e,windowOpen:!1,open:!1,event:null,values:new Float32Array(0),hover:null};case"panel":return t.open===e.open?e:{...e,open:t.open};case"colorMap":return t.map===e.colorMap?e:{...e,colorMap:t.map};case"hover":return t.hover===e.hover?e:{...e,hover:t.hover}}}function es(e,t,n){let o=e.event;return!o||t<0||t>=o.ny||n<0||n>=o.nx?NaN:e.values[t*o.nx+n]}function Eu(e,t){return e.ny<=1?e.tlo:e.tlo+(e.thi-e.tlo)*(t/(e.ny-1))}function Au(e,t){let n=/^(.*?)(-?\d+)\.\.(-?\d+)$/.exec(e.title);return n?`${n[1]}${Number(n[2])+t}`:""}var ni=(...e)=>e.every(t=>typeof t=="number"&&Number.isFinite(t)),oi=e=>typeof e=="number"||typeof e=="string"&&/^#[0-9a-f]{6}$/i.test(e);function _u(e){let t=[];for(let n of e){if(!Array.isArray(n))continue;let o=n;switch(o[0]){case"line":ni(o[1],o[2],o[3],o[4])&&oi(o[5])&&t.push({kind:"line",u1:o[1],v1:o[2],u2:o[3],v2:o[4],color:o[5],width:Number(o[6])||0});break;case"rect":ni(o[1],o[2],o[3],o[4])&&oi(o[5])&&t.push({kind:"rect",u1:o[1],v1:o[2],u2:o[3],v2:o[4],color:o[5],width:Number(o[6])||0,fill:!!o[7]});break;case"circle":case"ellipse":ni(o[1],o[2],o[3],o[4])&&oi(o[5])&&t.push({kind:o[0],u:o[1],v:o[2],ru:Math.abs(o[3]),rv:Math.abs(o[4]),color:o[5],width:Number(o[6])||0,fill:!!o[7]});break;case"dot":ni(o[1],o[2],o[3])&&oi(o[4])&&t.push({kind:"dot",u:o[1],v:o[2],r:o[3],color:o[4]});break;case"text":ni(o[1],o[2])&&typeof o[3]=="string"&&oi(o[4])&&t.push({kind:"text",u:o[1],v:o[2],text:o[3],color:o[4],size:Number(o[5])||0,font:Number(o[6])||0});break}}return t}function Mu(e){let t=e[2]-e[0],n=e[3]-e[1];return t>0&&n>0&&Number.isFinite(t/n)?t/n:1}function Cu(e,t,n,o=0){let i=Math.max(0,e-2*o),r=Math.max(0,t-2*o),a=i,l=n>0?i/n:r;return l>r&&(l=r,a=r*n),{x:(e-a)/2,y:(t-l)/2,w:a,h:l}}function Sn(e,t,n){return[e.x+t*e.w,e.y+(1-n)*e.h]}function Du(e,t,n){return[e.w?(t-e.x)/e.w:0,e.h?1-(n-e.y)/e.h:0]}function Lu(e,t,n){return!(t>0&&n>0)||!(e.w>0&&e.h>0)?1:Math.min(4,Math.max(.5,Math.sqrt(e.w*e.h/(t*n))))}function pr(e,t){return Math.max(1,e)*t}var Xb=[8,10,12,14,18];function Ou(e,t){return Xb[Math.max(0,Math.min(4,Math.round(e)))]*t}function Iu(e,t,n){return(t*e.w+n*e.h)/2}var Ub={a:"\u03B1",b:"\u03B2",c:"\u03C7",d:"\u03B4",e:"\u03B5",f:"\u03C6",g:"\u03B3",h:"\u03B7",i:"\u03B9",j:"\u03D5",k:"\u03BA",l:"\u03BB",m:"\u03BC",n:"\u03BD",o:"\u03BF",p:"\u03C0",q:"\u03B8",r:"\u03C1",s:"\u03C3",t:"\u03C4",u:"\u03C5",v:"\u03D6",w:"\u03C9",x:"\u03BE",y:"\u03C8",z:"\u03B6",A:"\u0391",B:"\u0392",C:"\u03A7",D:"\u0394",E:"\u0395",F:"\u03A6",G:"\u0393",H:"\u0397",I:"\u0399",K:"\u039A",L:"\u039B",M:"\u039C",N:"\u039D",O:"\u039F",P:"\u03A0",Q:"\u0398",R:"\u03A1",S:"\u03A3",T:"\u03A4",U:"\u03A5",W:"\u03A9",X:"\u039E",Y:"\u03A8",Z:"\u0396"};function $u(e){let t=e.text.replace(/\s+$/,"");return e.font===1?Array.from(t,n=>Ub[n]??n).join(""):t}var Nu={open:!1,exists:!1,loaded:!1,playing:!1,pos:0,rows:0,speed:10,skip:1,grab:!1,fly:!1,frame:null,frames:0};function To(e,t){switch(t.type){case"open":return t.open===e.open?e:{...e,open:t.open};case"window":return t.exists?{...e,exists:!0,open:!0}:{...e,exists:!1,playing:!1};case"state":{let n=t.ev;return{...e,pos:n.pos,rows:n.rows,speed:n.speed,skip:n.skip,grab:!!n.grab,fly:!!n.fly,loaded:n.loaded===void 0?e.loaded:!!n.loaded,exists:n.open===void 0?e.exists:!!n.open}}case"frame":{let n=t.ev,o={pos:n.pos,rows:n.rows,t:n.t,dim:n.dim,w:n.w,h:n.h,prims:_u(n.prims??[])};return{...e,frame:o,rows:n.rows,speed:n.speed,skip:n.skip,loaded:!0,frames:e.frames+1}}case"playing":return t.playing===e.playing?e:{...e,playing:t.playing}}}function Ru(e,t){let n=e.frame?.pos??e.pos;return Math.max(0,Math.min(Math.max(0,e.rows-1),n+t))}function hr(e){return{win:e.win,info:e.info,series:e.series,nullclines:e.nullclines,dfield:e.dfield,marks:e.marks,viewport:e.viewport}}var ts={frames:[],playing:!1,shown:null,cycles:1,delay:50};function ii(e,t){switch(t.type){case"capture":return{...e,frames:[...e.frames,t.frame]};case"clear":return ts;case"show":return t.index===e.shown?e:{...e,shown:t.index};case"playing":return{...e,playing:t.playing,cycles:t.cycles??e.cycles,delay:t.delay??e.delay}}}var zu=["x","y","y2","br","pt","ty","d","c","lw","f2","nw","fr"];function Vb(){return{x:[],y:[],y2:[],br:[],pt:[],ty:[],d:[],c:[],lw:[],f2:[],nw:[],fr:[]}}var Xu={x:null,y:null},Kb=50,mr={open:!1,shown:!1,axes:null,points:Vb(),labels:[],events:0,outOfStep:!1,viewport:Xu,viewportHistory:[],hover:null,info:null,stab:null,infoEvents:0,grabbing:!1,stored:null,run:null,earlier:0,showEarlier:!1,setupSaved:null};function rn(e){return e.x.length}var ns=e=>e??NaN;function Bb(e,t){if(t>=rn(e))return e;let n={};for(let o of zu)n[o]=e[o].slice(0,t);return n}function jb(e,t,n,o){let i={};for(let a of zu)i[a]=e[a].slice(0,n);let r=t.filter(a=>a.point<n);for(let a of o){let l=i.x.length;for(let d=0;d<a.x.length;d++){let c=ns(a.y[d]);i.x.push(ns(a.x[d])),i.y.push(c),i.y2.push(a.y2?ns(a.y2[d]):c),i.br.push(a.br),i.pt.push(a.pt+d),i.ty.push(a.ty),i.d.push(a.d),i.c.push(a.c),i.lw.push(a.lw),i.f2.push(a.f2??0),i.nw.push(d===0&&a.new?1:0),i.fr.push(d===0&&a.from?a.from:0)}for(let[d,c,p]of a.lab??[])r.push({point:l+d,lab:c,sym:p})}return{points:i,labels:r}}function Wb(e){let{xmin:t,xmax:n,ymin:o,ymax:i,x0:r,y0:a,wid:l,hgt:d,plot:c,xlabel:p,ylabel:m}=e;return{xmin:t,xmax:n,ymin:o,ymax:i,x0:r,y0:a,wid:l,hgt:d,plot:c,xlabel:p,ylabel:m}}function Yb(e,t){return!!e&&e.xmin===t.xmin&&e.xmax===t.xmax&&e.ymin===t.ymin&&e.ymax===t.ymax}function Fu(e,t){let n=Wb(t);return e.axes&&!Yb(e.axes,n)&&(e.viewport.x!==null||e.viewport.y!==null)?Uu({...e,axes:n},Xu,!0):{...e,axes:n}}function Uu(e,t,n=!1){if(JSON.stringify(t)===JSON.stringify(e.viewport))return e;let o=n?[...e.viewportHistory,e.viewport].slice(-Kb):e.viewportHistory;return{...e,viewport:t,viewportHistory:o}}function Gb(e,t){let n=e.events+1;switch(t.op){case"axes":return{...Fu(e,t),events:n};case"reset":{let o=Math.max(0,t.keep||0),i=Bb(e.points,o),r=e.hover&&e.hover.point<o?e.hover:null;return{...Fu(e,t),points:i,labels:e.labels.filter(a=>a.point<o),hover:r,outOfStep:!1,events:n}}case"add":{if(t.from>rn(e.points))return{...e,outOfStep:!0,events:n};let{points:o,labels:i}=jb(e.points,e.labels,t.from,t.runs),r=e.hover&&e.hover.point<t.from?e.hover:null;return{...e,points:o,labels:i,hover:r,events:n,outOfStep:t.from===0?!1:e.outOfStep}}default:return e}}function qu(e){return e.open?e:{...mr,open:!0,shown:!0,events:e.events,infoEvents:e.infoEvents}}function Hu(e){return e.open||e.axes||rn(e.points)?{...mr,events:e.events,infoEvents:e.infoEvents}:e}function an(e,t){switch(t.type){case"event":return Gb(e,t.ev);case"window":return t.op==="create"?qu(e):Hu(e);case"core":return t.open===e.open?e:t.open?qu(e):Hu(e);case"show":return t.shown===e.shown||!e.open?e:{...e,shown:t.shown};case"viewport":return Uu(e,t.viewport,t.push);case"undoViewport":return e.viewportHistory.length?{...e,viewport:e.viewportHistory[e.viewportHistory.length-1],viewportHistory:e.viewportHistory.slice(0,-1)}:e;case"hover":{let n=t.hover,o=e.hover;return n===o||n&&o&&n.point===o.point&&n.low===o.low?e:{...e,hover:n}}case"info":return{...e,info:t.ev.info??null,stab:t.ev.stab??null,infoEvents:e.infoEvents+1};case"grabbing":return t.on===e.grabbing?e:{...e,grabbing:t.on,shown:t.on&&e.open?!0:e.shown};case"stored":return{...e,stored:t.at};case"run":return Zb(e,t.op,t.at);case"runStopped":return e.run?.active?{...e,run:{...e.run,stopped:!0}}:e;case"clear":return{...e,earlier:rn(e.points),showEarlier:!1,hover:null};case"showEarlier":return t.show===e.showEarlier?e:{...e,showEarlier:t.show};case"setupSaved":return{...e,setupSaved:t.text}}}function Zb(e,t,n){return t==="start"?{...e,run:{active:!0,started:n,ended:null,first:rn(e.points),stopped:!1}}:e.run?.active?t==="clock"?{...e,run:{...e.run,started:n}}:{...e,run:{...e.run,active:!1,ended:n}}:e}function Vu(e){let t=an(e,{type:"grabbing",on:!1}),n=rn(t.points);return t.earlier>n?{...t,earlier:n}:t}function Ku(e){return Math.min(e.earlier,rn(e.points))}function Bu(e,t){let n=new Set;for(let o=0;o<t&&o<e.br.length;o++)n.add(e.br[o]);return n.size}var ju={open:!1,selected:0,page:null,pendingKey:null,exporting:!1,lastExport:null};function os(e,t){switch(t.type){case"open":return t.open===e.open?e:{...e,open:t.open};case"select":{let n=e.page?.rows??0,o=Math.max(0,Math.min(n>0?n-1:0,t.row));return o===e.selected?e:{...e,selected:o}}case"event":{let n=t.ev,o=!e.page||e.page.row0!==n.row0;return{...e,page:n,pendingKey:null,selected:o?n.row0:e.selected}}case"requested":return{...e,pendingKey:t.req?JSON.stringify(t.req):null};case"exporting":return{...e,exporting:!0};case"exported":return{...e,exporting:!1,lastExport:t.csv}}}var Jb=3,fr=2e3,is=500;function Wu(e,t,n){let o=Math.max(0,t),i=o+Math.max(0,n);return!!e&&e.col===1&&e.from<=o&&e.from+e.data.length>=i?null:{from:Math.max(0,o-Math.max(1,n)),count:Math.min(fr,Math.max(1,n)*Jb),col:1,ncol:is}}function Yu(e,t){return!e||t<e.from||t>=e.from+e.data.length?null:e.data[t-e.from]}function Gu(e){if(!e.length)return"";let t=[e[0].cols.join(",")];for(let n of e)for(let o of n.data)t.push(o.map(i=>i===null?"NaN":String(i)).join(","));return t.join(`
`)+`
`}var Zu={open:!1,tab:"equations",equations:null,source:null,equilibrium:null};function Qb(e){return e.map(([t,n])=>({text:t,hasAction:!!n}))}function ey(e,t){let n=0;return e.map(o=>{if(o.startsWith('"')&&n<t.length){let i=t[n];return{text:o,comment:{text:i.text,hasAction:i.hasAction,index:n++}}}return{text:o}})}function ri(e,t){switch(t.type){case"open":return t.open===e.open?e:{...e,open:t.open};case"tab":return t.tab===e.tab?e:{...e,tab:t.tab};case"equations":return{...e,equations:t.ev.lines};case"source":{let n=Qb(t.ev.comments);return{...e,source:{lines:ey(t.ev.lines,n),comments:n}}}case"equilibrium":{let{ev:n,...o}=t.ev;return{...e,equilibrium:o}}}}var ty="01-introduction",Ju={open:!1,chapter:ty,anchor:null,query:""};function Qu(e,t){switch(t.type){case"open":return{...e,open:!0,chapter:t.target?.chapter??e.chapter,anchor:t.target?t.target.anchor||null:e.anchor};case"close":return e.open?{...e,open:!1}:e;case"go":return{...e,chapter:t.target.chapter,anchor:t.target.anchor||null};case"query":return t.query===e.query?e:{...e,query:t.query}}}function ep(e){return!Number.isFinite(e)||e===0?{lo:-1,hi:1}:e>0?{lo:0,hi:2*e}:{lo:2*e,hi:0}}function ny(e){if(!Number.isFinite(e)||e<=0)return 1;let t=Math.floor(Math.log10(e)),n=e/10**t;return(n<=1?1:n<=2?2:n<=5?5:10)*10**t}function rs(e,t){let n=Math.abs(t-e);return n>0&&Number.isFinite(n)?ny(n/100):1}function tp(e){let t=Number(e.step);return e.step.trim()!==""&&Number.isFinite(t)&&t>0?t:null}function np(e){let t=Number(e.lo),n=Number(e.hi);return e.lo.trim()!==""&&e.hi.trim()!==""&&Number.isFinite(t)&&Number.isFinite(n)&&t!==n?{lo:t,hi:n}:null}var So=1e3;function op(e,t){return Math.max(0,Math.min(So,Math.round(So*(e-t.lo)/(t.hi-t.lo))))}function as(e,t){return t.lo+(t.hi-t.lo)*e/So}function ip(e,t){return e.map((n,o)=>({id:t+o,name:n.name,lo:String(n.lo),hi:String(n.hi),step:String(rs(n.lo,n.hi))}))}function rp(e,t,n){let o={},i=Number(e),r=Number(t),a=Number(n),l=e.trim()!==""&&Number.isFinite(i),d=t.trim()!==""&&Number.isFinite(r);return l||(o.lo="A number"),d||(o.hi="A number"),l&&d&&!(i<r)&&(o.lo=o.lo??"Min must be less than Max",o.hi=o.hi??"Min must be less than Max"),n.trim()===""||!Number.isFinite(a)?o.step="A number":a<=0?o.step="Step must be greater than 0":l&&d&&i<r&&a>r-i&&(o.step="Step must not be more than Max \u2212 Min"),o}function ap(e,t){let n=t.trim().toLowerCase();return n===""?e:e.filter(o=>o.name.toLowerCase().includes(n))}var cp={errors:{},pending:null,history:[],queue:[],queueRerun:!1,defaults:null,sliders:[],nextSlider:1,lastSaved:null,runOnChange:!0},oy=50;function zt(e,t){return`${e}:${typeof t=="number"?t:t.toLowerCase()}`}function sp(e,t){if(!(t in e))return e;let n={...e};return delete n[t],n}function Po(e,t){switch(t.type){case"edit":{let n=zt(t.edit.kind,t.edit.index??t.edit.name),o=[...e.history,t.edit].slice(-oy);return{...e,errors:sp(e.errors,n),pending:n,history:o}}case"undo":{if(!e.history.length)return e;let n=e.history[e.history.length-1],o=zt(n.kind,n.index??n.name);return{...e,errors:sp(e.errors,o),pending:o,history:e.history.slice(0,-1)}}case"error":return e.pending?{...e,errors:{...e.errors,[e.pending]:t.text}}:e;case"settled":return e.pending?{...e,pending:null}:e;case"defaulted":{let n=`${t.kind}:`,o=Object.keys(e.errors).filter(r=>r.startsWith(n));if(!o.length)return e;let i={...e.errors};for(let r of o)delete i[r];return{...e,errors:i}}case"queue":return{...e,queue:iy(e.queue,t.set),queueRerun:e.queueRerun||t.rerun};case"flushed":return e.queue.length||e.queueRerun?{...e,queue:[],queueRerun:!1}:e;case"defaults":{if(t.ifUnset&&e.defaults)return e;let n={};for(let[o,i]of t.pars)n[zt("par",o)]=i;for(let[o,i]of t.ics)n[zt("ic",o)]=i;return{...e,defaults:n}}case"presetSliders":return e.sliders.length||!t.defs.length?e:{...e,sliders:ip(t.defs,e.nextSlider),nextSlider:e.nextSlider+t.defs.length};case"addSlider":return{...e,sliders:[...e.sliders,{id:e.nextSlider,name:"",lo:"",hi:"",step:""}],nextSlider:e.nextSlider+1};case"addSliderWith":return{...e,sliders:[...e.sliders,{id:e.nextSlider,...t.def}],nextSlider:e.nextSlider+1};case"setSlider":return{...e,sliders:e.sliders.map(n=>n.id===t.id?{...n,...t.patch}:n)};case"removeSlider":return{...e,sliders:e.sliders.filter(n=>n.id!==t.id)};case"saved":return{...e,lastSaved:{kind:t.kind,text:t.text}};case"runOnChange":return e.runOnChange===t.on?e:{...e,runOnChange:t.on}}}function ss(e){return zt(e.kind,e.index??e.name)}function iy(e,t){let n=ss(t);return[...e.filter(o=>ss(o)!==n),t]}function dp(e,t){return e.some(n=>ss(n)===t)}function lp(e){return e.index!==void 0?{kind:e.kind,index:e.index,text:e.text}:{kind:e.kind,name:e.name,text:e.text}}function ls(e,t){if(!e.length)return null;let n=t?{rerun:1}:{};return e.length===1?{cmd:"set",...lp(e[0]),...n}:{cmd:"set",values:e.map(lp),...n}}function je(e){return Number.isFinite(e)?String(Number(e.toPrecision(6))):String(e)}var Pn=e=>({kind:"range",lo:e,hi:null}),sn=[{key:"ntst",label:"Ntst",integer:!0,rule:Pn(1),hint:"mesh intervals of a periodic orbit"},{key:"nmx",label:"Nmax",integer:!0,rule:Pn(1),hint:"the most points a run computes"},{key:"npr",label:"NPr",integer:!0,rule:Pn(1),hint:"a label every this many points"},{key:"ncol",label:"Ncol",integer:!0,rule:{kind:"range",lo:2,hi:7},hint:"collocation points per interval"},{key:"ds",label:"Ds",integer:!1,rule:{kind:"nonzero"},hint:"the first step (its sign is the direction)"},{key:"dsmin",label:"Dsmin",integer:!1,rule:{kind:"positive"},hint:"the smallest step"},{key:"dsmax",label:"Dsmax",integer:!1,rule:{kind:"positive"},hint:"the largest step"},{key:"rl0",label:"Par Min",integer:!1,rule:{kind:"any"},hint:"a run stops below it"},{key:"rl1",label:"Par Max",integer:!1,rule:{kind:"any"},hint:"a run stops above it"},{key:"a0",label:"Norm Min",integer:!1,rule:{kind:"any"},hint:"a run stops at a norm below it"},{key:"a1",label:"Norm Max",integer:!1,rule:{kind:"any"},hint:"a run stops at a norm above it"},{key:"epsl",label:"EPSL",integer:!1,rule:{kind:"positive"},hint:"Newton's tolerance for the parameters"},{key:"epsu",label:"EPSU",integer:!1,rule:{kind:"positive"},hint:"Newton's tolerance for the solution"},{key:"epss",label:"EPSS",integer:!1,rule:{kind:"positive"},hint:"the tolerance locating special points"},{key:"iad",label:"IAD",integer:!0,rule:Pn(0),hint:"adapt the mesh every this many steps (0 never)"},{key:"mxbf",label:"MXBF",integer:!0,rule:{kind:"any"},hint:"branch switches at most (negative: one way)"},{key:"iid",label:"IID",integer:!0,rule:{kind:"range",lo:0,hi:5},hint:"how much AUTO prints (0 to 5)"},{key:"itmx",label:"ITMX",integer:!0,rule:Pn(1),hint:"iterations locating a special point"},{key:"itnw",label:"ITNW",integer:!0,rule:Pn(1),hint:"Newton iterations"},{key:"nwtn",label:"NWTN",integer:!0,rule:Pn(1),hint:"Newton iterations before the Jacobian is frozen"},{key:"iads",label:"IADS",integer:!0,rule:Pn(0),hint:"adapt the step every this many steps (0 never)"},{key:"suppbp",label:"SuppBP",integer:!0,rule:{kind:"range",lo:0,hi:1},hint:"1: do not look for branch points"}],cs=[{title:"Mesh and steps",keys:["ntst","nmx","npr","ncol","ds","dsmin","dsmax"]},{title:"Limits and tolerances",keys:["rl0","rl1","a0","a1","epsl","epsu","epss"]},{title:"Solver",keys:["iad","mxbf","iid","itmx","itnw","nwtn","iads","suppbp"]}],ds=e=>sn.find(t=>t.key===e),pp={core:null,queued:null,sent:null,error:null};function ry(e,t){if(!e)return t;let n={...e};return t.numerics&&(n.numerics={...e.numerics,...t.numerics}),t.pars&&(n.pars=t.pars),t.axes&&(n.axes={...e.axes,...t.axes,fit:!!(e.axes?.fit||t.axes.fit)}),t.marks&&(n.marks=t.marks),n}function si(e,t){switch(t.type){case"event":return{...e,core:t.ev};case"queue":return{...e,queued:ry(e.queued,t.patch),error:null};case"sent":return{...e,sent:t.patch,queued:null,error:null};case"settled":return e.sent?{...e,sent:null}:e;case"error":return e.sent?{...e,error:t.text}:e;default:return e}}function up(e,t){if(!t)return e;let n=t.pars?e.pars.map((r,a)=>t.pars[a]?t.pars[a]:r):e.pars,{fit:o,...i}=t.axes??{};return{numerics:{...e.numerics,...t.numerics},pars:n,axes:{...e.axes,...i},marks:t.marks??e.marks}}function Eo(e){return e.core?up(up(e.core,e.sent),e.queued):null}function En(e){let t=new Set;for(let n of[e.sent,e.queued])if(n){for(let o of Object.keys(n.numerics??{}))t.add(`numerics.${o}`);for(let o of Object.keys(n.axes??{}))o!=="fit"&&t.add(`axes.${o}`);n.pars&&t.add("pars"),n.marks&&t.add("marks")}return t}function us(e){return{cmd:"auto",op:"set",...e}}function hp(e,t){let n=ds(e),o=Number(t),i=n.integer?"a whole number":"a number";if(!t.trim()||!Number.isFinite(o)||n.integer&&!Number.isInteger(o))return`${n.label} must be ${i}`;let r=n.rule;return r.kind==="positive"&&!(o>0)?`${n.label} must be a number above 0`:r.kind==="nonzero"&&o===0?`${n.label} must be a number other than 0`:r.kind==="range"&&(o<r.lo||r.hi!==null&&o>r.hi)?r.hi===null?`${n.label} must be ${i} of at least ${r.lo}`:`${n.label} must be ${i} from ${r.lo} to ${r.hi}`:null}function mp(e){return e.dsmin>e.dsmax?"Dsmin must be at most Dsmax":e.rl0<e.rl1?e.a0<e.a1?null:"Norm Min must be below Norm Max":"Par Min must be below Par Max"}var ay={0:"h",1:"n",2:"i",3:"p",4:"t",10:"r",11:"a"},fp=[["Y-axis","var"],["Main Parm","par1"],["Secnd Parm","par2"],["Xmin","xmin"],["Ymin","ymin"],["Xmax","xmax"],["Ymax","ymax"]];function sy(e){return e.replace(/^\*\d/,"").trim()}function gp(e){let t={};for(let[n,o]of fp)t[n]=e.axes[o]??"";return`${JSON.stringify({xppautX:"auto-settings",version:2,numerics:Object.fromEntries(sn.map(n=>[n.label,e.numerics[n.key]])),plot:e.axes.plot,axes:t,pars:e.pars,marks:e.marks},null,1)}
`}var ai=e=>!!e&&typeof e=="object"&&!Array.isArray(e),gr=e=>{if(typeof e=="number")return Number.isFinite(e)?e:null;if(typeof e=="string"&&e.trim()){let t=Number(e);return Number.isFinite(t)?t:null}return null};function bp(e){let t=a=>({patch:null,error:`Not an AUTO settings file: ${a}.`}),n;try{n=JSON.parse(e)}catch{return t("it is not JSON")}if(!ai(n)||n.xppautX!=="auto-settings")return t('it lacks "xppautX": "auto-settings"');if(n.numerics!==void 0&&!ai(n.numerics)||n.axes!==void 0&&!ai(n.axes))return t('"numerics" and "axes" must map names to values');let o={},i=a=>new Map(Object.entries(a).map(([l,d])=>[sy(l).toLowerCase(),d]));if(ai(n.numerics)){let a=i(n.numerics),l={};for(let d of sn){if(!a.has(d.label.toLowerCase()))continue;let c=gr(a.get(d.label.toLowerCase()));if(c===null)return t(`${d.label} is not a number`);l[d.key]=c}Object.keys(l).length&&(o.numerics=l)}let r={};if(n.plot!==void 0){let a=Number(n.plot);if(!(a in ay))return t(`"plot" ${String(n.plot)} is not one of AUTO's plot types`);r.plot=a}if(ai(n.axes)){let a=i(n.axes);for(let[l,d]of fp){if(!a.has(l.toLowerCase()))continue;let c=a.get(l.toLowerCase());if(d==="var"||d==="par1"||d==="par2"){if(typeof c!="string")return t(`${l} is not a name`);c.trim()&&(r[d]=c.trim())}else{let p=gr(c);if(p===null)return t(`${l} is not a number`);r[d]=p}}}if(Object.keys(r).length&&(o.axes=r),n.pars!==void 0){if(!Array.isArray(n.pars)||n.pars.some(a=>a!==null&&typeof a!="string"))return t('"pars" must be names');o.pars=n.pars.map(a=>a??"")}if(n.marks!==void 0){if(!(Array.isArray(n.marks)&&n.marks.every(l=>Array.isArray(l)&&typeof l[0]=="string"&&gr(l[1])!==null)))return t('"marks" must be [name, value] pairs');o.marks=n.marks.map(([l,d])=>[l,gr(d)])}return Object.keys(o).length?{patch:o,error:null}:t("it holds no values")}var ly=/^\s*-?\d+\s+-?\d+\s+\S\S\s+-?\d+(\s+-?\d+(\.\d+)?([eE][+-]?\d+)?){2,}\s*$/,cy=/BR\s+PT\s+TY\s+LAB|Generating starting data|Restart at EP label|Hopf point|Limit point|Periodic point|Max point|End point|NPARX|NCOL=|DSMIN|DSMAX|Division by Zero|Initialization Error CRASH|Restart label/;function dy(e){return ly.test(e)||cy.test(e)?"auto":"log"}var vp={connected:!1,exited:null,hello:null,core:null,busy:!1,stopping:!1,ask:null,pick:null,box:"",progress:null,bottom:"",title:"",plots:uu,seriesCount:0,seriesAppends:0,hover:null,log:[],toasts:[],nextToast:1,theme:"system",drawerOpen:!1,values:cp,valuesOpen:!1,table:ju,text:Zu,files:Yd,diagram:mr,autoSettings:pp,aplot:Pu,ani:Nu,kinescope:ts,help:Ju},yp=200,uy=4,py=104;function ps(e,t){let n=e.log.length>=yp?e.log.slice(1-yp):e.log.slice();return n.push(t),{...e,log:n}}function wp(e,t,n,o){let i=o?{id:e.nextToast,kind:t,text:n,action:o}:{id:e.nextToast,kind:t,text:n},r=[...e.toasts,i].slice(-uy);return{...e,toasts:r,nextToast:e.nextToast+1}}var hy=new Set(["abort","quit"]);function my(e){return hy.has(e.cmd)||e.cmd==="browser"&&"from"in e}function Ot(e,t){return t===e.plots?e:{...e,plots:t,hover:t.active===e.plots.active?e.hover:null}}function fy(e,t){return!!e&&e.win===t.win&&(e.xlo!==t.xlo||e.xhi!==t.xhi||e.ylo!==t.ylo||e.yhi!==t.yhi)}function gy(e,t){let n=e?.defaults,o=(i,r)=>r&&r.length===i.length?i.map(([a],l)=>[a,r[l]]):i;return{type:"defaults",pars:o(t.pars,n?.pars),ics:o(t.ics,n?.ics)}}var by=105;function yy(e,t){switch(t.ev){case"hello":{let n=Po({...e.values,defaults:null},{type:"presetSliders",defs:t.sliders??[]});return{...e,hello:t,title:t.title,values:n,autoSettings:{...e.autoSettings,sent:null}}}case"state":{let n=t.view&&fy(e.core?.view,t.view),o=an(e.diagram,{type:"core",open:!!t.auto});return{...e,core:t,plots:n?Su(e.plots,t.view.win):e.plots,diagram:o,values:e.values.defaults?e.values:Po(e.values,gy(e.hello,t))}}case"series":{let n=t.win===e.plots.active;if(t.op==="append"){let o=ku(e.plots,t);if(!o)return e;let i=e.hover&&(!n||e.hover.row<t.from)?e.hover:null;return{...e,plots:o,seriesAppends:e.seriesAppends+1,hover:i}}return{...e,plots:fu(e.plots,t),seriesCount:e.seriesCount+1,hover:n?null:e.hover}}case"plots":return Ot(e,hu(e.plots,t));case"erase":return Ot(e,gu(e.plots,t.win));case"redraw":return Ot(e,bu(e.plots,t.win));case"nullclines":return Ot(e,vu(e.plots,t));case"dfield":return Ot(e,wu(e.plots,t));case"marks":return Ot(e,xu(e.plots,t));case"window":if(t.win===101&&t.op!=="select")return{...e,diagram:an(e.diagram,{type:"window",op:t.op})};if(t.win===py&&t.op!=="select")return{...e,ani:To(e.ani,{type:"window",exists:t.op==="create"})};if(t.win===by){if(t.op==="destroy")return{...e,aplot:ko(e.aplot,{type:"window",open:!1})};if(t.op==="create"){let n=ko(ko(e.aplot,{type:"window",open:!0}),{type:"panel",open:!0});return{...e,aplot:n}}return e}return t.op==="select"&&t.win<=10?Ot(e,ur(e.plots,t.win)):e;case"ask":{let n=Number(t.win)===101&&e.diagram.open,o=Kd(t,e.core?.view,!!et(e.plots,Number(t.win))?.series,n),i={...e,ask:t,pick:o?Bd(e.pick,t,o):null};if(n&&(o||t.kind==="grab")){let r=an(an(e.diagram,{type:"show",shown:!0}),{type:"grabbing",on:t.kind==="grab"||e.diagram.grabbing});return{...i,diagram:r}}return o?Ot(i,ur(e.plots,Number(t.win))):i}case"idle":return{...e,busy:!1,stopping:!1,ask:null,pick:null,box:"",progress:null,values:Po(e.values,{type:"settled"}),ani:To(e.ani,{type:"playing",playing:!1}),diagram:Vu(e.diagram),autoSettings:si(e.autoSettings,{type:"settled"})};case"ani":return{...e,ani:To(e.ani,t.op==="frame"?{type:"frame",ev:t}:{type:"state",ev:t})};case"film":{if(t.op==="capture"){let n=et(e.plots,t.win);return n?{...e,kinescope:ii(e.kinescope,{type:"capture",frame:hr(n)})}:e}return t.op==="reset"?{...e,kinescope:ii(e.kinescope,{type:"clear"})}:{...e,kinescope:ii(e.kinescope,{type:"playing",playing:!0,cycles:t.cycles,delay:t.delay})}}case"stopped":return{...e,diagram:an(e.diagram,{type:"runStopped"})};case"autoinfo":return{...e,diagram:an(e.diagram,{type:"info",ev:t})};case"autosettings":return{...e,autoSettings:si(e.autoSettings,{type:"event",ev:t})};case"progress":return{...e,progress:t.of>0?{n:t.n,of:t.of}:null};case"title":return{...e,title:t.text};case"message":if(t.error!==void 0){let n=tu(t.error,e.files.run),o=n?{kind:"addFile",name:n,run:e.files.run}:void 0,i={...e.files,runFailed:!0},r=wp(ps({...e,bottom:t.error,files:i},{kind:"error",text:t.error}),"error",t.error,o),a=/^AUTO settings: /.test(t.error)?si(e.autoSettings,{type:"error",text:t.error}):e.autoSettings;return{...r,values:Po(r.values,{type:"error",text:t.error}),autoSettings:a}}return t.bottom!==void 0?{...e,bottom:t.bottom}:t.box!==void 0?{...e,box:t.box}:e;case"aplot":return{...e,aplot:ko(e.aplot,{type:"event",ev:t})};case"browser":return{...e,table:os(e.table,{type:"event",ev:t})};case"equations":return{...e,text:ri(e.text,{type:"equations",ev:t})};case"source":return{...e,text:ri(e.text,{type:"source",ev:t})};case"equilibrium":return{...e,text:ri(e.text,{type:"equilibrium",ev:t})};case"diagram":return{...e,diagram:an(e.diagram,{type:"event",ev:t})};case"log":return ps(e,{kind:dy(t.text),text:t.text});case"exit":return{...e,exited:t.code,busy:!1,stopping:!1};case"bye":return ps(e,{kind:"info",text:"XPP has exited."});default:return e}}function xp(e,t){switch(t.type){case"event":return yy(e,t.ev);case"connection":return t.open===e.connected?e:{...e,connected:t.open};case"sent":{let n=Zd(e.files,t.cmd,e.ask,e.core?.menu??0);if(n!==e.files&&(e={...e,files:n}),t.cmd.cmd==="answer"){let o=e.pick,i=t.cmd.ok===0;return{...e,ask:null,pick:!o||i?null:o.mode==="drag"?o:{...o,waiting:!0}}}return t.cmd.cmd==="ani"&&t.cmd.op==="go"&&(e={...e,ani:To(e.ani,{type:"playing",playing:!0})}),my(t.cmd)?e:{...e,busy:!0}}case"aborting":return e.busy?{...e,stopping:!0}:e;case"viewport":return Ot(e,Qa(e.plots,t.win??e.plots.active,t.viewport,t.push));case"undoViewport":return Ot(e,Tu(e.plots,t.win??e.plots.active));case"rotate3d":return Ot(e,mu(e.plots,t.win,t.theta,t.phi));case"selectWindow":return Ot(e,ur(e.plots,t.win));case"showRuns":return Ot(e,yu(e.plots,t.win,t.show));case"hover":return{...e,hover:t.hover};case"pick":return{...e,pick:t.pick};case"toast":return wp(e,t.kind,t.text);case"dismiss":return{...e,toasts:e.toasts.filter(n=>n.id!==t.id)};case"drawer":return t.open===e.drawerOpen?e:{...e,drawerOpen:t.open};case"theme":return{...e,theme:t.theme};case"values":return{...e,values:Po(e.values,t.action)};case"valuesPanel":return t.open===e.valuesOpen?e:{...e,valuesOpen:t.open};case"table":return{...e,table:os(e.table,t.action)};case"text":return{...e,text:ri(e.text,t.action)};case"files":return{...e,files:Gd(e.files,t.action)};case"diagram":{let n=an(e.diagram,t.action);return n===e.diagram?e:{...e,diagram:n}}case"autoSettings":return{...e,autoSettings:si(e.autoSettings,t.action)};case"aplot":return{...e,aplot:ko(e.aplot,t.action)};case"ani":return{...e,ani:To(e.ani,t.action)};case"kinescope":return{...e,kinescope:ii(e.kinescope,t.action)};case"help":return{...e,help:Qu(e.help,t.action)}}}function kp(e){if(!Number.isFinite(e))return Number.isNaN(e)?"nan":e>0?"inf":"-inf";if(e===0)return Object.is(e,-0)?"-0":"0";let t=Math.floor(Math.log10(Math.abs(Number(e.toPrecision(16)))));if(t<-4||t>=16){let[o,i]=e.toExponential(15).split("e"),r=o.includes(".")?o.replace(/0+$/,"").replace(/\.$/,""):o,a=Number(i);return`${r}e${a<0?"-":"+"}${String(Math.abs(a)).padStart(2,"0")}`}let n=e.toFixed(Math.max(0,15-t));return n.includes(".")?n.replace(/0+$/,"").replace(/\.$/,""):n}function Tp(e,t,n){return`${[`${e.length}   Number params`,...e.map(([i,r])=>`${kp(r)}  ${i}`)].join(`
`)}


File:${t}
${n}
`}function Sp(e){return e.map(([,t])=>kp(t)).join(`
`)+`
`}var vy=/^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$/,li=e=>vy.test(e);function Pp(e,t){let n=e.split(/\r?\n/),o=new Map(t.map(c=>[c.toLowerCase(),c])),i=c=>({values:[],error:c}),r=/^\s*(\d+)\s+Number params/i.exec(n[0]??"");if(r){let c=Number(r[1]);if(c!==t.length)return i(`The file has ${c} parameters, the model ${t.length}`);let p=[];for(let m=0;m<c;m++){let[u]=(n[1+m]??"").trim().split(/\s+/);if(!u||!li(u))return i(`Line ${m+2} is not a number: ${n[1+m]??""}`);p.push([t[m],u])}return{values:p,error:null}}let a=n.map(c=>c.trim()).filter(c=>c&&!c.startsWith("#"));if(a.length&&a.every(c=>c.split(/\s+/).every(li))){let c=a.flatMap(p=>p.split(/\s+/));return c.length!==t.length?i(`The file has ${c.length} values, the model ${t.length}`):{values:c.map((p,m)=>[t[m],p]),error:null}}let l=[],d=[];for(let c of a){let p=c.split(/\s*=\s*|\s+/).filter(Boolean);if(p.length<2)return i(`Not "name value": ${c}`);let[m,u]=li(p[1])&&!li(p[0])?[p[0],p[1]]:li(p[0])?[p[1],p[0]]:["",""];if(!u)return i(`Not "name value": ${c}`);let f=o.get(m.toLowerCase());f?l.push([f,u]):d.push(m)}return d.length?i(`Not in the model: ${d.join(", ")}`):l.length?{values:l,error:null}:i("No values in the file")}var Ep="xpp.values.runOnChange",br=class{constructor(t,n=null){this.transport=t;this.files=n;this.pendingKeys=[];this.typeahead=[];this.keyWaiting=!1;this.plan=[];this.planIdles=0;this.planCmds=[];this.planDone=null;this.grabbed=null;this.dragQueue=[];this.dragEnded=!1;this.pendingSave=null;this.replaceChoice=null;this.replayAnswers=[];this.rotate3dLast=new Map;this.rotate3dTimer=new Map;this.replayIdles=0;this.afterIdle=null;this.diagramAsked=!1;this.pendingAplotDy=0;this.filmTimer=null;this.store=ou(xp,vp)}start(){try{localStorage.getItem(Ep)==="0"&&this.store.dispatch({type:"values",action:{type:"runOnChange",on:!1}})}catch{}this.transport.open(t=>this.receive(t),t=>this.store.dispatch({type:"connection",open:t}))}receive(t){if(this.store.dispatch({type:"event",ev:t}),t.ev==="hello"){this.keyWaiting=!1,this.typeahead=[];let n=["series","plots","nullclines","dfield","marks","ani","autoinfo","autosettings"].filter(o=>t.features?.includes(o));n.length&&this.send({cmd:"data",events:n,enc:"f32"})}else if(t.ev==="film")this.onFilm(t);else if(t.ev==="ask"){if(this.keyWaiting=!1,this.plan.length&&t.kind!=="pixels"&&t.kind!=="alert"){this.continuePlan(t),this.checkDiagram(t);return}t.kind==="pixels"?this.answerPixels(t):t.kind==="alert"?(this.store.dispatch({type:"toast",kind:"info",text:t.message??""}),this.answer(t,{})):t.kind==="drag"&&(this.dragEnded||this.dragQueue.length)?this.dragQueue.length?this.answer(t,this.dragQueue.shift()):this.cancel(t):this.replayAnswers.length?this.continueReplay(t):this.continueKeys(t)}else if(t.ev==="progress")this.keyWaiting=!1;else if(t.ev==="idle"){this.pendingKeys=[],this.keyWaiting=!1,this.afterAuto(),this.dragQueue=[],this.dragEnded=!1,this.replayIdles>0&&--this.replayIdles===0&&(this.replayAnswers=[]);let n=this.pendingSave;this.pendingSave=null,n&&!this.store.getState().files.runFailed&&this.deliver(n.name,n.handle);let o=this.afterIdle;this.afterIdle=null,o?this.send(o):(this.flushValues(),this.flushAutoSettings());let i=this.typeahead.shift();i!==void 0&&!o&&!this.planIdles&&this.key(i)}this.checkDiagram(t)}checkDiagram(t){let n=this.store.getState().diagram;if(!n.open||n.axes&&!n.outOfStep){this.diagramAsked=!1;return}this.diagramAsked||t.ev!=="state"||(this.diagramAsked=!0,this.send({cmd:"redraw"}))}continueReplay(t){let n=this.replayAnswers[0];if(n.kind!==t.kind){this.replayAnswers=[];return}this.replayAnswers.shift(),this.answer(t,n.fields)}continueKeys(t){if(!this.pendingKeys.length&&this.typeahead.length&&(t.kind==="menu"||t.kind==="choice")){let n=this.typeahead.shift(),o=(t.keys??"").toLowerCase().indexOf(n.toLowerCase());o>=0?this.answer(t,{key:t.keys[o]}):this.typeahead=[];return}this.pendingKeys.length&&(t.kind==="menu"||t.kind==="choice"?this.answer(t,{key:this.pendingKeys.shift()}):this.pendingKeys=[])}send(t){this.store.dispatch({type:"sent",cmd:t}),this.transport.send(t)}key(t){this.keyWaiting=!0,this.send({cmd:"key",key:t})}typeKey(t){let n=this.store.getState().ask;if(n){let o=(n.keys??"").toLowerCase().indexOf(t.toLowerCase());(n.kind==="menu"||n.kind==="choice")&&t.length===1&&o>=0&&this.answer(n,{key:n.keys[o]});return}if(this.keyWaiting&&t!=="Escape"){this.typeahead.push(t);return}this.key(t)}keys(t,...n){this.pendingKeys=n,this.key(t)}answer(t,n){let{run:o,points:i}=this.store.getState().diagram;o?.active&&t.kind==="menu"&&i.x.length===o.first&&this.store.dispatch({type:"diagram",action:{type:"run",op:"clock",at:Date.now()}}),this.send({cmd:"answer",id:t.id,...n})}cancel(t){this.answer(t,{ok:0})}movePick(t){this.store.dispatch({type:"pick",pick:t})}confirmPick(t,n){let o=this.store.getState().ask;o&&o.id===t.ask&&this.answer(o,jd(t,n))}dragEvent(t,n,o){let i=this.store.getState().ask,r={what:t,xd:n,yd:o};if(i?.kind==="drag"&&!this.dragQueue.length){this.answer(i,r);return}let a=this.dragQueue[this.dragQueue.length-1];t==="move"&&a?.what==="move"&&this.dragQueue.pop(),this.dragQueue.push(r)}cancelPick(){let{ask:t,pick:n}=this.store.getState();t&&(t.kind==="mouse"||t.kind==="rubber"||t.kind==="drag"||t.kind==="grab")&&!this.dragQueue.length?this.cancel(t):n&&(this.dragEnded=!0,this.store.dispatch({type:"pick",pick:null}),t?.kind==="drag"&&this.answer(t,this.dragQueue.shift()))}selectWindow(t){let{plots:n,ask:o}=this.store.getState();n.active===t||o||(this.store.dispatch({type:"selectWindow",win:t}),this.send({cmd:"click",win:t}))}newWindow(){this.keys("m","c")}closeWindow(){this.keys("m","d")}useThisView(t,n){this.send({cmd:"view",win:t,xlo:n.x.min,xhi:n.x.max,ylo:n.y.min,yhi:n.y.max})}fitView(){this.keys("w","f")}rotate3d(t,n,o){this.store.dispatch({type:"rotate3d",win:t,theta:n,phi:o});let i=this.rotate3dTimer.get(t);i!==void 0&&clearTimeout(i);let r=this.rotate3dLast.get(t)??-1/0,a=()=>{this.rotate3dLast.set(t,performance.now()),this.rotate3dTimer.delete(t),this.send({cmd:"view3d",win:t,theta:n,phi:o})};performance.now()-r>=100?a():this.rotate3dTimer.set(t,setTimeout(a,150))}abort(){this.store.dispatch({type:"aborting"}),this.transport.send({cmd:"abort"})}autoOp(t){if(t==="clear"){this.store.dispatch({type:"diagram",action:{type:"clear"}});return}t==="run"&&this.store.dispatch({type:"diagram",action:{type:"run",op:"start",at:Date.now()}}),this.send({cmd:"auto",op:t})}runPlan(t,n,o){this.plan=n,this.planCmds=t.slice(1),this.planIdles=t.length,this.planDone=o??null,this.send(t[0])}continuePlan(t){let n=this.plan.shift()(t);if(n){this.answer(t,n);return}this.plan=[],this.planCmds=[],this.planDone=null}afterAuto(){if(this.store.getState().diagram.run?.active&&this.store.dispatch({type:"diagram",action:{type:"run",op:"end",at:Date.now()}}),this.planIdles>0&&--this.planIdles===0){let n=this.planDone;this.plan=[],this.planDone=null,n?.()}else this.planCmds.length?this.send(this.planCmds.shift()):this.planIdles>0&&!this.plan.length&&(this.planIdles=0);let t=this.grabbed;this.grabbed=null,t!==null&&this.importOrbit(t)}importOrbit(t){let{points:n,labels:o}=this.store.getState().diagram,i=n.ty[t];if(!(i!==3&&i!==4||n.f2[t])){if(!o.some(r=>r.point===t)){this.store.dispatch({type:"toast",kind:"info",text:"AUTO keeps the orbits of labelled points only: grab a labelled point of the periodic branch (Tab steps through them) to plot its limit cycle."});return}this.runPlan([{cmd:"auto",op:"file"}],[r=>r.kind==="menu"?{key:"i"}:null])}}autoSettings(t){let n=this.store.getState();if(n.busy||n.autoSettings.sent){this.store.dispatch({type:"autoSettings",action:{type:"queue",patch:t}});return}this.store.dispatch({type:"autoSettings",action:{type:"sent",patch:t}}),this.send(us(t))}flushAutoSettings(){let t=this.store.getState(),n=t.autoSettings.queued;!n||t.busy||t.autoSettings.sent||(this.store.dispatch({type:"autoSettings",action:{type:"sent",patch:n}}),this.send(us(n)))}saveAutoSettings(){let t=Eo(this.store.getState().autoSettings);if(!t)return;let n=gp(t);this.store.dispatch({type:"diagram",action:{type:"setupSaved",text:n}}),Ni(`${this.modelBase()}-auto.json`,new Blob([n],{type:"application/json"}))}modelBase(){return(this.store.getState().hello?.file??"").replace(/^.*[\\/]/,"").replace(/\.[^.]*$/,"")||"model"}loadAutoSettings(t){let{patch:n,error:o}=bp(t);return n?(this.autoSettings(n),null):(this.store.dispatch({type:"toast",kind:"error",text:o}),o)}closeAuto(){let{busy:t,stopping:n}=this.store.getState();if(!t){this.send({cmd:"auto",op:"close"});return}this.afterIdle={cmd:"auto",op:"close"},n||this.abort()}grabPoint(t,n=!1){let o=this.store.getState().ask;o?.kind==="grab"&&(n&&(this.grabbed=t),this.answer(o,n?{point:t,key:"Return"}:{point:t}))}grabTake(){let{ask:t,diagram:n}=this.store.getState();t?.kind==="grab"&&(this.grabbed=n.info?.point??null,this.answer(t,{key:"Return"}))}autoPoint(t,n){this.store.dispatch({type:"diagram",action:{type:"stored",at:{x:t,y:n}}}),this.send({cmd:"auto",op:"point",xd:t,yd:n})}showAuto(t){this.store.dispatch({type:"diagram",action:{type:"show",shown:t}})}recordEdit(t){this.store.dispatch({type:"values",action:{type:"edit",edit:t}})}rerunsOn(t){return this.store.getState().values.runOnChange&&(t==="par"||t==="ic")}submit(t,n){if(t.length){if(this.store.getState().busy){for(let o of t)this.store.dispatch({type:"values",action:{type:"queue",set:o,rerun:n}});return}this.send(ls(t,n))}}flushValues(){let{queue:t,queueRerun:n}=this.store.getState().values;t.length&&(this.store.dispatch({type:"values",action:{type:"flushed"}}),this.send(ls(t,n)))}setValue(t,n,o,i){this.recordEdit({kind:t,name:n,previous:i}),this.submit([{kind:t,name:n,text:o}],this.rerunsOn(t))}setValueByIndex(t,n,o,i){this.recordEdit({kind:t,index:n,previous:i}),this.submit([{kind:t,index:n,text:o}],!1)}slide(t,n,o){this.store.getState().busy?this.submit([{kind:t,name:n,text:String(o)}],!0):this.send({cmd:"slide",name:n,value:o,rerun:1})}undoValue(){let{history:t}=this.store.getState().values,n=t[t.length-1];if(!n)return;this.store.dispatch({type:"values",action:{type:"undo"}});let o=n.index!==void 0?{kind:n.kind,index:n.index,text:n.previous}:{kind:n.kind,name:n.name,text:n.previous};this.submit([o],this.rerunsOn(n.kind))}defaultOf(t,n){return this.store.getState().values.defaults?.[zt(t,n)]??null}resetValue(t,n,o){let i=this.defaultOf(t,n);i!==null&&this.setValue(t,n,String(i),o)}defaultValues(t){this.store.dispatch({type:"values",action:{type:"defaulted",kind:t}});let n=this.store.getState();if(n.busy){let i=((t==="par"?n.core?.pars:n.core?.ics)??[]).flatMap(([r])=>{let a=this.defaultOf(t,r);return a===null?[]:[{kind:t,name:r,text:String(a)}]});this.submit(i,this.rerunsOn(t))}else this.send(this.rerunsOn(t)?{cmd:"default",kind:t,rerun:1}:{cmd:"default",kind:t})}useCurrentState(){this.send({cmd:"set",kind:"ic",from:"last"})}setRunOnChange(t){this.store.dispatch({type:"values",action:{type:"runOnChange",on:t}});try{localStorage.setItem(Ep,t?"1":"0")}catch{}}saveValues(t){let n=this.store.getState(),o=(t==="par"?n.core?.pars:n.core?.ics)??[],i=n.hello?.file??"",r=i.replace(/^.*[\\/]/,"").replace(/\.[^.]*$/,"")||"model",a=t==="par"?Tp(o,i,new Date().toString()):Sp(o);return this.store.dispatch({type:"values",action:{type:"saved",kind:t,text:a}}),Ni(`${r}.${t==="par"?"par":"ic"}`,new Blob([a],{type:"text/plain"})),a}loadValues(t,n){let o=this.store.getState(),i=((t==="par"?o.core?.pars:o.core?.ics)??[]).map(([a])=>a),r=Pp(n,i);return r.error?(this.store.dispatch({type:"toast",kind:"error",text:r.error}),r.error):(this.store.dispatch({type:"values",action:{type:"defaulted",kind:t}}),this.submit(r.values.map(([a,l])=>({kind:t,name:a,text:l})),this.rerunsOn(t)),null)}userButton(t){this.send({cmd:"userbut",index:t})}openTable(){this.store.dispatch({type:"table",action:{type:"open",open:!0}})}closeTable(){this.store.dispatch({type:"table",action:{type:"open",open:!1}}),this.send({cmd:"browser",from:0,count:0})}selectTableRow(t){this.store.dispatch({type:"table",action:{type:"select",row:t}})}fetchTableRows(t,n){let{page:o,pendingKey:i}=this.store.getState().table,r=Wu(o,t,n);!r||JSON.stringify(r)===i||(this.store.dispatch({type:"table",action:{type:"requested",req:r}}),this.send({cmd:"browser",...r}))}browserOp(t){this.send({cmd:"browser",op:t,row:this.store.getState().table.selected})}getRow(){this.browserOp("get")}async exportTableCsv(){let t=this.store.getState().table.page?.rows??0;this.store.dispatch({type:"table",action:{type:"exporting"}});let n=[];for(let i=0;i<t;i+=fr)n.push(await this.browserBlock(i,fr));let o=Gu(n);return this.store.dispatch({type:"table",action:{type:"exported",csv:o}}),o}browserBlock(t,n){return new Promise(o=>{let i=this.store.getState().table.page,r=this.store.subscribe(()=>{let a=this.store.getState().table.page;a&&a!==i&&a.from===t&&(r(),o(a))});this.send({cmd:"browser",from:t,count:n,col:1,ncol:is})})}openAni(){this.store.dispatch({type:"ani",action:{type:"open",open:!0}});let{ani:t,busy:n}=this.store.getState();!t.exists&&!n&&this.keys("v","t")}closeAni(){this.aniPause(),this.store.dispatch({type:"ani",action:{type:"open",open:!1}})}aniLoad(){this.send({cmd:"ani",op:"file"})}aniPlay(){this.store.getState().ani.playing||this.send({cmd:"ani",op:"go"})}aniPause(){this.store.getState().ani.playing&&this.send({cmd:"ani",op:"pause"})}aniStep(t){let n=this.store.getState().ani,o=Ru(n,t);n.playing?this.aniSeek(o):this.send({cmd:"ani",op:"step",n:o-n.pos})}aniSeek(t){this.aniPause(),this.send({cmd:"ani",op:"seek",pos:Math.max(0,Math.round(t))})}aniSpeed(t){this.send({cmd:"ani",op:"speed",ms:Math.max(0,Math.round(t))})}aniGrab(){this.send({cmd:"ani",op:"grab"})}aniPointer(t,n,o){this.send({cmd:"ani",op:"mouse",what:t,u:n,v:o})}onFilm(t){if(t.op!=="play"&&t.op!=="autoplay")return;this.stopFilmTimer();let n=Math.min(t.count,this.store.getState().kinescope.frames.length);if(n<=0)return;let o=t.op==="autoplay"?Math.max(1,t.cycles):1,i=Math.max(0,t.delay),r=0,a=0,l=()=>{if(this.store.dispatch({type:"kinescope",action:{type:"show",index:r}}),r++,r>=n&&(r=0,a++,a>=o)){this.filmTimer=null,this.store.dispatch({type:"kinescope",action:{type:"playing",playing:!1}});return}this.filmTimer=setTimeout(l,i)};l()}stopFilmTimer(){this.filmTimer!==null&&(clearTimeout(this.filmTimer),this.filmTimer=null)}kinescopeMenu(t){this.store.getState().busy||this.keys("k",t)}kinescopeCapture(){this.kinescopeMenu("c")}kinescopeReset(){this.kinescopeMenu("r")}kinescopePlay(){this.kinescopeMenu("p")}kinescopeStop(){this.stopFilmTimer(),this.store.getState().kinescope.playing&&this.store.dispatch({type:"kinescope",action:{type:"playing",playing:!1}})}answerPixels(t){let n=document.documentElement.dataset.theme==="dark",o=null;if(typeof t.film=="number"){let i=this.store.getState().kinescope.frames[t.film];i&&(o=sr(i,n))}else if(typeof t.win=="number"&&(o=cr(t.win)?.pixels()??null,!o)){let i=et(this.store.getState().plots,t.win);i&&(o=sr(hr(i),n))}o?this.answer(t,{w:o.w,h:o.h,rgb:Ri(o.rgb)}):this.cancel(t)}kinescopeFramePixels(){let t=document.documentElement.dataset.theme==="dark",n=this.store.getState().kinescope.frames;if(!n.length)return null;let o=n.map(i=>sr(i,t));return o.every(i=>i!==null)?o:null}kinescopeGifBytes(){let t=this.kinescopeFramePixels();return t?Jl(t.map(n=>({w:n.w,h:n.h,rgb:n.rgb})),this.store.getState().kinescope.delay):null}downloadKinescopeGif(t="xpp-kinescope.gif"){let n=this.kinescopeGifBytes();if(!n)return;let o=URL.createObjectURL(new Blob([n],{type:"image/gif"}));Bt(t,o),setTimeout(()=>URL.revokeObjectURL(o),1e3)}openText(t){let n=this.store.getState().text,o=t??n.tab;this.store.dispatch({type:"text",action:{type:"open",open:!0}}),o!==n.tab&&this.store.dispatch({type:"text",action:{type:"tab",tab:o}}),this.refreshText(o)}closeText(){this.store.dispatch({type:"text",action:{type:"open",open:!1}})}selectTextTab(t){t!==this.store.getState().text.tab&&(this.store.dispatch({type:"text",action:{type:"tab",tab:t}}),this.refreshText(t))}refreshText(t){if(t==="equations")this.send({cmd:"equations"});else if(t==="source"){let n=this.store.getState().core?.menu??0;n===2&&this.key("Escape"),n!==1&&this.key("f"),this.key("p")}}runAction(t){this.send({cmd:"action",index:t})}findEquilibrium(){this.keys("s","g","n")}importEquilibrium(){this.send({cmd:"eqimport"})}failed(t){this.store.dispatch({type:"toast",kind:"error",text:t})}async openFiles(t,n){if(!this.files||!n.length)return!1;try{let o=await this.files.list();this.store.dispatch({type:"files",action:{type:"listing",files:o}});let i=new Set(o.map(l=>l.name)),r=[];for(let l of n){if(!Kn(l.name))return this.failed(`XPP cannot use a file named \u201C${l.name}\u201D in the model's folder. Rename it and pick it again.`),!1;if(l.size>Ap)return this.failed(`${l.name} is larger than 64 MB, the most the model's folder takes from the page.`),!1;let d=await ua(l),c=eu(l.name,d,o),p=l.name;if(c==="confirm"){let m=Jd(l.name,i),u=await this.confirmReplace(t,l.name,m);if(u==="cancel")return!1;u==="keep"&&(p=m)}c!=="same"&&await this.files.put(p,l),i.add(p),r.push({picked:l.name,name:p,sha256:d,copied:c!=="same"})}if(this.store.dispatch({type:"files",action:{type:"uploaded",uploads:r}}),this.store.getState().ask?.id!==t.id)return!1;let a=Qd(r.map(l=>l.picked),t.wild);return this.answer(t,{file:r.find(l=>l.picked===a).name}),!0}catch(o){return this.failed(o instanceof Error?o.message:String(o)),!1}}confirmReplace(t,n,o){return this.replaceChoice?.("cancel"),this.store.dispatch({type:"files",action:{type:"confirm",confirm:{ask:t.id,name:n,keepBoth:o}}}),new Promise(i=>{this.replaceChoice=i})}resolveReplace(t){let n=this.replaceChoice;this.replaceChoice=null,this.store.dispatch({type:"files",action:{type:"confirm",confirm:null}}),n?.(t)}saveFile(t,n,o){this.pendingSave={name:n,handle:o},this.answer(t,{file:n})}async deliver(t,n){if(this.files)try{let o=await this.files.get(t);if(!o)return;n?await Bl(n,o):Ni(t,o);let i=n?"picker":"download";this.store.dispatch({type:"files",action:{type:"offered",offered:{name:t,size:o.size,sha256:await ua(o),how:i}}})}catch(o){this.failed(`${t} is in the model's folder, but copying it failed: ${o instanceof Error?o.message:String(o)}`)}}async addMissingFile(t,n){let o=this.store.getState().toasts.find(l=>l.id===t)?.action;if(!this.files||!o)return;try{if(n.size>Ap)throw new Error(`${n.name} is larger than 64 MB, the most the model's folder takes from the page.`);await this.files.put(o.name,n)}catch(l){this.failed(l instanceof Error?l.message:String(l));return}this.store.dispatch({type:"dismiss",id:t});let{run:i}=o,r=this.store.getState(),a=i?nu(r.core?.menu??0,i.menu):null;if(!i||!a||r.busy){this.store.dispatch({type:"toast",kind:"info",text:`${o.name} is in the model's folder now. Run the command again.`});return}this.replayAnswers=i.answers.slice(),this.replayIdles=a.length+1;for(let l of a)this.key(l);this.send(i.cmd)}openAplot(){let{windowOpen:t}=this.store.getState().aplot;this.store.dispatch({type:"aplot",action:{type:"panel",open:!0}}),t?this.aplotOp("redraw"):this.keys("v","a")}closeAplot(){this.store.dispatch({type:"aplot",action:{type:"panel",open:!1}})}aplotOp(t){this.send({cmd:"aplot",op:t})}aplotScroll(t){let{send:n,pending:o}=Gl(this.pendingAplotDy,t,this.store.getState().busy);this.pendingAplotDy=o,n&&this.send({cmd:"aplot",op:"scroll",dy:n})}aplotKeyScroll(t){let n=Yl(t,this.store.getState().aplot.event?.ny??1);return n===null?!1:(this.aplotScroll(n),!0)}setAplotColorMap(t){this.store.dispatch({type:"aplot",action:{type:"colorMap",map:t}})}aplotHover(t){this.store.dispatch({type:"aplot",action:{type:"hover",hover:t}})}},Ap=64*1024*1024;var Ao=e=>e===2||e===3;function wy(e,t,n){return!e.nw[n]&&e.br[t]===e.br[n]&&e.ty[t]===e.ty[n]&&e.d[t]===e.d[n]&&e.c[t]===e.c[n]&&e.lw[t]===e.lw[n]&&e.f2[t]===e.f2[n]}function xy(e,t){return e.f2[t]?"two-parameter":Ao(e.d[t])?"periodic":"steady"}function ky(e,t,n,o){if(e.fr[n]){let m=-1;for(let f of t)f.lab===e.fr[n]&&f.point<n&&f.point>m&&(m=f.point);let u=m>=0?t.find(f=>f.point===m):void 0;return u&&u.sym==="HB"&&!e.f2[m]&&!Ao(e.d[m])?m:-1}let i=e.x[n],r=Math.min(e.y[n],e.y2[n]),a=Math.max(e.y[n],e.y2[n]),l=.05*o.x,d=.02*o.y,c=-1,p=1/0;for(let m of t){let u=m.point;if(m.sym!=="HB"||e.f2[u]||Ao(e.d[u])||e.br[u]===e.br[n])continue;let f=Math.abs(e.x[u]-i),w=e.y[u],k=w<r?r-w:w>a?w-a:0;if(!(f<=l&&k<=d))continue;let A=f/(l||1)+k/(d||1);A<p&&(p=A,c=u)}return c}function _p(e){let t=1/0,n=-1/0;for(let o of e)o<t&&(t=o),o>n&&(n=o);return n>t?n-t:1}function Mp(e,t,n,o,i){let r=xy(e,t),a=e.ty[t],l=r==="two-parameter"||a===1||a===3,d=o==="y"?e.y:e.y2,c=new Float64Array(n.length),p=new Float64Array(n.length),m=new Int32Array(n.length);n.forEach((f,w)=>{c[w]=e.x[f],p[w]=f===i?e.y[f]:d[f],m[w]=f});let u=r==="steady"?e.lw[t]>=2?2.25:1.25:l?1.75:1.25;return{branch:e.br[t],type:a,f2:e.f2[t],kind:r,stable:l,which:o,color:e.c[t],width:u,dashed:!l,xs:c,ys:p,idx:m,hopf:i}}function Cp(e,t,n,o=0){let i=e.x.length,r=Math.max(0,Math.min(o,i)),a=n&&n.xmax>n.xmin&&n.ymax>n.ymin?{x:n.xmax-n.xmin,y:n.ymax-n.ymin}:{x:_p(e.x),y:_p(e.y)},l=[],d=[];for(let p=r;p<i;){let m=p;for(;m+1<i&&wy(e,m,m+1);)m++;if(e.d[p]!==0){let u=[],f=-1,w=p-1;Ao(e.d[p])?w>=r&&!e.nw[p]&&e.br[w]===e.br[p]&&Ao(e.d[w])&&!e.f2[w]?u.push(w):e.f2[p]||(f=ky(e,t,p,a),f>=0&&(u.push(f),d.push({point:p,from:f}))):w>=r&&!e.nw[p]&&u.push(w);for(let A=p;A<=m;A++)u.push(A);l.push(Mp(e,p,u,"y",f));let k=!1;if(Ao(e.d[p]))for(let A=p;A<=m&&!k;A++)k=e.y2[A]!==e.y[A];k&&l.push(Mp(e,p,u,"y2",f))}p=m+1}let c=t.filter(p=>p.point>=r).map(p=>({...p,x:e.x[p.point],y:e.y[p.point],y2:e.y2[p.point]!==e.y[p.point]?e.y2[p.point]:null}));return{curves:l,labels:c,hopf:d,xLabel:n?.xlabel??"",yLabel:n?.ylabel??""}}function Dp(e,t,n,o,i=1/0,r=2){let a=t.width/(t.xmax-t.xmin),l=t.height/(t.ymax-t.ymin),d=new Set(e.labels.map(u=>u.point)),c=[],p=i;e.curves.forEach((u,f)=>{for(let w=0;w<u.xs.length;w++){let k=(u.xs[w]-t.xmin)*a-n,A=(t.ymax-u.ys[w])*l-o,C=Math.hypot(k,A);C<=p+r&&(c.push({curve:f,index:w,dist:C}),C<p&&(p=C))}});let m=c.filter(u=>u.dist<=p+r);return m.length?(m.sort((u,f)=>u.dist-f.dist),m.find(u=>d.has(e.curves[u.curve].idx[u.index]))??m[0]):null}function hs(e,t,n,o=-1){let i=a=>{let l=e.curves[a];if(!l||l.which==="y2"!==n)return-1;for(let d=l.idx.length-1;d>=0;d--)if(l.idx[d]===t)return d;return-1};if(o>=0){let a=i(o);if(a>=0)return{curve:o,index:a}}let r=null;return e.curves.forEach((a,l)=>{let d=i(l);d>=0&&(!r||d>0)&&(r={curve:l,index:d})}),r}function ms(e,t,n){if(!e.length)return null;let o=e.map(i=>i.point).sort((i,r)=>i-r);if(n>0)return o.find(i=>i>t)??o[0];for(let i=o.length-1;i>=0;i--)if(t<0||o[i]<t)return o[i];return o[o.length-1]}function Lp(e,t,n,o,i){if(o<=0)return null;switch(e){case"ArrowRight":case"ArrowDown":case"]":return n+1<o?n+1:0;case"ArrowLeft":case"ArrowUp":case"[":return n>0?Math.min(n-1,o-1):0;case"PageDown":return Math.min(Math.max(n,0)+10,o-1);case"PageUp":return Math.max(Math.min(n,o-1)-10,0);case"Home":return 0;case"End":return o-1;case"Tab":return ms(i.filter(r=>r.point<o),n,t?-1:1);default:return null}}var Ty=["","stable steady state","unstable steady state","stable periodic orbit","unstable periodic orbit"],Sy=["","limit point","limit point of periodic orbits","Hopf","torus","branch point","period doubling","fixed period"],Py={EP:"end point",LP:"limit point",HB:"Hopf bifurcation",BP:"branch point",PD:"period doubling",TR:"torus bifurcation",UZ:"user point",MX:"no convergence"};function St(e){return Number.isFinite(e)?Number(e.toPrecision(6)).toString():"NaN"}function Ey(e,t){return e.f2[t]?`${Sy[e.f2[t]]||"two-parameter"} curve`:Ty[e.ty[t]]||""}function ci(e){return Py[e]??""}function Op(e,t,n,o){let i=t.find(c=>c.point===o),r=n?.xlabel||"x",a=n?.ylabel||"y",l=e.y2[o]!==e.y[o],d=[`${r} = ${St(e.x[o])}`,l?`${a} max = ${St(e.y[o])}`:`${a} = ${St(e.y[o])}`];return l&&d.push(`${a} min = ${St(e.y2[o])}`),{head:`Branch ${e.br[o]}, point ${e.pt[o]}`,kind:Ey(e,o),label:i?`${i.sym?i.sym+" ":""}label ${i.lab}${ci(i.sym)?` (${ci(i.sym)})`:""}`:"",values:d}}var Ay=(e,t)=>e.x<t.x+t.w&&t.x<e.x+e.w&&e.y<t.y+t.h&&t.y<e.y+e.h;function Ip(e,t=6){let n=[];return e.map(o=>{let i=o.y;for(let r=0;r<=2*t;r++){let a=r===0?0:(r%2?1:-1)*Math.ceil(r/2),l={...o,y:o.y+a*o.h};if(!n.some(d=>Ay(d,l))){i=l.y;break}}return n.push({...o,y:i}),i})}var _y=40,My=[6,4];function yr(e){return getComputedStyle(document.documentElement).getPropertyValue(e).trim()}function vr(e,t){return Be(e>=20&&e<=29?e-19:0,t)}function $p(e,t){let n=1/0,o=-1/0;for(let r of e.curves){let a=t==="x"?r.xs:r.ys;for(let l=0;l<a.length;l++)a[l]<n&&(n=a[l]),a[l]>o&&(o=a[l])}if(!(n<=o))return{min:0,max:1};if(n===o)return{min:n-1,max:o+1};let i=(o-n)*.05;return{min:n-i,max:o+i}}var wr=class{constructor(t,n){this.root=t;this.cb=n;this.u=null;this.model=null;this.dark=!1;this.applying=!1;this.reportPending=!1;this.base={x:{min:0,max:1},y:{min:0,max:1}};this.draws=0;this.named=0;this.nameTops=[];this.styleKey="";this.onArea=()=>{};this.linePath=(t,n)=>{let o=this.model?.curves[n-1],i=new Path2D;if(o){let r=!1;for(let a=0;a<o.xs.length;a++){let l=o.xs[a],d=o.ys[a];if(!Number.isFinite(l)||!Number.isFinite(d)){r=!1;continue}let c=t.valToPos(l,"x",!0),p=t.valToPos(d,"y",!0);r?i.lineTo(c,p):i.moveTo(c,p),r=!0}}return{stroke:i,fill:null,clip:null,band:null,flags:1}}}set(t,n,o,i){let r=JSON.stringify([i,t.xLabel,t.yLabel,t.curves.map(a=>[a.color,a.width,a.dashed])]);this.model=t,this.dark=i,this.base=n??{x:$p(t,"x"),y:$p(t,"y")},!this.u||r!==this.styleKey?(this.styleKey=r,this.create()):this.u.setData(this.data(),!1),this.applyViewport(o)}data(){let t=this.model.curves;return[null,...t.length?t.map(o=>[o.xs,o.ys]):[[new Float64Array(0),new Float64Array(0)]]]}create(){let t=this.model;this.u?.destroy();let n=()=>yr("--fg-muted")||"#666",o=()=>yr("--grid")||"#eee",i=yr("--plot-font"),r=p=>({label:p,stroke:n,font:i,labelFont:i,grid:{stroke:o,width:1},ticks:{stroke:o,width:1}}),a=[{scale:"x",auto:!1},{scale:"y",auto:!1}],l=t.curves.map(p=>({label:`Branch ${p.branch}`,stroke:vr(p.color,this.dark),width:p.width,dash:p.dashed?My:void 0,paths:this.linePath,points:{show:!1},facets:a}));l.length||l.push({label:"",paths:this.linePath,points:{show:!1},facets:a});let{width:d,height:c}=this.size();this.u=new De({mode:2,width:d,height:c,legend:{show:!1},scales:{x:{time:!1,auto:!1},y:{auto:!1}},axes:[r(t.xLabel),r(t.yLabel)],series:[{},...l],cursor:{drag:{x:!0,y:!0,uni:20,setScale:!0},points:{show:!1},bind:{dblclick:()=>()=>(this.reset(),null)}},hooks:{setScale:[()=>this.scaleChanged()],draw:[p=>this.drawLabels(p)]}},this.data(),this.root),this.onArea(this.u.over)}drawLabels(t){this.draws++;let n=this.model;if(!n)return;let o=t.ctx,i=t.bbox,r=De.pxRatio,a=4*r,l=(u,f)=>u>=i.left&&u<=i.left+i.width&&f>=i.top&&f<=i.top+i.height,d=n.labels.map(u=>({l:u,px:t.valToPos(u.x,"x",!0),py:t.valToPos(u.y,"y",!0),py2:u.y2===null?null:t.valToPos(u.y2,"y",!0)})).filter(u=>l(u.px,u.py)||u.py2!==null&&l(u.px,u.py2)),c=yr("--fg")||"#000";o.save(),o.beginPath(),o.rect(i.left,i.top,i.width,i.height),o.clip(),o.strokeStyle=c,o.fillStyle=c,o.lineWidth=1.25*r,o.font=`${Math.round(11*r)}px Inter, system-ui, sans-serif`,o.textBaseline="top",this.named=d.length<=_y?d.length:0;let p=d.map(u=>`${u.l.sym?u.l.sym+" ":""}${u.l.lab}`),m=this.named?Ip(d.map((u,f)=>({x:u.px+5*r,y:u.py+3*r,w:o.measureText(p[f]).width,h:13*r}))):[];this.nameTops=m.map(u=>u/r),d.forEach((u,f)=>{for(let w of u.py2===null?[u.py]:[u.py,u.py2])o.beginPath(),o.moveTo(u.px-a,w),o.lineTo(u.px+a,w),o.moveTo(u.px,w-a),o.lineTo(u.px,w+a),o.stroke();this.named&&o.fillText(p[f],u.px+5*r,m[f])}),o.restore()}size(){let t=this.root.getBoundingClientRect();return{width:Math.max(120,Math.floor(t.width)),height:Math.max(120,Math.floor(t.height))}}resize(){this.u?.setSize(this.size())}applyViewport(t){let n=this.u;n&&(this.applying=!0,n.batch(()=>{n.setScale("x",t.x??this.base.x),n.setScale("y",t.y??this.base.y)}),this.applying=!1)}setView(t,n){this.cb.onViewport({x:t.x,y:t.y},n)}reset(){this.cb.onViewport({x:null,y:null},!0)}scaleChanged(){!this.u||this.applying||this.reportPending||(this.reportPending=!0,queueMicrotask(()=>{this.reportPending=!1;let t=this.u;if(!t)return;let n=t.scales.x,o=t.scales.y;n.min==null||n.max==null||o.min==null||o.max==null||this.cb.onViewport({x:{min:n.min,max:n.max},y:{min:o.min,max:o.max}},!0)}))}ranges(){let t=this.u;return{x:{min:t.scales.x.min,max:t.scales.x.max},y:{min:t.scales.y.min,max:t.scales.y.max}}}hit(t,n,o){let i=this.u,r=this.model;if(!i||!r)return null;let{x:a,y:l}=this.ranges();return Dp(r,{xmin:a.min,xmax:a.max,ymin:l.min,ymax:l.max,width:i.over.clientWidth,height:i.over.clientHeight},t,n,o)}position(t,n){let o=this.u,i=this.model?.curves[t];if(!o||!i||n<0||n>=i.xs.length)return null;let r=o.over.getBoundingClientRect(),a=this.root.getBoundingClientRect();return{left:r.left-a.left+o.valToPos(i.xs[n],"x"),top:r.top-a.top+o.valToPos(i.ys[n],"y")}}areaBox(){let t=this.u;if(!t)return null;let n=t.over.getBoundingClientRect(),o=this.root.getBoundingClientRect();return{left:n.left-o.left,top:n.top-o.top,width:n.width,height:n.height}}place(t,n){let o=this.u;if(!o||!Number.isFinite(t)||!Number.isFinite(n))return null;let i=o.over.getBoundingClientRect(),r=this.root.getBoundingClientRect();return{left:i.left-r.left+o.valToPos(t,"x"),top:i.top-r.top+o.valToPos(n,"y")}}png(){return this.u?this.u.ctx.canvas.toDataURL("image/png"):null}info(){let t=this.u,n=this.model;return!t||!n?null:{curves:n.curves.map(o=>({branch:o.branch,type:o.type,kind:o.kind,stable:o.stable,which:o.which,points:o.xs.length,dashed:o.dashed,width:o.width,color:vr(o.color,this.dark),hopf:o.hopf,first:[o.xs[0],o.ys[0],o.idx[0]]})),labels:n.labels.map(o=>({point:o.point,lab:o.lab,sym:o.sym,x:o.x,y:o.y,y2:o.y2})),named:this.named,nameTops:this.nameTops,...this.ranges(),width:t.over.clientWidth,height:t.over.clientHeight,draws:this.draws}}destroy(){this.u?.destroy(),this.u=null}},Np=null;function fs(e){Np=e}function Rp(){return Np}var gs=null;function Fp(){return gs}function bs(e,t,n){return Cu(e,t,Mu(n),8)}function Cy(e,t){return typeof e=="string"?e:Be(e,t)}function qp(e,t,n,o,i,r){let a=window.devicePixelRatio||1,l=Math.max(1,Math.round(t*a)),d=Math.max(1,Math.round(n*a));e.width!==l&&(e.width=l),e.height!==d&&(e.height=d);let c=e.getContext("2d");if(!c)return;if(c.setTransform(a,0,0,a,0,0),c.clearRect(0,0,t,n),!o){gs=null;return}let p=bs(t,n,o.dim),m=Lu(p,o.w,o.h);c.fillStyle=r,c.fillRect(p.x,p.y,p.w,p.h),c.save(),c.beginPath(),c.rect(p.x,p.y,p.w,p.h),c.clip(),c.lineCap="round",c.lineJoin="round";for(let u of o.prims){let f=Cy(u.color,i);switch(c.strokeStyle=f,c.fillStyle=f,u.kind){case"line":{let[w,k]=Sn(p,u.u1,u.v1),[A,C]=Sn(p,u.u2,u.v2);c.lineWidth=pr(u.width,m),c.beginPath(),c.moveTo(w,k),c.lineTo(A,C),c.stroke();break}case"rect":{let[w,k]=Sn(p,u.u1,u.v1),[A,C]=Sn(p,u.u2,u.v2),P=Math.min(w,A),$=Math.min(k,C),E=Math.abs(A-w),U=Math.abs(C-k);u.fill?c.fillRect(P,$,E,U):(c.lineWidth=pr(u.width,m),c.strokeRect(P,$,E,U));break}case"circle":case"ellipse":{let[w,k]=Sn(p,u.u,u.v),A=Iu(p,u.ru,u.rv),C=u.kind==="circle"?A:u.ru*p.w,P=u.kind==="circle"?A:u.rv*p.h;c.beginPath(),c.ellipse(w,k,C,P,0,0,2*Math.PI),u.fill?c.fill():(c.lineWidth=pr(u.width,m),c.stroke());break}case"dot":{let[w,k]=Sn(p,u.u,u.v);c.beginPath(),c.arc(w,k,Math.max(.5,u.r*m),0,2*Math.PI),c.fill();break}case"text":{let[w,k]=Sn(p,u.u,u.v);c.font=`${Ou(u.size,m)}px Inter, system-ui, sans-serif`,c.fillText($u(u),w,k);break}}}c.restore(),gs={pos:o.pos,prims:o.prims.length,width:t,height:n,box:p,scale:m,at:performance.now()}}var ys=200;function Dy(){let e=[];try{new PerformanceObserver(t=>{for(let n of t.getEntries())e.push({start:n.startTime,duration:n.duration}),e.length>ys&&e.shift()}).observe({type:"longtask",buffered:!0})}catch{}return e}function Hp(e){let t=[],n=[],o=Dy(),i=[],r=e.store.dispatch;e.store.dispatch=a=>{t.push(a.type==="event"?`event:${a.ev.ev}`:a.type==="viewport"?`viewport${a.push?":push":""}`:a.type),t.length>ys&&t.shift(),a.type==="event"&&a.ev.ev==="diagram"?(a.ev.op==="reset"&&!a.ev.keep&&(i=[]),i.push(a.ev)):a.type==="event"&&a.ev.ev==="window"&&a.ev.win===101&&a.ev.op==="destroy"&&(i=[]),a.type==="sent"&&(n.push(a.cmd),n.length>ys&&n.shift()),r(a)},window.__xpp={state:()=>e.store.getState(),actions:()=>t.slice(),sent:()=>n.slice(),plot:a=>cr(a??e.store.getState().plots.active)?.info()??null,diagram:()=>Rp()?.info()??null,diagramEvents:()=>i.slice(),longTasks:(a=0)=>o.filter(l=>l.start>=a),longTasksSupported:()=>PerformanceObserver.supportedEntryTypes?.includes("longtask")??!1,send:a=>e.send(a),ani:()=>Fp(),kinescopeStop:()=>e.kinescopeStop(),kinescopeGif:()=>{let a=e.kinescopeGifBytes();return a?Ri(a):null}}}var _o,qe,vs,zp,kr=0,Yp=[],We=Ae,Xp=We.__b,Up=We.__r,Vp=We.diffed,Kp=We.__c,Bp=We.unmount,jp=We.__;function Tr(e,t){We.__h&&We.__h(qe,e,kr||t),kr=0;var n=qe.__H||(qe.__H={__:[],__h:[]});return e>=n.__.length&&n.__.push({}),n.__[e]}function ie(e){return kr=1,xs(Jp,e)}function xs(e,t,n){var o=Tr(_o++,2);if(o.t=e,!o.__c&&(o.__=[n?n(t):Jp(void 0,t),function(l){var d=o.__N?o.__N[0]:o.__[0],c=o.t(d,l);d!==c&&(o.__N=[c,o.__[1]],o.__c.setState({}))}],o.__c=qe,!qe.__f)){var i=function(l,d,c){if(!o.__c.__H)return!0;var p=!1,m=o.__c.props!==l;if(o.__c.__H.__.some(function(f){if(f.__N){p=!0;var w=f.__[0];f.__=f.__N,f.__N=void 0,w!==f.__[0]&&(m=!0)}}),r){var u=r.call(this,l,d,c);return p?u||m:u}return!p||m};qe.__f=!0;var r=qe.shouldComponentUpdate,a=qe.componentWillUpdate;qe.componentWillUpdate=function(l,d,c){if(this.__e){var p=r;r=void 0,i(l,d,c),r=p}a&&a.call(this,l,d,c)},qe.shouldComponentUpdate=i}return o.__N||o.__}function G(e,t){var n=Tr(_o++,3);!We.__s&&Zp(n.__H,t)&&(n.__=e,n.u=t,qe.__H.__h.push(n))}function J(e){return kr=5,Ye(function(){return{current:e}},[])}function Ye(e,t){var n=Tr(_o++,7);return Zp(n.__H,t)&&(n.__=e(),n.__H=t,n.__h=e),n.__}function Gp(e){var t=qe.context[e.__c],n=Tr(_o++,9);return n.c=e,t?(n.__==null&&(n.__=!0,t.sub(qe)),t.props.value):e.__}function Ly(){for(var e;e=Yp.shift();){var t=e.__H;if(e.__P&&t)try{t.__h.some(xr),t.__h.some(ws),t.__h=[]}catch(n){t.__h=[],We.__e(n,e.__v)}}}We.__b=function(e){qe=null,Xp&&Xp(e)},We.__=function(e,t){e&&t.__k&&t.__k.__m&&(e.__m=t.__k.__m),jp&&jp(e,t)},We.__r=function(e){Up&&Up(e),_o=0;var t=(qe=e.__c).__H;t&&(vs===qe?(t.__h=[],qe.__h=[],t.__.some(function(n){n.__N&&(n.__=n.__N),n.u=n.__N=void 0})):(t.__h.some(xr),t.__h.some(ws),t.__h=[],_o=0)),vs=qe},We.diffed=function(e){Vp&&Vp(e);var t=e.__c;t&&t.__H&&(t.__H.__h.length&&(Yp.push(t)!==1&&zp===We.requestAnimationFrame||((zp=We.requestAnimationFrame)||Oy)(Ly)),t.__H.__.some(function(n){n.u&&(n.__H=n.u,n.u=void 0)})),vs=qe=null},We.__c=function(e,t){t.some(function(n){try{n.__h.some(xr),n.__h=n.__h.filter(function(o){return!o.__||ws(o)})}catch(o){t.some(function(i){i.__h&&(i.__h=[])}),t=[],We.__e(o,n.__v)}}),Kp&&Kp(e,t)},We.unmount=function(e){Bp&&Bp(e);var t,n=e.__c;n&&n.__H&&(n.__H.__.some(function(o){try{xr(o)}catch(i){t=i}}),n.__H=void 0,t&&We.__e(t,n.__v))};var Wp=typeof requestAnimationFrame=="function";function Oy(e){var t,n=function(){clearTimeout(o),Wp&&cancelAnimationFrame(t),setTimeout(e)},o=setTimeout(n,35);Wp&&(t=requestAnimationFrame(n))}function xr(e){var t=qe,n=e.__c;typeof n=="function"&&(e.__c=void 0,n()),qe=t}function ws(e){var t=qe;e.__c=e.__(),qe=t}function Zp(e,t){return!e||e.length!==t.length||t.some(function(n,o){return n!==e[o]})}function Jp(e,t){return typeof t=="function"?t(e):t}var Sr=[[68,1,84],[70,50,126],[54,92,141],[39,127,142],[31,161,135],[74,193,109],[160,218,57],[253,231,37]];function ks(e,t,n){return e+(t-e)*n}function Iy(e){let t=Qp(e)*(Sr.length-1),n=Math.min(Sr.length-2,Math.floor(t)),o=t-n,i=Sr[n],r=Sr[n+1];return[ks(i[0],r[0],o),ks(i[1],r[1],o),ks(i[2],r[2],o)]}function $y(e){return e>1/3?0:765*Math.sqrt((.333334-e)*(e+.33334))}function Ny(e){return e>.666666?0:765*Math.sqrt((.6666667-e)*e)}function Ry(e){return e<.333334?0:2.79*255*Math.sqrt((1.05-e)*(e-.333333333))}function Fy(e){let t=1-Qp(e);return[$y(t),Ny(t),Ry(t)]}function Qp(e){return e<0?0:e>1?1:e}function qy(e){return Math.max(0,Math.min(255,Math.round(e)))}function Hy(e,t,n){let o=i=>qy(i).toString(16).padStart(2,"0");return`#${o(e)}${o(t)}${o(n)}`}var eh="#8888884d";function th(e,t){if(!Number.isFinite(t))return eh;let[n,o,i]=e==="xpp"?Fy(t):Iy(t);return Hy(n,o,i)}function nh(e,t,n,o){if(!Number.isFinite(t))return eh;let i=o>n?(t-n)/(o-n):.5;return th(e,i)}function oh(e,t=9){return Array.from({length:t},(n,o)=>th(e,1-o/(t-1)))}var Mo="Busy: available when the current run ends",Ts=zl(null);function te(){let e=Gp(Ts);if(!e)throw new Error("no session");return e}function M(e){let t=te(),[,n]=xs(a=>a+1,0),o=e(t.store.getState()),i=J(o),r=J(e);return i.current=o,r.current=e,G(()=>{let a=()=>{Object.is(r.current(t.store.getState()),i.current)||n(0)},l=t.store.subscribe(a);return a(),l},[t]),o}var zy=0;function s(e,t,n,o,i,r){t||(t={});var a,l,d=t;if("ref"in d)for(l in d={},t)l=="ref"?a=t[l]:d[l]=t[l];var c={type:e,props:d,key:n,ref:a,__k:null,__:null,__b:0,__e:null,__c:null,constructor:void 0,__v:--zy,__i:-1,__u:0,__source:i,__self:r};if(typeof e=="function"&&(a=e.defaultProps))for(l in a)d[l]===void 0&&(d[l]=a[l]);return Ae.vnode&&Ae.vnode(c),c}var Xy='button:not([disabled]), select, [tabindex]:not([tabindex="-1"])',Uy=[["Redraw","redraw","Draw again from the data"],["Edit","edit","Columns, rows, skips and the z range"],["Fit","fit","Fit the z range to the data"],["Range","range","Save a GIF for each run of Integrate/Range"],["Print","print","Write a PostScript file"],["GIF","gif","Save the picture as a GIF"]];function An(e){if(!Number.isFinite(e))return"NaN";let t=e.toPrecision(6);return t.includes("e")?t:t.replace(/(\.\d*?)0+$/,"$1").replace(/\.$/,"")}function ih(){let e=te(),t=M(x=>x.aplot.open),n=M(x=>x.aplot.windowOpen),o=M(x=>x.aplot),i=o.event,r=o.colorMap,a=o.hover,l=J(null),d=J(null),c=J(null),p=J(null),m=()=>e.closeAplot();G(()=>{if(!t){l.current?.contains(document.activeElement)&&document.querySelector(".aplot-toggle")?.focus();return}l.current?.querySelector(Xy)?.focus();let x=v=>{v.key!=="Escape"||e.store.getState().ask||(v.preventDefault(),v.stopPropagation(),m())};return window.addEventListener("keydown",x,!0),()=>window.removeEventListener("keydown",x,!0)},[t]),G(()=>{let x=c.current,v=d.current;if(!x||!v)return;let T=new ResizeObserver(()=>{let L=Math.max(1,Math.floor(x.clientWidth)),H=Math.max(1,Math.floor(x.clientHeight));(v.width!==L||v.height!==H)&&(v.width=L,v.height=H)});return T.observe(x),()=>T.disconnect()},[]);let u=i?.nx??0,f=i?.ny??0;G(()=>{let x=d.current,v=x?.getContext("2d");if(!x||!v||(v.clearRect(0,0,x.width,x.height),!u||!f||!i))return;let T=x.width/u,L=x.height/f;for(let H=0;H<f;H++)for(let O=0;O<u;O++)v.fillStyle=nh(r,es(o,H,O),i.zmin,i.zmax),v.fillRect(Math.floor(O*T),Math.floor(H*L),Math.ceil(T)+1,Math.ceil(L)+1)},[o,r,u,f]);let w=(x,v)=>{let T=d.current;if(!T||!u||!f)return null;let L=T.getBoundingClientRect();if(L.width<=0||L.height<=0)return null;let H=Math.floor((x-L.left)/L.width*u),O=Math.floor((v-L.top)/L.height*f);return H<0||H>=u||O<0||O>=f?null:{row:O,col:H}},k=(x,v)=>{let T=w(x,v);e.aplotHover(T&&i?{...T,t:Eu(i,T.row),value:es(o,T.row,T.col)}:null)},A=x=>{x.preventDefault(),e.aplotScroll(jl(x.deltaY))},C=x=>{x.currentTarget.setPointerCapture(x.pointerId),p.current=x.clientY},P=x=>{p.current!==null&&x.buttons&&(e.aplotScroll(Wl(p.current,x.clientY)),p.current=x.clientY),k(x.clientX,x.clientY)},$=()=>{p.current=null},E=()=>{$(),e.aplotHover(null)},U=x=>{e.aplotKeyScroll(x.key)&&(x.preventDefault(),x.stopPropagation())},R=oh(r),N=i&&a?Au(i,a.col):"";return s("section",{id:"aplot-panel",ref:l,class:"aplot-panel"+(t?" open":""),"aria-label":"Array plot",children:[s("div",{class:"aplot-header",children:[s("button",{class:"aplot-back",onClick:m,children:"Back"}),s("h2",{children:"Array plot"}),s("label",{class:"aplot-map",children:["Colour map",s("select",{value:r,onChange:x=>e.setAplotColorMap(x.target.value),children:[s("option",{value:"viridis",children:"Viridis"}),s("option",{value:"xpp",children:"XPP"})]})]})]}),s("div",{class:"aplot-tools",children:Uy.map(([x,v,T])=>s("button",{title:T,onClick:()=>e.aplotOp(v),children:x},v))}),s("p",{class:"aplot-info",role:"status",children:n?!u||!f||!i?"Nothing to show: use Edit, or integrate first.":`${An(i.tlo)} < t < ${An(i.thi)}. ${u} columns, ${f} rows.`:"No array plot yet: use the Window/zoom menu (Axes, Array) to define one."}),s("div",{class:"aplot-body",children:[s("div",{class:"aplot-scale","aria-hidden":"true",children:[s("span",{children:i?An(i.zmax):""}),s("div",{class:"aplot-bar",style:{backgroundImage:`linear-gradient(to top, ${R.join(",")})`}}),s("span",{children:i?An(i.zmin):""})]}),s("div",{class:"aplot-grid-wrap",ref:c,tabIndex:0,role:"img","aria-label":i&&u&&f?`Array plot, ${u} columns by ${f} rows, coloured from ${An(i.zmin)} to ${An(i.zmax)}`:"Array plot, nothing to show",onWheel:A,onKeyDown:U,onPointerDown:C,onPointerMove:P,onPointerUp:$,onPointerCancel:$,onPointerLeave:E,children:s("canvas",{ref:d,class:"aplot-canvas"})})]}),s("p",{class:"aplot-hover",role:"status",children:a?`${N||`Column ${a.col}`}, row ${a.row}: t ${An(a.t)}, value ${An(a.value)}.`:""})]})}var Co="04-using-the-interface",Ue={page:{chapter:Co,anchor:"the-page-layout"},valuesPanel:{chapter:Co,anchor:"the-values-panel"},plotsAndAxes:{chapter:Co,anchor:"plots-and-axes"},files:{chapter:Co,anchor:"saving-pictures-and-files"},log:{chapter:Co,anchor:"the-log"},formulas:{chapter:Co,anchor:"formulas-as-values"},dataTab:{chapter:"07-data-browser"},mainMenu:{chapter:"05-commands"},fileMenu:{chapter:"05-commands",anchor:"file"},numericsMenu:{chapter:"06-numerical-parameters"},animation:{chapter:"10-animations",anchor:"the-animation-view"},autoView:{chapter:"09-auto",anchor:"the-auto-view"},autoAxes:{chapter:"09-auto",anchor:"diagram-axes"},autoNumerics:{chapter:"09-auto",anchor:"numerical-parameters"},autoPars:{chapter:"09-auto",anchor:"choosing-parameters"},autoMarks:{chapter:"09-auto",anchor:"user-functions"},autoSaving:{chapter:"09-auto",anchor:"saving-diagrams"}};function Ss(e){return e===1?Ue.fileMenu:e===2?Ue.numericsMenu:Ue.mainMenu}function rh(e,t){return t==="file"?Ue.files:e.table.open?Ue.dataTab:Ss(e.core?.menu??0)}var Vy=/^(\d\d-[a-z0-9-]+)\.md(?:#([\w-]+))?$|^#([\w-]+)$/i;function ah(e,t){let n=Vy.exec(e);return n?n[3]!==void 0?{chapter:t,anchor:n[3]}:{chapter:n[1],anchor:n[2]}:null}function st({target:e,label:t}){let n=te(),o=t?`Help: ${t}`:"Help for this";return s("button",{type:"button",class:"help-link icon-button","aria-label":o,title:o,onClick:()=>n.store.dispatch({type:"help",action:{type:"open",target:e}}),children:"?"})}var Ps="xppTheme",sh=()=>window.matchMedia("(prefers-color-scheme: dark)");function lh(){try{let e=localStorage.getItem(Ps);return e==="light"||e==="dark"?e:"system"}catch{return"system"}}function ch(e){try{e==="system"?localStorage.removeItem(Ps):localStorage.setItem(Ps,e)}catch{}}function Pr(e){let[t,n]=ie(()=>sh().matches);G(()=>{let i=sh(),r=()=>n(i.matches);return i.addEventListener("change",r),()=>i.removeEventListener("change",r)},[]);let o=e==="dark"||e==="system"&&t;return G(()=>{document.documentElement.dataset.theme=o?"dark":"light"},[o]),o}var Es=[0,5,10,20,50,100,200,500,1e3];function dh(){let e=te(),t=M(_=>_.ani.open),n=M(_=>_.ani.exists),o=M(_=>_.ani.loaded),i=M(_=>_.ani.playing),r=M(_=>_.ani.frame),a=M(_=>_.ani.rows),l=M(_=>_.core?.rows??0),d=M(_=>_.ani.speed),c=M(_=>_.ani.grab),p=M(_=>_.busy),m=M(_=>_.theme),u=Pr(m),f=Math.max(a,l),w=Math.max(0,f-1),k=r?.pos??0,A=J(null),C=J(null),P=J(null),[$,E]=ie({w:0,h:0}),[U,R]=ie(null),N=()=>e.closeAni();G(()=>{if(!t){A.current?.contains(document.activeElement)&&document.querySelector(".ani-toggle")?.focus();return}C.current?.focus();let _=j=>{j.key!=="Escape"||e.store.getState().ask||A.current?.contains(document.activeElement)&&(j.preventDefault(),j.stopPropagation(),N())};return window.addEventListener("keydown",_,!0),()=>window.removeEventListener("keydown",_,!0)},[t]),G(()=>{let _=C.current;if(!_)return;let j=new ResizeObserver(()=>E({w:_.clientWidth,h:_.clientHeight}));return j.observe(_),E({w:_.clientWidth,h:_.clientHeight}),()=>j.disconnect()},[]),G(()=>{if(!t||!P.current||!$.w||!$.h)return;let _=getComputedStyle(document.documentElement).getPropertyValue("--surface").trim()||"#fff";qp(P.current,$.w,$.h,r,u,_)},[t,r,$.w,$.h,u]);let x=()=>i?e.aniPause():e.aniPlay(),v=o&&f>=2,T=_=>{if(_.ctrlKey||_.metaKey||_.altKey||!v)return;let j=_.shiftKey?10:1,B={" ":x,ArrowLeft:()=>e.aniStep(-j),ArrowDown:()=>e.aniStep(-j),ArrowRight:()=>e.aniStep(j),ArrowUp:()=>e.aniStep(j),PageUp:()=>e.aniStep(-10),PageDown:()=>e.aniStep(10),Home:()=>e.aniSeek(0),End:()=>e.aniSeek(w)}[_.key];B&&(_.preventDefault(),_.stopPropagation(),B())},L=J(0),H=(_,j)=>{if(!c||!r||!P.current)return;let W=P.current.getBoundingClientRect(),[B,re]=Du(bs(W.width,W.height,r.dim),j.clientX-W.left,j.clientY-W.top);if(_==="move"){if(!(j.buttons&1)||j.timeStamp-L.current<50)return;L.current=j.timeStamp}_==="down"&&j.target.setPointerCapture?.(j.pointerId),e.aniPointer(_,B,re)},O=Es.includes(d)?Es:[...Es,d].sort((_,j)=>_-j),X=n?o?f<2?"No data yet: integrate first. An animation plays the stored rows.":r?`Frame ${k} of ${f}`+(r.t===null?"":`, t = ${je(r.t)}`)+(i?". Playing.":"")+(c?". Grab: drag a point on the picture.":""):"Loaded. Play, step or seek to draw a frame.":"No animation loaded: Load an .ani file.":"The animation window is closed.";return s("section",{id:"ani-panel",ref:A,class:"ani-panel"+(t?" open":""),"aria-label":"Animation",children:[s("div",{class:"ani-header",children:[s("button",{class:"ani-back",onClick:N,children:"Back"}),s("h2",{children:"Animation"}),s(st,{target:Ue.animation,label:"the animation"}),!n&&s("button",{onClick:()=>e.openAni(),disabled:p,children:"Open"}),s("button",{onClick:()=>e.aniLoad(),disabled:p||!n,title:"Load an animation (.ani) file",children:"Load\u2026"})]}),s("div",{class:"ani-stage"+(c?" grabbing":""),ref:C,tabIndex:0,role:"application","aria-roledescription":"animation","aria-label":`Animation. ${X}`,"aria-describedby":"ani-keys",onKeyDown:T,onPointerDown:_=>H("down",_),onPointerMove:_=>H("move",_),onPointerUp:_=>H("up",_),children:s("canvas",{ref:P,class:"ani-canvas","aria-hidden":"true"})}),s("p",{id:"ani-keys",class:"visually-hidden",children:"Space plays or pauses; the arrow keys step a frame, with Shift ten; Home and End go to the first and last frame."}),s("p",{class:"ani-info",role:"status",children:X}),s("div",{class:"ani-seek",children:s("input",{type:"range",class:"ani-slider",min:0,max:w,step:1,value:U??k,disabled:!v,"aria-label":"Frame","aria-valuetext":`Frame ${U??k} of ${f}`,onInput:_=>R(Number(_.target.value)),onChange:_=>{R(null),e.aniSeek(Number(_.target.value))}})}),s("div",{class:"ani-controls",role:"group","aria-label":"Player",children:[s("button",{onClick:()=>e.aniSeek(0),disabled:!v,title:"First frame (Home)","aria-label":"First frame",children:"\u23EE"}),s("button",{onClick:()=>e.aniStep(-1),disabled:!v,title:"One frame back (Left arrow)","aria-label":"One frame back",children:"\u25C0"}),s("button",{class:"primary ani-play",onClick:x,disabled:!v||p&&!i,title:"Play or pause (Space)","aria-pressed":i,children:i?"Pause":"Play"}),s("button",{onClick:()=>e.aniStep(1),disabled:!v,title:"One frame forward (Right arrow)","aria-label":"One frame forward",children:"\u25B6"}),s("button",{onClick:()=>e.aniSeek(w),disabled:!v,title:"Last frame (End)","aria-label":"Last frame",children:"\u23ED"}),s("label",{class:"ani-speed",children:[s("span",{children:"Delay"}),s("select",{value:String(d),disabled:!o,onChange:_=>e.aniSpeed(Number(_.target.value)),children:O.map(_=>s("option",{value:String(_),children:[_," ms"]},_))})]}),s("button",{onClick:()=>e.aniGrab(),disabled:!v||p,"aria-pressed":c,title:"Drag the animation's grab points with the pointer",children:"Grab"})]})]})}function ph(e){let t=/^\*(\d)(.*)$/s.exec(e);return t?{label:t[2],list:Number(t[1])}:{label:e,list:null}}function Ky(e){let t=/^(-?\d+)\s+\S/.exec(e);return{value:t?t[1]:e,label:e}}var uh=e=>e.trim()!==""&&Number.isFinite(Number(e));function hh(e,t){let n=e.map(Ky),o=t.trim(),i=n.find(r=>r.value===o)??n.find(r=>r.value.toLowerCase()===o.toLowerCase())??(uh(o)?n.find(r=>uh(r.value)&&Number(r.value)===Number(o)):void 0);return i?{options:n,selected:i.value}:{options:[{value:t,label:o===""?"(none)":t},...n],selected:t}}function mh(e){e.key!=="Enter"||e.target.tagName!=="INPUT"||(e.preventDefault(),e.currentTarget.requestSubmit())}function By(){let e=te(),t=M(i=>i.files.confirm),n=J(null);return G(()=>n.current?.querySelector("[data-choice=keep]")?.focus(),[t.name]),s("div",{class:"file-confirm",role:"group","aria-labelledby":"file-confirm-text","data-confirm":"replace",ref:n,onKeyDown:i=>{i.key==="Escape"&&(i.preventDefault(),i.stopPropagation(),e.resolveReplace("cancel"))},children:[s("p",{id:"file-confirm-text",children:["The model's folder already has a ",s("b",{children:t.name})," with other content. Replace it, or keep both and copy yours as ",s("b",{children:t.keepBoth}),"?"]}),s("div",{class:"dialog-actions",children:[s("button",{type:"button","data-choice":"cancel",onClick:()=>e.resolveReplace("cancel"),children:"Cancel"}),s("button",{type:"button","data-choice":"keep",onClick:()=>e.resolveReplace("keep"),children:"Keep both"}),s("button",{type:"button","data-choice":"replace",class:"danger",onClick:()=>e.resolveReplace("replace"),children:"Replace"})]})]})}function jy({ask:e}){let t=te(),n=M(d=>d.files.confirm),o=J(null),[i,r]=ie(!1),a=d=>{d.length&&(r(!0),t.openFiles(e,d).finally(()=>r(!1)))},l=async()=>{if(!Ii()){o.current.value="",o.current.click();return}try{let d=await $i(!0);d&&a(d)}catch{o.current.click()}};return s(ve,{children:[s("p",{children:["Pick the file to open",e.wild&&e.wild!=="*"?s(ve,{children:[" (",e.wild,")"]}):null,". XPP reads it from the model's folder, so it is copied there first. Pick the files it refers to at the same time (tables, included files) to copy them too."]}),s("input",{ref:o,type:"file",multiple:!0,class:"visually-hidden",tabIndex:-1,"aria-hidden":"true","data-file-input":"open",onChange:d=>{let c=d.target,p=[...c.files??[]];c.value="",a(p)}}),n&&n.ask===e.id?s(By,{}):s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:()=>t.cancel(e),children:"Cancel"}),s("button",{type:"button",class:"primary","data-autofocus":"",disabled:i,onClick:()=>{l()},children:i?"Copying\u2026":"Choose file\u2026"})]})]})}function Wy({ask:e}){let t=te(),[n,o]=ie(dr(e.file??"")),i=Kn(n),r=Ul(),a=async l=>{if(l.preventDefault(),!i)return;if(!r){t.saveFile(e,n,null);return}let d=await Kl(n).catch(()=>null);d&&t.saveFile(e,Kn(d.name)?d.name:n,d)};return s("form",{onSubmit:l=>{a(l)},onKeyDown:mh,children:[s("p",{children:["XPP writes the file into the model's folder,"," ",r?"then it is copied to where you choose.":"then your browser downloads it."]}),s("div",{class:"form-grid",children:s("label",{children:[s("span",{children:"File name"}),s("input",{value:n,"data-autofocus":"","data-file-name":"","aria-invalid":!i,"aria-describedby":i?void 0:"file-name-error",onInput:l=>o(l.target.value)})]})}),!i&&s("p",{id:"file-name-error",class:"field-error",children:'A name only: no folders, no leading dot, none of \\ / : * ? " < > |.'}),s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:()=>t.cancel(e),children:"Cancel"}),s("button",{type:"submit",class:"primary",disabled:!i,children:r?"Save\u2026":"Save"})]})]})}function Yy({ask:e}){let t=te(),[n,o]=ie(e.file??""),[i,r]=ie(e.wild??"*"),a=(e.dirs??[]).filter(l=>l!=="."&&l!=="..");return s("form",{onKeyDown:mh,onSubmit:l=>{l.preventDefault(),t.answer(e,{file:n})},children:[s("p",{class:"muted file-dir",children:e.dir}),s("div",{class:"form-grid",children:[s("label",{children:[s("span",{children:"File"}),s("input",{value:n,"data-folder-file":"",onInput:l=>o(l.target.value)})]}),s("label",{children:[s("span",{children:"Show"}),s("input",{value:i,title:"Which files to list; Enter lists again",onInput:l=>r(l.target.value),onKeyDown:l=>{l.key==="Enter"&&(l.preventDefault(),l.stopPropagation(),t.answer(e,{wild:i}))}})]})]}),s("ul",{class:"file-list","aria-label":"Files and folders",children:[s("li",{children:s("button",{type:"button",class:"file-entry dir",onClick:()=>t.answer(e,{cd:".."}),children:"../"})}),a.map(l=>s("li",{children:s("button",{type:"button",class:"file-entry dir",onClick:()=>t.answer(e,{cd:l}),children:[l,"/"]})},`d${l}`)),(e.files??[]).map(l=>s("li",{children:s("button",{type:"button",class:"file-entry"+(l===n?" selected":""),"aria-pressed":l===n,onClick:()=>o(l),onDblClick:()=>t.answer(e,{file:l}),children:l})},`f${l}`))]}),s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:()=>t.cancel(e),children:"Cancel"}),s("button",{type:"submit",class:"primary",children:e.mode==="write"?"Save":"Open"})]})]})}function fh({ask:e}){let[t,n]=ie("computer"),o=[["computer","This computer"],["folder","In the model's folder"]],i=r=>{if(r.key!=="ArrowLeft"&&r.key!=="ArrowRight")return;r.preventDefault();let a=t==="computer"?"folder":"computer";n(a),document.getElementById(`file-tab-${a}`)?.focus()};return s("div",{class:"file-ask","data-mode":e.mode??"read",children:[s("div",{class:"file-tabs",role:"tablist","aria-label":"Where the file is",onKeyDown:i,children:o.map(([r,a])=>s("button",{id:`file-tab-${r}`,type:"button",role:"tab",class:"plot-tab","aria-selected":t===r,"aria-controls":`file-panel-${r}`,tabIndex:t===r?0:-1,onClick:()=>n(r),children:a},r))}),s("div",{id:`file-panel-${t}`,role:"tabpanel","aria-labelledby":`file-tab-${t}`,children:t==="folder"?s(Yy,{ask:e}):e.mode==="write"?s(Wy,{ask:e}):s(jy,{ask:e})})]})}function gh(e){let t=Math.ceil(e/8);return Math.max(1,Math.ceil(e/Math.max(1,t)))}var bh='button:not([disabled]), input, select, textarea, [tabindex]:not([tabindex="-1"])';function Zy({ask:e}){let t=te(),n=e.keys??"";G(()=>{let i=r=>{if(r.ctrlKey||r.metaKey||r.altKey||r.key.length!==1)return;let a=n.toLowerCase().indexOf(r.key.toLowerCase());a<0||(r.preventDefault(),r.stopPropagation(),t.answer(e,{key:n[a]}))};return window.addEventListener("keydown",i,!0),()=>window.removeEventListener("keydown",i,!0)},[e]);let o=e.kind==="menu"?e.items??[]:e.choices??[];return s(ve,{children:[e.question&&s("p",{children:e.question}),s("ul",{class:"menu-list"+(o.length>8?" menu-columns":""),role:"menu","aria-label":e.title||e.name||"Choices",style:{"--menu-rows":String(gh(o.length))},children:o.map((i,r)=>s("li",{role:"none",children:s("button",{role:"menuitem",class:"menu-item",title:e.hints?.[r],"aria-keyshortcuts":n[r],onClick:()=>t.answer(e,{key:n[r]}),children:[s("kbd",{"aria-hidden":"true",children:n[r]?.toUpperCase()}),s("span",{children:i})]})},r))}),s("div",{class:"dialog-actions",children:s("button",{onClick:()=>t.cancel(e),children:"Cancel"})})]})}function Jy({ask:e}){let t=te(),n=M(c=>c.hello?.lists),o=e.kind==="string",i=o?[e.name??""]:e.names??[],[r,a]=ie(o?[e.value??""]:[...e.values??[]]);return s("form",{onSubmit:c=>{c.preventDefault(),t.answer(e,o?{ok:1,value:r[0]}:{ok:1,values:r})},onKeyDown:c=>{let p=c.target.tagName;c.key==="Enter"&&(p==="SELECT"||p==="INPUT")&&(c.preventDefault(),c.currentTarget.requestSubmit())},children:[s("div",{class:"form-grid",children:i.map((c,p)=>{let m=o?{label:c,list:null}:ph(c),u=m.list!==null?n?.[m.list]:void 0,f=w=>{let k=r.slice();k[p]=w.target.value,a(k)};if(u){let{options:w,selected:k}=hh(u,r[p]??"");return s("label",{children:[s("span",{children:m.label}),s("select",{value:k,"data-list":m.list,"data-autofocus":p===0?"":void 0,onChange:f,children:w.map(A=>s("option",{value:A.value,children:A.label},A.value))})]},p)}return s("label",{children:[s("span",{children:m.label}),s("input",{value:r[p],"data-autofocus":p===0?"":void 0,onInput:f})]},p)})}),s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:()=>t.cancel(e),children:e.cancel||"Cancel"}),s("button",{type:"submit",class:"primary",children:e.ok||"OK"})]})]})}function Qy({ask:e}){let t=te(),n=e.names??[],[o,i]=ie(n.map((l,d)=>e.flags?.[d]?1:0)),r=l=>i(n.map(()=>l));return s("form",{onSubmit:l=>{l.preventDefault(),t.answer(e,{ok:1,flags:o})},children:[s("fieldset",{class:"checklist",children:[s("legend",{class:"visually-hidden",children:e.title||"Choose"}),n.map((l,d)=>s("label",{children:[s("input",{type:"checkbox",checked:!!o[d],"data-autofocus":d===0?"":void 0,onChange:c=>{let p=o.slice();p[d]=c.target.checked?1:0,i(p)}}),s("span",{children:l})]},d))]}),s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:()=>r(1),children:"All"}),s("button",{type:"button",onClick:()=>r(0),children:"None"}),s("button",{type:"button",onClick:()=>t.cancel(e),children:"Cancel"}),s("button",{type:"submit",class:"primary",children:"OK"})]})]})}var ev={grab:"a point of the AUTO diagram",mouse:"a click in a window this interface does not show yet",rubber:"a box in a window this interface does not show yet",drag:"a drag in a window this interface does not show yet"};function tv({ask:e}){let t=te();return s(ve,{children:[s("p",{children:["XPP asks for ",ev[e.kind]??s("b",{children:e.kind}),", which this interface does not offer yet. Cancel it here."]}),s("div",{class:"dialog-actions",children:s("button",{class:"primary",onClick:()=>t.cancel(e),children:"Cancel"})})]})}function nv({ask:e,children:t}){let n=te(),o=M(d=>d.table),i=M(d=>d.core),r=J(null);G(()=>{let d=document.activeElement,c=r.current,p=c.querySelector("[data-autofocus]")??c.querySelector(bh);return p?.focus(),p instanceof HTMLInputElement&&p.select(),()=>d?.focus?.()},[e.id]);let a=d=>{if(d.key==="Escape")d.preventDefault(),d.stopPropagation(),n.cancel(e);else if(d.key==="Tab"){let c=[...r.current.querySelectorAll(bh)];if(!c.length)return;let p=c.indexOf(document.activeElement),m=d.shiftKey?p<=0?c.length-1:p-1:p===c.length-1?0:p+1;d.preventDefault();let u=c[m];u.focus(),(u instanceof HTMLInputElement&&/^(text|search|number|url|tel|email|password)?$/.test(u.getAttribute("type")??"")||u instanceof HTMLTextAreaElement)&&u.select()}},l=e.title||e.name||"XPP";return s("div",{class:"dialog-backdrop",children:s("div",{class:"dialog",ref:r,role:"dialog","aria-modal":"true","aria-labelledby":"ask-title","data-ask":e.kind,onKeyDown:a,children:[s("div",{class:"dialog-title-row",children:[s("h2",{id:"ask-title",children:l}),s(st,{target:rh({table:o,core:i},e.kind),label:l})]}),t]})})}function yh(){let e=M(i=>i.ask),t=M(i=>i.pick),n=M(i=>i.diagram.open);if(!e||e.kind==="pixels"||e.kind==="alert"||t&&t.ask===e.id||e.kind==="grab"&&n)return null;let o=e.kind==="menu"||e.kind==="choice"?s(Zy,{ask:e}):e.kind==="string"||e.kind==="form"?s(Jy,{ask:e}):e.kind==="checklist"?s(Qy,{ask:e}):e.kind==="file"?s(fh,{ask:e}):s(tv,{ask:e});return s(nv,{ask:e,children:o},e.id)}function Do(e,t,n,o){let i=e.x.min+(e.x.max-e.x.min)*t,r=e.y.max-(e.y.max-e.y.min)*n;return{x:{min:i-(i-e.x.min)*o,max:i+(e.x.max-i)*o},y:{min:r-(r-e.y.min)*o,max:r+(e.y.max-r)*o}}}function hn(e,t,n){let o=e.x.max-e.x.min,i=e.y.max-e.y.min;return{x:{min:e.x.min-t*o,max:e.x.max-t*o},y:{min:e.y.min+n*i,max:e.y.max+n*i}}}var vh=.85,ov=400,iv=24,rv=32,av=8;function Er(e,t,n,o){let i=x=>{let v=t.getBoundingClientRect();return{x:x.clientX-v.left,y:x.clientY-v.top,w:v.width,h:v.height}},r=x=>{let v=i(x),T=L=>Math.max(0,Math.min(1,L));return{fx:T(v.x/v.w),fy:T(v.y/v.h)}},a=null,l=x=>{!o?.mode()||a!==null||x.pointerType==="mouse"&&(x.button!==0||x.shiftKey)||(x.preventDefault(),x.stopImmediatePropagation(),a=x.pointerId,t.setPointerCapture?.(x.pointerId),o.press(r(x)))},d=x=>{o&&(a===x.pointerId?(x.stopImmediatePropagation(),o.drag(r(x))):a===null&&x.pointerType!=="touch"&&!x.buttons&&o.mode()&&o.hover(r(x)))},c=x=>{!o||a!==x.pointerId||(x.stopImmediatePropagation(),a=null,x.type==="pointerup"&&o.release(r(x)))},p=-1/0,m=x=>{x.preventDefault();let v=i(x),T=performance.now();e.setView(Do(e.ranges(),v.x/v.w,v.y/v.h,x.deltaY<0?vh:1/vh),T-p>ov),p=T},u=x=>{if(!(x.button===1||x.button===0&&x.shiftKey))return;x.preventDefault(),x.stopImmediatePropagation();let v=e.ranges(),T=i(x),L=!0,H=X=>{let _=i(X);e.setView(hn(v,(_.x-T.x)/_.w,(_.y-T.y)/_.h),L),L=!1},O=()=>{window.removeEventListener("mousemove",H),window.removeEventListener("mouseup",O)};window.addEventListener("mousemove",H),window.addEventListener("mouseup",O)},f=x=>{if(x.pointerType==="touch"||x.buttons)return;let v=i(x),T=e.hit(v.x,v.y,iv);T?n.hover(T.curve,T.index):n.leave()},w=x=>{x.pointerType!=="touch"&&n.leave()},k=new Map,A=null,C=[],P=!1,$=!1,E=()=>{A=e.ranges(),C=[...k.values()].map(x=>({...x}))},U=x=>{x.pointerType==="touch"&&(x.preventDefault(),t.setPointerCapture?.(x.pointerId),k.set(x.pointerId,i(x)),k.size===1&&(P=!1,$=!1),E())},R=x=>{if(x.pointerType!=="touch"||!k.has(x.pointerId)||!A)return;let v=i(x);k.set(x.pointerId,v);let T=[...k.values()];if(T.length===1){let L=T[0].x-C[0].x,H=T[0].y-C[0].y;if(!P&&Math.hypot(L,H)<av)return;P=!0,e.setView(hn(A,L/v.w,H/v.h),!$)}else if(T.length>=2&&C.length>=2){P=!0;let L=Math.hypot(C[0].x-C[1].x,C[0].y-C[1].y),H=Math.hypot(T[0].x-T[1].x,T[0].y-T[1].y);if(L<1||H<1)return;let O={x:(C[0].x+C[1].x)/2,y:(C[0].y+C[1].y)/2},X={x:(T[0].x+T[1].x)/2,y:(T[0].y+T[1].y)/2},_=Do(A,O.x/v.w,O.y/v.h,L/H);e.setView(hn(_,(X.x-O.x)/v.w,(X.y-O.y)/v.h),!$)}$=!0},N=x=>{if(x.pointerType!=="touch"||!k.has(x.pointerId))return;let v=k.get(x.pointerId);if(k.delete(x.pointerId),k.size===0&&!P){let T=e.hit(v.x,v.y,rv);T?n.hover(T.curve,T.index):n.leave()}k.size&&E()};return t.addEventListener("pointerdown",l,{capture:!0}),t.addEventListener("pointermove",d,{capture:!0}),t.addEventListener("pointerup",c,{capture:!0}),t.addEventListener("pointercancel",c,{capture:!0}),t.addEventListener("wheel",m,{passive:!1}),t.addEventListener("mousedown",u,{capture:!0}),t.addEventListener("pointermove",f),t.addEventListener("pointerleave",w),t.addEventListener("pointerdown",U),t.addEventListener("pointermove",R),t.addEventListener("pointerup",N),t.addEventListener("pointercancel",N),()=>{t.removeEventListener("pointerdown",l,{capture:!0}),t.removeEventListener("pointermove",d,{capture:!0}),t.removeEventListener("pointerup",c,{capture:!0}),t.removeEventListener("pointercancel",c,{capture:!0}),t.removeEventListener("wheel",m),t.removeEventListener("mousedown",u,{capture:!0}),t.removeEventListener("pointermove",f),t.removeEventListener("pointerleave",w),t.removeEventListener("pointerdown",U),t.removeEventListener("pointermove",R),t.removeEventListener("pointerup",N),t.removeEventListener("pointercancel",N)}}var kh="Arrow keys pan, plus and minus zoom, 0 resets, Control Z undoes a zoom, square brackets and Page Up or Down step through the points, Home and End go to the ends, braces change the curve, Escape clears the readout.",Ar=.1,wh=.8;function xh(e,t,n){for(let o=1;o<=e.length;o++){let i=(t+n*o+e.length*2)%e.length;if(e[i]>0)return i}return-1}function _r(e,t){let n=t.ranges;switch(e){case"ArrowLeft":return{view:hn(n,Ar,0)};case"ArrowRight":return{view:hn(n,-Ar,0)};case"ArrowUp":return{view:hn(n,0,Ar)};case"ArrowDown":return{view:hn(n,0,-Ar)};case"+":case"=":return{view:Do(n,.5,.5,wh)};case"-":case"_":return{view:Do(n,.5,.5,1/wh)};case"0":return{reset:!0};case"Undo":return{undo:!0};case"Escape":return t.hover?{hover:null}:null}let o=t.hover?null:xh(t.counts,-1,1),i=t.hover?t.hover.curve:o;if(i<0||!(t.counts[i]>0))return null;let r=t.counts[i]-1,a=t.hover?.index??-1,l=d=>Math.max(0,Math.min(r,d));switch(e){case"]":return{hover:{curve:i,index:l(a+1)}};case"[":return{hover:{curve:i,index:l(a<0?0:a-1)}};case"PageDown":return{hover:{curve:i,index:l(a+10)}};case"PageUp":return{hover:{curve:i,index:l(a<0?0:a-10)}};case"}":case"{":{let d=t.hover?xh(t.counts,i,e==="}"?1:-1):i;return d<0?null:{hover:{curve:d,index:Math.min(Math.max(a,0),t.counts[d]-1)}}}case"Home":return{hover:{curve:i,index:0}};case"End":return{hover:{curve:i,index:r}}}return null}var Th=[{plot:0,text:"Maximum (Hi)"},{plot:2,text:"Maximum and minimum (hI-lo)"},{plot:1,text:"Norm"},{plot:11,text:"Average"},{plot:3,text:"Period"},{plot:10,text:"Frequency"},{plot:4,text:"Two parameters"}];function As(e){return e===0||e===2||e===11?"var":e===4?"par2":null}function Sh(e){if(!e)return{par1:"",yvar:"",par2:""};let t=As(e.plot),n=e.plot===11?e.ylabel.replace(/_bar$/,""):e.ylabel;return{par1:e.xlabel,yvar:t==="var"?n:"",par2:t==="par2"?n:""}}function Ph(e){return!(e>0)||!Number.isFinite(e)?1:10**(Math.floor(Math.log10(e))-1)}function _s(e,t){if(!e.trim()||!t.trim())return null;let n=Number(e),o=Number(t);return Number.isFinite(n)&&Number.isFinite(o)&&n<o?{min:n,max:o}:null}function di(e){for(let t=6;t<=17;t++){let n=String(Number(e.toPrecision(t)));if(Number(n)===e)return n}return String(e)}function Eh({axis:e,onClose:t}){let n=te(),o=M(B=>B.diagram.axes),i=M(B=>B.diagram.viewport),r=M(B=>B.busy),a=M(B=>B.autoSettings),l=Eo(a),d=En(a),p=(M(B=>B.core?.ics)??[]).map(([B])=>B),m=l?l.pars.filter(B=>!!B):[],u=J(null),f=J(!1),w=()=>i[e]??(o?e==="x"?{min:o.xmin,max:o.xmax}:{min:o.ymin,max:o.ymax}:null),[k,A]=ie(()=>w()?di(w().min):""),[C,P]=ie(()=>w()?di(w().max):""),$=J({min:k,max:C}),E=B=>{$.current.min=B,A(B)},U=B=>{$.current.max=B,P(B)};G(()=>{let B=w();!B||u.current?.contains(document.activeElement)&&document.activeElement?.tagName==="INPUT"||(E(di(B.min)),U(di(B.max)))},[i,o]),G(()=>{u.current?.querySelector("select:not([disabled]), input")?.focus()},[]);let R=(B,re)=>{let se=_s(B,re);if(!se)return;let le={...n.store.getState().diagram.viewport,[e]:se};!le.x&&o&&(le.x={min:o.xmin,max:o.xmax}),!le.y&&o&&(le.y={min:o.ymin,max:o.ymax}),n.store.dispatch({type:"diagram",action:{type:"viewport",viewport:le,push:!f.current}}),f.current=!0},N=_s(k,C),x=Ph((N??w()??{min:0,max:10}).max-(N??w()??{min:0,max:10}).min),v=l?{par1:l.axes.par1??"",yvar:l.axes.var??"",par2:l.axes.par2??""}:Sh(o),T=l?.axes.plot??o?.plot??0,L=As(T),H=B=>{l&&n.autoSettings({axes:{...B,fit:!0}})},O=["plot","var","par1","par2"].some(B=>d.has(`axes.${B}`)),X=(B,re,se,le,q,V)=>s("label",{class:d.has(`axes.${V}`)?"queued":void 0,children:[s("span",{children:B}),s("select",{value:re,"data-field":q,"data-queued":d.has(`axes.${V}`)?"1":void 0,"aria-describedby":O?"auto-axis-pending":void 0,onChange:ce=>le(ce.target.value),children:[!se.some(ce=>ce.value===re)&&s("option",{value:re,children:re||"(as set)"}),se.map(ce=>s("option",{value:ce.value,children:ce.text},ce.value))]})]}),_=B=>B.map(re=>({value:re,text:re})),j=B=>{B.key==="Escape"&&(B.preventDefault(),B.stopPropagation(),t())},W=e==="x"?"Horizontal axis":"Vertical axis";return s("div",{class:"auto-axis-dialog",ref:u,role:"dialog","aria-modal":"false","aria-label":W,"data-axis":e,onKeyDown:j,children:[s("div",{class:"dialog-title-row",children:[s("h3",{children:W}),s(st,{target:Ue.autoAxes,label:W})]}),s("div",{class:"auto-axis-fields",children:[e==="x"?X("Parameter",v.par1,_(m),B=>H({par1:B}),"par1","par1"):X("Plots",String(T),Th.map(B=>({value:String(B.plot),text:B.text})),B=>H({plot:Number(B)}),"plot","plot"),e==="y"&&L==="var"&&X("Variable",v.yvar,_(p),B=>H({var:B}),"yvar","var"),e==="y"&&L==="par2"&&X("Second parameter",v.par2,_(m),B=>H({par2:B}),"par2","par2"),s("label",{children:[s("span",{children:"Minimum"}),s("input",{type:"number",step:x,value:k,"data-field":"min","aria-invalid":!N,onInput:B=>{let re=B.target.value;E(re),R(re,$.current.max)}})]}),s("label",{children:[s("span",{children:"Maximum"}),s("input",{type:"number",step:x,value:C,"data-field":"max","aria-invalid":!N,onInput:B=>{let re=B.target.value;U(re),R($.current.min,re)}})]})]}),O&&s("p",{id:"auto-axis-pending",class:"muted",role:"status",children:r?"What the axis plots changes when AUTO stops (dashed); the range changes now.":"Applying\u2026"}),!N&&s("p",{class:"error-text",role:"alert",children:"The minimum must be a number below the maximum."}),s("div",{class:"dialog-actions",children:s("button",{onClick:t,children:"Close"})})]})}var sv={numerics:Ue.autoNumerics,pars:Ue.autoPars,marks:Ue.autoMarks},Ah='button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';function Ms(){let e=M(t=>t.autoSettings);return Eo(e)}function Cs({title:e,id:t,kind:n,onClose:o,children:i}){let r=J(null);G(()=>{let l=document.activeElement,d=r.current?.querySelector("[data-autofocus]")??r.current?.querySelector(Ah);return d?.focus(),d instanceof HTMLInputElement&&d.select(),()=>l?.focus?.()},[]);let a=l=>{if(l.key==="Escape")l.preventDefault(),l.stopPropagation(),o();else if(l.key==="Tab"){let d=[...r.current.querySelectorAll(Ah)];if(!d.length)return;let c=d.indexOf(document.activeElement),p=l.shiftKey?c<=0?d.length-1:c-1:c===d.length-1?0:c+1;l.preventDefault();let m=d[p];m.focus(),m instanceof HTMLInputElement&&m.select()}else l.stopPropagation()};return s("div",{class:"dialog-backdrop",children:s("div",{class:"dialog auto-settings-dialog",ref:r,role:"dialog","aria-modal":"true","aria-labelledby":`${t}-title`,"data-settings":t,onKeyDown:a,children:[s("div",{class:"dialog-title-row",children:[s("h2",{id:`${t}-title`,children:e}),s(st,{target:sv[n],label:e})]}),i]})})}function Ds({pending:e}){let t=M(o=>o.busy),n=M(o=>o.autoSettings.error);return s(ve,{children:[t&&s("p",{class:"auto-settings-note muted",role:"status",children:["AUTO is busy: changes apply when the current run ends",e?" (the dashed ones wait already)":"","."]}),!t&&e&&s("p",{class:"auto-settings-note muted",role:"status",children:"Dashed fields are being applied."}),n&&s("p",{class:"field-error",role:"alert",children:n})]})}function Ls({onClose:e,ok:t,disabled:n}){return s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:e,children:"Cancel"}),s("button",{type:"submit",class:"primary",disabled:n,onClick:o=>{o.preventDefault(),t()},children:"OK"})]})}var Os=e=>t=>{let n=t.target.tagName;t.key==="Enter"&&(n==="INPUT"||n==="SELECT")&&(t.preventDefault(),e())};function lv({onClose:e}){let t=te(),n=Ms(),o=En(M(u=>u.autoSettings)),[i,r]=ie(()=>Object.fromEntries(sn.map(u=>[u.key,n?String(n.numerics[u.key]):""])));if(!n)return null;let a=Object.fromEntries(sn.map(u=>[u.key,hp(u.key,i[u.key]??"")])),l=sn.some(u=>a[u.key]),d=Object.fromEntries(sn.map(u=>[u.key,Number(i[u.key])])),c=l?null:mp(d),p=()=>{if(l||c)return;let u=sn.filter(f=>d[f.key]!==n.numerics[f.key]);u.length&&t.autoSettings({numerics:Object.fromEntries(u.map(f=>[f.key,d[f.key]]))}),e()},m=sn.some(u=>o.has(`numerics.${u.key}`));return s(Cs,{title:"AUTO Numerics",id:"auto-numerics",kind:"numerics",onClose:e,children:[s("form",{class:"auto-num-groups",onKeyDown:Os(p),onSubmit:u=>{u.preventDefault(),p()},children:cs.map(u=>s("fieldset",{class:"form-grid auto-num-group",children:[s("legend",{children:u.title}),u.keys.map((f,w)=>{let k=ds(f),A=a[f],C=o.has(`numerics.${f}`);return s("label",{class:C?"queued":void 0,title:C?"Sent when the running command ends":k.hint,children:[s("span",{children:k.label}),s("input",{type:"text",inputMode:k.integer?"numeric":"decimal",value:i[f],"data-field":f,"data-queued":C?"1":void 0,"aria-invalid":A?"true":void 0,"aria-describedby":A?`auto-num-${f}-err`:void 0,"data-autofocus":u===cs[0]&&w===0?"":void 0,onInput:P=>r($=>({...$,[f]:P.target.value}))}),A&&s("p",{class:"field-error",id:`auto-num-${f}-err`,children:A})]},f)})]},u.title))}),c&&s("p",{class:"field-error",role:"alert",children:c}),s(Ds,{pending:m}),s(Ls,{onClose:e,ok:p,disabled:l||!!c})]})}function cv({onClose:e}){let t=te(),n=Ms(),o=En(M(d=>d.autoSettings)).has("pars"),i=M(d=>d.core?.pars)??[],[r,a]=ie(()=>(n?.pars??[]).map(d=>d??""));if(!n)return null;let l=()=>{r.some((d,c)=>d!==(n.pars[c]??""))&&t.autoSettings({pars:r}),e()};return s(Cs,{title:"AUTO's parameters",id:"auto-pars",kind:"pars",onClose:e,children:[s("p",{class:"muted auto-settings-note",children:"The parameters AUTO can continue in; the axes and Mark values name them."}),s("form",{class:"form-grid auto-pars",onKeyDown:Os(l),onSubmit:d=>{d.preventDefault(),l()},children:r.map((d,c)=>s("label",{class:o?"queued":void 0,children:[s("span",{children:["Par",c+1]}),s("select",{value:d,"data-field":`par${c+1}`,"data-autofocus":c===0?"":void 0,onChange:p=>{let m=r.slice();m[c]=p.target.value,a(m)},children:[!i.some(([p])=>p===d)&&s("option",{value:d,children:d||"(none)"}),i.map(([p])=>s("option",{value:p,children:p},p))]})]},c))}),s(Ds,{pending:o}),s(Ls,{onClose:e,ok:l})]})}var dv=9;function uv({onClose:e}){let t=te(),n=Ms(),o=En(M(p=>p.autoSettings)).has("marks"),[i,r]=ie(()=>(n?.marks??[]).map(([p,m])=>[p,String(m)]));if(!n)return null;let a=[...n.pars.filter(p=>!!p),"T"],l=i.map(([,p])=>!p.trim()||!Number.isFinite(Number(p))),d=()=>{if(l.some(m=>m))return;let p=i.map(([m,u])=>[m,Number(u)]);JSON.stringify(p)!==JSON.stringify(n.marks)&&t.autoSettings({marks:p}),e()},c=(p,m,u)=>r(f=>f.map((w,k)=>k===p?m?[w[0],u]:[u,w[1]]:w));return s(Cs,{title:"Mark values",id:"auto-marks",kind:"marks",onClose:e,children:[s("p",{class:"muted auto-settings-note",children:"AUTO labels (UZ) the points where a parameter, or the period T, reaches one of these values."}),s("form",{class:"auto-marks",onKeyDown:Os(d),onSubmit:p=>{p.preventDefault(),d()},children:[i.length===0&&s("p",{class:"muted",children:"No Mark values."}),i.map(([p,m],u)=>s("div",{class:"auto-mark-row"+(o?" queued":""),children:[s("select",{value:p,"aria-label":`Mark ${u+1}: parameter`,"data-field":`mark${u+1}-name`,"data-autofocus":u===0?"":void 0,onChange:f=>c(u,0,f.target.value),children:[!a.includes(p)&&s("option",{value:p,children:p}),a.map(f=>s("option",{value:f,children:f==="T"?"T (period)":f},f))]}),s("span",{"aria-hidden":"true",children:"="}),s("input",{type:"text",inputMode:"decimal",value:m,"aria-label":`Mark ${u+1}: value`,"data-field":`mark${u+1}-value`,"aria-invalid":l[u]?"true":void 0,onInput:f=>c(u,1,f.target.value)}),s("button",{type:"button",class:"small","aria-label":`Remove mark ${u+1}`,onClick:()=>r(f=>f.filter((w,k)=>k!==u)),children:"Remove"})]},u)),s("button",{type:"button",class:"auto-mark-add",disabled:i.length>=dv,"data-autofocus":i.length===0?"":void 0,onClick:()=>r(p=>[...p,[a[0]??"T","0"]]),children:"Add a value"})]}),l.some(p=>p)&&s("p",{class:"field-error",role:"alert",children:"Each value must be a number."}),s(Ds,{pending:o}),s(Ls,{onClose:e,ok:d,disabled:l.some(p=>p)})]})}function _h({kind:e,onClose:t}){return e==="numerics"?s(lv,{onClose:t}):e==="pars"?s(cv,{onClose:t}):s(uv,{onClose:t})}var pv=["","Stable steady state","Unstable steady state","Stable periodic orbit","Unstable periodic orbit"];function Lh(e){let t=o=>o===null?"NaN":St(o),n=[["Branch",String(e.br)],["Point",String(e.pt)],["Type",pv[e.type]??""]];(e.sym||e.lab)&&n.push(["Label",`${e.sym?e.sym+" ":""}${e.lab}${ci(e.sym)?` (${ci(e.sym)})`:""}`]);for(let o of e.par)n.push([o.name,t(o.value)]);return n.push(["Norm",t(e.norm)],[e.var,t(e.u)]),(e.type===3||e.type===4)&&n.push(["Period",t(e.per)]),n}function Mh(e,t){return e===null||t===null?"none (below the smallest number)":t===0?St(e):`${St(e)} ${t<0?"\u2212":"+"} ${St(Math.abs(t))}i`}var Ch=1.95,Dh=e=>Math.max(-Ch,Math.min(Ch,e));function Is(e){return e.circle.map(([t,n],o)=>{let i=t??0,r=n??0,a=e.eig?.[o];return{x:Dh(i),y:Dh(r),inside:Math.hypot(i,r)<=1,text:e.periodic?Mh(t,n):Mh(a?a[0]:null,a?a[1]:null)}})}function Oh(e){let t=Is(e),n=t.filter(r=>r.inside).length,o=e.periodic?"Floquet multipliers":"eigenvalues",i=e.periodic?"inside the unit circle":"with a negative real part (inside the circle)";return`${t.length} ${o}, ${n} ${i}`}var _n=2;function hv(){let e=M(o=>o.diagram.stab);if(!e||!e.circle.length)return null;let t=Is(e),n=Oh(e);return s("figure",{class:"auto-stab",children:[s("svg",{viewBox:`${-_n} ${-_n} ${2*_n} ${2*_n}`,role:"img","aria-label":`Stability circle: ${n}`,children:[s("line",{class:"auto-stab-axis",x1:-_n,y1:0,x2:_n,y2:0}),s("line",{class:"auto-stab-axis",x1:0,y1:-_n,x2:0,y2:_n}),s("circle",{class:"auto-stab-unit",cx:0,cy:0,r:1}),t.map((o,i)=>o.inside?s("circle",{class:"auto-stab-in",cx:o.x,cy:-o.y,r:.13},i):s("path",{class:"auto-stab-out",d:`M${o.x-.13},${-o.y-.13}L${o.x+.13},${-o.y+.13}M${o.x-.13},${-o.y+.13}L${o.x+.13},${-o.y-.13}`},i))]}),s("figcaption",{children:[s("span",{class:"muted",children:e.periodic?"Multipliers":"Eigenvalues"}),s("ul",{class:"auto-stab-list",children:t.map((o,i)=>s("li",{children:[o.inside?"\u25CF":"\xD7"," ",o.text]},i))})]})]})}function Ih(){let e=M(n=>n.diagram.info),t=M(n=>n.diagram.stab);return!e&&!t?null:s("aside",{class:"auto-info","aria-label":"Point information",children:[e?s("dl",{class:"auto-info-rows",children:Lh(e).map(([n,o])=>s("div",{children:[s("dt",{children:n}),s("dd",{children:o})]},n))}):s("p",{class:"muted auto-info-rows",children:"Grab shows a point's branch, label and values here."}),s(hv,{})]})}function mv(e,t){return t?"two-parameter curve":e===3||e===4?"periodic orbits":e===1||e===2?"steady states":""}function $h(e){let t=Math.max(0,e)/1e3;if(t<10)return`${t.toFixed(1)} s`;if(t<60)return`${Math.floor(t)} s`;let n=Math.floor(t/60);return`${n}:${String(Math.floor(t-60*n)).padStart(2,"0")}`}function Nh(e,t,n,o,i){if(!e)return{phase:"idle",text:"Idle",kind:"",branch:null,point:null,points:0,label:null,elapsed:null};let r=t.x.length,a=Math.min(e.first,r),l=r-1,d=r-a,c=d>0?mv(t.ty[l],t.f2[l]):"",p=[...n].reverse().find(k=>k.point>=a&&k.point<r),m=p?`${p.sym||"Label"} ${p.lab} at point ${t.pt[p.point]}`:null,u=(e.ended??o)-e.started,f={kind:c,branch:d>0?t.br[l]:null,point:d>0?t.pt[l]:null,points:d,label:m,elapsed:u};if(e.active)return i.stopping?{...f,phase:"stopping",text:"Stopping\u2026"}:i.asking&&d===0?{...f,phase:"starting",text:"Waiting for how to start",elapsed:null}:{...f,phase:"running",text:`Running: ${c||"starting"}`};let w=c?`: ${c}`:"";return e.stopped?{...f,phase:"stopped",text:`Stopped${w}`}:{...f,phase:"done",text:`Done${w}`}}function Rh(){let e=te(),t=M(m=>m.diagram.run),n=M(m=>m.diagram.points),o=M(m=>m.diagram.labels),i=M(m=>!!m.ask),r=M(m=>m.stopping),a=M(m=>m.busy),[l,d]=ie(()=>Date.now()),c=!!t?.active;G(()=>{if(!c)return;d(Date.now());let m=setInterval(()=>d(Date.now()),250);return()=>clearInterval(m)},[c]);let p=Nh(t,n,o,l,{asking:i,stopping:r});return s("div",{class:"auto-status","data-phase":p.phase,children:[s("span",{class:`status-dot ${p.phase==="running"||p.phase==="stopping"?"busy":"up"}`,"aria-hidden":"true"}),s("span",{class:"auto-status-text",role:"status","data-testid":"auto-status",children:[s("b",{children:p.text}),p.branch!==null&&s("span",{children:[" \xB7 branch ",p.branch,", point ",p.point]}),t&&s("span",{children:[" \xB7 ",p.points," point",p.points===1?"":"s"]}),p.label&&s("span",{children:[" \xB7 last label ",p.label]}),p.elapsed!==null&&s("span",{children:[" \xB7 ",$h(p.elapsed)]})]}),a&&s("button",{class:"small danger auto-stop",disabled:r,onClick:()=>e.abort(),title:"Stop the running command (AUTO keeps the points computed so far)",children:r?"Stopping\u2026":"Stop"})]})}function Fh(){let t=M(r=>r.log).filter(r=>r.kind==="auto"),n=J(null),[o,i]=ie(!1);return G(()=>{let r=n.current;o&&r&&(r.scrollTop=r.scrollHeight)},[t.length,o]),s("details",{class:"auto-output",open:o,onToggle:r=>i(r.target.open),children:[s("summary",{children:["Output (",t.length,")"]}),t.length?s("pre",{ref:n,class:"auto-output-text",tabIndex:0,"aria-label":"AUTO's output",children:t.map(r=>r.text.replace(/\n$/,"")).join(`
`)}):s("p",{class:"muted",children:"AUTO has printed nothing yet: its table appears here while it runs."})]})}function fv(e,t,n){let o=e??(t&&t.win===n?t:null);return!o||o.three||!(o.xlo<o.xhi&&o.ylo<o.yhi)?null:{x:{min:o.xlo,max:o.xhi},y:{min:o.ylo,max:o.yhi}}}function $s(e){return Number.isFinite(e)?Number(e.toPrecision(6)).toString():String(e)}function qh(e,t,n,o){let i=t?.curves[n];if(!t||!i||o<0||o>=i.xs.length)return;let r=i.row0+o,a=e.store.getState().hover;a&&a.curve===n&&a.row===r||e.store.dispatch({type:"hover",hover:{curve:n,row:r,x:i.xs[o],y:i.ys[o],t:t.t?t.t[r]:null}})}function Hh(e){e.store.getState().hover&&e.store.dispatch({type:"hover",hover:null})}function Lo(e,t){let n=e.store.getState().pick;return n&&!n.waiting&&n.win===t?n:null}function Ns(e,t,n){let o=(r,a)=>{let l=n();if(!l)return;let d=Tn(l.ranges(),a);e.dragEvent(r,d.x,d.y)},i=r=>{let a=n();a&&e.confirmPick(r,a.ranges())};return{mode:()=>Lo(e,t)?.mode??null,press(r){let a=Lo(e,t);a&&(a.mode==="drag"&&o("down",r),e.movePick({...a,cursor:r,anchor:a.mode==="box"||a.mode==="line"?r:null}))},drag(r){let a=Lo(e,t);a&&(a.mode==="drag"?o("move",r):e.movePick({...a,cursor:r}))},release(r){let a=Lo(e,t);if(!a)return;if(a.mode==="drag"){o("up",r);return}let l={...a,cursor:r,anchor:a.mode==="point"?null:a.anchor??r};e.movePick(l),i(l)},hover(r){let a=Lo(e,t);a&&a.mode!=="drag"&&e.movePick({...a,cursor:r})}}}function Rs({pick:e,chart:t}){let n=t.areaBox();if(!n||e.mode==="drag")return null;let o=e.cursor.fx*n.width,i=e.cursor.fy*n.height,r=e.anchor?e.anchor.fx*n.width:o,a=e.anchor?e.anchor.fy*n.height:i;return s("svg",{class:"pick-overlay","aria-hidden":"true",width:n.width,height:n.height,style:{left:`${n.left}px`,top:`${n.top}px`},children:[s("line",{x1:o,y1:0,x2:o,y2:n.height}),s("line",{x1:0,y1:i,x2:n.width,y2:i}),e.anchor&&e.mode==="box"&&s("rect",{class:"pick-shape",x:Math.min(r,o),y:Math.min(a,i),width:Math.abs(o-r),height:Math.abs(i-a)}),e.anchor&&e.mode==="line"&&s("line",{class:"pick-shape",x1:r,y1:a,x2:o,y2:i}),e.anchor&&s("circle",{class:"pick-corner",cx:r,cy:a,r:4})]})}function Fs({pick:e}){let t=te(),n=M(i=>i.box);G(()=>{let i=r=>{r.key!=="Escape"||r.defaultPrevented||(r.preventDefault(),r.stopPropagation(),t.cancelPick())};return window.addEventListener("keydown",i,!0),()=>window.removeEventListener("keydown",i,!0)},[t]);let o=typeof matchMedia=="function"&&matchMedia("(pointer: coarse)").matches;return s("div",{class:"pick-bar","data-pick":e.mode,children:[s("span",{id:"pick-instruction",role:"status",children:[n&&e.mode!=="drag"&&s("b",{children:[n.replace(/[\s.:]+$/,""),". "]}),Wd(e,o),s("span",{class:"visually-hidden",children:e.anchor?" First corner set.":""})]}),s("button",{onClick:()=>t.cancelPick(),children:e.mode==="drag"?"Done":"Cancel"})]})}function zh({win:e,dark:t,shown:n,tabbed:o}){let i=te(),r=M(q=>et(q.plots,e)),a=r?.series??null,l=r?.info??null,d=r?.nullclines??null,c=r?.dfield??null,p=r?.marks??null,m=Ye(()=>[...or(d,c),...ir(p)],[d,c,p]),u=r?.viewport??xo,f=!!r?.viewportHistory.length,w=M(q=>q.core?.view),k=M(q=>n?q.hover:null),A=M(q=>q.busy),C=M(q=>q.pick?.win===e?q.pick:null),P=C&&!C.waiting?C:null,$=J(null),E=J(null),[,U]=ie(0),R=r?.history,N=!!R?.erased,x=r?.showRuns??!0,v=Ye(()=>a?Vo(a,N):null,[a,N]),T=Ye(()=>(R?.runs??[]).map(q=>Vo(q)),[R?.runs]),L=J(v);L.current=v;let H=Ye(()=>fv(l,w,e),[l,w,e]);G(()=>{let q=new vo($.current,{onViewport:(Ne,fe)=>i.store.dispatch({type:"viewport",viewport:Ne,push:fe,win:e})}),V=()=>{};q.onArea=Ne=>{V(),V=Er(q,Ne,{hover:(fe,pe)=>qh(i,L.current,fe,pe),leave:()=>Hh(i)},Ns(i,e,()=>E.current))},E.current=q,wo(e,q);let ce=new ResizeObserver(()=>{$.current?.clientWidth&&q.resize()});return ce.observe($.current),()=>{ce.disconnect(),V(),wo(e,null),q.destroy()}},[i,e]),G(()=>{let q=et(i.store.getState().plots,e);v&&n&&E.current.set(v,H,q?.viewport??xo,t)},[v,H,t,n]),G(()=>{n&&E.current.setPhase(d,c)},[d,c,n]),G(()=>{n&&E.current.setMarks(p)},[p,n]),G(()=>{n&&E.current.setRuns(T,x)},[T,x,n]),G(()=>{n&&E.current.applyViewport(u)},[u]),G(()=>{P&&$.current?.focus({preventScroll:!0})},[P?.ask]);let O=q=>{let V=E.current,ce=L.current,Ne=Lo(i,e);if(V&&Ne&&!q.ctrlKey&&!q.metaKey&&!q.altKey){let me=lr(Ne,q.key,q.shiftKey);if(me){if(q.preventDefault(),q.stopPropagation(),"pick"in me)i.movePick(me.pick);else if("confirm"in me)i.confirmPick(me.confirm,V.ranges());else if("drag"in me){let lt=V.ranges();for(let wt of me.drag){let ct=Tn(lt,wt.at);i.dragEvent(wt.what,ct.x,ct.y)}}else i.cancelPick();return}}if(!V||!ce||!ce.curves.length)return;let fe=i.store.getState().hover,pe=fe?ce.curves[fe.curve]:null,pt=(q.ctrlKey||q.metaKey)&&q.key.toLowerCase()==="z"?"Undo":q.ctrlKey||q.metaKey||q.altKey?"":q.key,Re=_r(pt,{ranges:V.ranges(),hover:fe&&pe?{curve:fe.curve,index:fe.row-pe.row0}:null,counts:ce.curves.map((me,lt)=>V.isVisible(lt)?me.xs.length:0)});Re&&(q.preventDefault(),q.stopPropagation(),"view"in Re?V.setView(Re.view,!0):"reset"in Re?V.reset():"undo"in Re?i.store.dispatch({type:"undoViewport",win:e}):Re.hover?qh(i,ce,Re.hover.curve,Re.hover.index):Hh(i))},X=k&&v&&E.current?E.current.position(k.curve,k.row-(v.curves[k.curve]?.row0??0)):null,_=u.x!==null||u.y!==null,j=!v||v.curves.every(q=>q.xs.length===0),W=j&&!m.length&&!(T.length&&x),B=p?.text.length?`; text: ${p.text.map(q=>q.plain).join("; ")}`:"",re=m.length?`; ${m.map(q=>q.label).join(", ")}${B}`:"",se=v?.curves.length?`Plot of ${v.curves.map(q=>q.label).join(", ")}, ${v.curves[0].xs.length} points${re}`:`Plot, no data yet${re}`,le=o?{role:"tabpanel",id:`plot-panel-${e}`,"aria-labelledby":`plot-tab-${e}`}:{"aria-label":"Plot"};return s("section",{class:"plot-view",hidden:!n,...le,children:[s("header",{class:"plot-bar",children:[s("div",{class:"legend",role:"group","aria-label":"Curves",children:[v?.curves.map((q,V)=>s("button",{class:"legend-item"+(E.current?.isVisible(V)===!1?" off":""),"aria-pressed":E.current?.isVisible(V)!==!1,title:"Show or hide this curve",onClick:()=>{E.current.setVisible(V,!E.current.isVisible(V)),U(ce=>ce+1)},children:[s("span",{class:"swatch",style:{background:Be(q.color,t)},"aria-hidden":"true"}),q.label]},V)),T.length>0&&s("button",{class:"legend-item layer runs"+(x?"":" off"),"data-layer":"runs","aria-pressed":x,title:"Show or hide the earlier runs (Erase clears them)",onClick:()=>i.store.dispatch({type:"showRuns",win:e,show:!x}),children:[s("span",{class:"swatch swatch-runs","aria-hidden":"true"}),"previous runs (",T.length,")"]}),m.map(q=>s("button",{class:"legend-item layer"+(E.current?.isLayerVisible(q.key)===!1?" off":""),"data-layer":q.key,"aria-pressed":E.current?.isLayerVisible(q.key)!==!1,title:`Show or hide the ${q.label}`,onClick:()=>{E.current.setLayerVisible(q.key,!E.current.isLayerVisible(q.key)),U(V=>V+1)},children:[s("span",{class:`swatch swatch-${q.key.replace(/-\d+$/,"")}`,style:{background:Be(q.color,t)},"aria-hidden":"true"}),q.label]},q.key))]}),s("div",{class:"plot-tools",children:[s("button",{disabled:!f,onClick:()=>i.store.dispatch({type:"undoViewport",win:e}),title:"Undo the last zoom or pan (Ctrl+Z on the plot)",children:"Undo zoom"}),s("button",{disabled:!_,onClick:()=>E.current.reset(),title:"Back to the window's axes (double click, or 0 on the plot)",children:"Reset view"}),s("button",{disabled:!_,onClick:()=>i.useThisView(e,E.current.ranges()),title:"Make this zoom the window's own axes (Window/Window), for PostScript/SVG export and Restore",children:"Use this view"}),s("button",{disabled:j,onClick:()=>i.fitView(),title:"Fit the window's axes to the data (Window/Fit)",children:"Fit"}),s("button",{disabled:W,onClick:()=>{let q=E.current.png();q&&Bt("xpp-plot.png",q)},title:"Save the plot as a PNG picture",children:"PNG"}),s("button",{disabled:j,onClick:()=>v&&Xl("xpp-curves.csv",v),title:"Save the plotted numbers as CSV",children:"CSV"})]})]}),P&&s(Fs,{pick:P}),s("div",{class:"plot-host"+(P?` picking pick-${P.mode}`:""),ref:$,tabIndex:0,role:"application","aria-roledescription":"plot","aria-label":se,"aria-describedby":P?"pick-instruction plot-keys-help":"plot-keys-help",onKeyDown:O,children:[P&&E.current&&s(Rs,{pick:P,chart:E.current}),X&&s("span",{class:"hover-dot",style:{left:`${X.left}px`,top:`${X.top}px`}}),W&&s("div",{class:"plot-empty",children:[s("p",{children:A?"Integrating\u2026":N?"Erased: Redraw (R) draws the data again.":"No trajectory yet."}),!A&&!N&&s("button",{class:"primary",onClick:()=>i.keys("i","g"),children:"Integrate (I, G)"})]})]}),s("footer",{class:"readout",role:"status","aria-live":"polite",children:k&&v?s("span",{children:[s("b",{children:v.curves[k.curve]?.label})," ","row ",k.row,k.t!==null&&s(ve,{children:[" \xB7 T = ",$s(k.t)]})," ","\xB7 ",v.curves[k.curve]?.xName," = ",$s(k.x)," \xB7 ",v.curves[k.curve]?.yName," = ",$s(k.y)]}):s("span",{class:"muted",children:[s("span",{class:"hint-mouse",children:"Drag to zoom \xB7 wheel zooms \xB7 Shift+drag pans \xB7 double click resets"}),s("span",{class:"hint-touch",children:"Pinch zooms \xB7 drag pans \xB7 tap a point to read it"})]})})]})}var Uh=[["Parameter","param","p"],["Axes","axes","a"],["Numerics","numerics","n"],["Run","run","r"],["Grab","grab","g"],["Mark values\u2026","usr","u"],["Clear","clear","c"],["File","file","f"]],gv={clear:"Hide the branches computed so far: new runs draw alone (the key shows them again)",param:"The parameters AUTO can continue in",numerics:"AUTO's numerical settings: mesh, steps, limits, tolerances",usr:"Label the points where a parameter or the period reaches a value (AUTO's user points, UZ)"},Xh={param:"pars",numerics:"numerics",usr:"marks"},bv={pars:e=>e==="pars",numerics:e=>e.startsWith("numerics."),marks:e=>e==="marks"},qs=new Set(["clear","param","numerics","usr"]),yv=Object.fromEntries(Uh.map(([,e,t])=>[t,e])),vv=["param","axes","numerics","run","grab","usr","clear","redraw","file"],wv="Arrow keys pan, plus and minus zoom, 0 resets, Control Z undoes a zoom, square brackets and Page Up or Down step through the points of a branch, braces change the branch, less than and greater than go from label to label, Escape clears the readout. The letters of the buttons run them.";function ui(e,t){e.store.dispatch({type:"diagram",action:{type:"hover",hover:t}})}var Hs=101,xv=48;function kv(e){let t=e.store.getState().pick;return t&&!t.waiting&&t.win===Hs?t:null}function Tv(e,t,n){let o=Ns(e,Hs,t),i=()=>e.store.getState().diagram.grabbing;return{mode:()=>o.mode()??(i()?"point":null),press(r){o.mode()&&o.press(r)},drag(r){o.mode()&&o.drag(r)},release(r){if(o.mode()){o.release(r);return}let a=t(),l=a?.areaBox();if(!a||!l||!i())return;let d=a.hit(r.fx*l.width,r.fy*l.height,xv),c=d?n().curves[d.curve]?.idx[d.index]:void 0;c!==void 0&&e.grabPoint(c,!0)},hover(r){o.mode()&&o.hover(r)}}}var Sv="Arrow keys or square brackets step to the next or previous point, Page Up and Down ten points, Home and End the first and the last, Tab and Shift+Tab the next and previous labelled point. Enter takes it, Escape cancels.";function Pv(){let e=te(),t=M(o=>o.ask?.kind!=="grab");G(()=>{let o=i=>{i.key!=="Escape"||i.defaultPrevented||(i.preventDefault(),i.stopPropagation(),e.cancelPick())};return window.addEventListener("keydown",o,!0),()=>window.removeEventListener("keydown",o,!0)},[e]);let n=typeof matchMedia=="function"&&matchMedia("(pointer: coarse)").matches;return s("div",{class:"pick-bar","data-pick":"grab",children:[s("span",{id:"grab-instruction",role:"status",children:[s("b",{children:"Grab a point. "}),n?"Tap a point to take it.":"Click a point to take it, or step with the arrow keys and Tab, then press Enter."]}),s("button",{disabled:t,onClick:()=>e.grabTake(),children:"Take"}),s("button",{disabled:t,onClick:()=>e.cancelPick(),children:"Cancel"})]})}function Ev(e){let t=new Map;for(let n of e.curves){let o=n.kind==="two-parameter"?"Two-parameter curve":`${n.stable?"Stable":"Unstable"} ${n.kind==="periodic"?"periodic orbits (max, min)":"steady states"}`,i=`${o}/${n.color}`;t.has(i)||t.set(i,{text:o,color:n.color,dashed:n.dashed})}return[...t.values()]}function Av({dark:e}){let t=te(),n=M(I=>I.diagram.points),o=M(I=>I.diagram.labels),i=M(I=>I.diagram.axes),r=M(I=>I.diagram.viewport),a=M(I=>I.diagram.viewportHistory.length>0),l=M(I=>I.diagram.hover),d=M(I=>I.busy),c=M(I=>I.hello?.auto_hints),p=M(I=>I.diagram.grabbing),m=M(I=>I.diagram.info),u=M(I=>I.diagram.axes?.plot===4?I.diagram.stored:null),f=M(I=>I.pick?.win===Hs&&!I.pick.waiting?I.pick:null),w=M(I=>Ku(I.diagram)),k=M(I=>I.diagram.showEarlier),[A,C]=ie(null),[P,$]=ie(null),E=En(M(I=>I.autoSettings)),U=I=>{let oe=Xh[I];oe?$(oe):t.autoOp(I)},[,R]=ie(0),N=J(null),x=J(null),v=J(null),T=J(null),L=J(-1),H=k?0:w,O=Ye(()=>Cp(n,o,i,H),[n,o,i,H]),X=J(O);X.current=O;let _=Ye(()=>i&&i.xmax>i.xmin&&i.ymax>i.ymin?{x:{min:i.xmin,max:i.xmax},y:{min:i.ymin,max:i.ymax}}:null,[i]),j=(I,oe)=>{let be=X.current.curves[I];if(!be||oe<0||oe>=be.idx.length)return;let he=be.idx[oe],He=t.store.getState().diagram.points;L.current=I,ui(t,{point:he,low:be.which==="y2"&&He.y2[he]!==He.y[he]})};G(()=>{let I=new wr(v.current,{onViewport:(he,He)=>t.store.dispatch({type:"diagram",action:{type:"viewport",viewport:he,push:He}})}),oe=()=>{};I.onArea=he=>{oe();let He=Er(I,he,{hover:j,leave:()=>ui(t,null)},Tv(t,()=>T.current,()=>X.current)),ht=null,Se=Ge=>{ht=Ge.button===0&&!Ge.shiftKey?{x:Ge.clientX,y:Ge.clientY}:null},xt=Ge=>{let Pt=t.store.getState(),de=ht;if(ht=null,!de||Math.hypot(Ge.clientX-de.x,Ge.clientY-de.y)>4||Pt.diagram.axes?.plot!==4||Pt.diagram.grabbing||Pt.pick||Pt.ask||Pt.busy)return;let Y=he.getBoundingClientRect(),Ee=Tn(I.ranges(),{fx:(Ge.clientX-Y.left)/Y.width,fy:(Ge.clientY-Y.top)/Y.height});t.autoPoint(Ee.x,Ee.y)};he.addEventListener("mousedown",Se,!0),he.addEventListener("click",xt),oe=()=>{He(),he.removeEventListener("mousedown",Se,!0),he.removeEventListener("click",xt)}},T.current=I,fs(I);let be=new ResizeObserver(()=>{v.current?.clientWidth&&I.resize(),R(he=>he+1)});return be.observe(v.current),()=>{be.disconnect(),oe(),fs(null),I.destroy()}},[t]),G(()=>{T.current.set(O,_,t.store.getState().diagram.viewport,e),R(I=>I+1)},[O,_,e]),G(()=>{T.current.applyViewport(r)},[r]),G(()=>{let I=document.querySelector(".status-bar"),oe=x.current;if(!I||!oe)return;let be=new ResizeObserver(()=>oe.style.setProperty("--auto-bottom",`${I.offsetHeight}px`));return be.observe(I),()=>be.disconnect()},[]),G(()=>{t.store.getState().ask||v.current?.focus({preventScroll:!0})},[]),G(()=>{(p||f)&&v.current?.focus({preventScroll:!0})},[p,f?.ask]),G(()=>{p&&m&&m.point>=0&&m.point<rn(n)&&(L.current=-1,ui(t,{point:m.point,low:!1}))},[p,m]);let W=I=>{let oe=t.store.getState();if(!oe.diagram.grabbing||I.ctrlKey||I.metaKey||I.altKey)return!1;let be=oe.ask?.kind==="grab"?oe.ask:null;if(I.key==="Enter"||I.key==="Escape")return be&&(I.key==="Enter"?t.grabTake():t.cancelPick()),!0;let he=Lp(I.key,I.shiftKey,oe.diagram.info?.point??-1,rn(oe.diagram.points),oe.diagram.labels);return he===null?!1:(be&&t.grabPoint(he),!0)},B=I=>{let oe=T.current,be=kv(t);if(!oe||!be||I.ctrlKey||I.metaKey||I.altKey)return!1;let he=lr(be,I.key,I.shiftKey);if(!he)return!1;if("pick"in he)t.movePick(he.pick);else if("confirm"in he)t.confirmPick(he.confirm,oe.ranges());else if("drag"in he){let He=oe.ranges();for(let ht of he.drag){let Se=Tn(He,ht.at);t.dragEvent(ht.what,Se.x,Se.y)}}else t.cancelPick();return!0},re=I=>{let oe=T.current,be=X.current;if(W(I)||B(I)){I.preventDefault(),I.stopPropagation();return}if(!oe||I.altKey)return;let he=t.store.getState().diagram.hover,He=he?hs(be,he.point,he.low,L.current):null;if((I.key==="<"||I.key===">")&&!I.ctrlKey&&!I.metaKey){let xt=ms(o,he?he.point:-1,I.key===">"?1:-1);if(xt===null)return;I.preventDefault(),I.stopPropagation(),L.current=-1,ui(t,{point:xt,low:!1});return}let ht=(I.ctrlKey||I.metaKey)&&I.key.toLowerCase()==="z"?"Undo":I.ctrlKey||I.metaKey?"":I.key,Se=_r(ht,{ranges:oe.ranges(),hover:He,counts:be.curves.map(xt=>xt.xs.length)});Se&&(I.preventDefault(),I.stopPropagation(),"view"in Se?oe.setView(Se.view,!0):"reset"in Se?oe.reset():"undo"in Se?t.store.dispatch({type:"diagram",action:{type:"undoViewport"}}):Se.hover?j(Se.hover.curve,Se.hover.index):ui(t,null))},se=I=>{if(I.defaultPrevented||I.ctrlKey||I.metaKey||I.altKey)return;let{ask:oe,busy:be}=t.store.getState();if(oe)return;let he=I.target;if(I.key==="Escape"&&!be){I.preventDefault(),I.stopPropagation(),t.showAuto(!1);return}let He=yv[I.key.toLowerCase()];He&&(!be||qs.has(He))&&!he.closest('input, select, textarea, [role="dialog"]')&&(I.preventDefault(),I.stopPropagation(),U(He))},le=l?hs(O,l.point,l.low,L.current):null,q=le&&T.current?T.current.position(le.curve,le.index):null,V=p&&m&&m.x!==null&&m.y!==null&&T.current?T.current.place(m.x,m.y):null,ce=u&&T.current?T.current.place(u.x,u.y):null,Ne=r.x!==null||r.y!==null,fe=!O.curves.length,pe=l&&l.point<n.x.length?Op(n,o,i,l.point):null,pt=i?`${i.ylabel} against ${i.xlabel}`:"",Re=fe?"AUTO diagram, no branches yet":`AUTO diagram of ${pt}: ${new Set(O.curves.map(I=>I.branch)).size} branches, ${n.x.length} points, ${o.length} labelled points`,me=T.current?.areaBox()??null,lt=(I,oe)=>s("button",{class:`auto-axis-name auto-axis-${I}`,"aria-haspopup":"dialog","aria-expanded":A===I,"data-axis":I,title:`Change the ${I==="x"?"horizontal":"vertical"} axis: what it plots and its range`,onKeyDown:be=>be.stopPropagation(),onClick:()=>C(A===I?null:I),style:I==="x"?{left:`${me.left+me.width/2}px`,bottom:"0px"}:{left:"0px",top:`${me.top+me.height/2}px`},children:oe||(I==="x"?"x axis":"y axis")}),wt=()=>{let I=A;C(null),v.current?.querySelector(`.auto-axis-name[data-axis="${I}"]`)?.focus()},ct=w?Bu(n,w):0;return s("section",{id:"auto-panel",ref:x,class:"auto-panel","aria-label":"AUTO",onKeyDown:se,children:[s("div",{class:"auto-header",children:[s("button",{class:"auto-back",onClick:()=>t.showAuto(!1),title:"Hide the AUTO view (AUTO stays open; Show AUTO brings it back)",children:"Back"}),s("h2",{children:["AUTO ",s("span",{class:"muted auto-what",children:pt})]}),s(st,{target:Ue.autoView,label:"AUTO"}),s("button",{class:"auto-close",onClick:()=>t.closeAuto(),title:"Done with AUTO: close its window (a running continuation is stopped first; File/Auto opens it again)",children:"Close"})]}),s(Rh,{}),s("div",{class:"auto-tools",role:"toolbar","aria-label":"AUTO",children:[Uh.map(([I,oe,be])=>{let he=Xh[oe],He=!!he&&[...E].some(bv[he]);return s("button",{disabled:d&&!qs.has(oe),"aria-keyshortcuts":be.toUpperCase(),class:He?"auto-pending":void 0,"data-op":oe,title:d&&!qs.has(oe)?Mo:(gv[oe]??c?.[vv.indexOf(oe)]??I)+(He?" (changes wait for the run to end)":""),onClick:()=>U(oe),children:I},oe)}),s("button",{onClick:()=>t.saveAutoSettings(),title:"Save AUTO's Numerics, parameters, axes and Mark values as a file, to set up this model again in one step",children:"Save settings"}),s("button",{onClick:()=>N.current?.click(),title:"Load AUTO's settings from a saved file (while AUTO runs they apply when it stops)",children:"Load settings"}),s("input",{ref:N,id:"auto-settings-load",type:"file",accept:".json,application/json",hidden:!0,onChange:async I=>{let oe=I.target,be=oe.files?.[0];be&&t.loadAutoSettings(await be.text()),oe.value=""}})]}),s("div",{class:"auto-view",children:[p&&s(Pv,{}),f&&s(Fs,{pick:f}),s("header",{class:"plot-bar",children:[s("ul",{class:"auto-legend","aria-label":"Key",children:[Ev(O).map(I=>s("li",{children:[s("span",{class:"auto-swatch"+(I.dashed?" dashed":""),"aria-hidden":"true",style:{borderColor:vr(I.color,e)}}),I.text]},`${I.text}/${I.color}`)),w>0&&s("li",{children:s("button",{class:"small auto-earlier"+(k?" active":""),"aria-pressed":k,title:k?"Hide the branches computed before Clear":"Show the branches computed before Clear",onClick:()=>t.store.dispatch({type:"diagram",action:{type:"showEarlier",show:!k}}),children:["Earlier branches (",ct,")"]})})]}),s("div",{class:"plot-tools",children:[s("button",{disabled:!a,onClick:()=>t.store.dispatch({type:"diagram",action:{type:"undoViewport"}}),title:"Undo the last zoom or pan (Ctrl+Z on the diagram)",children:"Undo zoom"}),s("button",{disabled:!Ne,onClick:()=>T.current.reset(),title:"Back to AUTO's axes (double click, or 0 on the diagram)",children:"Reset view"}),s("button",{disabled:fe,onClick:()=>{let I=T.current.png();I&&Bt("xpp-auto.png",I)},title:"Save the diagram as a PNG picture",children:"PNG"})]})]}),s("div",{class:"plot-host auto-host"+(p?" picking pick-grab":f?` picking pick-${f.mode}`:""),ref:v,tabIndex:0,role:"application","aria-roledescription":"diagram","aria-label":Re,onKeyDown:re,"aria-describedby":p?"grab-instruction grab-keys-help":f?"pick-instruction auto-keys-help":"auto-keys-help",children:[q&&s("span",{class:"hover-dot",style:{left:`${q.left}px`,top:`${q.top}px`}}),V&&s("span",{class:"auto-cursor","aria-hidden":"true",style:{left:`${V.left}px`,top:`${V.top}px`}}),ce&&s("span",{class:"auto-stored",style:{left:`${ce.left}px`,top:`${ce.top}px`},title:`Stored point: ${St(u.x)}, ${St(u.y)}`}),f&&T.current&&s(Rs,{pick:f,chart:T.current}),me&&i&&lt("x",i.xlabel),me&&i&&lt("y",i.ylabel),fe&&s("div",{class:"plot-empty auto-empty",children:s("p",{children:d?"AUTO is running\u2026":"No branches yet: Run starts a continuation from the current point."})})]}),s("footer",{class:"readout auto-readout",role:"status","aria-live":"polite",children:pe?s("span",{children:[s("b",{children:pe.head})," \xB7 ",pe.kind,pe.label&&s(ve,{children:[" \xB7 ",s("b",{class:"auto-label",children:pe.label})]}),pe.values.map(I=>s("span",{children:[" \xB7 ",I]},I))]}):s("span",{class:"muted",children:[s("span",{class:"hint-mouse",children:"Drag to zoom \xB7 wheel zooms \xB7 Shift+drag pans \xB7 double click resets \xB7 < > step through the labels"}),s("span",{class:"hint-touch",children:"Pinch zooms \xB7 drag pans \xB7 tap a point to read it"})]})})]}),A&&s(Eh,{axis:A,onClose:wt},A),P&&s(_h,{kind:P,onClose:()=>$(null)}),s(Ih,{}),s(Fh,{}),u&&s("p",{class:"auto-stored-text muted",children:["Stored point for File/sElect 2par pt: ",St(u.x),", ",St(u.y)]}),s("p",{id:"auto-keys-help",class:"visually-hidden",children:wv}),s("p",{id:"grab-keys-help",class:"visually-hidden",children:Sv})]})}function Vh({dark:e}){let t=te(),n=M(a=>a.diagram.open),o=M(a=>a.diagram.shown),i=J(null),r=J(!1);return G(()=>{let a=!!document.activeElement?.closest?.(".auto-panel")||document.activeElement===document.body;r.current&&(!o||!n)&&a&&document.querySelector(".plot-view:not([hidden]) .plot-host")?.focus(),r.current=n&&o},[n,o]),n?o?s(Av,{dark:e}):s("button",{ref:i,class:"auto-show","aria-controls":"auto-panel","aria-expanded":"false",onClick:()=>t.showAuto(!0),title:"Show the AUTO view again",children:"Show AUTO"}):null}var pi=[{id:"01-introduction",title:"Introduction",html:`<p>XPP (XPPAUT is another name; I will use the two interchangeably) is a tool for solving differential equations, difference equations, delay equations, functional equations, boundary value problems, and stochastic equations. It evolved from a chapter written by John Rinzel and myself on the qualitative theory of nerve membranes and eventually became a commercial product for MSDOS computers called PHASEPLANE. It is now available as a program running under X11 and UNIX.</p>
<p>The code brings together a number of useful algorithms and is extremely portable. Upstream XPPAUT&#39;s graphics and interface were written completely in Xlib, which explains the somewhat idiosyncratic and primitive widgets interface; this fork (xppautX) replaced that X11 front end with a modern browser page (<strong>web2</strong>, see <a href="04-using-the-interface.md">Using the interface</a>) that keeps the same menus, hotkeys and numerics.</p>
<p>XPP contains the code for the popular bifurcation program, AUTO. Thus, you can switch back and forth between XPP and AUTO, using the values of one program in the other and vice-versa. I have put a \u201Cfriendly\u201D face on AUTO as well. You do not need to know much about it to play around with it.</p>
<p>XPP has the capabilities for handling up to 5000 differential equations. There are solvers for delay and stiff differential equations a well as some code for boundary value problems. Difference equations are also handled. Up to 10 graphics windows can be visible at once and a variety of color combinations is supported. PostScript/SVG output is supported. Post processing is easy and includes the ability to make histograms, FFTs and applying functions to columns of your data. Equilibria and linear stability as well as one-dimensional invariant sets can be computed. Nullclines and flow fields aid in the qualitative understanding of two-dimensional models. Poincare maps and equations on cylinders and tori are also supported. Some useful averaging theory tricks and various methods for dealing with coupled oscillators are included primarily because that is what I do for a living. Equations with Dirac delta functions are allowable.</p>
<p>There is an animation package that allows you to create animated versions of your simulations, such as a little pendulum moving back and forth or lamprey swimming. The animation view is opened by invoking the <code>(V)iew axes</code> <code>(T)oon</code> menu item. See <a href="10-animations.md">Creating Animations</a> for complete info.</p>
<p>I will assume that you are well versed in the theory of ordinary differential equations although you need not be to use the program. There are a number of useful features designed for people who use dynamical systems to <em>model</em> their experiments. There is a curve-fitter based on the Marquardt-Levenberg algorithm which lets you fit data points to the solutions to dynamical systems. Gnuplot-like graphics and support for some graphics objects such as text, arrows, and pointers are part of the package. You can also import bifurcation curves as part of your graphs. It is possible to automatically generate \u201Cmovies\u201D of three-dimensional views of attractors or parametric changes in the attractor as some parameters vary. I have also included a small preprocessing utility that allows one to create files for large systems of coupled equations.</p>
<p>There are a number of other such programs available, but they all seem to require that your problems be compiled before using them. XPP does not; I have devised a simple and fairly fast formula compiler that is based on the idea of the inner interpretor used in the language FORTH (which remains my first love as far as language is concerned) Fear not, the differential equations and boundary conditions and other formulae are written in usual algebraic notation. However, in order to run big problems very quickly, I have written the code so that it is possible to create a library that can be linked to your problem and thus create a binary with the right-hand sides compiled. This can run much faster than the parsed code. See <a href="11-dll-libraries.md">Creating C-files for faster simulations</a> and <a href="15-generated-c-files.md">C Files</a>.</p>
<p>XPP has been compiled on most UNIX machines and works fine now on Windows, Macs, and Linux. Building XPP requires only the standard C compiler, and Xlib. Look at the any README files that come with the distribution for solutions to common compilation problems.</p>
<p>The basic unit for XPP is a single ASCII file (hereafter called an ODE file) that has the equations, parameters, variables, boundary conditions, and functions for your model. You can also include numerical parameters such as time step size and method of integration although these can also be changed within the program. The graphics and postprocessing are all done within the program using the mouse (or touch) and various menus and buttons. The impatient user should look at some sample <code>.ode</code> files instead of actually reading the documentation. There are many command line arguments and options that can be added to set up numerics (<a href="16-quick-reference.md">Quick reference</a>). There is also a resource file <code>.xpprc</code> that you can add to your home directory (below). The documentation here is pretty incomplete, but covers much of the basics. There is a book from SIAM available, and <code>docs/upstream/tree.pdf</code> (the original manual&#39;s historical companion) gives a description of <em>every</em> command.</p>
<h2 id="notes-on-the-interface">Notes on the Interface</h2>
<p>Upstream XPPAUT&#39;s X11 text fields had no cut and paste, and BackSpace and
Delete behaved differently across systems; this no longer applies in
web2&#39;s browser text fields, which behave like any other web form (full
cut/paste, Home/End, arrow keys, selection). See
<a href="04-using-the-interface.md">Using the interface</a> for what&#39;s the same as
X11 (the menus, the single-letter hotkeys) and what&#39;s different. Almost
every command has a keyboard shortcut; these are given below.</p>
<h2 id="disclaimer">Disclaimer</h2>
<p>XPP is distributed as is. The author makes no claims as to the performance of the program. Anyone is allowed to modify and distribute XPP as long as the original code is also made available. See the LICENSE file for the full caveats.</p>
<h2 id="acknowledgements">Acknowledgements</h2>
<p>Artie Sherman, John Rinzel for many suggestions. Daniel Dougherty and Robert McDougal for contribution actual code! Also Sebius Doedel for making AUTO available. I want to also thank Bart Oldmann for some pointers on the porting of the most recent AUTO version.</p>
<p>Please let me know of any bugs or other stuff that you\u2019d like to see incorporated into XPP. I will usually fix them quickly.</p>
<p>My EMAIL address is <a href="mailto:bard@pitt.edu">bard@pitt.edu</a>.</p>
<h2 id="note">Note</h2>
<p>The easiest way to get a thorough understanding of the program as well as a short tutorial in dynamical systems is to use the World Wide Web tutorial which can be accessed from my home page at <a href="http://www.pitt.edu/$%60%5Cequiv%60$phase">http://www.pitt.edu/$\`\\equiv\`$phase</a>. This tutorial is geared toward computational neuroscientists (in the choice of problems) but provides a fairly detailed introduction to the program.</p>
<h2 id="environment-variables">Environment variables.</h2>
<p>While you can make a file in your home directory called <code>.xpprc</code> which contains a list of commonly used options, XPP also uses some environment variables. Note, on Windows computers the default home directory is taken to be a user\u2019s Desktop folder.</p>
<p>The environment variables which XPP uses are:</p>
<ul>
<li><strong>XPPBROWSER</strong>: Web browser to view documentation (e.g. /usr/bin/firefox)</li>
<li><strong>XPPEDITOR</strong>: Text editor to view/edit documentation (e.g. /usr/bin/gedit)</li>
<li><strong>XPPHELP</strong>: Path to the XPPAUT documentation file $<code>&lt;</code>$xpphelp.html$<code>&gt;</code>$ (e.g. /usr/share/doc/xppaut/html/xpphelp.html)</li>
<li><strong>XPPSTART</strong>: File browser will open to the specified path. This may be useful in an instructional setting to point to a mapped drive containing course materials or an NFS file share.</li>
</ul>
<p>On Mac/Linux/Unix systems, these environment variables are typically set within a user\u2019s .bashrc file using export commands. For example:</p>
<pre><code>        export XPPHELP=/usr/share/doc/xppaut/html/xpphelp.html
        export XPPBROWSER=/usr/bin/firefox
        export XPPEDITOR=/usr/bin/nedit
        export XPPSTART=/usr/share/doc/xppaut/examples/ode
</code></pre>
<p>When I run XPP on Windows, I run the following bat file (xppaut.bat) which serves to set various environment variables (the <code>DISPLAY</code> line is an X11 leftover; xppautX needs no X server and ignores it):</p>
<pre><code>:: Specify location of where you want your .xpprc to be.
:: For most Windows users their Desktop is probably a safe bet.
set HOME=%HOMEDRIVE%%HOMEPATH%\\Desktop
:: Change path to your favorite browser
set XPPBROWSER=c:/Program Files/Netscape/Communicator/Program/netscape.exe
set XPPHELP=c:/xppall/help/xpphelp.html
set DISPLAY=127.0.0.1:0.0

:: Might want to change this to JEdit or whatever text editor you use.
:: set XPPEDITOR=c:/Windows/notepad
set XPPEDITOR=notepad++

:: You may want to set XPPSTART to your research or course directory
set XPPSTART=c:/xppall/ode

set argC=0
for %%x in (%*) do Set /A argC+=1
IF %argC%==0 (c:/xppall/xppaut) ELSE (c:/xppall/xppaut %1 %2 %3)
pause
</code></pre>
<p>Here is my <code>.xpprc</code> file:</p>
<pre><code># xpprc file
@ but=quit:fq
@ maxstor=50000,bell=0
@ meth=qualrk,tol=1e-6,atol=1e-6
# thats it
</code></pre>
`,headings:[{id:"notes-on-the-interface",text:"Notes on the Interface",level:2},{id:"disclaimer",text:"Disclaimer",level:2},{id:"acknowledgements",text:"Acknowledgements",level:2},{id:"note",text:"Note",level:2},{id:"environment-variables",text:"Environment variables.",level:2}]},{id:"02-ode-files",title:"ODE Files",html:`<p><strong>NOTE.</strong> <em>Pre 1992, XPP used a different form for ODE files. I no longer document them</em> A command line option lets you convert old-style to new style format.</p>
<p>ODE files are ASCII readable files that the XPP parser reads to create machine usable code. Lines can be continued with the standard backslash character, however, the total length of any line cannot exceed 1000 characters.</p>
<p><strong>Example.</strong> I will start with a very simple example to get you up and running. The model is the periodically driven Fitzhugh-Nagumo equation:</p>
<pre><code class="language-math">\\begin{eqnarray}
dv/dt &amp;=&amp; f(v)-w+s(t)+I_0 \\\\
dw/dt &amp;=&amp; \\epsilon(v-\\gamma w) \\\\
f(v) &amp;=&amp; v(1-v)(v-a) \\\\
s(t) &amp;=&amp; \\alpha \\sin \\omega t
\\end{eqnarray}
</code></pre>
<p>Here is the ODE file:</p>
<pre><code># Forced Fitzhugh-Nagumo fhn.ode 
dv/dt = f(v)-w+s(t)+I_0
dw/dt = eps*(v-gamma*w)
f(v)=v*(1-v)*(v-a)
s(t)=al*sin(omega*t)
param a=.25,eps=.05,gamma=1,I_0=.25
param al=0,omega=2
@ total=100,dt=.2,xhi=100
done
</code></pre>
<p>The file is pretty self-explanatory. The first line cannot contain a number as its first character (this makes the parser think that the format of the ODE file is the old style.) The last line should be the word \u201Cdone.\u201D The names of all parameters must be declared with optional values (the default sets them to 0.) There can be as many as you can fit on each line (up to 2000 parameters) but they must be separated by commas or spaces and the \u201C=\u201D sign must have <em>no</em> spaces on either side of it.</p>
<p>You could optionally include initial data by adding either of the following sets of lines to the file:</p>
<pre><code>init v=.25,w=.3
</code></pre>
<p>or</p>
<pre><code>v(0)=.25
w(0)=.3
</code></pre>
<p>As you have probably guessed, comments have the form:</p>
<pre><code># This is a comment
</code></pre>
<p>The \u201C@\u201D sign tells XPP that you want to preset some of the internal parameters for numerical integration and graphing, AUTO, etc. Please put a space after the \u201C@\u201D on each line or it wont work. In this case, we have told XPP to integrate the equations until t=100 with a timestep of 0.25 and to set the high value for the x-axis to 100. You can, of course, change all these internal options from within XPP; this provides an easy way \u201Cset\u201D up the problem for \u201Cone-button\u201D operation.</p>
<h2 id="quick-exploration">Quick exploration</h2>
<p>Once you have written an ODE file, you can run XPP by typing</p>
<pre><code>xpp ffhn.ode
</code></pre>
<p>where <code>ffhn.ode</code> is the filename you created (<code>xppautX</code> for this fork&#39;s
binary; see <a href="04-using-the-interface.md">Using the interface</a> for how it
starts and opens the front end in your browser).</p>
<p>The program loads the file. If there are errors, it reports them and exits
(<code>-silent</code>) or keeps serving the page so you can read what it printed
(the browser front end). Almost every command has a keyboard shortcut,
either the first letter or the letter in parentheses/capitalized, shown on
the menu next to the item; use the mouse to click the command or the
keyboard to type its key. To solve the differential equation with the
current parameters, click <code>Initialconds</code> and then <code>Go</code> (or type <code>I G</code>).
You will see the variable $<code>V(t)</code>$ plotted across the screen as a function
of time. Click <code>Xivst</code>. When the prompt comes up backspace over <code>V</code>, and
type in <code>w</code> and <code>Enter.</code> The variable $<code>w</code>$ will be plotted versus time.
Note that the vertical axis of the window is automatically adjusted. Click
<code>Viewaxes</code> to choose the view and fill in the form as follows:</p>
<ul>
<li>X-axis: V</li>
<li>Y-axis: W</li>
<li>Xmin: -.5</li>
<li>Ymin: 0</li>
<li>Xmax: 1.5</li>
<li>Ymax: 1</li>
<li>Xlabel: V</li>
<li>Ylabel: w</li>
</ul>
<p>and then click on <code>Ok.</code> The phase-plane will be drawn showing a limit cycle. Click <code>Nullclines</code> and then <code>New</code> to draw the nullclines. Click <code>Text,etc</code> then <code>Text</code> and type in \u201CV-nullcline\u201D followed by <code>Enter</code> at the prompt. Accept the defaults for text size and font by typing <code>Enter</code> twice. Move the mouse pointer to the cubic-like curve and click the button. The text should appear on the screen. Repeat this but type in \u201Cw-nullcline\u201D for the text. Click <code>Graphic stuff</code> and then <code>Postscript</code>. Accept the defaults and a hardcopy postscript file will be produced which you can view or printout on an appropriate printer. Click <code> File</code> and then <code>Quit</code> and answer <code>Yes</code> to exit XPP.</p>
<p>A much more extensive tutorial is available on the World Wide Web (see above). This document is mainly a reference to all the features (bugs :) of XPP.</p>
<h2 id="ode-file-format">ODE File format</h2>
<p>ODE files consist of ascii readable text which XPP uses to describe the program it wants to solve. The line length is limited to 256 characters total. Individual lines can be continued with the UNIX backslash character, $<code>\\backslash.</code>$ ODE files have any combination of the following lines. The order is not too important but can matter (see below).</p>
<pre><code># comment line - name of file, etc   
...
#include &lt;filename&gt;
... 
options &lt;filename&gt;
...
d&lt;name&gt;/dt=&lt;formula&gt;
&lt;name&gt;&#39;=&lt;formula&gt;
...
&lt;name&gt;(t)=&lt;formula&gt;
...
volt &lt;name&gt;=&lt;formula&gt;
...
&lt;name&gt;(t+1)=&lt;formula&gt;
...
markov &lt;name&gt; &lt;nstates&gt;
{t01} {t02} ... {t0k-1}
{t10} ...
...
{tk-1,0} ...
...
aux &lt;name&gt;=&lt;formula&gt;
...
&lt;name&gt;=&lt;formula&gt;
...
parameter &lt;name1&gt;=&lt;value1&gt;,&lt;name2&gt;=&lt;value2&gt;, ...
...
!&lt;name&gt;=&lt;formula&gt;
...
wiener &lt;name1&gt;, &lt;name2&gt;, ...
...
number &lt;name1&gt;=&lt;value1&gt;,&lt;name2&gt;=&lt;value2&gt;, ...
...
&lt;name&gt;(&lt;x1&gt;,&lt;x2&gt;,...,&lt;xn&gt;)=&lt;formula&gt;
...
table &lt;name&gt; &lt;filename&gt;
...
table &lt;name&gt; % &lt;npts&gt; &lt;xlo&gt; &lt;xhi&gt; &lt;function(t)&gt;
...
global sign {condition} {name1=form1;...}
...
init &lt;name&gt;=&lt;value&gt;,...
...
&lt;name&gt;(0)=&lt;value&gt; or &lt;expr&gt;
...
bdry &lt;expression&gt;
...
%[i1 .. i2]
...
%
command[i1..i2] ...
...
name[i1..i2] ...
...
0= &lt;expression&gt;
...
solv &lt;name&gt;=&lt;expression&gt;
...
special &lt;name&gt;=conv(type,npts,ncon,wgt,rootname)
           fconv(type,npts,ncon,wgt,rootname,root2,function)
           sparse(npts,ncon,wgt,index,rootname)
               fsparse(npts,ncon,wgt,index,rootname,root2,function)
               mmult(n,m,w,root)
               fmmult(n,m,w,root1,root2,function)
           gill(0,rxnlist)
               findext(type,n,skip,root)
...
only &lt;name2&gt;,&lt;name2&gt;,...
... 
# comments
...
@ &lt;name&gt;=&lt;value&gt;, ...
...
set &lt;name&gt; {x1=z1,x2=z2,...,}
..
&quot; More comments
&quot;  {name=val,...,name=val} active comments
done
</code></pre>
<p>The typical ODE file contains some or all of the above types of lines. Continuous variables, auxiliary quantities, and Markov variables are all plottable quantities in XPP. That is, once you have solved your equation, you can plot or view any of the continuous and Markov variables or the auxiliary quantities.</p>
<p>XPP uses a bunch of defaults when it is started up by looking for a file called \u201Cdefault.opt.\u201D If it cannot find it, it uses internal options. Alternatively, you can tell XPP the name of the options file you want to use. The description of these files is below. The format for such a statement is:</p>
<pre><code>option &lt;filename&gt;
</code></pre>
<p>which loads the options file specified in <code>&lt;filename&gt;</code>. You will probably not want to use this very much as you can now specify all of the parameters in the options file within your ODE file by using the \u201C@\u201D symbol.</p>
<p>XPP lets you include files in the ODE file so that for example, you can create a library of functions which your XPP ode file can call. Here is an example of two files, the first is called <code>test.ode</code> and the second is called <code>test.inc</code>:</p>
<pre><code># test.ode
#include test.inc
x&#39;=f(x)
par a=.25
done

# test.inc
f(x)=x*(1-x)*(x-a)
#done
</code></pre>
<p>The contents of <code>test.inc</code> will be included into the ODE file as if you had written</p>
<pre><code># test.ode
f(x)=x*(1-x)*(x-a)
x&#39;=f(x)
par a=.25
done
</code></pre>
<p><strong>NOTES:</strong> (1) At the end of every include file you have to have the statement <code>#done</code> (2) include files can include other files.</p>
<p>Variables are the quantities you wish to integrate in time. There are two types of variables: (i) continuous and (ii) Markov. I will first describe continuous variables. Variable names (as can all names in XPP) can have up to 64 characters each (up to 9 in XPPAUT 8 and earlier). XPP is case insensitive. Any combination of letters and numbers is valid as is the underscore, \u201C_\u201D. There are 5 ways that you can tell XPP the names of the continuous variables and their right-hand sides. The following three are equivalent:</p>
<pre><code>d&lt;name&gt;/dt=&lt;formula&gt;
&lt;name&gt;&#39;=&lt;formula&gt;
&lt;name&gt;(t)=&lt;formula&gt;
</code></pre>
<p>Here <code>&lt;name&gt;</code> is the name of the variable. The last version is for notational convenience only sincd the <code>dx/dt</code> notation makes no sense for discrete dynamical systems. The <code>&lt;formula&gt; </code> is exactly that, the formula for the right-hand sides of the equations. These equations can appear anywhere in your file and in any order. However, the order in which they are written determines the order in which they appear in the Data Browser (see below.)</p>
<p>The fourth way of defining a continuous variable is:</p>
<pre><code>&lt;name&gt;(t) = &lt;formula&gt;
</code></pre>
<p>which tells XPP that this defines a Volterra integral equation. (It is distinguished from a definition of some function of the dummy variable <code>t</code> by the presence of an integral operator (<code>int{</code> or <code> int[</code> ) in the right-hand side. (see below). For example, the convolution equation:</p>
<pre><code class="language-math">v(t) = \\exp(-t) + \\int_0^t e^{-(t-s)^2}v(s) ds
</code></pre>
<p>would be written as:</p>
<pre><code>v(t) = exp(-t) + int{exp(-t^2)#v}
</code></pre>
<p>Integro-differential equations use the <code>dv/dt</code> etc notation so that</p>
<pre><code class="language-math">\\frac{dv(t)}{dt} = -v(t)+ \\int_0^t e^{-(t-s)^2}v(s) ds
</code></pre>
<p>becomes</p>
<pre><code>dv/dt= -v+int{exp(-t^2)#v}
</code></pre>
<p>In the event that the right-hand side does not contain any integral operator (as would be the case, for example, if there was a fixed or hidden variable definition) then you can force the parser into making the equation a Volterra integral equation by typing</p>
<pre><code>volt v= exp(-t)+int{exp(-t^2)#v}
</code></pre>
<p><strong>NOTE.</strong> In this format you do not write <code>v(t)=...</code> but just <code>v=...</code></p>
<p>XPP gives you the option of defining many variables at once using an array-like declaration which XPP expands into a set of declarations. For example, you could declare 10 equations with the following command:</p>
<pre><code>x[1..10]&#39;=-x[j]
</code></pre>
<p>and XPP would internally expand this is</p>
<pre><code>dX1/dt=-X1
dX2/dt=-X2
dX3/dt=-X3
dX4/dt=-X4
dX5/dt=-X5
dX6/dt=-X6
dX7/dt=-X7
dX8/dt=-X8
dX9/dt=-X9
dX10/dt=-X10
</code></pre>
<p>Thus, you can make networks and discretizations of PDE\u2019s compactly. Note the appearance of the expression <code>[j] </code>. This is expanded by XPP to the value of the index. Similarly, the following are allowable indices:</p>
<pre><code>[j+n]
[j-n]
[j*n]
</code></pre>
<p>where <code>n </code> is any integer. The <code>[1..10] </code> notation tells XPP to start <code>j</code> at 1 and go to 10. You can start with any nonnegative integer and end with any. The first number can be less than or greater than the second. XPP does not treat arrays in any efficient manner, it is as if you defined 10 or whatever variables. The names of the variables are the root name with the index appended.</p>
<p>Related to pseudo arrays are array blocks that have (for example) the form</p>
<pre><code>%[1..3]
x[j]&#39;=-y[j]
y[j]&#39;=x[j]
init x[j]=1
%
</code></pre>
<p>This will be expanded as follows:</p>
<pre><code>x1&#39;=-y1
y1&#39;=x1
init x1=1
x2&#39;=-y2
y2&#39;=x2
init x2=1
x3&#39;=-y3
y3&#39;=x3
init x3=1
</code></pre>
<p>which groups the \u201Carrays\u201D along their index rather than along the variable name. This has many disadvantages particularly if you want to put in initial data within the program. However, if you are attempting to solve a discretized version of a partial differential equation that is very stiff, then this has the advantage that the Jacobi matrix that arises from the linearization (required for stiff systems) is banded rather than dense. If you choose CVODE as the integration method (recommended for stiff systems) then there is an option to use the banded version of CVODE. For large systems (say 200 spatial points) this can result in a speed up of the order of 500- to 1000-fold! That is, a problem that would take an hour to integrate, instead takes 4 or 5 seconds. Note that Markov variables cannot be defined in one of these blocks. It will screwup!</p>
<p>There are two ways to define initial data in the ODE file. Either use the method of typing <code>init x=1.23,y=423.6 ...</code> or <code> x(0)=1.23</code>. In the latter case, you can also initialize variables that involve delayed arguments. For example, <code>x(0)=sin(t)</code> will initialize <code>x</code> to be <code>sin(t)</code> for <code>-DELAY &lt; t &lt; 0</code> where <code>DELAY</code> is the maximum delay. WARNING: this has a few bugs in it; the formula <code>x(0)=t+1</code> will initialize <code>x(0)=0</code> but <code> x(0)=1+t</code> will initialze <code>x(0)=1</code>. The values for $<code>t&lt;0</code>$ will be properly evaluated but $<code>t=0</code>$ will not be.</p>
<p>Markov variables are finite state quantities that randomly flip from one integer state to another according to the transition probability table that is given in the ODE file. They are treated like variables in that they have initial conditions and are accessible to the user. Markov variables are declared as</p>
<pre><code>markov &lt;name&gt; &lt;nstates&gt;
{t01} {t02} ... {t0k-1}
{t10} ...
...
{tk-1,0} ...
...
</code></pre>
<p>Each Markov variable has its own line which must begin with the letter \u201Cm\u201D. (All other letters are ignored, but for readability, it is best to write it out.) The name of the variable, <code>&lt;name&gt;</code>, and the number of states, <code>&lt;nstates&gt;</code>, are included on the first line. The possible values are 0,1, and so on up to $<code>k-1</code>$ where $<code>k</code>$ is the number of states.</p>
<p>A finite state Markov variable with $<code>k</code>$ states must have associated with it a $<code>k\\times k</code>$ matrix, $<code>T_{ij}</code>$ which contains the probability of going from state $<code>i</code>$ to state $<code>j</code>$ per unit of time. Thus the effective transition probability is <code>DeltaT</code> times $<code>T_{ij}</code>$; the larger is <code>DeltaT</code> the higher the probability. Since the probabilities must add to 1 in any row, the diagonal terms are automatic and ignored by XPP. If there are $<code>k</code>$ states to the variable, then there must be $<code>k</code>$ rows following the declaration of the transition matrix. Each row contains $<code>k</code>$ entries delimited by curly brackets and separated by spaces. For example, suppose $<code>z</code>$ is a two-state variable with transition probabilities, $<code>P_{ij}</code>$ then it would be defined by:</p>
<pre><code>markov z 2
{0} {P01}
{P10} {0}
</code></pre>
<p>where <code>P01, P10</code> are any <em>algebraic expression or number involving the parameters and variables</em> of the XPP file. Note that this implies that the state transitions can be dependent on any other quantities.</p>
<p>At each output time step, the probabilities are computed, multiplied by the timestep, and a random number is chosen. If it is in the appropriate range, then the transition will be made. Transitions of several such variables are made in parallel and then each is updated.</p>
<p>In many cases, you might one to keep track of some combination of your variables. For example, you might want to track the potential energy of a damped pendulum as it swings. They are declared as:</p>
<pre><code>aux &lt;name&gt;=formula
</code></pre>
<p>where <code>&lt;name&gt;</code> is the name of the quantity and <code>&lt;formula&gt;</code> is the formula for it. <em>Note that a formula cannot refer to an auxiliary named quantity; use fixed or hidden variables for this.</em> An example using the auxiliary quantity is the damped pendulum:</p>
<pre><code class="language-math">ml\\frac{d^2x}{dt^2} = -mg\\sin x -\\mu\\frac{dx}{dt}
</code></pre>
<p>with potential energy:</p>
<pre><code class="language-math">P.E. = mg(1-\\cos x)
</code></pre>
<p>and kinetic energy</p>
<pre><code class="language-math">K.E.= \\frac{1}{2}ml(\\frac{dx}{dt})^2.
</code></pre>
<p>Since XPP solves systems of first order equations, this is first converted and results in the ODE file:</p>
<pre><code># damped pendulum 
dx/dt = xp
dxp/dt = (-mu*xp-m*g*sin(x))/(m*l)
aux P.E.=m*g*(1-cos(x))
aux K.E.=.5*m*l*xp^2
param m=10,mu=.1,g=9.8,l=1
done
</code></pre>
<p>where I have also given some values to the parameters.</p>
<p>As with differential equations, you can also define many auxialiar variables at once with a statement like:</p>
<pre><code>aux r[1..10]=sqrt(x[j]^2+y[j]^2)
</code></pre>
<p>which will be expanded in the obvious fashion.</p>
<p>XPP allows you to define intermediate quantities that can be used in the right-hand sides of the equations. They are kept internally by XPP and here, the order in which they are declared matters. They are evaluated in the order in which they are defined, so earlier defined ones should not refer to later defined ones. The format is:</p>
<pre><code>&lt;name&gt; = &lt;formula&gt;
</code></pre>
<p>They are most useful if you want to use a complicated quantity in several right-hand sides. The <code>&lt;name&gt;</code> is kept internal to XPP and their values are not stored (unlike variables). <em>Note that they are different from functions which can take arguments and are not hidden from the user.</em></p>
<p>For example, in the pendulum model above, you might also want the total energy:</p>
<pre><code class="language-math">T.E. = K.E. + P.E.
</code></pre>
<p>Now, as I remarked above, you cannot just add another auxiliary variable using <code>P.E.</code> and <code>K.E.</code> since they are not known to XPP. But why compute them twice. Here is how to use fixed variables in this example.</p>
<pre><code># damped pendulum pend.ode
dx/dt = xp
dxp/dt = (-mu*xp-m*g*sin(x))/(m*l)
pe=m*g*(1-cos(x))
ke=.5*m*l*xp^2
aux P.E.=pe
aux K.E.=ke
aux T.E=pe+ke
param m=10,mu=.1,g=9.8,l=1
done
</code></pre>
<p>Both energies are only computed once. (For this example, the performance difference for computing the additional quantity is negligible, but for more complex formulae, fixed quantities are useful.)</p>
<p>Hidden variables can also be declared in groups like ODEs:</p>
<pre><code>ica[1..10]=gca*minf(v[j])*(v[j]-eca)
</code></pre>
<p>this is expanded into 10 declarations:</p>
<pre><code>ica1=gca*minf(v1)*(v1-eca)
ica2=gca*minf(v2)*(v2-eca)
...
</code></pre>
<p>DAEs can be solved with XPP by combining the <code>0= </code> statement with the <code>solv </code> statement. A general DAE has the form $<code>F(X,X&#39;,W,t)=0</code>$ where $<code>X,W</code>$ are vector quantities and $<code>X&#39;</code>$ is the derivative of $<code>X.</code>$ XPP treats these in generality but currently cannot integrate past singularities that better integrators such as those found in MANPAK will traverse. I plan to add the MANPAK integrator DAEN1 shortly. In any case, the integrator still handles alot of different problems. The syntax is pretty simple. Algebraic constraints are written using the <code>0= </code> command and the algebraic quantities (i.e. those that don\u2019t involve derivatives) are defined using the <code>solv </code> command. For example:</p>
<pre><code class="language-math">\\begin{eqnarray*}
x&#39; &amp;=&amp; -x \\\\
0 &amp;=&amp; x+y-1
\\end{eqnarray*}
</code></pre>
<p>with $<code>(x(0)=1,y(0)=0)</code>$ would be written as:</p>
<pre><code># dae_ex1.ode
x&#39;=-x
0= x+y-1 
x(0)=1
solv y=0
aux yy=y
done
</code></pre>
<p>The <code>solv</code> statement tells XPP that $<code>y</code>$ is an algebraic quantity and its initial value is 0. The <code>aux</code> statement will let you also plot the value of $<code>y</code>$ since it is \u201Chidden\u201D from the user. Here is a more complicated equation which could not be solved by XPP without the DAE stuff:</p>
<pre><code class="language-math">x&#39;+exp(x&#39;)+x=0
</code></pre>
<p>with $<code>x(0)=-(1+e),x&#39;(0)=1.</code>$ Note that the function $<code>x+exp(x)</code>$ has no closed inverse so that we cannot write this in terms of $<code>x&#39;</code>$. Here is the ode file:</p>
<pre><code># dae_ex2.ode
x&#39;=xp
0= xp+exp(xp)+x
x(0)=-3.7182
solv xp=1
aux xdot=xp
done
</code></pre>
<p>Note that we create a dummy algebraic variable called <code>xp</code> which is the derivative of $<code>x.</code>$ This is because XPP treats the derivatives in a special manner so we have to accomodate its idiosyncrasies by adding an additional algebraic variable. This last example exploits numerical errors to get the DAE solver to go beyond where it should go legally! It is a relaxation oscillator:</p>
<pre><code class="language-math">\\begin{eqnarray*}
w&#39; &amp;=&amp; v \\\\
0 &amp;=&amp; v(1-v^2)-w
\\end{eqnarray*}
</code></pre>
<p>with $<code>w(0)=0,v(0)=1.</code>$ Note that the algebraic equation has multiple roots for some values of $<code>w</code>$ and thus as $<code>w</code>$ groes it must \u201Cjump\u201D to a new branch. This cannot happen in a true DAE and in fact, one has to set tolerances low to get the numerical errors to let it work. Here is the next DAE example:</p>
<pre><code>#dae_ex3.ode
w&#39;=v_
0= v_*(1-v_*v_)-w
solv v_=1
aux v=v_
@ NEWT_ITER=1000,NEWT_TOL=1e-3,JAC_EPS=1e-5,METH=qualrk
done
</code></pre>
<p>The important numerical parameters for the DAEs are the maximum iterates, the tolerance for Newton\u2019s method, and the epsilon value for computing the Jacobian. These are found in the numerics menu under the menu item SingPt Control.</p>
<p>The DAE algebraic variables are initialized in the ODE file as formulae or constants. However, once integrated, the DAEs retain their current values, not their initial values. To change the initial DAE values, you use the Initialconds menu under the DAE sub menu.</p>
<p>Parameters are named quantities that represent constants in your ODE and which you can change from within the program. The format is:</p>
<pre><code>parameter &lt;name1&gt;=&lt;value1&gt;, &lt;name2&gt;=&lt;value2&gt;,...
</code></pre>
<p>There can be many declarations on each line. It is very important that there be <em>no spaces</em> between the <code>&lt;name&gt;</code> the <code>=</code> sign, and the <code>value</code> of the parameter. Without an <code>=</code> sign and a value, the parameter is set to zero by default.</p>
<p>Numbers are just like parameters but they are \u201Chidden\u201D from the user; they do not appear in the parameters windows once ypu run the program. Their only advantage is that in a problem with many defined constants, of which only a few can be freely chosen, the parameter window is not cluttered by dozens of parameters.</p>
<p>Derived parameters are also \u201Chidden\u201D from the user and allow you to define constants in terms of other parameters through formulas. Each time you change a parameter, these derived parameters are updated. They differ from \u201Cfixed\u201D quantities in that they are not updated at every integration step. As an example, suppose you want to define area in terms of radius and length:</p>
<pre><code>par length=50,diam=10
!area=pi*length*diam
</code></pre>
<p>will create a quantity called <code>area</code> that will be altered whenever you change the parameters <code>length,diam</code>. Note that the <strong>!</strong> in front of the name tells XPP that this is not a fixed variable and should only be updated when parameters are changed. Their values can be examined using the calculator by just inputting their names.</p>
<p>Wiener parameters are more properly \u201Cfunctions\u201D that return scaled white noise. They are held fixed for $<code>t</code>$ to $<code>t+dt</code>$ during an integration. At each time step, they are then changed and their value is a normally distributed random number with zero mean and unit variance. The program scales them by the appropriate time step as well. Their purpose is so that one can use methods other than Euler for solving noisy problems. In particular, large steps can be taken using backward Euler without loss of stability.</p>
<p>The declarations:</p>
<pre><code>par a[1..5]=.25
wiener w[1..5]
number z[1..5]=.123456
</code></pre>
<p>behave in the obvious fashion. Note that this expression will lead to an error:</p>
<pre><code>par a[1..2]=.5, c=.1234
</code></pre>
<p>as XPP will expand it into 2 lines:</p>
<pre><code>par a1=.5, c=.1234
par a2=.5, c=.1234
</code></pre>
<p>which will give an \u201Cduplicate name\u201D error. On the other hand, this expression will work:</p>
<pre><code>par a[1..2]=.25,b[j]=.3
</code></pre>
<p>and is the same as:</p>
<pre><code>par a1=.25,b1=.3
par a2=.25,b2=.3
</code></pre>
<p>User defined functions have the following form:</p>
<pre><code>&lt;name&gt;(x1,x2,...)=&lt;expression&gt;
</code></pre>
<p>where <code>&lt;name&gt;</code> is the name of the function and <code>x1,x2,...</code> are the dummy arguments and <code>&lt;expression&gt;</code> is a formula defining the function. There can be at most 9 arguments.</p>
<p>Tables are another type of function but (at least as of now) are only of one argument. The \u201Ctable\u201D declaration takes one of two forms: (i) file based and (ii) function-based. The file based version has the form:</p>
<pre><code>table &lt;name&gt; &lt;filename&gt;
</code></pre>
<p>allows you to declare a function called <code> &lt;name&gt;</code> that reads in values from the file, <code>&lt;filename&gt;</code> or as a function of one variable over some interval and then is used in your program as a function of 1 variable interpolated from the tabulated values. The values of this table are assumed to be equally spaced and the file is an ASCII file with the format:</p>
<pre><code>&lt;number of values&gt;
&lt;xlo&gt;
&lt;xhi&gt;
y1
y2
.
.
.
yn
</code></pre>
<p>Thus, $<code>f(xlo)=y1</code>$ and $<code>f(xhi)=yn.</code>$ (If the number of points in the file description of the table starts with the ASCII character \u2018i\u2019 , (e.g. i50 instead of 50) then the interpolation will be piecewise constant; otherwise it is linear.) These tables can also be read in from within XPP but a valid table must be given to start the program. This table can be arbitrarily long (as memory permits) and thus you can use experimental data as inputs to differential equations or even sketch curves and use that as your nonlinearity.</p>
<p>You can directly input tabulated functions as well to speed up computations with complicated functions. In this case, after the table name put a parenthesis symbol followed by the number of points, the minimum argument and the maximum argument and then the function. This should be written as a function of \u201Ct\u201D. Thus, the statement</p>
<pre><code>table f % 501 -10 10 tanh(t)
</code></pre>
<p>will produce a table of the hyperbolic tangent function from -10 to 10 consisting of 501 points.</p>
<p>Using the data browser, you can create tabulated data from a simulation to use later in a different simulation as a function or as input or whatever.</p>
<p>The \u201Cglobal\u201D declaration allows you to set some conditions and then if these conditions hold reset dynamic variables according to the conditions. The form of \u201Cglobal\u201D declarations is:</p>
<pre><code>global sign {condition} {name1=form1;...}
</code></pre>
<p>The <code>condition</code> is any combination of variables such that if it is zero, then the desired event has occurred. <code>sign</code> is either 1,-1, or 0. A sign of 1 means that if the condition goes from less than zero to greater than zero, the event has occurred. A sign of -1 means that if the condition <em>decreases</em> through zero that event has occurred. Finally, a sign of 0, means that any crossing of zero signals an event. Each time the condition is met, events occur. There can be up to 20 events per condition and they are delimited by braces and separated by semicolons. Events are always of the form: <code>variable=formula</code> where <code>variable</code> is one of the differential equation variables or parameters and formula is some formula involving the variables. All formulae are first evaluated and then the variables are updated. Some examples are shown below. <strong>WARNING!</strong> The global flags are <em>ignored</em> by the adams integrator. Use GEAR, EULER, RUNGE-KUTTA, BACKWARD EULER, MODIFIED EULER, STIFF, QUALITY-RK, DORMAND-PRINCE, ROSENBROCK or CVODE. There is one special event that you can put into the list of events: <code>out_put=val</code> if <code>val&gt;0</code> then the current value of all variables etc is stored. This allows you to, for example, get faster more accurate Poincare maps.</p>
<p><em>Note.</em> You will sometimes get the rather obscure message that the program is \u201CWorking too hard.\u201D This is a diagnostic that one of two things is occurring. First, due to round-off, sometimes the extrapolation to zero for the condition is actually not zero but is instead some very small number. Thus XPP checks for the definition of this which is called <code>s </code> and if it is less than a user defined value of <code>smin</code> (changed in the numerics menu under \u201CsIng pt ctrl\u201D) then it is treated as zero. The console reports this number when the error message occurs so that you can change <code>smin</code> to be larger, say 1e-13, to avoid this message.</p>
<p>The second situation that causes this to arise is that the time step <code>DT</code> is too large and the same condition is occuring twice in that step. Since the interpolation is linear, this means you should try to take smaller steps; otherwise numerical errors will accumulate.</p>
<p>Global flags can also be defined in groups, for example:</p>
<pre><code>global 1 x[1..5]-1 {x[j]=0}
</code></pre>
<p>The default for initial conditions of Markov and continuous variables is zero. Initial data can also be set in the ODE file (and of course easily set from within XPP) in one of two ways:</p>
<pre><code>&lt;name&gt;(0)=value
</code></pre>
<p>will set the variable <code>&lt;name&gt;</code> to the specified value. Alternatively, you can initialize many variables on one line by typing</p>
<pre><code>init &lt;name1&gt;=value1, &lt;name2&gt;=value2, ... 
</code></pre>
<p>Boundary conditions can be placed anywhere in the file or ignored altogether if you don\u2019t plan on solving boundary value problems. They have the form:</p>
<pre><code>bndry &lt;expression&gt;
</code></pre>
<p>where stands for boundary condition. The expression is one involving your variables and which will be set to zero. In order to distinguish left and right boundary conditions, the following notation is used. For the values of the variables at the left end of the interval, use the symbol for the variable. For the values at the right end of the interval, use the symbol for the variable appended by a single quote, \u2019. Thus to specify the boundary conditions $<code>x(0)=1</code>$, $<code>y(1)=2</code>$, the following is used:</p>
<pre><code>bndry x-1
bndry y&#39;-2
</code></pre>
<p>Periodic boundary conditions would be written as:</p>
<pre><code>bndry x-x&#39;
bndry y-y&#39;
</code></pre>
<p>Note that it is not necessary to specify the BCs at this point. They can be specified within the program. Also note that the notation I have used allows the specification of mixed boundary conditions such as periodic BCs. (XPP has some additional special commands for periodic boundary conditions that enable the user to specify fewer equations than usual.) The number of boundary conditions must match the number of variables in your problem. Do not use the boundary value solver with Volterra, delay, stochastic, or discrete equations.</p>
<p>Initial and boundary conditions can also be defined <em>en masse</em> via:</p>
<pre><code>bdry x[1..2]-2
x[1..2](0)=.345
</code></pre>
<p>and this will be expanded in the expected fashion.</p>
<p>This is relevant only if you are using XPP in silent model. The <code>only</code> declaration will save to the output only the variables specified by this command.</p>
<p>XPP has many many internal parameters that you can set from within the program and four parameters that can only be set before it is run. Most of these internal parameters can be set from the \u201COptions\u201D files described above and whose format is at the end of this document. However, it is often useful to put the options right into the ODE file. <em>NOTE: Any options defined in the ODE file override all others such as the onres in the OPTIONS file.</em> In addition, there are several options not available in the options file. These options are used by the \u201Csilent\u201D integrator to produce a file for output when running without X.</p>
<p>The format for changing the options is:</p>
<pre><code>@ name1=value1, name2=value2, ...
</code></pre>
<p>where <code>name</code> is one of the following and <code>value</code> is either an integer, floating point, or string. (All names can be upper or lower case). The first four options <em>can only be set outside the program.</em> They are:</p>
<ul>
<li><p>MAXSTOR=<code>integer</code> sets the total number of time steps that will be kept in memory. The default is 5000. If you want to perform very long integrations change this to some large number.</p>
</li>
<li><p>BACK= <code>{Black,White}</code> sets the background to black or white.</p>
</li>
<li><p>SMALL=<code>fontname</code> where <code>fontname</code> is some font available to your X-server. This sets the \u201Csmall\u201D font which is used in the Data Browser and in some other windows.</p>
</li>
<li><p>BIG=<code>fontname</code> sets the font for all the menus and popups.</p>
</li>
<li><p>SMC={0,...,10} sets the stable manifold color</p>
</li>
<li><p>UMC={0,...,10} sets the unstable manifold color</p>
</li>
<li><p>XNC={0,...,10} sets the X-nullcline color</p>
</li>
<li><p>YNC={0,...,10} sets the Y-nullcline color</p>
</li>
<li><p>BUT=s1:s2 defines a user button. You can use this many times in the same ODE file. The first string is the name of the button.The second is the set of key stroke short cuts. For example: <code> BUT=mouse:im</code> will put a button on the main window labeled <code>mouse</code> and when you press it, it will act as though you had clicked <code> InitConds Mouse</code>.</p>
</li>
</ul>
<p>The remaining options can be set from within the program. They are</p>
<ul>
<li><p>LT=<code>int</code> sets the linetype. It should be less than 2 and greater than -6.</p>
</li>
<li><p>SEED=<code>int</code> sets the random number generator seed.</p>
</li>
<li><p>XP=name sets the name of the variable to plot on the x-axis. The default is <code>T</code>, the time-variable.</p>
</li>
<li><p>YP=name sets the name of the variable on the y-axis.</p>
</li>
<li><p>ZP=name sets the name of the variable on the z-axis (if the plot is 3D.)</p>
</li>
<li><p>NPLOT=<code>int</code> tells XPP how many plots will be in the opening screen.</p>
</li>
<li><p>XP2=name,YP2=name,ZP2=name tells XPP the variables on the axes of the second curve; XP8 etc are for the 8th plot. Up to 8 total plots can be specified on opening. They will be given different colors.</p>
</li>
<li><p>AXES=<code>{2,3}</code> determine whether a 2D or 3D plot will be displayed.</p>
</li>
<li><p>TOTAL=value sets the total amount of time to integrate the equations (default is 20).</p>
</li>
<li><p>DT=value sets the time step for the integrator (default is 0.05).</p>
</li>
<li><p>NJMP=<code>integer</code>, NOUT=<code>integer</code> tell XPP how frequently to output the solution to the ODE. The default is 1, which means at each integration step. It is also used to specify a the period for maps in the continuation package AUTO.</p>
</li>
<li><p>T0=value sets the starting time (default is 0).</p>
</li>
<li><p>TRANS=value tells XPP to integrate until <code>T=TRANS</code> and then start plotting solutions (default is 0.)</p>
</li>
<li><p>NMESH=<code>integer</code> sets the mesh size for computing nullclines (default is 40).</p>
</li>
<li><p>{BANDUP=int, BANDLO=int} sets the upper and lower limits for banded systems which use the banded version of the CVODE integrator.</p>
</li>
<li><p>METH=<code>{ discrete,euler,modeuler,rungekutta,adams,gear,volterra, backeul, qualrk,stiff,cvode,5dp,83dp,2rb, ymp}</code> sets the integration method (see below; default is Runge-Kutta.) The latter four are the two Dormand-Prince integrators, the Rosenbrock, and the symplectic integrators.</p>
</li>
<li><p>DTMIN=value sets the minimum allowable timestep for the Gear integrator.</p>
</li>
<li><p>DTMAX=value sets the maximum allowable timestep for the Gear integrator</p>
</li>
<li><p>VMAXPTS=value sets the number of points maintained in for the Volterra integral solver. The default is 4000.</p>
</li>
<li><p>{ JAC_EPS=value, NEWT_TOL=value, NEWT_ITER=value} set parameters for the root finders.</p>
</li>
<li><p>ATOLER=value sets the absolute tolerance for several of the integrators.</p>
</li>
<li><p>TOLER=value sets the error tolerance for the Gear, adaptive RK, and stiff integrators. It is the relative tolerance for CVODE and the Dormand-Prince integrators.</p>
</li>
<li><p>BOUND=value sets the maximum bound any plotted variable can reach in magnitude. If any plottable quantity exceeds this, the integrator will halt with a warning. The program will not stop however (default is 100.)</p>
</li>
<li><p>DELAY=value sets the maximum delay allowed in the integration (default is 0.)</p>
</li>
<li><p>PHI=value,THETA=value set the angles for the three-dimensional plots.</p>
</li>
<li><p>XLO=value,YLO=value,XHI=value,YHI=value set the limits for two-dimensional plots (defaults are 0,-2,20,2 respectively.) Note that for three-dimensional plots, the plot is scaled to a cube with vertices that are $<code>\\pm1</code>$ and this cube is rotated and projected onto the plane so setting these to $<code>\\pm2</code>$ works well for 3D plots.</p>
</li>
<li><p>XMAX=value, XMIN=value, YMAX=value, YMIN=value, ZMAX=value, ZMIN=value set the scaling for three-d plots.</p>
</li>
<li><p>OUTPUT=filename sets the filename to which you want to write for \u201Csilent\u201D integration. The default is \u201Coutput.dat\u201D.</p>
</li>
<li><p>POIMAP=<code>{ section,maxmin} </code> sets up a Poincare map for either sections of a variable or the extrema.</p>
</li>
<li><p>POIVAR=name sets the variable name whose section you are interested in finding.</p>
</li>
<li><p>POIPLN=value is the value of the section; it is a floating point.</p>
</li>
<li><p>POISGN=<code>{ 1, -1, 0 }</code> determines the direction of the section.</p>
</li>
<li><p>POISTOP=1 means to stop the integration when the section is reached.</p>
</li>
<li><p>RANGE=1 means that you want to run a range integration (in batch mode).</p>
</li>
<li><p>RANGEOVER=name, RANGESTEP, RANGELOW, RANGEHIGH, RANGERESET=<code> Yes,No</code>, RANGEOLDIC=<code>Yes,No</code> all correspond to the entries in the range integration option (see below).</p>
</li>
<li><p>TOR_PER=value, defined the period for a toroidal phasespace and tellx XPP that there will be some variables on the circle.</p>
</li>
<li><p>FOLD=name, tells XPP that the variable &lt;name&gt; is to be considered modulo the period. You can repeat this for many variables.</p>
</li>
<li><p>STOCH=<code>1,2</code> is useful in batch mode and allows one to use the RANGE parameters to compute a mean (1) trajectory or its variance (2). That is, suppose you want to average a simulation over 100 trials. Then set RANGESTEP=500,RANGELOW=0,RANGEHIGH=0, STOCH=1.</p>
</li>
<li><p>AUTOEVAL=<code>{0,1}</code> tells XPP whether or not to automatically re-evaluate tables everytime a parameter is changed. The default is to do this. However for random tables, you may want this off. Each table can be flagged individually within XPP.</p>
</li>
<li><p>Postscript options. <code>PS_COLOR,PS_FONT,PS_LW,PS_FSIZE</code> represent respectively, the color flag (0/1), the name of the font (default is Times-Roman), the linewidth in tenths of a point (default 5), the fontsize (in points, default 14).</p>
</li>
<li><p>AUTO-stuff. The following AUTO-specific variables can also be set: <code>NTST, NMAX, NPR, EPSU, EPSS, EPSL, DSMIN, DSMAX, DS, PARMIN, PARMAX, NORMMIN, NORMMAX, AUTOXMIN, AUTOXMAX, AUTOYMIN, AUTOYMAX, AUTOVAR</code>. The last is the variable to plot on the y-axis. The x-axis variable is always the first parameter in the ODE file unless you change it within AUTO.</p>
</li>
</ul>
<p>Sometimes, you wnat to prepare a bunch of simulations that use different initial data or parameter values or numerical methods, etc. You can, of course, change these within the program by choosing the desired option and changing it. Or, you can do the simulation and save the results in a \u201C.set\u201D file (see below). Another way to do this is by adding a bunch of parameter sets to the ode file. The format for this is:</p>
<pre><code>set name {item1=value1, item2=value2, ..., }
</code></pre>
<p>Then, you tell XPP to use the named set and it will do all of the things inside the brackets. The items are any parameter name, any variable name, or any of the internal options named above. The named sets are accessed through the <code>Get par set</code> menu item in the <code> File</code> menu. Here is an example:</p>
<pre><code># test
x&#39;=-a*x+c
par a=1,c=1
set set1 {a=1,c=1,x=0,dt=.25}
set set2 {a=.25,c=0,x=1,dt=.1}
done
</code></pre>
<p>If you load \u201Cset1\u201D then the parameters, initial conditions, and <code>DeltaT</code> will be set to the values in the brackets. Choosing \u201Cset2\u201D sets them differently. The names can be anything you like.</p>
<p>The <code>special </code> directive allows you to create dense coupled systems of ODEs and is much faster than using the more general summation operator <code>sum</code>. There are two types of convolutions and 2 types of \u201Csparse\u201D coupling functions. The synatx is</p>
<pre><code>special zip=conv(type,npts,ncon,wgt,root)
</code></pre>
<p>This will produce an array, <code>zip</code> of <code>npts</code> is length defined as:</p>
<pre><code class="language-math">\\hbox{zip}[i] =\\sum_{j=-\\hbox{ncon}}^{\\hbox{ncon}}\\hbox{wgt}[j+ncon]
\\hbox{root}[i+j]
</code></pre>
<p>for $<code>i=0,\\ldots,npts-1.</code>$ <code>root</code> is the name of a variable and thus, there must be at least <code>npts-1</code> variables defined after <code> root.</code> The array <code>wgt</code> is defined as a table using the <code> tabular</code> command and must be of length <code>2 ncon + 1.</code> The <code> type</code> determines the nature of the convolution at the edges. Type <code> even</code> reflects the boundaries, <code>periodic</code> makes them periodic, and <code>0</code> does not include them in the sum. The object <code>zip</code> behaves as a function of ne variable with domain 0 to <code>npts-1</code>. Here is an example</p>
<pre><code># neural network
tabular wgt % 25 -12 12 1/25
f(u)=1/(1+exp(-beta*u))
special k=conv(even,51,12,wgt,u0)
u[0..50]&#39;=-u[j]+f(a*k([j])-thr)
par a=4,beta=10,thr=1
done
</code></pre>
<p>The <code>sparse</code> network has the syntax:</p>
<pre><code>special zip=sparse(npts,ncon,wgt,index,root)
</code></pre>
<p>where <code>wgt</code> and <code>index</code> are tables with at least <code>npts * ncon</code> entries. The array <code>index</code> returns the indices of the offsets to with which to connect and the array <code>wgt</code> is the coupling strength. The return is</p>
<pre><code>zip[i] = sum(j=0;j&lt;ncon) w[i*ncon+j]*root[k]
k = index[i*ncon+j] 
</code></pre>
<p>Thus one can make complicated types of couplings. The following is a randomly coupled network with 5 random connections of random strength:</p>
<pre><code># junk2.ode
table w % 255 0 255 .4*ran(1)
table ind % 255 0 255 flr(51*ran(1))
special bob=sparse(51,5,w,ind,v0)
v[0..50]&#39;=-v[j]+f(k*bob([j])-thr-c*delay(v[j],tau))
par k=3,thr=1,beta=1,c=2.5,tau=5
f(u)=1/(1+exp(-beta*u))
done
</code></pre>
<p>The other two types of networks allow more complicated interactions:</p>
<pre><code>special zip=fconv(type,npts,ncon,wgt,root1,root2,f)
</code></pre>
<p>evaluates as</p>
<pre><code>zip[i]=sum(j=-ncon;j=ncon) wgt[ncon+j]*f(root1[i+j],root2[i])
</code></pre>
<p>and</p>
<pre><code>special zip=fsparse(npts,ncon,wgt,index,root1,root2,f)
</code></pre>
<p>evaluates as</p>
<pre><code>zip[i]=sum(j=0;j&lt;ncon) wgt[ncon*i+j]*f(root1[k],root2[i])
k = index[i*ncon+j] 
</code></pre>
<p>They are useful for coupled phase oscillator models.</p>
<p>There are two more such functions which are essentially just matric multiplications.</p>
<pre><code>special k=mmult(n,m,w,u)
</code></pre>
<p>returns a vector <code>k</code> of length <code>m </code> defined as</p>
<pre><code class="language-math">k(j)=\\sum_{i=0}^{n-1} w(i+nj)u(i)
</code></pre>
<p>The associated functional operator:</p>
<pre><code>special k=fmmult(n,m,w,u,v,f)
</code></pre>
<p>returns</p>
<pre><code class="language-math">k(j)=\\sum_{i=0}^{n-1} w(i+nj)f(u(i),v(j)).
</code></pre>
<p>The command</p>
<pre><code>special z=gill(0,rxnlist)
</code></pre>
<p>sets up a list of reactions to implement the gillespie method. (The 0 is superfluous at this point and serves as a place-holder for implementing faster approximations) The reactions are a set of fixed variable that you include. The vector <code>z</code> returns the following information: $<code>z(0)</code>$ returns the time to the next reaction; $<code>z(1,\\ldots,m)</code>$ returns a 0 or 1 according as to whether that reaction took place. Here is an example from Gillespie</p>
<pre><code># gillesp_bruss.ode
# gillespie algorithm for brusselator
#
# x1  -&gt; y1 (c1)
# x2+y1 -&gt; y2+Z (c2)
# 2 y1 + y2 -&gt; 3 y1 (c3)
# y1 -&gt; Z2 (c4)
par c1x1=5000,c2x2=50,c3=.00005,c4=5
init y1=1000,y2=2000
#  compute the reaction rates
r1=c1x1
r2=c2x2*y1
r3=c3*y1*y2*(y1-1)/2
r4=c4*y1
special z=gill(0,r{1-4})
tr&#39;=tr+z(0)
y1&#39;=y1+z(1)-z(2)+z(3)-z(4)
y2&#39;=y2+z(2)-z(3)
@ bound=100000000,meth=discrete,total=1000000,njmp=1000
@ xp=y1,yp=y2
@ xlo=0,ylo=0,xhi=10000,yhi=10000
done
</code></pre>
<p>Note the form of the reaction list. Other reactions can be added separated by commas. This appears to be easier to code that the way that I used in the XPP book before i implemented this new algorithm.</p>
<p>A line that starts with a quote mark <code>&quot; </code> is treated differently from the normal comments and is included in a special buffer. This is to separate out comments that are descriptive of the general file as opposed to line by line comments which when separated from the ODE file have no context. <strong>Furthermore</strong> you can add \u201Cactions\u201D associated with these comments. That is you can set XPP parameters like integration method, etc and also parameters and initial data. These comments have the form:</p>
<pre><code>&quot; {gca=1.2,gk=0} Set the potassium to zero and turn on the calcium
</code></pre>
<p>Clicking on <code>File Prt info</code> brings up a window with the ODE source code. Clicking on <code>Action</code> in this window brings up the active comments. The user does not see <code>{gca=1.2,gk=0} </code> but instead sees:</p>
<pre><code>* Set the potassium to zero and turn on the calcium
</code></pre>
<p>with an asterisk to indicate there is n action associated with the line. Clicking on the will set <code>gca=1.2</code> and <code>gk=0</code>. Thus, you can make nice little tutorials within the ODE file. These are limited to 500 lines.</p>
<p>The last line in the file should be \u201Cdone\u201D telling the ODE reader that the file is over.</p>
<p>All of the declarations, <code>markov, parameter, wiener, table, aux, init, bndry, global, done,</code> can be abbreviated by their first letter.</p>
<h2 id="reserved-words">Reserved words</h2>
<p>You should be aware of the following keywords that should not be used in your ODE files for anything other than their meaning here.</p>
<pre><code>sin cos tan atan atan2 sinh cosh tanh
exp delay ln log log10 t pi if then else mod
asin acos heav sign  flr ran abs del\\_shft
max min normal besselj bessely erf erfc poisson
arg1 ... arg9  @ $ + - / * ^ ** shift
| &gt; &lt; == &gt;= &lt;= != not \\# int sum of i&#39;
</code></pre>
<p>These are mainly self-explanatory. The nonobvious ones are:</p>
<ul>
<li><p><strong><code>atan2(x,y)</code></strong>: is the argument of the complex number $<code>x+iy.</code>$</p>
</li>
<li><p><strong><code>heav(arg1)</code></strong>: the step function, zero if <code>arg1&lt;0</code> and 1 otherwise.</p>
</li>
<li><p><strong><code>sign(arg)</code></strong>: which is the sign of the argument (zero has sign 0)</p>
</li>
<li><p><strong><code>ran(arg)</code></strong>: produces a uniformly distributed random number between 0 and <code>arg.</code></p>
</li>
<li><p><strong><code>besselj, bessely </code></strong>: take two arguments, $<code>n,x</code>$ and return respectively, $<code>J_n(x)</code>$ and $<code>Y_n(x),</code>$ the Bessel functions.</p>
</li>
<li><p><strong><code>erf(x), erfc(x)</code></strong>: are the error function and the complementary function.</p>
</li>
<li><p><strong><code>normal(arg1,arg2)</code></strong>: produces a normally distributed random number with mean <code>arg1</code> and variance <code>arg2</code>.</p>
</li>
<li><p><strong><code>poisson(arg1)</code></strong>: produces an integer which is the expected number of events for the value of <code>arg</code> from a Poisson process.</p>
</li>
<li><p><strong><code>max(arg1,arg2)</code></strong>: produces the maximum of the two arguments and <code>min</code> is the minimum of them.</p>
</li>
<li><p><strong><code>mod(arg1,arg2)</code></strong>: is <code>arg1</code> modulo <code>arg2</code>.</p>
</li>
<li><p><strong><code>if(&lt;exp1&gt;)then(&lt;exp2&gt;)else(&lt;exp3&gt;)</code></strong>: evaluates <code> </code> If it is nonzero it evaluates to otherwise it is . E.g. <code>if(x&gt;1)then(ln(x))else(x-1)</code> will lead to <code>ln(2)</code> if <code>x=2</code> and <code>-1 if x=0.</code></p>
</li>
<li><p><strong><code>delay(&lt;var&gt;,&lt;exp&gt;)</code></strong>: returns variable <code>&lt;var&gt;</code> delayed by the result of evaluating <code>&lt;exp&gt;</code>. In order to use the delay you must inform the program of the maximal possible delay so it can allocate storage. (See the section on the NUMERICS menus.)</p>
</li>
<li><p><strong><code>flr(arg)</code></strong>: is the integer part of<code>&lt;arg&gt;</code> returning the largest integer less than <code>&lt;arg&gt;</code>.</p>
</li>
<li><p><strong><code>t </code></strong>: is the current time in the integration of the differential equation.</p>
</li>
<li><p><strong><code>pi</code></strong>: is $<code>\\pi.</code>$</p>
</li>
<li><p><strong><code>arg1, ..., arg9</code></strong>: are the formal arguments for functions</p>
</li>
<li><p><strong><code>int, #</code></strong>: concern Volterra equations.</p>
</li>
<li><p><strong><code>shift(&lt;var&gt;,&lt;exp&gt;)</code></strong>: This operator evaluates the expression <code>&lt;exp&gt;</code> converts it to an integer and then uses this to indirectly address a variable whose address is that of <code>&lt;var&gt;</code> plus the integer value of the expression. This is a way to imitate arrays in XPP. For example if you defined the sequence of 5 variables, <code> u0,u1,u2,u3,u4</code> one right after another, then <code>shift(u0,2)</code> would return the value of <code>u2.</code></p>
</li>
<li><p><strong><code>del_shft(&lt;var&gt;,&lt;shft&gt;,&lt;delay&gt;).</code></strong>: This operator combines the <code>delay</code> and the <code>shift</code> operators and returns the value of the variable <code>&lt;var&gt;</code> shifted by <code>&lt;shft&gt;</code> at the delayed time given by <code>&lt;delay&gt;</code>. It is of limited utility as far as I know, but I needed it for a problem, so here it is.</p>
</li>
<li><p><strong><code>sum(&lt;ex1&gt;,&lt;ex2&gt;)of(&lt;ex3&gt;)</code></strong>: is a way of summing up things. The expressions <code>,&lt;ex1&gt;</code> are evaluated and their integer parts are used as the lower and upper limits of the sum. The index of the sum is <code>i\u2019</code> so that you cannot have double sums since there is only one index. is the expression to be summed and will generally involve <code>i\u2019.</code> For example <code>sum(1,10)of(i\u2019)</code> will be evaluated to 55. Another example combines the sum with the shift operator. <code>sum(0,4)of(shift(u0,i\u2019))</code> will sum up <code>u0</code> and the next four variables that were defined after it. An example below shows how this can be used to solve large systems of equations that are densely coupled.</p>
</li>
</ul>
<p>The parser in this distribution is a great improvement over the old style parser. The parser distinguishes between the old style and the new style by whether or not the first line of the file contains a number. If the first line is a number, then the old style parser is used. Otherwise, the new style is used.</p>
`,headings:[{id:"quick-exploration",text:"Quick exploration",level:2},{id:"ode-file-format",text:"ODE File format",level:2},{id:"reserved-words",text:"Reserved words",level:2}]},{id:"03-examples",title:"Examples",html:`<p>Nothing helps one understand how to use a program better than lots of examples.</p>
<h2 id="morris-lecar-equations">Morris-Lecar Equations</h2>
<p>The Morris-Lecar equations arise as a simplification of a model for barnacle muscle oscillations. They have the form:</p>
<pre><code class="language-math">\\begin{eqnarray*}
C\\frac{dV}{dt} &amp;=&amp;
g_L(V_L-V)+g_Kw(V_k-V)+g_{Ca}m_\\infty(V)(V_{Ca}-V)+I \\\\
\\frac{dw}{dt} &amp;=&amp; \\phi \\lambda_w(V)(w_\\infty(V)-w)
\\end{eqnarray*}
</code></pre>
<p>where</p>
<pre><code class="language-math">\\begin{eqnarray*}
m_\\infty(V) &amp;=&amp; .5(1+\\tanh((V-V_1)/V_2)) \\\\
w_\\infty(V) &amp;=&amp; .5(1+\\tanh((V-V_3)/V_4)) \\\\
\\lambda_w &amp;=&amp; \\cosh((V-V3)/(2V_4))
\\end{eqnarray*}
</code></pre>
<p>This is a very straightforward model and so the equation file is pretty simple:</p>
<pre><code># The Morris-Lecar equations 

# Declare the parameters
p gl=.5,gca=1,gk=2
p vk=-.7,vl=-.5,vca=1
p v1=.01,v2=.145,v3=.1,v4=.15
p i=.2,phi=.333   
  
# Define some functions
minf(v)=.5*(1+tanh((v-v1)/v2))
winf(v)= .5*(1+tanh((v-v3)/v4))
lamw(v)= cosh((v-v3)/(2*v4))

# define the right-hand sides
v&#39;= gl*(vl-v)+gk*w*(vk-v)+gca*minf(v)*(vca-v)+i
w&#39;= phi*lamw(v)*(winf(V)-w)

# some initial conditions -- not necessary but for completeness
v(0)=.05
w(0)=0

# Done!!
d
</code></pre>
<p>Note that some errors are now caught by the parser. For example, duplicate names and illegal syntax are found.</p>
<p>Suppose you want to keep track of the calcium current as an auxiliary variable. Then, the following file will work</p>
<pre><code># The Morris-Lecar equations

# Declare the parameters
p gl=.5,gca=1,gk=2
p vk=-.7,vl=-.5,vca=1
p v1=.01,v2=.145,v3=.1,v4=.15
p i=.2,phi=.333   
  
# Define some functions
minf(v)=.5*(1+tanh((v-v1)/v2))
winf(v)= .5*(1+tanh((v-v3)/v4))
lamw(v)= cosh((v-v3)/(2*v4))

# define the right-hand sides
v&#39;= gl*(vl-v)+gk*w*(vk-v)-gca*minf(v)*(v-vca)+i
w&#39;= phi*lamw(v)*(winf(v)-w)
#
aux ica=gca*minf(v)*(v-vca)

# some initial conditions -- not necessary but for completeness
v(0)=.05
w(0)=0

# Done!!
d
</code></pre>
<p>Note that we are wasting computational time since we compute $<code>I_{Ca}</code>$ twice; once as a contribution to the potential change and once as an auxiliary variable. In a FORTRAN or C program, one would compute it as a local variable and use it in both instances. This is the purpose of fixed variables. (For the present problem, the computational overhead is trivial, but for coupled arrays and other things, this can be quite substantial.) The last program uses a fixed variable to reduce the computation:</p>
<pre><code># The Morris-Lecar equations ml1.ode

# Declare the parameters
p gl=.5,gca=1,gk=2
p vk=-.7,vl=-.5,vca=1
p v1=.01,v2=.145,v3=.1,v4=.15
p i=.2,phi=.333   
  
# Define some functions
minf(v)=.5*(1+tanh((v-v1)/v2))
winf(v)= .5*(1+tanh((v-v3)/v4))
lamw(v)= cosh((v-v3)/(2*v4))

# define the right-hand sides
v&#39;= gl*(vl-v)+gk*w*(vk-v)-icaf+i
w&#39;= phi*lamw(v)*(winf(v)-w)

# where
icaf=gca*minf(v)*(v-vca)

# and
aux ica=icaf

# some initial conditions -- not necessary but for completeness
v(0)=.05
w(0)=0

# Done!!
d
</code></pre>
<p>The calcium current is computed once instead of twice. This is why \u201Cfixed\u201D variables are useful.</p>
<p><strong>NOTE:</strong> Since fixed quantities are not \u201Cvisible\u201D to the user and auxiliary quantities are not \u201Cvisible\u201D to the internal formula compiler, we can use the same name for the fixed as the auxiliary variable; the <code>aux </code> declaration essentially makes it visible to the user with almost no computational overhead. However, it does generate an error message (not fatal) so it is best to make all names unique.</p>
<h2 id="a-linear-cable-equation-with-boundary-conditions">A linear cable equation with boundary conditions</h2>
<p>In studying a dendrite, one is often interested in the steady-state voltage distribution. Consider a case where the dendrite is held at $<code>V=V_0</code>$ at $<code>x=0</code>$ and has a leaky boundary condition at $<code>x=1.</code>$ Then the equations are:</p>
<pre><code class="language-math">\\begin{eqnarray*}
\\lambda^2\\frac{d^2V}{dx^2} &amp;=&amp; V(x) \\\\
V(0)&amp;=&amp; V0 \\\\
a\\frac{dV(1)}{dx} + b V(1) &amp;=&amp; 0
\\end{eqnarray*}
</code></pre>
<p>When $<code>a=0,b\\ne0</code>$ the voltage at $<code>x=1</code>$ is held at 0. When $<code>a\\ne0,b=0</code>$ there is no leak from the cable and the conditions are for sealed end. Since this is a second order equation and XPP can only handle first order, we write it as a system of two first order equations in the file which is:</p>
<pre><code># Linear Cable Model cable.ode

# define the 4 parameters, a,b,v0,lambda^2
p a=1,b=0,v0=1,lam2=1
# now do the right-hand sides
v&#39;=vx
vx&#39;=v/lam2

# The initial data
v(0)=1
vx(0)=0

# and finally, boundary conditions
# First we want V(0)-V0=0
b v-v0
#
# We also want aV(1)+bVX(1)=0
b a*v&#39;+b*vx&#39;
# Note that the primes tell XPP to evaluate at the right endpoint
d
</code></pre>
<p>Note that I have initialized $<code>V</code>$ to be 1 which is the appropriate value to agree with the first boundary condition.</p>
<h2 id="the-delayed-inhibitory-feedback-net">The delayed inhibitory feedback net</h2>
<p>A simple way to get oscillatory behavior is to introduce delayed inhibition into a neural network. The equation is:</p>
<pre><code class="language-math">\\frac{dx(t)}{dt} = -x(t) + f(ax(t)-bx(t-\\tau)+P)
</code></pre>
<p>where $<code>f(u)=1/(1+\\exp(-u))</code>$ and $<code>a,b,\\tau</code>$ are nonnegative parameters. Here $<code>p</code>$ is an input. The XPP file is:</p>
<pre><code># delayed feedback

# declare all the parameters, initializing the delay to 3
p tau=3,b=4.8,a=4,p=-.8
# define the nonlinearity
f(x)=1/(1+exp(-x))
# define the right-hand sides; delaying x by tau
dx/dt = -x + f(a*x-b*delay(x,tau)+p)
x(0)=1
# done
d
</code></pre>
<p>Try this example after first going into the numerics menu and changing the maximal delay from 0 to say, 10. Glass and Mackey describe a few other delay systems some of which have extremely complex behavior. Try them out.</p>
<h2 id="a-population-problem-with-random-mutation">A population problem with random mutation</h2>
<p>This example illustrates the use of Markov variables and is due to Tom Kepler. There are two variables, $<code>x_1,x_2</code>$ and a Markov state variable, $<code>z.</code>$ The equations are:</p>
<pre><code class="language-math">\\begin{eqnarray*}
x_1&#39; &amp;=&amp; x_1(1-x_1-x_2) \\\\
x_2&#39; &amp;=&amp; z(ax_2(1-x_1-x_2)+\\epsilon x_1)
\\end{eqnarray*}
</code></pre>
<p>and $<code>z</code>$ switches from 0 to 1 proportionally to $<code>x_1.</code>$ The transition matrix is 0 everywhere except in the 0 to 1 switch where it is $<code>\\epsilon x_1.</code>$ So z=1 is an absorbing state.</p>
<p>Initial conditions should be $<code>x_1(0)=1.e-4</code>$ or so, $<code>x_2(0)</code>$ the same (this is just for convenience; we really want $<code>x_2(0)=0</code>$ and then have it jump discontinuously to $<code>1.e-4</code>$ or so when $<code>z</code>$ makes its transition, but this shouldn\u2019t matter that much)</p>
<p>This models the population dynamics of two populations $<code>x_1,x_2</code>$ in competition with each other. $<code>x_2</code>$, initially absent, is a mutant of $<code>x_1.</code>$ The mutation rate $<code>\\epsilon</code>$ should be smaller than one. The relative advantage, $<code>a</code>$, should be larger than one.</p>
<p>One expects that $<code>x_1</code>$ grows for a while, eventually $<code>z</code>$ makes its transition, $<code>x_2</code>$ begins to grow and eventually overtakes $<code>x_1.</code>$</p>
<p>The equation file for this is</p>
<pre><code># Kepler model kepler.ode

init x1=1.e-4,x2=1.e-4,z=0
p eps=.1,a=1
x1&#39; = x1*(1-x1-x2)
x2&#39;= z*(a*x2*(1-x1-x2)+eps*x1)
# the markov variable and its transition matrix
markov z 2
{0} {eps*x1}
{0} {0}
d
</code></pre>
<h2 id="some-equations-with-flags">Some equations with flags</h2>
<p>Tyson describes a model which involves cell growth:</p>
<pre><code class="language-math">\\begin{eqnarray*}
\\frac{du}{dt} &amp;=&amp; k_4(v-u)(a+u^2)-k_6u \\\\
\\frac{dv}{dt} &amp;=&amp; k_1m-k_6u \\\\
\\frac{dm}{dt} &amp;=&amp; bm
\\end{eqnarray*}
</code></pre>
<p>This is a normal looking model with exponential growth of the mass. However, if $<code>u</code>$ decreases through 0.2, then the \u201Ccell\u201D divides in half. That is the mass is set to half of its value. Thus, we want to flag the event $<code>u=.2.</code>$ The file in the <em>new format</em> (no sense using the old format since global variables did not appear in earlier versions of XPP)</p>
<pre><code># tyson.ode 
i u=.0075,v=.48,m=1
p k1=.015,k4=200,k6=2,a=.0001,b=.005
u&#39;=  k4*(v-u)*(a+u^2) - k6*u
v&#39;= k1*m - k6*u
m&#39;= b*m
global -1 {u-.2} {m=.5*m} 
d
</code></pre>
<p>Everything is fairly straightforward. When $<code>u-.2</code>$ decreases through zero tha is, $<code>u</code>$ is greater than .2 and then less than .2, the mass is cut in half. Integration using any of the integrators <em>except ADAMS!</em> yields a regular limit cycle oscillation.</p>
<p>Another example with discontinuities arises in the study of coupled oscillators. Two phase oscillators $<code>x,y</code>$ travel uniformly around the circle. If $<code>x</code>$ hits $<code>2\\pi</code>$ it is reset to 0 and adds an amount $<code>r(y)</code>$ to the phase of $<code>y.</code>$ The XPP file is</p>
<pre><code># delta coupled oscillators delta.ode
x&#39;=1
y&#39;=w
global 1 {x-2*pi} {x=0;y=b*r(y)+y}
global 1 {y-2*pi} {y=0;x=b*r(x)+x}
r(x)=sin(x+phi)-sin(phi)
par b=-.25,phi=0,w=1.0
done
</code></pre>
<p>Note that there are 2 conditions and each creates 2 events.</p>
<h2 id="large-coupled-systems">Large coupled systems</h2>
<p>Many times you want to solve large coupled systems of differential equations. Writing the ODE files for these can be a tedious exercise. The array declaration described above simplifies the creation of ODE file for this. For example, suppose that you want to solve:</p>
<pre><code class="language-math">\\frac{du_j}{dt} = -u_j + f(a\\sum_{i=0}^{n-1}\\cos\\beta(i-j) u_i)
</code></pre>
<p>which is a discrete convolution. Suppose that <code>n=20</code> so that this is 20 equations. Its not very convenient to type these equations so instead you can create a simple file in which the equations are typed just once:</p>
<pre><code># chain of 20 neurons
param a=.25,beta=.31415926
u[0..19]&#39;=-u[j]+f(a*sum(0,19)of(cos(beta*([j]-i&#39;))*shift(u0,i&#39;)))
f(x)=tanh(x)
done
</code></pre>
<p>The <code>u[0..19]</code> line with an index spanning <code>0</code> to <code>19</code> tells XPP to repeat this from 0 to 19. Thereafter, the string <code> [j]</code> or some simple arithmetic expressions of <code>j</code> are evaluated and substituted verbatim. Note how I have used the shift operator on <code>u0</code> to make it act like an array. (Recall that <code>shift(x,n)</code> gives the value of the variable that is defined <code>n</code> after the variable <code>x</code> is defined.</p>
<p>Here is one more example of nearest neighbor coupling in an excitable medium. This is a discretization of a PDE. I will use the analogue of \u201Cno flux\u201D boundaries so that the end ODEs will be defined separately.</p>
<pre><code># pulse wave
param a=.1,d=.2,eps=.05,gamma=0,i=0
v0(0)=1
v1(0)=1
f(v)=-v+heav(v-a)
#
v0&#39;=i+f(v0)-w0+d*(v1-v0)
v[1..19]&#39;=i+f(v[j])-w[j]+d*(v[j-1]-2*v[j]+v[j+1])
v20&#39;=i+f(v20)-w20+d*(v19-v20)
#
w[0..20]&#39;=eps*(v[j]-gamma*w[j])
@ meth=modeuler,dt=.1,total=100,xhi=100
done
</code></pre>
<p>I only need to define <code>v0,v20</code> separately since <code>w</code> does not diffuse. Note how much typing I saved.</p>
<p>XPP comes with some other examples that I urge you to look at. A Volterra example is shown below.</p>
`,headings:[{id:"morris-lecar-equations",text:"Morris-Lecar Equations",level:2},{id:"a-linear-cable-equation-with-boundary-conditions",text:"A linear cable equation with boundary conditions",level:2},{id:"the-delayed-inhibitory-feedback-net",text:"The delayed inhibitory feedback net",level:2},{id:"a-population-problem-with-random-mutation",text:"A population problem with random mutation",level:2},{id:"some-equations-with-flags",text:"Some equations with flags",level:2},{id:"large-coupled-systems",text:"Large coupled systems",level:2}]},{id:"04-using-the-interface",title:"Using the interface",html:`<p>Upstream XPPAUT&#39;s interface was a set of X11 windows: a main window with
its own menu, and separate windows for parameters, initial data, the
data browser, AUTO, and so on, each independently resizeable and
iconifiable. This fork (xppautX) replaced that with <strong>web2</strong>, a single
responsive page served by the program itself and opened in your browser.
It is the same XPPAUT underneath: the same menus, the same single-letter
hotkeys, the same numerics (<a href="05-commands.md">The main commands</a>,
<a href="06-numerical-parameters.md">Numerical parameters</a>). This chapter covers
what looks and behaves differently from the X11 windows the rest of this
manual otherwise describes; where a feature has no web2 equivalent yet,
it says so plainly instead of describing the old window.</p>
<h2 id="starting-xppautx">Starting xppautX</h2>
<pre><code>xppautX examples/ode/lecar.ode      # prints http://127.0.0.1:8765/?t=... and opens it
xppautX --port 9000 model.ode       # another port
xppautX --no-open model.ode         # print the address, open it yourself
xppautX --verbose | --debug model.ode  # raise the log level (see &quot;The log&quot;, below)
xppautX --version                   # which release this is
</code></pre>
<p>Everything runs on your machine: the address it prints is only reachable
from this computer and carries a one-time token. Closing the browser tab
does not stop xppautX; press <code>Ctrl+C</code> in the terminal, or use <code>File</code>
<code>Quit</code> in the page. The VS Code extension
(docs/vscode-extension.md) shows the same page in a panel and starts the
program for you.</p>
<p>xppautX also has two front-end-free modes, for scripting and automated
checks rather than interactive use:</p>
<ul>
<li><strong><code>xppautX model.ode -silent</code></strong>: no interface at all
(<code>xpp_batch_main</code>); loads the model, does whatever the ODE file&#39;s <code>@</code>
options, an options file, or further command-line flags
(<a href="16-quick-reference.md#command-line-arguments">Quick reference</a>) tell
it to (typically: integrate) and writes <code>output.dat</code>, then exits. This
is what to use from a shell script or a test.</li>
<li><strong><code>xppautX --server model.ode</code></strong>: the same JSON protocol web2 speaks
over HTTP, instead over stdin/stdout, for a process that wants to drive
xppautX directly (docs/protocol.md is the contract). <code>--script FILE</code>
replays a recorded session of that protocol (used by this project&#39;s own
checks); a recorded <code>{&quot;cmd&quot;:&quot;abort&quot;,...}</code> line replays to the same
point in a run, not just to some arbitrary later one, so an interrupted
integration or AUTO run reproduces exactly (docs/roadmap.md W10).</li>
</ul>
<p><code>--web</code> (browser mode, opening the page) is the default when none of
<code>--server</code>, <code>--script</code> or <code>-silent</code> is given.</p>
<h2 id="the-page-layout">The page layout</h2>
<p>The page has a title bar (the model&#39;s name, the connection state, a
Classic-style link is not offered \u2014 web2 is the only page), the <strong>main
menu</strong> as a panel (a column beside the plot from 48 rem wide, a drawer
below that, opened from the title bar), the plot area with its tabs, and
a <strong>status bar</strong> along the bottom that shows &quot;Working\u2026&quot; with a progress
bar and one <strong>Stop</strong> control whenever a command is running (in any view;
there is no per-view Abort button \u2014 see &quot;Long-running commands&quot; below).
What XPP prints appears under <strong>Messages</strong> at the bottom of the page as
well as in the terminal (&quot;The log&quot;, below); a problem it reports (an
illegal formula, a value out of bounds, a file it cannot read) shows as a
toast notification and stays until dismissed. One message is shown at a
time; starting the next command clears it. If the model cannot be loaded
at all, the program keeps serving the page so you can read what it
printed.</p>
<h2 id="tabs-instead-of-windows">Tabs instead of windows</h2>
<p>The plot, AUTO, the animation, an array plot, the data browser, the
equations and the ODE source are <strong>tabs</strong> above the plot area, not
separate windows (docs/front-end-gaps.md &quot;Different on purpose&quot;). A tab
comes forward by itself when its window opens in X11 terms, or when XPP
waits for a click or a key in it (grabbing a point in AUTO, for example).
Keys go to the tab you are looking at: with the AUTO tab in front, <code>a</code>,
<code>n</code>, <code>r</code>, <code>g</code>, <code>d</code>, <code>c</code>, <code>u</code>, <code>p</code> and <code>f</code> are AUTO&#39;s own keys, as they are
in the X11 AUTO window. <code>Window/Bottom</code> (raise a plot window) does
nothing: tabs replace stacking, and there is nothing to iconify or
resize \u2014 a tab always fills the space it has.</p>
<p>The equilibrium box, which upstream XPP keeps as its own small window,
appears at the top of the right-hand panel, next to the initial
conditions it can import into (<code>(I)nitial conds</code> <code>s(H)oot</code>, see
<a href="05-commands.md">The main commands</a>).</p>
<h2 id="the-values-panel">The values panel</h2>
<p>In place of the separate parameter, initial-data, delay and boundary
value windows, one <strong>values panel</strong> holds them all, always visible
beside the plot (a sheet on a phone):</p>
<ul>
<li><strong>Parameters</strong> and <strong>State</strong> are always visible. State has two
columns: <strong>Initial</strong>, the initial conditions you edit, and <strong>Now</strong>, the
last point of the latest run (read only). <code>Go</code> runs from Initial;
<code>Last</code> (Initialconds/Last) copies Now into Initial, then runs;
<strong>\u2190 Use current state</strong> copies Now into Initial without running.</li>
<li>A value takes effect when you leave the field (Tab, Enter or a click
elsewhere), so typing a value and clicking <code>Integrate</code> uses the value
you typed \u2014 there is no <code>Ok</code>/<code>Cancel</code> for the whole box, as in the X11
windows; <code>Escape</code> puts back the value that was there. Each changed
field has a reset button whose tooltip shows the ODE file&#39;s value.</li>
<li>A field takes a number or a <code>%formula</code>, exactly as the X11 boxes did
(see &quot;Formulas as values&quot;, below): <code>%2*pi</code>.</li>
<li><strong>Default</strong> puts back the values from the ODE file, as the X11
Parameter window&#39;s Default button did.</li>
<li><strong>The checkboxes</strong> next to the variables pick what <strong>x vs t</strong>,
<strong>Phase</strong> and <strong>Array</strong> plot, like <code>xvst</code>, <code>pp</code> and <code>arry</code> in X11.</li>
<li><strong>Sliders</strong> sit under the plot, any number of them (the X11 main
window had three), including the ones an ODE file sets with <code>@ s1=...</code>;
each is added or edited with a dialog (searchable variable, min, max,
step/precision), not the small binding window upstream describes.
Dragging one changes the value and integrates again.</li>
<li><strong>Buttons the ODE file defines</strong> (<code>@ but=name:keys</code>) appear above the
sliders.</li>
<li><strong>Boundary conditions</strong> and <strong>Delay initial data</strong> are collapsed
sections; delays appear only for delay equations.</li>
<li><strong>Data</strong> opens the Data tab (<a href="07-data-browser.md">The Data Browser</a>),
<strong>Equations</strong> lists the equations (the X11 equation-listing window).</li>
</ul>
<p>Sections can be collapsed and their state (and the sliders) saved to and
loaded from a settings file.</p>
<h2 id="plots-and-axes">Plots and axes</h2>
<p>Each plot window from X11 (a phase plane in xy mode, a time plot in
aligned mode) is a tab, starting at the window&#39;s own axes
(<code>Viewaxes</code>), keeping its own zoom while tabs switch, and all done
without a round trip to the core: drag a box to zoom, the wheel zooms
about the pointer, Shift+drag or the middle button pans, pinch and
one-finger drag on touch, a double click (or <code>0</code>) goes back to the
window&#39;s axes, <code>Ctrl+Z</code> undoes a zoom step, the nearest point is named
under the mouse, on a tap, or by stepping with <code>[</code> <code>]</code> from the keyboard,
curves can be hidden from the legend, and PNG and CSV export what is
shown. &quot;Use this view&quot; (<code>Window</code> <code>Fit</code>, or the plot&#39;s own control) makes
the client&#39;s current zoom the window&#39;s axes, so PostScript/SVG export and
Restore agree with what&#39;s on screen.</p>
<p>Nullclines, direction fields, equilibria (Sing pts), Text/etc labels and
markers, and frozen curves all draw on the same plot from data the core
sends (docs/ui-v2.md sections 2 and 3), not as separate drawing
operations, so they scale and theme with the rest of the page.</p>
<p>3D plots are XPP&#39;s curves-in-a-box, projected and rotated in the browser
with the window&#39;s angles; rotate by dragging or the keys that rotated the
X11 window.</p>
<h2 id="the-auto-view">The AUTO view</h2>
<p>See <a href="09-auto.md#the-auto-view">Auto interface</a>.</p>
<h2 id="the-animation-view">The animation view</h2>
<p>See <a href="10-animations.md#the-animation-view">Creating Animations</a>.</p>
<h2 id="dialogs-and-prompts">Dialogs and prompts</h2>
<p>Every X11 pop-up menu, string box, form, yes/no and file selector is now
a dialog: <code>role=dialog</code>, focus moves to its first field, Tab cycles
inside it, Escape cancels, and focus returns to where it was. A form
field that picked from a fixed X11 list (<code>*n</code> in the protocol) is now a
proper select; a mouse-driven prompt (pick a point, drag a box, drag the
plot) is a mode of the plot itself, with an instruction bar and Cancel,
answered by clicking, tapping or the keyboard (arrows move a crosshair or
corner, Enter picks or fixes it) \u2014 never in raw pixels, so the same
prompt works with a mouse, a trackpad or touch.</p>
<h2 id="long-running-commands">Long-running commands</h2>
<p>There is no per-view Abort button (upstream&#39;s Escape-to-abort still
works from the keyboard). The status bar&#39;s <strong>Stop</strong> is the one control
for whatever is running, in any view; it exists only while something
runs and turns into a disabled &quot;Stopping\u2026&quot; until the run&#39;s next idle
point. A view&#39;s own close (\xD7) means &quot;done with it&quot;: it stops a running
job first, then closes; <strong>Stop</strong> itself keeps the view and whatever
partial result it has (an interrupted AUTO branch ending on its last
point, ready to Grab and continue). Buttons that would queue behind the
run (<code>Integrate</code>) are disabled while it runs, and other clicks show
&quot;Busy \u2014 press Stop to stop&quot; instead of doing nothing.</p>
<h2 id="saving-pictures-and-files">Saving pictures and files</h2>
<p>Files (PostScript/SVG, GIF, <code>.dat</code>, <code>.set</code>, tables, kinescope frames) are
written next to the ODE file, by the program, exactly as in X11. The
browser never silently downloads anything on its own.</p>
<ul>
<li><strong>Open</strong> (Read set, Load diagram, the browser&#39;s Load, ...): the page
shows the browser&#39;s own file picker; picked files are uploaded into
xppautX&#39;s working directory (the model&#39;s folder) so relative names in
<code>#include</code>, tables and diagrams keep resolving as they always have. A
name that already exists with different content asks to Replace, Keep
both, or Cancel.</li>
<li><strong>Save</strong> (Write set, Save diagram, PostScript/SVG, ...): where the
browser supports it, a native Save dialog is offered with the name
suggested; xppautX writes the file into the working directory and the
page then offers (or directly saves) that same copy. Elsewhere the page
offers the file as a download. Either way the working directory has
the latest copy, so a later Read by name finds it.</li>
<li><strong>Missing companions</strong>: when xppautX reports it cannot open a file, the
notification offers &quot;Add file\u2026&quot;, which uploads it under that name and
repeats the command.</li>
<li><strong>The core&#39;s own file listing</strong> stays reachable as a second tab of the
dialog (&quot;In the model&#39;s folder&quot;), for the rare case that needs a path
elsewhere on the machine running xppautX.</li>
</ul>
<p>Two differences from X11 worth knowing:</p>
<ul>
<li>Pictures saved as GIF or PPM (kinescope, animation frames, array plots)
come from what the browser drew, not a copy of an X11 pixmap, and their
colours are rounded to 216 shades (the GIF format takes 256 colours,
and a browser smooths its lines).</li>
<li>Kinescope frames live in the page as data (not bitmaps): reloading the
page loses them.</li>
</ul>
<h2 id="what-still-needs-a-compiler">What still needs a compiler</h2>
<p>A model that loads user C functions (<code>load dll</code>,
<a href="11-dll-libraries.md">Creating C-files for faster simulations</a>) needs
that library built for the machine that runs xppautX; the Windows build
loads <code>.dll</code> files. Everything else in XPPAUT works without any
compiler.</p>
<p><code>Help</code> and &quot;Edit .xpprc&quot; open a browser or an editor on the machine that
runs the program, as in X11 (the <code>XPPHELP</code>, <code>XPPBROWSER</code>, <code>XPPEDITOR</code>
environment variables, <a href="01-introduction.md">Introduction</a>). If you ever
run the server on another machine (a remote VS Code session, say), they
appear there, not in front of you. A proper in-app Help view, searching
this manual, is planned (docs/roadmap.md W12b) and will replace this.</p>
<h2 id="the-log">The log</h2>
<p>What xppautX prints \u2014 including AUTO&#39;s table (<code>Output</code>, in the AUTO
view) \u2014 goes to <strong>Messages</strong> at the bottom of the page, as well as to
the terminal or <code>-logfile</code>&#39;s file, quiet by default (warnings and
errors); <code>--verbose</code>/<code>--debug</code> raise the level (core/xpp_log.h). The
core never prints to stdout or stderr directly, so nothing bypasses
Messages.</p>
<h2 id="formulas-as-values">Formulas as values</h2>
<p>You can enter a formula instead of a plain number in the values panel or
almost any dialog field that asks for one: the first character must be
<code>%</code>, e.g. <code>%2*pi</code> or <code>%sin(1.5)</code>; it is evaluated and converted to a
number when the field takes effect.</p>
`,headings:[{id:"starting-xppautx",text:"Starting xppautX",level:2},{id:"the-page-layout",text:"The page layout",level:2},{id:"tabs-instead-of-windows",text:"Tabs instead of windows",level:2},{id:"the-values-panel",text:"The values panel",level:2},{id:"plots-and-axes",text:"Plots and axes",level:2},{id:"the-auto-view",text:"The AUTO view",level:2},{id:"the-animation-view",text:"The animation view",level:2},{id:"dialogs-and-prompts",text:"Dialogs and prompts",level:2},{id:"long-running-commands",text:"Long-running commands",level:2},{id:"saving-pictures-and-files",text:"Saving pictures and files",level:2},{id:"what-still-needs-a-compiler",text:"What still needs a compiler",level:2},{id:"the-log",text:"The log",level:2},{id:"formulas-as-values",text:"Formulas as values",level:2}]},{id:"05-commands",title:"The main commands",html:`<p>All commands can be invoked by typing the hot key for that command (capitalized on the menu) or clicking on the menu with the mouse. Usually most commands can be aborted by pressing the <code>Esc</code> key. Once one of these is chosen, the program begins to calculate and draw the trajectories. If you want to stop prematurely, press the <code>Esc</code> key and the integration will stop.</p>
<p><strong>In web2</strong>, the main menu and every one of these commands and hotkeys
are unchanged; see <a href="04-using-the-interface.md">Using the interface</a> for
the menu panel, the values panel (parameters, ICs, sliders), where dialogs
and prompts appear, and how Stop (the status bar) replaces the X11
window-specific Abort/Esc behaviour for a long-running command described
below.</p>
<h3 id="initial-conds">(I)nitial conds</h3>
<p>This invokes a list of options for integrating the differential equations. The choices are:</p>
<ul>
<li><strong>(R)ange</strong>: This lets you integrate multiple times with the results shown in the graphics window. Pressing this option produces a new window with several boxes to fill in. First choose the quantity you want to range over. It can be a parameter or a variable. The integrator will be called and this quantity will be changed at the beginning of each integration. Then choose the starting and ending value and the number of steps. The option Reset storage only stores the last integration. If you choose not to reset, each integration is appended to storage. Most likely, storage will be exceeded and the integration will overwrite or stop. The option to use last initial conditions will automatically use the final result of the previous integration as initial dat for the next integration. Otherwise, the current ICs will be used at each step (except of course for the variable through which you are ranging.) If you choose <code>Yes</code> in the <code>Movie</code> item, then after each integration, XPP will take a snapshot of the picture. You can then replay this series of snapshots back using the Kinescope. When you are happy with the parameters, simply press the OK button. Otherwise, press the Cancel button to abort. Assuming that you have accepted, the program will compute the trajectories and plot them storing none of them or all of them. If you press <code>Esc</code> it will abort the current trajectory and move on to the next. Pressing the <code>/ </code> key will abort the whole process.</li>
<li><strong>(2)par range</strong>: is similar to range integration but allows you to range over two items. The <code>Crv(1) Array(2)</code> item determines how the range is done. If you choose <code>Crv</code> then the two paramaters are varied in concert, $<code>[a(i),b(i)]</code>$ for $<code>i=0,\\ldots,N</code>$. The more useful <code>Array</code> varies them independently as $<code>[a(i),b(j)]</code>$ for $<code>i=0,\\ldots,N</code>$ and $<code>j=0,\\ldots,M.</code>$</li>
<li><strong>(L)ast</strong>: This uses the end result of the most recent integration as the starting point of the curret integration.</li>
<li><strong>(O)ld</strong>: This uses the most recent initial data as the current initial data. It is essentially the same as Go.</li>
<li><strong>(G)o</strong>: which uses the initial data in the IC window and the current numerics parameters to solve the equation. The output is drawn in the current selected graphics window and the data are saved for later use. The solution continues until either the user aborts by pressing <code>Esc</code>, the integration is complete, or storage runs out.</li>
<li><strong>(M)ouse</strong>: allows you to specify the values with the mouse. Click at the desired spot.</li>
<li><strong>(S)hift</strong>: This is like Last except that the stating time is shifted to the current integration time. This is irrelevant for autonomous systems but is useful for nonautonomous ODEs.</li>
<li><strong>(N)ew</strong>: This prompts you at the command line for each initial condition. Press <code>Return</code> to accept the value presented.</li>
<li><strong>s(H)oot</strong>: allows you to use initial data that was produced when you last searched for an equilibrium. When a rest state has a single positive or negative eigenvalue, then XPP will ask if you want to approximate the invariant manifold. If you choose <code>yes</code> to this, then the initial data that were used to compute the trajectories are remembered. Thus, when you choose this option, you will be asked for a number 1-4. This number is the order in which the invariant trajectories were computed. Note if the invariant set is a stable manifold, then you should integrate backwards in time.</li>
<li><strong>(F)ile</strong>: prompts you for a file name which has the initial data as a sequence of numerical values.</li>
<li><strong>form(U)la</strong>: allows you to set all the initial data as a formula. This is good for systems that represent chains of many ODEs. When prompted for the variable, type in <code>u[2..10]</code> for example to set the variables <code>u2,u3, ..., u10</code> and then put in a formula using the index <code>[j]</code>. Note you must use <code>[j]</code> and not <code>j</code> by itself. For example <code>sin([j]*2*pi/10)</code>. Repeat this for different variables hitting enter twice to begin the integration.</li>
<li><strong>M(I)ce</strong>: allows you to choose multiple points with the mouse. Click Esc when done.</li>
<li><strong>(D)AE guess</strong>: lets you choose a guess for the algebraic variables of the DAE.</li>
<li><strong>(B)ackward</strong>: is the same as \u201CGo\u201D but the integration is run backwards in time.</li>
</ul>
<h3 id="continue">(C)ontinue</h3>
<p>This allows you to continue integrating appending the data to the current curve. Type in the new ending time.</p>
<h3 id="nullclines">(N)ullclines</h3>
<p>This option allows you to draw the nullclines of systems. They are most useful for two-dimensional models, but XPP lets you draw them for any model. The constraints are the same as in the direction fields option above. The menu has 4 items.</p>
<ul>
<li><strong>(N)ew</strong>: draws a new set of nullclines.</li>
<li><strong>(R)estore</strong>: restores the most recently computed set.</li>
<li><strong>(A)uto</strong>: turns on a flag that makes XPP redraw them every time it is necessary because some other window obscured them.</li>
<li><strong>(M)anual</strong>: turns this flag off so that you must restore them manually. The X-axis nullcline is blue and the Y-axis nullcline is red.</li>
<li><strong>(F)reeze</strong>: allows you to freeze and play back multiple nullclines<ul>
<li><strong>(F)reeze</strong>: freezes the current nullclines</li>
<li><strong>(D)elete all</strong>: deletes all frozen nullclines</li>
<li><strong>(R)ange</strong>: lets you vary a parameter through some range, computes the nullclines and stores them</li>
<li><strong>(A)nimate</strong>: redraws all the frozen nullclines erasing the screen between each one. The user specifies a delay between drawing.</li>
</ul>
</li>
<li><strong>(S)ave</strong>: saves the nullclines into a file. They can then be plotted using other software. <strong>NOTE:</strong> The way that XPP computes nullclines (by computing zero contours of a two-variable function) means that the nullclines are composed of a series of small line segments. This means that if you try to plot them as a continuous curve, your plotting program will produce garbage. Thus, you should plot them as points and not draw line segments between them. The data file produced has 4 columns. The first two are the \u201Cx\u201D nullcline and the second two are the \u201Cy\u201D nullcline. Data is as follows:</li>
<li>xnx1 xny1 ynx1 yny1     xnx2 xny2 ynx2 yny2     ...</li>
<li>There are an even number of entries. The \u201Cx\u201D nullcline consists of segments <code>(xnx1,xny1),(xnx2,xny2)</code> between pairs of points. Similarly for the \u201Cy\u201D nullcline.</li>
</ul>
<h3 id="direction-fieldflow">(D)irection Field/Flow</h3>
<p>This option is best used for two-dimensional systems however, it can be applied to any system. The current graphics view must be a two-d plot in which both variables are different and neither is the time variable, T. There are five items.</p>
<ul>
<li><strong>(D)irection fields</strong>: Choosing the direction field option will prompt you for a grid size. The two dimensional plane is broken into a grid of the size specified and lines are drawn at each point specifying the direction of the flow at that point. The length of the line gives the magnitude. If the system is more than two-dimensional, the other variables will be held at the values in the initial conditions window.</li>
<li><strong>(F)low</strong>: Choosing the flow, you will be prompted for a grid size and trajectories started at each point on the grid will be integrated according to the numerical parameters. Any given trajectory can be aborted by pressing <code>Esc</code> and the whole process stopped by pressing <code>/</code>. The remaining variables if in more than two-dimensions are initialized with the values in the IC window.</li>
<li><strong>(N)o Dir Field</strong>: turns off redrawing of direction fields when you click on (Redraw). Erasing the screen automatically turns this off.</li>
<li><strong>(C)olorize</strong>: This draws a grid of filled rectangles on the screen whose color is coded by the velocity or some other quantity. (See the Numerics Colorize menu item).</li>
<li><strong>(S)caled Dir. Fld</strong>: is the same as (D)irection field but the lengths are all scaled to 1 so only directional information is given.</li>
</ul>
<h3 id="window">(W)indow</h3>
<p>allows you to rewindow the current graph. Pressing this presents another menu with the choices:</p>
<ul>
<li><strong>(W)indow</strong>: A parameter box pops up prompting you for the values. Press OK or CANCEL when done.</li>
<li><strong>(Z)oom in</strong>: Use the mouse to expand a region by clicking, dragging and releasing. The view in the rectangle will be expanded to the whole window.</li>
<li><strong>Zoom (O)ut</strong>: As above but the whole window will be shrunk into the rectangle.</li>
<li><strong>(F)it</strong>: The most common command will automatically fit the window so the entire curve is contained within it. For three-D stuff the window data will be scaled to fit into a cube and the cube scaled to fit in the window. Use this often.
(In web2, Zoom/Zoom out/Fit are client-side plot modes drawn on the canvas \u2014 see <a href="04-using-the-interface.md#plots-and-axes">Using the interface</a> \u2014 so there is no <code>-xorfix</code>-style rubber-band drawing bug to work around.)</li>
</ul>
<h3 id="phase-space">ph(A)se space</h3>
<p>XPP allows for periodic domains so that you can solve equations on a torus or cylinder. You will be prompted to make (A)ll variables periodic, (N)o variables periodic or (C)hoose the ones you want. You will be asked for the period which is the same for all periodic variables (if they must be different, rescale them) Choose them by clicking the appropriate names from the list presented to you. An <code>X</code> will appear next to the selected ones. Clicking toggles the <code>X</code>. Type <code>Esc</code> when done or CANCEL or DONE. XPP mods your variables by this period and is smart enough when plotting to not join the two ends.</p>
<h3 id="kinescope">(K)inescope</h3>
<p>This allows you to capture the active window and play it back. Another menu pops up with the choices:</p>
<ul>
<li><strong>(C)apture</strong>: which takes a snapshot of the currently active window</li>
<li><strong>(R)eset</strong>: which deletes all the snapshots</li>
<li><strong>(P)layback</strong>: which cycles thru the pictures each time you click the left mouse button and stops if you click the middle.</li>
<li><strong>(A)utoplay</strong>: continuously plays back snapshots. You tell it how many cycles and how much time between frames in milliseconds.</li>
<li><strong>(S)ave</strong>: Save the frames in either ppm or gif format</li>
<li><strong>(M)ake anigif</strong>: Create an animated gif from the frames. The file is always called <code>anim.gif</code>.</li>
</ul>
<p><strong>In web2</strong> a snapshot is data (the series, marks and viewport of the
active plot window, docs/ui-v2.md T15), not a bitmap: reloading the
page loses captured frames (they live in the client), and Save/Make
anigif render the GIF or PNG in the page itself from that data, so the
picture is only ever as good as what&#39;s on screen, not a copy of an X11
pixmap.</p>
<h3 id="graphic-stuff">(G)raphic stuff</h3>
<p>This induces a popup menu with several choices.</p>
<ul>
<li><strong>(A)dd curve</strong>: This lets you add another curve to the picture. A parameter box will appear aking you for the variables on each axis, a color, and line type (1 is solid, 0 is a point, and negative integers are small circles of increasing radii.) All subsequent integrations and restorations will include the new graph. Up to 10 per window are allowed.</li>
<li><strong>(D)elete last</strong>: Will remove most recent curve from the added list.</li>
<li><strong>(R)emove all</strong>: Deletes all curves but the first.</li>
<li><strong>(E)dit curve</strong>: You will be asked for th curve to edit. The first is 0, the second 1, etc. You will get a parameter box like the add curve option.</li>
<li><strong>(P)ostscript</strong>: This will ask you for a file name and write a postscript representation of the current window. Nullclines, text, and all graphs will be plotted. You will be asked for Black and White or Color. Color tries to match the color on the screen. Black and white will use a variety of dashed curves for the plots. The Land/Port option lets you draw either in Landscape (default) or Portrait style. Note that Portrait is rather distorted and is created in for those who cannot rotate their postscript plots. Font size sets the size of the fonts on the axes.</li>
<li><strong>(F)reeze</strong>: This will create a permanent curve in the window. Usually, when you reintegrate the equations or load in some new data, the current curve will be replace by the new data. Freeze prevents this. Up to 26 curves can be frozen.<ul>
<li><strong>(F)reeze</strong> : This freezes the current curve 0 for the current plotting window. It will not be plotted in other windows. If you change the axes from 2 to 3 dimensions and it was frozen as a 2D curve (and <em>vice versa</em> ) then it will also not be plotted. It is better to create another window to work in 3 dimensions so this is avoided. A parameter box pops up that asks you for the color (linetype) as well as the key name and the curve name. The curve name is for easy reference and should be a few characters. The key name is what will be printed on the graph if a key is present.</li>
<li><strong>(D)elete</strong>: This gives you a choice of available curves to delete.</li>
<li><strong>(E)dit</strong>: This lets you edit a named curve; the key, name, and linetype can be altered.</li>
<li><strong>(R)emove all</strong>: This gets rid of all of the frozen curves in the current window.</li>
<li><strong>(K)ey</strong>: This turns the key on or off. If you turn it on, then you can position it with the mouse on the graph. The key consists of a line followed by some text describing the line. Only about 15 characters are permitted.</li>
<li><strong>(B)if.diag</strong>: will prompt you for a filename and then using the current view, draw the diagram. The file must be of the same format as is produced by the <code>Write pts</code> option in the AUTO menus. (see AUTO below.) The diagram is colored according to whether the points are stable/unstable fixed points or periodics. The diagram is \u201Cfrozen\u201D and there can only be one diagram at a time.</li>
<li><strong>(C)lr. BD</strong>: clears out the current bifurcation diagram.</li>
<li><strong>(O)n freeze</strong>: Toggles a flag that automatically freezes the curves as you integrate them.</li>
</ul>
</li>
<li><strong>a(X)es opts</strong>: This puts up a window which allows you to tell XPP where you want the axes to be drawn, whether you want them, and what fontsize to make the PostScript axes labels.</li>
<li><strong>exp(O)rt</strong>: This lets you save the points that are currently plotted on the screen in XY format. Thus if you have a phaseplane on the screen, only the X and Y values are saved. This makes it compatible with porgrams like XMGR which assume X Y1 Y2 ... data. If you have several traces on the screen at once, it saves the X values of the first trace and the Y values of the first and all subsequent traces.</li>
<li><strong>(C)olormap</strong>: This lets you choose a different color map from the default. There are a bunch of them; try them all and pick your favorite.</li>
</ul>
<h3 id="numerics">n(U)merics</h3>
<p>This is so important that a section is devoted to it. See below.</p>
<h3 id="file">(F)ile</h3>
<p>This brings up a menu with several options. Type <code>Esc</code> to abort.</p>
<ul>
<li><strong>(P)rt info</strong>: Brings up a window with the source code for the ODE file. If you click on <code>Action</code> it brings up the active comments so you can make little tutorials.</li>
<li><strong>(W)rite set</strong>: This creates a file with all of the info about the current numerics, etc as well as all of the currently highlighted graphics window. It is readable by the user. It in some sense saves the current state of XPP and can be read in later.</li>
<li><strong>(R)ead set</strong>: This reads a set that you have previously written. The files are very tightly connected to the current ODE file so you should not load a saved file from one equation for a different problem.</li>
<li><strong>(A)uto</strong>: This brings up the AUTO window if you have installed AUTO. See below for a description of this.</li>
<li><strong>(C)alculator</strong>: This pops up a little window. Type formulae in the command line involving your variables and the results are displayed in the popup. Click on Quit or type <code>Esc</code> to exit.</li>
<li><strong>(E)dit</strong>: You can edit the equations from within XPP. <em>Note that XPP is capable of understanding right-hand sides of up to 256 characters. However, the RHS editor will not accept anything longer than about 72 characters.</em> This menu item presents a list of four options:<ul>
<li><strong>(R)HS</strong>: Edit the right-hand sides of the ODEs IDEs, and auxiliary variables. If you are happy with the editing, then type <code>TAB</code> or click on <code>OK.</code> The program will parse the new equations and if they are syntactically correct, alter the corresponding equation. If there is an error, the you will be told of the offending right-hand-side and that will not be changed.</li>
<li><strong>(F)unctions</strong>: This lets you alter any user-defined functions. It is otherwise the same as the above.</li>
<li><strong>(S)ave as</strong>: This creates an \u201CODE\u201D file based on the current parameter values, functions, and right-hand sides. You will be asked for a filename.</li>
<li><strong>(L)oad DLL</strong>: invokes the dynamic linker. You can load in complicated RHS\u2019s that would be awkward to create using XPP\u2019s simple language.</li>
</ul>
</li>
<li><strong>(S)ave info</strong>: This is like <code>(P)rt info</code> but saves the info to a file. It is human readable.</li>
<li><strong>(B)ell on/off</strong>: This toggles the stupid noisy bell on and off.</li>
<li><strong>C-(H)ints</strong>: spews out stuff to the terminal that represents a skeletal C program for the right-hand sides. Presumably, you could use this to create faster code by replacing the file <code>myrhs.c</code> with your compiled version. Hah! I don\u2019t khow why this is even here \u2013 Good luck!.</li>
<li><strong>(Q)uit</strong>: This exits XPP first asking if you are sure.</li>
<li><strong>(T)ranspose</strong> : This is not a very good place to put this but I stuck it here just to get it into the program. The point of this routine is to allow one to transpose chunks of the output. For example, if you are solving the discretization of some spatial problem and find a steady state, there is no way to plot the steady state as a function of the index of the discrete system. This routine lets you do that. The idea is to take something that looks like:</li>
<li>t1  x11  x21  x31 ... xm1      t2  x12  x22  x32 ... xm2     ...     tn  x1n  x2n  x3n ... xmn</li>
<li>and transpose some subset of it. You are prompted for 6 items. They are the name of the first column you want to index, the number of columns (<code>ncols</code> and amount you want to skip across columns, <code> colskip</code> (so that <code>colskip = 2</code> would be every other column. You must also provide the starting row <code>j1</code>, the number of rows, <code> nrows</code> and the row skip, <code>rowskip.</code> the The storage array is temporarily replaced by a new array that has <code>M=ncols</code> rows and <code>nrows+1</code> columns (since the data is transposed, the rows and columns are as well; confusing ain\u2019t it). The form of the array is:</li>
<li>1  x(i1,j1) x(i1,j2) x(i1,j3) ...     2  x(i2,j1) x(i2,j2) x(i2,j3) ...     ...     M  x(iM,j1) x(iM,j2) x(iM,j3) ...</li>
<li>where <code>i2=i1+colskip, i3=i1+2*colskip, ...</code> and <code>i1</code> is the index corresponding to the name of the first column you provide. Similarly, <code>j2=j1+rowskip, ...</code>. As a brief example, suppose that you solve a system of equations of the form: <code>math x_j&#39; = f(x_{j-1},x_j,x_{j+1},I_j)</code> where $<code>j=1,\\dots,20.</code>$ Click on transpose and choose <code>x1</code> as the first column, <code>colskip=1, ncols=20</code> and say <code>row1=350, nrows=1,rowskip=1</code> then a new array will be produced. The first column is the index from 1 to 20 and the second is <code>xj(350)</code> where 350 is the index and not the actual value of time. By plotting the second column versus the first you get a \u201Cspatial profile.\u201D</li>
<li><strong>t(I)ps</strong>: This toggles the tips on and off that appear in the message line.</li>
<li><strong>(G)et par set</strong>: This loads one of the parameter sets that you have defined in the ODE file.</li>
</ul>
<h3 id="parameters">(P)arameters</h3>
<p>Type the name of a parameter to change and enter its value. Repeat for more parameters. Hit <code>Enter</code> a few times to exit. Type <code>default</code> to get back the values when you started XPP. Use this if you don\u2019t want to mess with the mouse.</p>
<h3 id="erase">(E)rase</h3>
<p>erases the contents of the active window, redraws the axes, deletes all text in the window, and sets the redraw flag to Manual.</p>
<h3 id="make-window">(M)ake window</h3>
<p>The option allows you to create and destroy graphics windows. There are several choices.</p>
<ul>
<li><strong>(C)reate</strong>: makes a copy of the currently active window and makes itself active. You can change the graphs in this window without affecting the other windows.</li>
<li><strong>(K)ill all</strong>: Removes all but the main graphics window.</li>
<li><strong>(D)estroy</strong>: This destroys the currently active window. The main window cannot be destroyed.</li>
<li><strong>(B)ottom</strong>: puts the active window on the bottom.</li>
<li><strong>(A)uto</strong>: turns on a flag so that the window will automatically be redrawn when needed.</li>
<li><strong>(M)anual</strong>: turns off the flag and the user must restore the picture manually.</li>
<li><strong>(S)imPlot on/off</strong>: lets you plot the solution in all active windows while the simulation is running. This slows you down quite a bit.</li>
<li>After a window is created, you can use the mouse to find the coordinates by pressing and moving in the window. The coordinates are given near the top of the window.</li>
</ul>
<h3 id="text-etc">(T)ext, etc</h3>
<p>allows you to write text to the display in a variety of sizes and in two different fonts. You can also add other symbols to your graph.</p>
<ul>
<li><strong>Text</strong>: This prompts you for the text you want to add. Then you are asked for the size; there are five choices (0-5): 0-8pt, 1-12pt, 2-14pt, 3-18pt, 4-24pt. Text also has several escape sequences:<ul>
<li>$<code>\\backslash</code>$<!-- -->1 \u2013 switches to Greek font</li>
<li>$<code>\\backslash</code>$<!-- -->0 \u2013 switches to Roman font</li>
<li>$<code>\\backslash</code>$s \u2013 subscript</li>
<li>$<code>\\backslash</code>$S \u2013 superscript</li>
<li>$<code>\\backslash</code>$n \u2013 neither sub nor superscript</li>
<li>$<code>\\backslash</code>\${expr} \u2013 evaluate the expression in the braces before rendering.</li>
</ul>
</li>
<li>Note that not all X-servers will have these fonts, but the postscript file will still draw them. Finally, place the text with the mouse.</li>
<li><strong>Arrow</strong>: This lets you draw an arrow-head to indicate a direction on a trajectory. You will be prompted for the size, which should be some positive number, usually less than 1. Then you must move the the mouse and select a direction and starting point. Click on the starting point and holding the mouse button down, drag the mouse to indicate the direction of the arrow-head. Then release the mouse-button and the arrow will be drawn.</li>
<li><strong>Pointer</strong>: This is like an arrow, but draws the stem as well as the arrow head. It can be used to point to important features of your graph. The prompts are like those for <code>Arrow.</code></li>
<li><strong>Marker</strong>: This lets you draw little markers, such as triangles, squares, etc on the picture. When prompted to position the marker with the mouse, you can over-ride the mouse and manually type in coordinates if you hit the (Tab) key.</li>
<li><strong>Edit</strong>: This lets you edit the text, arrows, and pointers in one of three ways:<ul>
<li><strong>Move</strong>: lets you move the object to another location without changing any of its properties. Choose the object with the mouse by clicking near it. You will then be prompted as to whether you want to move the item that XPP selected. If you answer <code>yes</code> use the mouse to reposition it.</li>
<li><strong>Change</strong>: lets you change the properties: for text, the text itself, size, and font can be change; for arrows and pointers, only the size of the arrow head can be changed. As above, select the object with the mouse and then edit the properties.</li>
<li><strong>Delete</strong>: deletes the object that you select with the mouse.</li>
</ul>
</li>
<li><strong>(D)elete All</strong>: Deletes all the objects in the current window.</li>
<li><strong>marker(S)</strong>: This is similar to the Marker command, but allows you to automatically mark a number of points along a computed trajectory. You use the data browser to move the desired starting point of the list to the top line of the browser. Then click on the (Text) (markerS) command and choose a size and color. Then tell XPP how many markers and how many browser lines to jump between markers. (Thus, 10 would put a marker at every 10th data point)..</li>
</ul>
<h3 id="sing-pts">(S)ing pts</h3>
<p>This allows you to calculate equilibria for a discrete or continuous system. The program also attempts to determine stability for delay-differential equations (see below in the numerics section.) There are three options.</p>
<ul>
<li><strong>(G)o</strong>: begins the calculation using the values in the initial data box as a first guess. Newton\u2019s method is applied. If a value is found XPP tries to find the eigenvalues and asks you if you want them printed out. If so, they are written to the console. Then if there is a single real positive or real negative eigenvalue, the program asks you if you want the unstable or stable manifolds to be plotted. Answer yes if so and they will be approximated. The calculation will continue until either a variable goes out of bounds or you press <code>Esc</code>. If <code>Esc</code> is pressed, the other branch is computed. (Unstable manifolds are yellow and computed first followed by the stable manifolds in color turquoise.) The program continues to find any other invariant sets until it has gotten them all. These are not stored, however, the initial data needed to create them are and can be accessed with the <code>Initial Conds</code> <code> sHoot</code> command. Once an equilibrium is computed a window appears with info on the value of the point and its stability. The top of the window tells you the number of complex eigenvalues with positive,<code>(c+)</code>, negative <code>(c-)</code>, zero <code>(im)</code> real parts and the number of real positive <code>(r+)</code> and real negative <code>(r-)</code> eigenvalues. If the equation is a difference equation, then the symbols correspond the numbers of real or complex eigenvalues outside <code>(+)</code> the unit circle or inside <code>(-)</code> This window remains and can be iconified.</li>
<li><strong>(M)ouse</strong>: This is as above but you can specify the initial guess by clicking the mouse. Only the two variables in the two-D window will reflect the mouse values. This is most useful for 2D systems.</li>
<li><strong>(R)ange</strong>: This allows you to find a set of equilibria over a range of parameters. A parameter box will prompt you for the parameter, starting and ending values, number of steps. Additionally, two other items are requested. Column for stability will record the stability of the equilibrium point in the specified column (Use column number greater than 1). If you elect to shoot at each, the invariant manifolds will be drawn for each equilibrium computed. The stability can be read as a decimal number of the form <code>u.s </code> where <code>s</code> is the number of stable and <code>u</code> the number of unstable eigenvalues. So <code>2.03</code> means 3 eigenvalues with negative real parts (or in the unit circle) and 2 with positive real parts (outside the unit circle.) For delay equations, if a root is found, its real part is included in this column rather than the stability summary since there are infinitely many possible eigenvalues. The result of a range calculation is saved in the data array and replaces what ever was there. The value of the parameter is in the time column, the equilibria in the remaining columns and the stability info in whatever column you have specified. <code>Esc</code> aborts one step and <code>/</code> aborts the whole procedure. As with the initial data/range option, you can also make a movie. This is useful mainly for systems where invariant sets are to be computed.</li>
</ul>
<h3 id="view-axes">(V)iew axes</h3>
<p>This selects one of different types of graphs: a 2D box or a 3D Box; brings up a threed window; or lets you create animations of your simulation. If you select the 2D curve, you will be asked for limits as in the window command as well as the variables to place on the axes and the labels for the axes. 3D is more complicated. You will be asked for the 3 variables for the 3 axes, their max and min values and 4 more numbers, <code>XLO</code>, etc. XPP first scales the data to fit into a cube with corners (-1,-1,-1) and (1,1,1). Rotation of this cube is performed and then projected into the two-D window. <code>XLO</code>, etc define the scales of this projection and are thus unrelated to the values of your data. You are also asked for labels of the axes. Use the <code>(F)it </code> option if you don\u2019t know whats going on.</p>
<ul>
<li>(A)rray plots introduce a new window that lets the user plot many variables at once as a function of time with color coded values. The point is to let one plot, e.g., an array of voltages, <code>V1, V2, ..., VN</code> across the horizontal as time varies in the vertical dimension. For example, suppose you have discretized some PDE and want to see the evolution in space and time of the variables. Then use this plotting option. You will be prompted for the first column name of the \u201Carray\u201D, then number of columns, the first row, then number of rows, and the number of rows to skip. For example, if the discretized PDE variables are <code>u0,u1, ... ,u50</code>, then type in <code> u0</code> for the first element and <code>51</code> for the number of columns. If you want rows 200 through 800 only every 4th time unit, you would put 200 for the first row, 201 as the number of rows, and 4 as the skip value. You can also set the column skip as well. This is useful if you have defined a series of variables with the array blocks. If the array block is for a two-component system, then plot every 2, so you would put 2 in this entry. The <code>Print</code> button asks you for a file name and top and bottom labels and a render style. The render styles are:<ul>
<li><strong>-1</strong>: Grey scale</li>
<li><strong>0</strong>: Blue-red</li>
<li><strong>1</strong>: Red-Yellow-Green-Blue-Violet</li>
<li><strong>2</strong>: Like 1 but periodic</li>
</ul>
</li>
<li>The <code>Style</code> button does nothing yet. The <code>Edit</code> button lets you change ranges and arrays to plot. The <code>Redraw</code> button is obvious.</li>
<li>Since the animation option requires learning lots of new stuff, see <a href="10-animations.md">Creating Animations</a> for a description of the animation language and what you can do with it.</li>
</ul>
<h3 id="xi-vs-t">(X)i vs t</h3>
<p>This chooses a certain 2D view and prompts you for the variable name. The window is automatically fitted and the data plotted. It is a shortcut to choosing a view and windowing it.</p>
<h3 id="restore">(R)estore</h3>
<p>redraws the most recent data in the browser in accordance with the graphics parameters of the active window.</p>
<h3 id="3d-params">(3)d params</h3>
<p>This lets you choose rotations of the axes and perspective planes. Play with this to see. You must be have a 3D view in the active graph to use this.</p>
<ul>
<li>There is another selection: <code>Movie</code>. If you choose <code> Yes</code> for this, then after you click <code>Ok</code>, you will be prompted for some additional parameters. There are two angles you can vary, <code> theta</code> and <code>phi</code>. Choose one, give an initial value, an increment, and the number of rotations you want to perform. XPP will then use the <code>Kinescope</code> to take successive snapshots of the screen after performing each rotation. You can then play these back from the Kinescope or save them as an animated gif.</li>
</ul>
<h3 id="bndry-val">(B)ndry val</h3>
<p>This solves boundary value problems by numerical shooting. There are 4 choices.</p>
<ul>
<li><strong>(S)how</strong>: This shows the successive results of the shooting and erases the screen at the end and redraws the last solution. The program uses the currently selected numerical integration method, the current starting point, <code>T0</code> as the left end time and <code>T0+TEND</code> as the right end. Thus, if the interval of interest is <code>(2.5,6)</code> then set <code>T0=2.5</code> and <code> TEND=3.5</code> in the numerics menu.</li>
<li><strong>(N)o show</strong>: This is as above but will not show successive solutions.</li>
<li><strong>(R)ange</strong>: This allows you to range over a parameter keeping starting or ending values of each of the variables. A window will appear asking you for the parameter, the start, end, and steps. You will also be asked if you want to cycle color which means that the results of each successful solution to the BVP will appear in different colors. Finally the box labeled<code>side</code> tells the program whether to save the initial <code>(0)</code> or final<code>(1)</code> values of the solution. As the program progresses, you will see the current parameter in the info window under the main screen. You can abort the current step by pressing <code>Esc</code> and the whole process by pressing <code>/</code>. As in the <code>Initialconds Range</code> option, you can also choose <code>Movie</code>. Then, as before, after each solution is computed, a snapshot is take. Thus, you can playback the solutions as a function of the range parameter.</li>
<li><strong>(P)eriodic</strong>: Periodic boundary conditions can be solved thru the usual methods, but one then must write an addition equation for the frequency parameter. This option eliminates that need so that a 2-D autonomous system need not be suspended into a 3D one. You will be asked for the name of the adjustable parameter for frequency. You will also be asked for the section variable and section. This is an additional condition that must be satisfied, namely, $<code>x(0)=x_0</code>$ where $<code>x</code>$ is the section variable and $<code>x_0</code>$ is the section. Type <code>yes</code> if you want the progress shown.</li>
</ul>
`,headings:[{id:"initial-conds",text:"(I)nitial conds",level:3},{id:"continue",text:"(C)ontinue",level:3},{id:"nullclines",text:"(N)ullclines",level:3},{id:"direction-fieldflow",text:"(D)irection Field/Flow",level:3},{id:"window",text:"(W)indow",level:3},{id:"phase-space",text:"ph(A)se space",level:3},{id:"kinescope",text:"(K)inescope",level:3},{id:"graphic-stuff",text:"(G)raphic stuff",level:3},{id:"numerics",text:"n(U)merics",level:3},{id:"file",text:"(F)ile",level:3},{id:"parameters",text:"(P)arameters",level:3},{id:"erase",text:"(E)rase",level:3},{id:"make-window",text:"(M)ake window",level:3},{id:"text-etc",text:"(T)ext, etc",level:3},{id:"sing-pts",text:"(S)ing pts",level:3},{id:"view-axes",text:"(V)iew axes",level:3},{id:"xi-vs-t",text:"(X)i vs t",level:3},{id:"restore",text:"(R)estore",level:3},{id:"3d-params",text:"(3)d params",level:3},{id:"bndry-val",text:"(B)ndry val",level:3}]},{id:"06-numerical-parameters",title:"Numerical parameters",html:`<p>When you click on the <code>nUmerics</code> command in the main menu, a new list appears. This is the numerics menu and allows you to set all of the numerical parameters as well as some post-processing. Some of these may not yet be implemented. Press <code>Esc</code> or click on the <code> exit</code> to get the main menu back.</p>
<p>The items on this menu are:</p>
<h3 id="total">(T)otal</h3>
<p>This is the amount of time to integrate. It is called <code>TEND</code> in the documentation. If it is negative then it will be made positive and no data will be stored. Thus you can integrate for very long periods of time without being told that the storage is full.</p>
<h3 id="start-time">(S)tart time</h3>
<p>This is the initial time <code>T0</code> For autonomous systems it is usually irrelevant.</p>
<h3 id="transient">tRansient</h3>
<p>The program will integrate silently for this amount of time before plotting output. It is used to get rid of transients.</p>
<h3 id="dt">(D)t</h3>
<p>This is the step size used by the fixed step integrators and the output step for Gear,CVODE, Quality RK, Rosen,and Stiff algorithms. It is positive or negative depending on the direction of integration.</p>
<h3 id="ncline-ctrl">n(C)line ctrl</h3>
<p>will prompt you for the grid size for computing nullclines.</p>
<h3 id="sing-pt-ctrl">s(I)ng pt ctrl</h3>
<p>This prompts you for errors and epsilons for eigenvalues and equilibria calculations as well as the maximum iterates. If you have global flags, then you will be asked for <code>smin</code> as well.</p>
<h3 id="nout">n(O)ut</h3>
<p>This sets the number of integration steps to perform between output to storage. Thus, if you output every 10 steps with a <code>Dt</code> of .05, XPP will yield output at times that are $<code>10*.05=.5</code>$ timesteps apart. The advantage of this is that lengthier records of data can be made without losing accuracy of the integrator. This parameter is also used in the continuation of fixed point for maps in AUTO. If this parameter is $<code>n&gt;1</code>$, then in AUTO, the fixed point of $<code>F^(n)(x)</code>$ is found where $<code>F</code>$ is the right-hand side of the equations. This allows AUTO to continue periodic points of maps.</p>
<h3 id="bounds">(B)ounds</h3>
<p>This sets a global bound on the integrator. If any variable exceeds this value in magnitude, a message appears and the integration stops.</p>
<h3 id="method">(M)ethod</h3>
<p>allows you to choose the methods of integration. DoPri5, DoPri83, Gear, CVODE, Quality RK, Rosen,and Stiff are adaptive. The first two are the Dormand-Prince integrators and are the most modern of the group. Gear,CVODE, Rosen, and Stiff are the best to use for stiff problems. If you choose the adaptive methods, you will be asked for error tolerance, minimum step, and maximum step size. The discrete method should be used for difference equations. If you choose CVODE/Rosen, you can choose to use the banded version of it. You should set the upper and lower bandwidths. Note that this is recommended for stiff PDEs only and should be used in conjunction with the block arrays. You can get huge speed up in the integration. I have gotten 100 fold on some problems. If the method is adaptive, the output <code>NOUT</code> is set to 1. Also the adaptive methods generate several possible error messages that you may have to respond to. These suggest how to fix the error. Markov processes are ignored by GEAR. Backward Euler prompts you for a tolerance and the maximum number of iterates for each step. The Volterra method, described below, prompts for a tolerance, maximum iterates, and a \u201Cmemory size.\u201D You will also be asked if you want the convolution kernels to be re-evaluated after any parameter is changed in either the range integration or through a manual change in parameters. If this flag is 1 then the kernels will be re-computed. The default is to not recompute them. Memory size determines how far back to save the results for the integrator. If this is small, there is a big speed-up in the integration, but you could be neglecting important terms. The symplectic integrator should only be used for systems of the form: <code>math \\begin{eqnarray*} x_1&#39; &amp;=&amp; v_1 \\ v_1&#39; &amp;=&amp; F_1(x_1,\\ldots,x_n) \\ x_2&#39; &amp;=&amp; v_2 \\ v_2&#39; &amp;=&amp; F_2(x_1,\\ldots,x_n) \\ \\vdots &amp;=&amp; \\vdots \\ x_n&#39; &amp;=&amp; v_n \\ v_n&#39; &amp;=&amp; F_n(x_1,\\ldots,x_n) \\end{eqnarray*}</code> where $<code>F_j = \\partial_{x_j} V(x_1,\\ldots,x_n).</code>$ That is, it is for frictionless mechanical problems. The equations must be written in the above order as well or it won\u2019t work. Symplectic integrators preserve a discrete analogue of the energy so that unlike other integrators, they preserve the invariants of the flow.</p>
<h3 id="delay">d(E)lay</h3>
<p>This sets the upper bound for the maximum delay and three other parameters that have to do with stability of delay equations. If in your delay equations, the delay exceeds this, a message will appear and the integration will stop. Each time this is changed all previous delay data is destroyed and you must begin your integration anew. Thus, it should be the first thing you set when solving a delay equation. Since the storage depends on the size of <code>Dt</code> when you change this, then the delay storage will also be destroyed. Delay equations require data for $<code>t&lt;t_0</code>$ so that you should edit the <code>Delay ICs</code> to achieve this. Use the fixed step integrators for this, althought, adaptive sometimes will work. In addition to the maximal delay, you will be asked for <code>real part </code> and <code>im part</code>. When the program looks at stability of delay equations, it considers a rectangular region defined by the four points in the complex plane. It also attempts to find one root (usually the one with the most positive real part. These two quatities provide a \u201Cguess\u201D for the root. Once a root is found, these quantities are replaced by that root. So, if you want to find a root that is different, then change the initial guess.(See the section on numerical methods.) Finally, you will be asked for <code>DelayGrid</code> which tells the program how many steps to take on each side of the contour to determine stability. Basically, the larger this parameter is the more accurate the stability determination.</p>
<h3 id="color-code">(C)olor code</h3>
<p>If you have a color system, XPP can code the output according to the magnitude of the velocity or the magnitude of another variable. A choice pops up for these two options or for turning off the color. This overrides any color on any other curves in your picture. Once you choose to color code, you are asked to either choose max and min values or have XPP do it for you via optimize. The latter will compute the max and min and set the scales accordingly.</p>
<h3 id="poincare-map">(P)oincare map</h3>
<p>This sets up parameters for Poincare sections. A choice of four items will appear: the <code>Max/Min</code> option the <code>Poincare section</code> option, the <code>Periodic</code> option, and the option to turn <code>off</code> all maps. A parameter box will pop up and you should type in the parameters. They are the variable to check and the section and the direction. That is, a point will be recorded if the variable crosses the section such that it is either positive going to negative or vice versa according to the direction parameter. If the direction is set to zero, the, the point will be recorded from either direction. The flag <code>Stop on Section</code> instructs XPP to halt when the section is crossed. Note that automatic interpolation is done. If the section variable is <code>Time, T</code> and the section is say <code>T1</code>, then each time <code>T=0 modulo T1</code> the point is recorded. This is useful for periodically driven systems. If you have opted for the <code>Max/Min</code> option, then the section is irrelevant and the point will be recorded when a local maximum (if the direction is 1) minimum (direction=-1) or both (direction=0) of the variable is encountered. The <code>Periodic</code> option does the following. When ever the specified variable hits the section, the time of the hit is recorded and the previous time is subtracted from the current time and recorded in the time column. This leads to a list of intervals between hits. If you create an auxiliary variable that is a complicated function of the other variables, you can use this for the section thus allowing you to have sections which are not the coordinate axes.</p>
<h3 id="ruelle-plot">R(U)elle plot</h3>
<p>This allows you to retard any of the axes by an integral number of steps. This is useful for chaotic orbits and delayed systems. Choosing a number for any of the axes will result in the variable associated with that axis being delayed by the number of steps inputted. Thus if you plot X vs X then of course you will get a diagonal line, but if you make the Y-axis delayed by say 50 and the output is every .1 timesteps, then the plot will be X(t-5) vs X(t). This does not appear during integration of the equations and is available only after a computation. You set it up and then click Restore from the main menu.</p>
<h3 id="stochastic">stoc(H)astic</h3>
<p>This brings up a series of items that allow you to compute many trajectories and find their mean and variance. It also contains commands for post-simulation data analysis. It is most useful when used with systems that are either Markovian or have noise added to the right-hand sides. The items are:</p>
<ul>
<li><strong>New seed</strong>: Use this to reseed the random number generator. If you use the same seed then the results will not change from run to run.</li>
<li><strong>Compute</strong>: This will put up the same dialog box as the \u201CIntegrate\u201D \u201CRange\u201D choice. Two new data sets will be created that will compute the mean and the variance of the point by point values of the trajectories over the number of trial runs you choose. If the system is completely deterministic and the parameters and initial conditions are identical for each run, then this is superfluous. Otherwise, the mean and variance are computed. You can then access these new arrays as described below. If you fire up the sample Markov problem, choose the \u201CCompute\u201D option, and set keep the initial data constant over say 20 runs, then you can look at the mean trajectory and its variance for each of your variables.</li>
<li><strong>Data</strong>: This puts the results of the most recent run into the data browser and enables plotting of them.</li>
<li><strong>Mean</strong>: This puts the results of the mean value of the most recently computed set of trials.</li>
<li><strong>Variance</strong>: This does the same for the variance.</li>
<li><strong>Histogram</strong>: This computes a histogram for a chosen variable and additional conditions and replaces the \u201Ct\u201D column and the first variable column with the bin values and the number per bin respectively. You will be prompted for the number of bins, a maximum and minimum value and the variable on which to perform the histogram. Finally, you will be asked for additional conditions that involve the other stored variables (not the fixed ones though.) For example, suppose you have run an ODE/Markov system and you want the distribution of a continuous variable when the Markov variable is in state 1. Then the additional condition would be <code>z==1</code> where <code> z</code> is the Markov variable. Multiple conditions are made by using the <code>&amp;</code> and <code>| </code> expressions. Note that <code>==</code> is the logical equal and is not the same as the algebraic one.</li>
<li><strong>Old Hist</strong>: brings back the most recently computed histogram.</li>
<li><strong>Fourier</strong>: This prompts you for a data column. It then computes a Fourier transform (FFT). The results are in the Browser. The first column (labeled \u201CT\u201D) is the mode. The second, the cosine component and the third, the sine component.</li>
<li><strong>Power</strong>: computes the power spectrum and the phase using the FFT.</li>
<li><strong>Spec.dens</strong>: this uses welch windowing to compute a smoother power spectrum than the straight-up power method. You divide your data into windows of a certain length and then premultiply by a window (square, parabolic,cosine or triangular) before taking the power over that window. The windows are then normalized and the result is normalized so that the sum is 1.</li>
<li><strong>fIt curve</strong>: This is a routine based on Marquardt-Levenberg algorithm for nonlinear least squares fitting. A description of the method can be found in <em>Numerical Recipes in C.</em> In this implementation, one can choose parameters and initial data to vary in an attempt to minimize the least-squares difference between solutions to a dynamical system and data. The data must be in a file in which the first column contains the independent values in increasing order. The remaining columns contain data which are to be fitted to solutions to a DE. Not all the columns need be used. When you choose this option, a window pops up with 10 entries describing the fit parameters. The items are:<ul>
<li><strong>File</strong>: This is the name of the data file. The first column must contain the times at which the data was taken.</li>
<li><strong>NCols</strong>: This contains the total number of columns in the data file. This includes all columns in the file, even those that you will not use.</li>
<li><strong>Fitvar</strong>: This is a list separated by commas or spaces of variables that you want to fit to the data. These must not be Markov variables or Auxiliary variables. They are restricted to the items that you define as \u201CVariables\u201D in the ODE file. Due to laziness, you can only have as many variables as you can type in 25 or fewer characters.</li>
<li><strong>To Col</strong>: This should contain a list of column numbers in the data file associated with each of the variables you want to fit. Thus, for example, if you want to fit \u201Cx\u201D to column 5 and \u201Cy\u201D to column 2, you would type \u201Cx y\u201D in the \u201CFitvar\u201D entry and \u201C5 2\u201D in the \u201CTo Col\u201D entry. The number of columns in this must equal the number of variables to be fit.</li>
<li><strong>Params</strong>: These items (there are 2 of them in case you have lots of parameters you need to vary) contain the names of parameters and variables. If the name is a variable, then the initial data for that variable will be adjusted. If it is a parameter, then the parameter will be adjusted. On the initial call, the current initial data and parameter values are used. The lists of parameters and initial data can be separated by spaces or commas.</li>
<li><strong>Tolerance</strong>: This is just a small number that tells the algorithm when the least square error is not changing enough to be significant. That is if either the difference is less than \u201CTOL\u201D or the ratio of the difference with the least square is less than \u201CTOL\u201D then the program will halt successfully. On should not make this too small as such differences are insignificant and a waste of CPU time. The default of .001 seems to work well.</li>
<li><strong>Npts</strong>: This is the number of points in the data file that you want to fit to.</li>
<li><strong>Epsilon</strong>: This is used for numerical differentiation. 1e-5 is a good value since we really don\u2019t need precise derivatives.</li>
<li><strong>Max iter</strong>: This is the maximum number of iterates you should use before giving up.</li>
</ul>
</li>
<li>On return, the program will put the best set of parameters that it has found. It currently is quite verbose and prints a lot of stuff to the console. This is mainly info about the current values of the parameters and the least square.</li>
<li><strong>Liapunov exponent</strong>: attempts to compute the maximum Liapunov exponent of the current simulation. The method is pretty simplistic but works on the examples I have fed it. Given a solution $<code>x(t)</code>$ at a series of points $<code>t_1,\\ldots,t_n</code>$ I perturb the solution at each time point, integrate the equation to the next time point, and compute the logarithm of the rate of growth. This is averaged over the whole time series to give an approximation. The size of the perturbation is determined by the numerical parameter <code>JAC_EPS</code> which can be set from the numerics menu under Sing pt ctl. You will be prompted as to whether you want to compute the exponent over a range of parameters. If you choose a range, then the range dialog box will appear; fill it in as usual.</li>
<li>is spike time auto-correlation. I needed this once for a class so I put it into XPP. Basically, if the data is a list of spike-times then this will make a histogram of the differences between these over the data set. That is it makes a histogram of $<code>x_i-x_j</code>$ over all values of $<code>i,j.</code>$</li>
<li>is a cross correlation between two time series - more properly, it is the covariance. It returns: <code>math C_j = \\frac{1}{M}\\sum_{i=0}^{N-1} (x_i-bar{x})(y_{i+j}-\\bar{y})</code> where $<code>M</code>$ is the total entries used. That is, because of zero padding, calculations with low $<code>j</code>$ have more points than those with high $<code>j</code>$; this divides by the number of counts. I don\u2019t know if this is proper, but it seems a fairer comparison.</li>
<li><strong>loo(K)up</strong> : This allows you to change the definitions of tabulated functions by reading in a different file or changing the formula. Thus, if you have many experimental sets of data, you can read them in one by one and integrate the equations. You are prompted for the name of a tabulated function. Then you give the filename to read in. You will continue to be prompted and can type a few carriage returns to get out. If the table was defined as a function instead of a file, then you will be prompted for the number of points, the limits of the range (<code> Xhi,Xlo</code>) ad finally, the formula of for the function defining the table. Note that it must be a function of <code>t</code>. Note that if the function contains parameters and these are changed, it will be automatically recomputed unless the AUTOEVALUATE flag is set to 0. By default, it is set to 1. You would likely set it to zero if you want to create a random table in order to implement \u201Cfrozen\u201D noise.</li>
</ul>
<h3 id="bndryval">bndry(V)al</h3>
<p>prompts you for the maximum iterates, the error tolerance, and the deviation for the numerical Jacobian for the shooting method for solving BVPs.</p>
<h3 id="averaging">(A)veraging</h3>
<p>This allows you to compute the adjoint and do averaging for weakly coupled oscillators. To use this option, you must successfully compute a periodic orbit. This does not work well with stiff systems so good luck. The menu that pops up is:</p>
<ul>
<li><strong>(N)ew adj</strong>: This makes the adjoint from the computed periodic data. Success or failure will be noted. Separate storage is maintained for the adjoint. The program automatically puts the data from the adjoint into the browser so it can be viewed and plotted or saved.</li>
<li><strong>(A)djoint</strong>: This will place the adjoint data in the browser,</li>
<li><strong>(O)rbit</strong>: This places the periodic orbit in the browser.</li>
<li><strong>(M)ake H</strong>: This will prompt you for the coupling function and the result will be averaged and placed in 2 columns of storage, the first is the time, then the H function. If there are enough columns, the odd and even parts of the averaged function will be retained. As with the adjoint, this list is automatically placed in the browser. The user will be prompted for the coupling functions and should type in the formulas. The coupling is between two identical units. Say you want to couple two oscillators via a variable called <code>V</code> Then, you <code>V</code> refers to the oscillator getting the input and <code>V\u2019</code> refers to the oscillator providing it. For example, suppose you want to study the behavior of two weakly diffusively coupled Fitzhugh-Nagumo equations: <code>math \\begin{eqnarray}     dv_1/dt &amp;=&amp; f(v_1,w_1)+\\epsilon (v_2-v_1) \\     dw_1/dt &amp;=&amp; g(v_1,w_1) \\     dv_2/dt &amp;=&amp; f(v_2,w_2)+\\epsilon (v_1-v_2) \\     dw_2/dt &amp;=&amp; g(v_2,w_2) \\end{eqnarray}</code> Then for the coupling you would input</li>
<li>v&#39;-v</li>
<li>0</li>
<li>for the required coupling.</li>
<li><strong>(H) function</strong>: This places the computed H function in the browser.</li>
<li>Anytime you integrate, etc, the data will be placed back into the storage area.</li>
</ul>
`,headings:[{id:"total",text:"(T)otal",level:3},{id:"start-time",text:"(S)tart time",level:3},{id:"transient",text:"tRansient",level:3},{id:"dt",text:"(D)t",level:3},{id:"ncline-ctrl",text:"n(C)line ctrl",level:3},{id:"sing-pt-ctrl",text:"s(I)ng pt ctrl",level:3},{id:"nout",text:"n(O)ut",level:3},{id:"bounds",text:"(B)ounds",level:3},{id:"method",text:"(M)ethod",level:3},{id:"delay",text:"d(E)lay",level:3},{id:"color-code",text:"(C)olor code",level:3},{id:"poincare-map",text:"(P)oincare map",level:3},{id:"ruelle-plot",text:"R(U)elle plot",level:3},{id:"stochastic",text:"stoc(H)astic",level:3},{id:"bndryval",text:"bndry(V)al",level:3},{id:"averaging",text:"(A)veraging",level:3}]},{id:"07-data-browser",title:"The Data Browser",html:`<p>The Data Browser (DB) lets you look at the numbers a run produced, save
them to a file and otherwise manipulate them; it is a very primitive
spread sheet. Once you have computed a trajectory, use the Data Browser to
look at the data.</p>
<p><strong>In web2</strong> the DB is the <strong>Data</strong> tab (<code>ui/TableView.tsx</code>): a virtualized
table, so there is no window to iconify, resize or &quot;shake&quot; to make the
data show up (all known bugs of the X11 DB window). Across the top is a
menu of commands and then there follows a list of titles for the
variables, time and the auxiliary functions. Using the arrow keys, the
page keys or clicking on the appropriate commands allows you to scroll
through the data; every button and its keyboard shortcut also reaches the
table&#39;s focus in reading order. Clicking on Left shifts the data columns
to the left and Right moves them to the right. The time column always
stays fixed. If you do parametric or range calculations, the range
variable is kept in the time column. Home takes you to the top of the
data and End to the last row. The remaining commands are described
separately below; the keyboard shortcut to invoke each is in parentheses.</p>
<h3 id="find">(F)ind</h3>
<p>This pops up a window and asks for a variable and a value. It then looks through the data until it comes to the closest value to the specified that the variable takes. It only moves down the file so that you can find successive values by starting at the top.</p>
<h3 id="get">(G)et</h3>
<p>This makes the top row of data the initial conditions for a new run.</p>
<h3 id="replace">Re(p)lace</h3>
<p>This pops up a window asking you for a column to replace. Then it prompts you for a formula. Suppose you want to replace <code>AUX1</code> with <code>x+y-t</code> where <code>x,y</code> are two variables. Then type this in when prompted and the column that held <code>AUX1</code> will be replaced by the values in these columns. Any valid XPP function or user function can be used. Two special symbols can also be used when applied to single variables:</p>
<ul>
<li><strong>@VARIABLE</strong>: replaces the column with the numerical derivative of the variable. You cannot use this within a formula, but once the column is replaced, it can be treated as any other column.</li>
<li><strong>&amp;VARIABLE</strong>: replaces the column with the numerical integral. Note that successive applications of the derivative and then the integral will result in the original plus a constant.</li>
</ul>
<h3 id="unreplace">(U)nreplace</h3>
<p>This undoes the most recent replacement.</p>
<h3 id="first">Fir(s)t</h3>
<p>This marks the top row in the DB (nothing is shown)as the start or first row for saving or restoring.</p>
<h3 id="last-e">Last (e)</h3>
<p>This marks the top row as the last or end row for saving or restoring.</p>
<h3 id="table">(T)able</h3>
<p>This lets you save data in a tabulated format that can then be used by XPP as a function or inputs. You must use the <code> First</code> and <code>Last</code> keys to mark the desired data you want to save. You are then prompted for the column name, the minimum and maximum you want your independent variable to range and the file name. The result is a table file of the format shown in the section on tables.</p>
<h3 id="restore">(R)estore</h3>
<p>replots the data marked by First and Last. The default is the entire data set</p>
<h3 id="write">(W)rite</h3>
<p>prompts you for a filename and writes the marked data to a file, in the working directory (see <a href="04-using-the-interface.md#saving-pictures-and-files">Using the interface</a> for how the browser gets it). The file is ASCII readable and reflects the current contents of the DB:</p>
<pre><code>t0 x1(t0) ... xn(t0)
t1 x1(t1) ... xn(t1)
.
.
.
tf x1(tf) ... xn(tf)
</code></pre>
<h3 id="load">(L)oad</h3>
<p>Will load in as much of a similarly formatted data file as possible for graphing.</p>
<h3 id="addcol">(A)ddcol</h3>
<p>This allows you to add an additional column to the data browser. You are prompted for the name you want to give the column and for the formula. It is thus, like an auxiliary variable with a name. Thereafter, it will be computed along with any other quantities that you have defined. It is as though you had included another auxiliary variable in your original file.</p>
<h3 id="delcol">(D)elcol</h3>
<p>This lets you delete a column. You can only delete columns which you have created with the <code>Addcol</code> command.</p>
<ul>
<li>You can delete a column that is itself referred to by a different column; this will result in wrong answers in the column. Thus, do not delete columns whose contents are used by other columns. Also, if you delete a column, its name is still known by the internal system but it has no real value. For these reasons, you should delete columns with caution. If the purpose of deleting them is to change the formula, use the right-hand-side editor, (File-Edit) instead.</li>
</ul>
`,headings:[{id:"find",text:"(F)ind",level:3},{id:"get",text:"(G)et",level:3},{id:"replace",text:"Re(p)lace",level:3},{id:"unreplace",text:"(U)nreplace",level:3},{id:"first",text:"Fir(s)t",level:3},{id:"last-e",text:"Last (e)",level:3},{id:"table",text:"(T)able",level:3},{id:"restore",text:"(R)estore",level:3},{id:"write",text:"(W)rite",level:3},{id:"load",text:"(L)oad",level:3},{id:"addcol",text:"(A)ddcol",level:3},{id:"delcol",text:"(D)elcol",level:3}]},{id:"08-functional-equations",title:"Functional equations",html:`<p>In the course of some research problems, I was pursuing, I ran into some Volterra equations of the form:</p>
<pre><code class="language-math">u(t)=f(t)+\\int_0^t K(t,s,u(s))ds
</code></pre>
<p>that I could not convert to ODES. (If $<code>K</code>$ is a convolution with a sum of powers and exponentials, then it can be converted to an ODE. Since XPP is much more efficient with ODEs and has been thoroughly debugged with respect to them, you should always attempt this conversion first.) Thus, I have added the capability to solve equations with this type of term in them. This has necessitated the addition of two new commands for the ODE file and a solver for such problems. The solver is described below in the numerical section. Since the equation above requires \u201Cmemory\u201D all the way back to $<code>t=0</code>$ and one often is interested in long time behavior, XPP truncates this to:</p>
<pre><code class="language-math">u(t)=f(t)+\\int_{\\max(t-T,0)}^t K(t,s,u(s))ds
</code></pre>
<p>where <code>T=DT*MaxPoints</code>, the latter being a parameter that you set in the numerics menu. The default is 4000. Thus, one assumes that the kernel function decays for large $<code>t</code>$ and so the tail will be small. As <code>MaxPoints</code> is a parameter, you can always make it larger at the price of taking longer to evaluate right-hand sides. Let us consider the following equation:</p>
<pre><code class="language-math">u(t) = \\sin(t)+\\frac{1}{2}(\\cos(t)-\\exp(t)-t\\exp(t))+\\int_0^t
(t-s)\\exp(-(t-s))u(s)ds
</code></pre>
<p>whose solution is $<code>u(t)=\\sin(t).</code>$ The following ODE file will create this model and also add an auxiliary variable with the solution for purposes of comparison.</p>
<pre><code>#  voltex1.ode
u(t)=sin(t)+.5*cos(t)-.5*t*exp(-t)-.5*exp(-t)+int{(t-t&#39;)*exp(t&#39;-t)*u}
aux utrue=sin(t)
done
</code></pre>
<p>The <code>int{K(u,t,t\u2019)}</code> construction tells XPP that this is a Volterra integral. If your problem can be cast as a convolution problem, considerable speedup can be obtained since lookup tables are created. The present example is in fact a convolution problem, so that instead of the full declaration, one could instead write:</p>
<pre><code>#  voltex2.ode
u(t)=sin(t)+.5*cos(t)-.5*t*exp(-t)-.5*exp(-t)+int{t*exp(-t)#u}
aux utrue=sin(t)
done
</code></pre>
<p>which convolves the first expression (of $<code>t</code>$ <strong>only</strong>) with $<code>u.</code>$ For this example, using a time step of .05 and integrating to 40, there is a 3-fold speed-up using the convolution. For more complicated kernels, it will be more.</p>
<p>If one wants to solve, say,</p>
<pre><code class="language-math">u(t) = exp(-t) + \\int^t_0 (t-t&#39;)^{-mu} K(t,t&#39;,u(t&#39;))dt&#39;
</code></pre>
<p>the form is:</p>
<pre><code>u(t)= exp(-t) + int[mu]{K(t,t&#39;,u}
</code></pre>
<p>Note that the \u201Cmu\u201D must be a number between 0 and 1. If \u201Cmu\u201D is greater than or equal to 1, the integral is singular at 0.</p>
<h2 id="warning">Warning</h2>
<p>If you have parameters in your definition of the kernel and you change them, then you will have to go into the numerics menu and recompute the kernels by calling the Method command which automatically recomputes the kernels. Alternatively, turn on the AutoEval flag and it will be done automatically.</p>
<p>I close this section with an example of a pair of coupled oscillators in an infinite bath which has diffusion and passive decay. The equations are:</p>
<pre><code class="language-math">\\begin{eqnarray*}
u(t) &amp;=&amp;\\int_0^t k(t-s)F(u(s),v(s))ds + \\int_0^t
k_d(t-s)F(u_1(s),v_1(s))ds \\\\
v(t) &amp;=&amp;\\int_0^t k(t-s)G(u(s),v(s))ds + \\int_0^t
k_d(t-s)G(u_1(s),v_1(s))ds \\\\
u_1(t) &amp;=&amp;\\int_0^t k(t-s)F(u_1(s),v_1(s))ds + \\int_0^t
k_d(t-s)F(u(s),v(s))ds \\\\
v_1(t) &amp;=&amp;\\int_0^t k(t-s)G(u_1(s),v_1(s))ds + \\int_0^t
k_d(t-s)G(u(s),v(s))ds
\\end{eqnarray*}
</code></pre>
<p>where</p>
<pre><code class="language-math">\\begin{eqnarray*}
k(t) &amp;=&amp; \\exp(-t)/\\sqrt(\\pi t) \\\\
k_d(t) &amp;=&amp; \\exp(-t)\\exp(-d/t)/\\sqrt(\\pi t)
\\end{eqnarray*}
</code></pre>
<p>and</p>
<pre><code class="language-math">\\begin{eqnarray*}
F(u,v) &amp;=&amp; \\lambda u -v - (u+qv)(u^2+v^2) \\\\
G(u,v) &amp;=&amp; \\lambda v + u - (v-qu)(u^2+v^2).
\\end{eqnarray*}
</code></pre>
<p>Note that the kernel $<code>k</code>$ is weakly singular and thus $<code>\\mu=.5.</code>$</p>
<p>Note that in addition there is a singularity at $<code>t=0</code>$ for the diffusive kernel (division by zero); this can be rectified by adding a small amount to the denominator. The XPP file is as follows</p>
<pre><code># lamvolt.ode 
# the four variables:   
init u=0  v=0  u1=0  v1=0  
par lam=1.5  q=0.8  d=1  u0=1  u10=0.95  
# 1/sqrt(pi)=
number spi=0.56419  
# the integral equations; since (0,0,0,0) is a rest point, I
# add a small quickly decaying transient
u(t)=u0*exp(-5*t)+spi*(int[.5]{exp(-t)#f}+int[.5]{exp(-t-d/(t+.0001))#f1})
v(t)=spi*(int[.5]{exp(-t)#g}+int[.5]{exp(-t-d/(t+.0001))#g1})
u1(t0)=u10*exp(-5*t)+spi*(int[.5]{exp(-t)#f1}+int[.5]{exp(-t-d/(t+.0001))#f})
v1(t)=spi*(int[.5]{exp(-t)#g1}+int[.5]{exp(-t-d/(t+.0001))#g})
# the four functions f,g,f1,g1
f=lam*u-v-(u*u+v*v)*(u+q*v)
g=lam*v+u-(u*u+v*v)*(v-q*u)
f1=lam*u1-v1-(u1*u1+v1*v1)*(u1+q*v1)
g1=lam*v1+u1-(u1*u1+v1*v1)*(v1-q*u1)
done
</code></pre>
<p>Try it.</p>
`,headings:[{id:"warning",text:"Warning",level:2}]},{id:"09-auto",title:"Auto interface",html:`<p>AUTO is a program that was written several years ago by Eusebius Doedel. It has the ability to track bifurcation curves for steady-state and periodic systems. The program is very powerful particularly for following periodic orbits. A full FORTRAN implementation of it is available along with documentation from Doedel. His Email is <a href="mailto:doedel@cs.concordia.edu">doedel@cs.concordia.edu</a>.</p>
<p>The version supported in XPP is a subset of AUTO but allows you to do most of the things you would nornally want to for autonomous ODEs and with BVPS. In particular, you can track fixed points, find turning points and Hopf bifurcation points, compute two-parameter curves of turning points and Hopf points, compute branches of periodic solutions emanating from a Hopf point, track period-doubling bifurcations, torus bifurcations, and two-parameter curves of fixed period orbits. Points can be imported into XPP as well as complete orbits. The bifurcation diagrams are dynamically produced and you can move around them using the arrow keys. Curves can be saved and reloaded for later use. Diagrams can be saved and imported into the main XPP window.</p>
<p>Click on the <code>File Auto</code> menu item to bring up AUTO.</p>
<h2 id="the-auto-view">The AUTO view</h2>
<p><strong>In web2</strong>, AUTO opens as its own view (<code>ui/AutoView.tsx</code>), anchored to
the right of the plot on screens from 48 rem wide and a full-screen sheet
below that; both stop above the status bar. It has:</p>
<ul>
<li>the <strong>diagram</strong>, drawn from the <code>diagram</code> data event: a curve per branch
and stability run (stable solid, unstable dashed, periodic branches as
their maximum and minimum), labelled points as crosses, and the segment
from a Hopf point to the first point of the periodic branch it started
(<code>from</code>, which XPP itself leaves blank); it zooms (wheel), pans
(Shift+drag or the middle button) and undoes (<code>Ctrl+Z</code>) in the client,
and a tooltip or the readout names the point under the mouse or cursor;</li>
<li>a <strong>status strip</strong> and <strong>stability circle</strong> (<code>ui/AutoStatus.tsx</code>,
<code>ui/AutoInfo.tsx</code>) below it, from the <code>autoinfo</code> event: branch, point,
type, label, parameters, norm, the plotted variable, the period, and
the eigenvalues/multipliers, in place of the small square and info
windows of the X11 AUTO window;</li>
<li>an <strong>axis dialog</strong> (<code>ui/AutoAxes.tsx</code>), opened by clicking an axis name
next to the diagram, for Axes&#39; choices (Hi, Norm, Hi-lo, Period, Two
par, Frequency, Average and their ranges) below \u2014 usable during a run,
applied when idle;</li>
<li>an <strong>Output</strong> panel with AUTO&#39;s printed table (<code>xpp_log_auto()</code>, always
written in browser mode);</li>
<li>Numerics as a dialog with Save/Load to a settings file, in place of the
X11 Numerics window;</li>
<li><strong>Grab</strong> as a mode of the diagram: arrows, <code>[</code> <code>]</code>, Tab to the labelled
points, Enter takes the point, Escape cancels, a click or tap takes the
nearest point (see &quot;Points and labels&quot; below for what grabbing does);</li>
<li><strong>Clear</strong> hides the branches computed so far behind a key entry
(&quot;Earlier branches&quot;) instead of blanking the window until a redraw (AUTO
has no reDraw button any more: the diagram is always current); and
<strong>Mark values</strong> for user points (the <code>Usr Period</code> dialog below).</li>
</ul>
<p>There is no Abort button in the view (A10 in docs/ui-v2.md): the status
bar&#39;s Stop stops a running continuation, as it stops any long job; the
view&#39;s own \xD7 or Close stops a run first, then closes.</p>
<p>Everything below this section (parameters, axes, numerics, points and
labels, the algorithm itself) is AUTO&#39;s own vocabulary and unchanged; only
where you click or type differs, as above.</p>
<h2 id="preparation">Preparation</h2>
<p>Before you can use AUTO, you must prepare your system for it. You must start your bifurcation analysis from either a fixed point of your model, a periodic orbit, or a solution to a boundary value problem. AUTO seems to work best when you start from a steady state, but I have had success starting at periodic orbits. If you want to start at a steady state, find one and integrate so that the system is at rest. If you want to start at a periodic orbit, then find one and make sure that the total integration time is the \u201Cperiod\u201D of your orbit. This is what the AUTO interface uses as an approximate starting period. There are several ways to do this; the best is to use the boundary value solver of XPP but just plain old integration often works fine. To solve a boundary value problem, it is necessary to find an initial set of parameters for which you can solve the problem within XPP. You should arrange the \u201Clength\u201D of the interval to always be 1. That is you must scale the problem so that the domain interval of interest is $<code>[0,1].</code>$ You must then compute a solution using XPP before calling AUTO.</p>
<p>For discrete dynamical systems, I have added the capability of continuation of $<code>n-</code>$periodic orbits by having AUTO find fixed points of $<code>F^n(x).</code>$ To do this, just set the parameter <strong>nOut</strong> in the XPP numerics menu to the desired period. The example <code>del_log.ode</code> has set this up for a period 7 orbit.</p>
<p>For the example file <code>lecar.ode</code> the parameter of interest is <code>iapp</code> and this has been set at a negative value so that the system has a stable rest state. The variables have been initialized to their rest states as well. Once you have prepared the problem as such, you are ready to run.</p>
<p>Click on the \u201CFile\u201D item and choose \u201CAuto\u201D to open the AUTO view (see
above).</p>
<h2 id="choosing-parameters">Choosing parameters</h2>
<p>The first thing you should do is tell AUTO which parameters you might use in the bifurcation analysis. Up to 5 are allowed. Click on \u201CParameter\u201D and a list of 5 parameters will appear. Type in the names of the parameters you want to use. For <code>lecar.ode</code> use <code> iapp,phi,gk,vk,gna</code> The default is the first 5 or fewer parameters in your <code>ode</code> file. If you have fewer than 5 parameters, only the available ones will appear.</p>
<h2 id="diagram-axes">Diagram axes</h2>
<p>Next, you should tell AUTO the axes and the main bifurcation parameters. Click on \u201CAxes\u201D and 6 choices appear:</p>
<h3 id="hi">(H)i</h3>
<p>This plots the maximum of the chosen variable.</p>
<h3 id="norm">(N)orm</h3>
<p>This plots the $<code>L_2</code>$ norm of the solution.</p>
<h3 id="hi-lo">h(I)-lo</h3>
<p>This plots both the max and min of the chosen variable (convenient for periodic orbits.)</p>
<h3 id="period">(P)eriod</h3>
<p>Plot the period versus a parameter</p>
<h3 id="two-par">(T)wo par</h3>
<p>Plot the second parameter versus the primary parameter for two-parameter continuations.</p>
<h3 id="zoom">(Z)oom</h3>
<p>Use the mous to zoom in on a region.</p>
<h3 id="last-1-par">last (1) par</h3>
<p>Use the plot parameters from the last 1-parameter plot.</p>
<h3 id="last-2-par">last (2) par</h3>
<p>Use plot parameters from last 2-parameter plot.</p>
<h3 id="frequency">(F)requency</h3>
<p>Plot Frequency vesus parameter.</p>
<h3 id="average">(A)verage</h3>
<p>Plot the average of a variable versus the parameter.</p>
<p>After clicking, a new window pops up with the following items:</p>
<h3 id="y-axis">Y-axis</h3>
<p>This is the variable for the y-axis of the plot. For two-parameter and period plots, its contents is ignored.</p>
<h3 id="main-parm">Main Parm</h3>
<p>This is the principal bifurcation parameter. It must be one of those you specified in the parameter window. The default is the first parameter in the parameter list.</p>
<h3 id="2nd-parm">2nd Parm</h3>
<p>This is the other parameter for two-parameter continuations.</p>
<h3 id="xmin-ymax">Xmin ... Ymax</h3>
<p>The plotting dimensions of the diagram.</p>
<p>Once you press <code>OK</code> the axes will be redrawn and labeled. For the present model, set <code>Xmin=-.5, Ymin=-1.5, Xmax=.5, Ymax=1.0.</code></p>
<h2 id="numerical-parameters">Numerical parameters</h2>
<p>Next, set the NUMERICAL parameters. When you click on this, a new window appears with the following items:</p>
<h3 id="ntst">Ntst</h3>
<p>This is the number of mesh intervals for discretization of periodic orbits. If you are getting apparently bad results or not converging, it helps to increase this. For following period doubling bifurcations, it is automatically doubled so you should reset it later.</p>
<h3 id="nmax">Nmax</h3>
<p>The maximum number of steps taken along any branch. If you max out, make this bigger.</p>
<h3 id="npr">Npr</h3>
<p>Give complete info every <code>Npr</code> steps.</p>
<h3 id="ds">Ds</h3>
<p>This is the initial step size for the bifurcation calculation. <em>The sign of <code>Ds</code> tells AUTO the direction to change the parameter.</em> Since stepsize is adaptive, <code>Ds</code> is just a \u201Csuggestion.\u201D</p>
<h3 id="dsmin">Dsmin</h3>
<p>The minimum stepsize (positive).</p>
<h3 id="dsmax">Dsmax</h3>
<p>The maximum step size. If this is too big, AUTO will sometimes miss important points.</p>
<h3 id="par-min">Par Min</h3>
<p>This is the left-hand limit of the diagram for the principle parameter. The calculation will stop if the parameter is less than this.</p>
<h3 id="par-max">Par Max</h3>
<p>This is the right-hand limit of the diagram for the principle parameter. The calculation will stop if the parameter is greater than this.</p>
<h3 id="norm-min">Norm Min</h3>
<p>The lower bound for the $<code>L_2</code>$ norm of the solution. If it is less than this the calculation will stop.</p>
<h3 id="norm-max">Norm Max</h3>
<p>The upper bound for the $<code>L_2</code>$ norm of the solution. If it is greater than this the calculation will stop.</p>
<p>For the present model, you should set <code>Dsmax</code> to be 0.05, <code>Par Min</code> to -0.45 and <code>Par Max</code> to 0.45.</p>
<h2 id="user-functions">User functions</h2>
<p>Suppose you want to get plots at specific values of parameters or at fixed periods of a limit cycle. Then you can click on \u201CUser\u201D which produces a menu 0-9 asking you how many points you want to keep. Click on 0 for none or some other number. A new window will appear with slots for 9 items. You can type in anything of the form:</p>
<pre><code>    &lt;parameter&gt;=&lt;value&gt;
</code></pre>
<p>or</p>
<pre><code>    T=&lt;value&gt;
</code></pre>
<p>AUTO will mark and save complete information for any point that satisfies either of these criteria. The second is used to indicate that you want to keep a point with a particular period, e.g., <code> T=25</code> will save the any periodic orbit with period 25.</p>
<h2 id="running">Running</h2>
<p>At this point, you are probably ready to run. But before doing a run, here is a hint. You can \u201Csave\u201D the diagram at this point (see below under \u201CFile\u201D). Although it is an \u201Cempty\u201D diagram, all parameters axes, and numerics are saved. You can then reload them later on.</p>
<p>Click on \u201CRun\u201D to run the bifurcation. Depending on the situation, a number of menus can come up. For initial exploration, there are three choices, starting at a new steady state, periodic, or boundary value solution. If you are running the example, click on the steady-state option and a nice diagram will show up and a bunch of points will move around in the stability circle. These indicate stability: for fixed points, they represent exponentials of the eigenvalues; for periodics, the Floquet multipliers. Thus those in the circle are stable and those out of the circle are unstable. Bifurcations occur on the circle. The outer ones are \u201Cclipped\u201D so that they will always lie in the square, thus you can keep count of them.</p>
<p>The diagram,itself, has two different lines and two different circles. Stable fixed points are thick lines, stable periodics are solid circles, unstable fixed points are thin lines, and unstable periodics are open circles. Additionally, there are crosses occasionally dispersed with numbers associated with them. These represent \u201Cspecial\u201D points that AUTO wants to keep. There are several of them:</p>
<h3 id="ep">EP</h3>
<p>Endpoint of a branch</p>
<h3 id="lp">LP</h3>
<p>Limit point or turning point of a branch</p>
<h3 id="tr">TR</h3>
<p>Torus bifurcation from a periodic</p>
<h3 id="pd">PD</h3>
<p>Period doubling bifurcation</p>
<h3 id="uz">UZ</h3>
<p>User defined function</p>
<h3 id="mx">MX</h3>
<p>Failure to converge</p>
<h3 id="bp">BP</h3>
<p>Bifurcation or branch point</p>
<ul>
<li>: Output every $<code>Npr^{th}</code>$ point.</li>
</ul>
<h2 id="grabbing">Grabbing</h2>
<p>You can use these special points to continue calculations with AUTO. The \u201CGrab\u201D item lets you peruse the diagram at a leisurely pace and to grab special points or regular points for importing into XPP or continuing a bifurcation calculation. Click on \u201CGrab\u201D and stuff appears in the info window and a cross appears on the diagram. Use the left and right arrow keys to cruise through the diagram. The right key goes forward and the left backward. At the bottom, information about the branch, the point number, the type of point, the AUTO label, the parameters, and the period are given. The points marked by crosses have lables and types associated with them. The type is one of the above. The label corresponds to the number on the diagram. If point is positive, it is an unstable solution and if it is negative it is stable. As you traverse the diagram, stability is shown in the circle. In web2,
grabbing is a mode of the diagram itself (arrows, <code>[</code> <code>]</code>, Tab to the
labelled points, Enter takes the point, Escape cancels, a click or tap
takes the nearest point); there is no display-dependent drawing bug to
work around.</p>
<p>You can traverse the diagram very quickly by tapping the <code>Tab</code> key which takes you the special points only. Type <code>Esc</code> to exit with no action or type <code>Return</code> to grab the point. If it is a regular point (i.e., not special) then the parameters and the variables will be set to the values for that point within XPP. You can then integrate the equations or look at nullclines, etc. If you grab a special point, then you can use this as a restart point for more AUTO calculations, such as fixed period, two-parameter studies, and continuations. Then, you can run AUTO again. Bifurcation diagrams are cumulative unless you reset them in the \u201CFile\u201D menu. That is, new stuff is continually appended to the old. The only limit is machine memory.</p>
<p>If you grab a special point and click on \u201CRun\u201D several possibilities arise depending on the point:</p>
<h3 id="regular-point">Regular Point</h3>
<p>Reset the diagram and begin anew. You will be asked first if you want to do this.</p>
<h3 id="hopf-point">Hopf Point</h3>
<ul>
<li><strong>Periodic</strong>: Compute the branch of periodics emanating from the Hopf point</li>
<li><strong>Extend</strong>: Continue the branch of steady states through this point.</li>
<li><strong>New Point</strong>: Restart whole calculation using this as a starting point</li>
<li><strong>Two Param</strong>: Compute a two parameter diagram of Hopf points.</li>
</ul>
<h3 id="period-doubling">Period doubling</h3>
<ul>
<li><strong>Doubling</strong>: Compute the branch of period 2 solutions.</li>
<li><strong>Two-param</strong>: Compute two-parameter curve of period doubling points.</li>
</ul>
<h3 id="limit-point">Limit point</h3>
<p>Compute two parameter family of limit points (fixed points or periodic.)</p>
<h3 id="periodic-point">Periodic point</h3>
<p>The point is periodic so</p>
<ul>
<li><strong>Extend</strong>: Extend the branch</li>
<li><strong>Fixed Period</strong>: Two parameter branch of fixed period points.</li>
</ul>
<h3 id="torus-point">Torus point</h3>
<p>Compute two-parameter family of torus bifurcations or extend the branch or compute two-parameter fixed period.</p>
<p><em>Before running, after a point is grabbed, be sure to set up the correct axes and ranges for the parameters.</em></p>
<h2 id="aborting">Aborting</h2>
<p>Any calculation can be gracefully stopped by clicking on the \u201CAbort\u201D key. This produces a new end point from which you can continue. Note that if there are many branches, you may have to press \u201CAbort\u201D several times.</p>
<p><code>Clear</code> just erases the screen and <code>reDraw</code> redraws it.</p>
<h2 id="saving-diagrams">Saving diagrams</h2>
<p><code>File</code> allows you to do several things:</p>
<h3 id="import-orbit">Import orbit</h3>
<p>If the grabbed point is a special one and is a periodic orbit, this loads the orbit into XPP for plotting. This is useful for unstable orbits that cant be computed by integrating initial data.</p>
<h3 id="save-diagram">Save diagram</h3>
<p>Writes a file for the complete diagram which you can use later.</p>
<h3 id="load-diagram">Load Diagram</h3>
<p>Loads a previously saved one.</p>
<h3 id="postscript">Postscript</h3>
<p>This makes a hard copy of the bifurcation diagram</p>
<h3 id="reset-diagram">Reset diagram</h3>
<p>This clears the whole thing.</p>
<h3 id="write-pts">Write pts</h3>
<p>This writes a file specified by the user which has 5 columns and describes the currently visible bifurcation diagram. The first column has the coordinates of the x-axis, the second and third columns hold the contents of the y-axis, (e.g. max and min of the orbit). The fourth column is one of 1-4 meaning stable fixed point, unstable fixed point, stable periodic, unstable periodic, respectively. The fifth column is the branch number. The main window of XPP can import files in this format and plot them</p>
<h2 id="homoclinics-and-heteroclinics">Homoclinics and heteroclinics</h2>
<p>A recent version of AUTO includes a library of routines called HOMCONT which allow the user to track homoclinic and heteroclinic orbits. XPP incorporates some aspects of this package. The hardest part of computing a branch of homoclinics is finding a starting point. Consider a differential equation:</p>
<pre><code class="language-math">x&#39;=f(x,\\alpha)
</code></pre>
<p>where $<code>\\alpha</code>$ is a free parameter. Homoclinics are codimension one trajectories; that is, they are expected to occur only at a particular value of a parameter, say, $<code>\\alpha=0.</code>$ We suppose that we have computed an approximate homoclinic to the fixed point $<code>\\bar{x}</code>$ which has an $<code>n_s-</code>$dimensional stable manifold and an $<code>n_u-</code>$dimensional unstable manifold. We assume $<code>n_s+n_u=n</code>$ where $<code>n</code>$ is the dimension of the system. The remaining discussion is based on Sandstede et al. The way that a homoclinic is computed is to approximate it on a finite interval; say $<code>[0,P].</code>$ We rescale time by $<code>t=Ps.</code>$ We double the dimension of the system so that we can simultaneously solve for the equilibrium point as the parameters vary. We want to start along the unstable manifold and end on the stable manifold. Let $<code>L_u</code>$ be the projection onto the unstable subspace of the linearization of $<code>f</code>$ about the fixed point and let $<code>L_s</code>$ be the projection onto the stable space. Then we want to solve the following system:</p>
<pre><code class="language-math">\\begin{eqnarray*}
\\frac{dx}{ds} &amp;=&amp; P f(x,\\alpha) \\\\
\\frac{dx_e}{ds}&amp;=&amp; 0 \\\\
f(x_e(0)) &amp;=&amp; 0 \\\\
L_s (x(0)-x_e(0)) &amp;=&amp; 0 \\\\
L_u (x(1)-x_e(1)) &amp;=&amp; 0
\\end{eqnarray*}
</code></pre>
<p>Note that there are $<code>2n</code>$ differential equations and $<code>2n</code>$ boundary conditions; $<code>n</code>$ for the equilibrium, $<code>n_s</code>$ at $<code>s=0</code>$ and $<code>n_u</code>$ at $<code>s=1.</code>$ There is one more condition required. Clearly one solution to this boundary value problem is $<code>x(s)\\equiv x_e(s)\\equiv \\bar{x}</code>$ which is pretty useless. However, any translation in time of the homoclinic is also a homoclinic so we have to somehow define a phase of the homoclinic. Suppose that we have computed a homoclinic, $<code>\\hat{x}(s).</code>$ Then we want to minimize the least-squares difference between the new solution and the old solution to set the phase. This leads to the following integral condition:</p>
<pre><code class="language-math">\\int_0^1 \\hat{x}&#39;(s)(\\hat{x}(s)-x(s))\\ ds = 0.
</code></pre>
<p>This is <em>one</em> more condition which accounts for the need for an additional free parameter.</p>
<p>XPP allows you to specify the projection boundary conditions and by setting a particular flag on in AUTO, you can implement the integral condition. Since the XPP version of AUTO does not allow you to have more conditions than there are differential equations, you should pick one parameter which will be slaved to all the other ones you vary and let this satisfy a trivial differential equation,</p>
<pre><code class="language-math">\\alpha&#39;=0.
</code></pre>
<p>Here is the first example of continuing a homoclinic in two-dimensions.</p>
<pre><code class="language-math">x&#39;=y \\quad y&#39;=x(1-x)-ax+\\sigma xy
</code></pre>
<p>When $<code>(a,\\sigma)=(0,0)</code>$ there is a homoclinic orbit (Prove this by integrating the equations; this is a conservative dynamical system.) For small $<code>a</code>$ it is possible to prove that there is a homoclinic orbit for a particular choice of $<code>\\sigma(a)</code>$ using Melnikov methods (see Holmes and Guckenheimer). We now write the equations as a 5-dimensional system using $<code>\\sigma</code>$ as the slaved parameter and introducing a parameter, $<code>P</code>$ for the period:</p>
<pre><code class="language-math">\\begin{eqnarray*}
x&#39; &amp;=&amp; P f(x,y) \\\\
y&#39; &amp;=&amp; P g(x,y)  \\\\
x_e&#39; &amp;=&amp; 0 \\\\
y_e&#39; &amp;=&amp; 0   \\\\
\\sigma&#39; &amp;=&amp; 0
\\end{eqnarray*}
</code></pre>
<p>where $<code>f(x,y)=y</code>$, $<code>g(x,y)=x(1-x)-ax+\\sigma xy</code>$ and the following boundary conditions</p>
<pre><code class="language-math">\\begin{eqnarray*}
0 &amp;=&amp; f(x_e,y_e) \\\\
0 &amp;=&amp; g(x_e,y_e) \\\\
0 &amp;=&amp; L_s (x(0)-x_e,y(0)-y_e)      \\\\
0 &amp;=&amp; L_u (x(1)-x_e,y(1)-y_e)
\\end{eqnarray*}
</code></pre>
<p>and the integral condition. XPP has a defined function for the projection boundary conditions called <code>hom_bcs(k)</code> where <code> k=0,1,...,n-1</code> corresponding to the total number required. You do not need to be concerned with ordering etc as long as you get them all and you give XPP the required information. Here is the ODE file:</p>
<pre><code># tsthomi.ode
f(x,y)=y
g(x,y)=x*(1-x)-a*y+sig*x*y
x&#39;=f(x,y)*per
y&#39;=g(x,y)*per
# auxiliary ODE for fixed point
xe&#39;=0
ye&#39;=0
# free parameter
sig&#39;=0
# boundary conditions
b f(xe,ye)
b g(xe,ye)
# project off the fixed point from unstable manifold
b hom_bcs(0)
# project onto the stable manifold
b hom_bcs(1)
par per=8.1,a=0
init x=.1,y=.1
@ total=1.01,meth=8,dt=.001
@ xlo=-.2,xhi=1.6,ylo=-1,yhi=1,xp=x,yp=y
done
</code></pre>
<p>The only new feature is the projection conditions. <em>XPP\u2019s boundary value solver will not work here since there are more equations than conditions and it doesn\u2019t know about the integral condition.</em> I have set the total integration time to 1 and have added the additional parameter <code>per</code> corresponding to the parameter $<code>P</code>$ in the differential equation. I use the Dormand-Prince order 8 integrator as it is pretty accurate. I have also set the view to be the $<code>(x,y)-</code>$plane. Note that this is a pretty rough approximation of the true homoclinic. We will use AUTO to improve this before continuing in the parameter $<code>a</code>$. Run XPP with this ODE file and integrate the equations. You will get a rough homoclinic pretty far from the fixed point. Click on File Auto to the the AUTO window. Now click on Axes Hi. Choose <code>xmin=0,xmax=50,ymin=-6,ymax=6</code> and also select <code>sig</code> as the variable in the y-axis. Click on OK and bring up the Auto Numerics dialog. Change <code>Ntst=35, Dsmin=1e-4,Dsmax=5</code>, <code>Par Max=50,EPSL=EPSU=EPSS=1e-7</code> and click OK. Now, before you run the program, click on Usr Period and choose 3 for the number. We want AUTO to output at particular values of the parameter <code>per</code> corresponding to $<code>P</code>$. When the dialog comes up, fill the first three entries in as <code>per=20,per=35,per=50</code> respectively and click OK. This forces AUTO to output when $<code>P</code>$ reaches these three values. Now, click on Run and choose Homoclinic. A little dialog box appears. Fill it in as follows: <code>Left Eq: Xe</code> <code>Right Eq: Xe</code> <code>NUnstable: 1</code> <code>NStable: 1</code>. You must tell AUTO the dimension of the stable and unstable manifolds as well as the fixed point to which the orbit is homoclinic. (Note that if you ever fill this in wrong or need to change it, you can access it from the main XPP menu under Bndry Value Homoclinic.) Once you click on OK, you should see a straight line across the screen as the homoclinic approximation gets better. Click on Grab and grab the second point corresponding to the point <code>Per=35</code>. For fun, in the XPP window, click on Initial Conds Go and you will see a much better homoclinic orbit.</p>
<p>Now that we have a much improved homoclinic orbit, we will continue in the parameter $<code>a</code>$ as desired. First, lets make sure we get the orbits when $<code>a=-6,-4,-2,2,4,6</code>$ so we will click on Usr Period and choose 6. Type in <code>a=-6,a=-4, etc</code> for the first 6 entries and then click OK. Click on Axes Hi to change the axes and the continuation parameter. Change the <code>Main Parm</code> to <code>a</code>, <code>Xmin=-7,Xmax=7</code> and click OK. Click on Numerics and change <code>Par Min=-6, Par Max=6</code> and then click OK. Now click on Run and you will see a line that is almost diagonal. When done, click on Grab again, and watch the bottom of the AUTO window until you see Per=35 and click Enter. In the Numerics menu, change <code>Ds=-.02</code> to change directions, and click Ok. Now click Run and there will be another diagonal line that is in the opposite direction. Click on Grab and grab point number 7 corresponding to <code>a=6</code>. In the XPP window, click on Init Conds go and you will see a distorted homoclinic. It is not that great and could be improved probably by continuing with <code>Per</code> some more. Grab the point labeled 11 (<code>a=-6</code>) and in XPP try to integrate it. It doesn\u2019t look even close. This is because the homoclinic orbit is unstable and shooting (which is what we are doing when we integrate the equation) is extremely sensitive to the stability of the orbits. In the AUTO window, click on File Import Orbit to get the orbit that AUTO computed using collocation. In the XPP main window, click on Restore and you will see a much better version of the homoclinic orbit. This is because collocation methods are not sensitive to the stability of orbits! In fact, you can verify that the fixed point (0,0) is a saddle-point with a positive eigenvalue, $<code>\\lambda_u</code>$ and a negative one of $<code>\\lambda_s</code>$ whose sum is the trace of the linearized matrix, $<code>-a</code>$. The sum of the eigenvalues is called the saddle-quantity and if it is positive (for us, $<code>a&lt;0</code>$), then the homoclinic is unstable.</p>
<p>We now describe how to find heteroclinic orbits. The methods are the same except that we must track two <em>different</em> fixed points. Thus, we need an additional $<code>n</code>$ equations for the other fixed point. As with homoclinic orbits, we go from the unstable manifold to the stable manifold. In this case, the \u201Cleft\u201D fixed point is the one emerging from the unstable manifold and the \u201Cright\u201D fixed point is the one going into the stable manifold. Thus, the dynamical system is :</p>
<pre><code class="language-math">\\begin{eqnarray*}
\\frac{dx}{ds} &amp;=&amp; P f(x,\\alpha) \\\\
\\frac{dx_{left}}{ds}&amp;=&amp; 0 \\\\
\\frac{dx_{right}}{ds}&amp;=&amp; 0 \\\\
f(x_{left}(0)) &amp;=&amp; 0 \\\\
f(x_{right}(1)) &amp;=&amp; 0 \\\\
L_s (x(0)-x_{left}(0)) &amp;=&amp; 0 \\\\
L_u (x(1)-x_{right}(1)) &amp;=&amp; 0.
\\end{eqnarray*}
</code></pre>
<p>The only difference is that we have the additional $<code>n</code>$ equations for the right fixed point and the $<code>n</code>$ additional boundaty conditions. It is important that you give good values for the initial conditions for the two fixed points since they are different and you need to converge to them. The classic bistable reactrion-diffusion equation provides a nice example of a heteroclinic. The equations are:</p>
<pre><code class="language-math">-cu&#39;=u&#39;&#39;+u(1-u)(u-a)
</code></pre>
<p>which we rewrite as a system:</p>
<pre><code class="language-math">\\begin{eqnarray*}
u&#39; &amp;=&amp; u_p \\equiv f(u,u_p) \\\\
u_p&#39; &amp;=&amp; -cu_p - u(1-u)(u-a) \\equiv g(u,u_p)
\\end{eqnarray*}
</code></pre>
<p>The fixed point $<code>(1,0)</code>$ has a one-dimensional unstable manifold and $<code>(0,0)</code>$ as a one-dimensional stable manifold. We seek a solution from $<code>(1,0)</code>$ to $<code>(0,0).</code>$ For $<code>a=0.5</code>$ and $<code>c=0</code>$, there is an exact solution joining the two saddle points. (Prove this by showing that</p>
<pre><code class="language-math">u_p^2 +u^4/2-2u^3/3+u^4/2
</code></pre>
<p>is constant along solutions when $<code>a=0.5,c=0.</code>$) We will use this as a starting point in our calculation. Here is the ODE file:</p>
<pre><code># tstheti.ode
# a heteroclinic orbit
# unstable at u=1, stable at u=0
f(u,up)=up
g(u,up)=-c*up-u*(1-u)*(u-a)
# the dynamics
u&#39;=up*per
up&#39;=(-c*up-u*(1-u)*(u-a))*per
# dummy equations for the fixed points
uleft&#39;=0
upleft&#39;=0
uright&#39;=0
upright&#39;=0
# the velocity parameter
c&#39;=0
# fixed points
b f(uleft,upleft)
b g(uleft,upleft)
b f(uright,upright)
b g(uright,upright)
# projection conditions 
b hom_bcs(0)
b hom_bcs(1)
# parameters
par per=6.67,a=.5
# initial data
init u=.918,up=-.0577,c=0
# initial fixed points
init uleft=1,upleft=0,uright=0,upright=0
@ total=1.01,dt=.01
@ xp=u,yp=up,xlo=-.25,xhi=1.25,ylo=-.75,yhi=.25
# some AUTO parameters
@ epss=1e-7,epsu=1e-7,epsl=1e-7,parmax=60,dsmax=5,dsmin=1e-4,ntst=35
done
</code></pre>
<p>I have added a few AUTO numerical settings so that I don\u2019t have to set them later. Run XPP with this ODE file and integrate the equations. We will now continue this approximate heteroclinic in the parameter <code>per</code>. Fire up AUTO (File Auto) and click on Axes Hi. Put <code>c</code> on the y-axis and make <code> Xmin=0,Xmax=60,Ymin=-2,Ymax=2</code>. Then click OK. As above, we will also keep solutions at particular values of <code>per</code> by clicking on <code> Usr period</code> <code>3</code>, choosing <code>per=20,per=40,per=60</code>, and then OK. Now we are ready to run. Click on Run Homoclinic. When the dialog box comes up set the following: <code>Left Eq: ULEFT, Right Eq: URIGHT, NUnstable:1, NStable:1</code> and then click OK. You should see a nice straight line go across the screen. Grab the point labeled <code> per=40</code> and then click on File Import Orbit. Look at it in the XPP main window and freeze it. Now in the Auto window, click on Axes Hi and change the Main Parm to <code>a</code> and <code>Xmax=1</code>. Click OK and then click on Numerics to get the Auto Numerics dialog. Change <code> Dsmax=0.1, Par Max=1</code> and click OK. Now click on Usr Period 4 and make the four user functions <code>a=.75,a=.9,a=.25,a=.1</code> and click OK. Now click on Run and watch a line drawn across the screen. This is the velocity, $<code>c</code>$ as a function of the threshold, $<code>a</code>$. Click on Grab and looking at the bottom of the screen, wait until you see <code>per=40</code> and then click on Enter. Now, open the Numerics dialog box and change <code>Ds=-.02</code> to go the other direction. Click on Run and you should see the rest of the line drawn across the screen. Click on Grab and move to the point labeled <code>a=0.25</code> Click on File Import Orbit and plot this in the main XPP window.</p>
`,headings:[{id:"the-auto-view",text:"The AUTO view",level:2},{id:"preparation",text:"Preparation",level:2},{id:"choosing-parameters",text:"Choosing parameters",level:2},{id:"diagram-axes",text:"Diagram axes",level:2},{id:"hi",text:"(H)i",level:3},{id:"norm",text:"(N)orm",level:3},{id:"hi-lo",text:"h(I)-lo",level:3},{id:"period",text:"(P)eriod",level:3},{id:"two-par",text:"(T)wo par",level:3},{id:"zoom",text:"(Z)oom",level:3},{id:"last-1-par",text:"last (1) par",level:3},{id:"last-2-par",text:"last (2) par",level:3},{id:"frequency",text:"(F)requency",level:3},{id:"average",text:"(A)verage",level:3},{id:"y-axis",text:"Y-axis",level:3},{id:"main-parm",text:"Main Parm",level:3},{id:"2nd-parm",text:"2nd Parm",level:3},{id:"xmin-ymax",text:"Xmin ... Ymax",level:3},{id:"numerical-parameters",text:"Numerical parameters",level:2},{id:"ntst",text:"Ntst",level:3},{id:"nmax",text:"Nmax",level:3},{id:"npr",text:"Npr",level:3},{id:"ds",text:"Ds",level:3},{id:"dsmin",text:"Dsmin",level:3},{id:"dsmax",text:"Dsmax",level:3},{id:"par-min",text:"Par Min",level:3},{id:"par-max",text:"Par Max",level:3},{id:"norm-min",text:"Norm Min",level:3},{id:"norm-max",text:"Norm Max",level:3},{id:"user-functions",text:"User functions",level:2},{id:"running",text:"Running",level:2},{id:"ep",text:"EP",level:3},{id:"lp",text:"LP",level:3},{id:"tr",text:"TR",level:3},{id:"pd",text:"PD",level:3},{id:"uz",text:"UZ",level:3},{id:"mx",text:"MX",level:3},{id:"bp",text:"BP",level:3},{id:"grabbing",text:"Grabbing",level:2},{id:"regular-point",text:"Regular Point",level:3},{id:"hopf-point",text:"Hopf Point",level:3},{id:"period-doubling",text:"Period doubling",level:3},{id:"limit-point",text:"Limit point",level:3},{id:"periodic-point",text:"Periodic point",level:3},{id:"torus-point",text:"Torus point",level:3},{id:"aborting",text:"Aborting",level:2},{id:"saving-diagrams",text:"Saving diagrams",level:2},{id:"import-orbit",text:"Import orbit",level:3},{id:"save-diagram",text:"Save diagram",level:3},{id:"load-diagram",text:"Load Diagram",level:3},{id:"postscript",text:"Postscript",level:3},{id:"reset-diagram",text:"Reset diagram",level:3},{id:"write-pts",text:"Write pts",level:3},{id:"homoclinics-and-heteroclinics",text:"Homoclinics and heteroclinics",level:2}]},{id:"10-animations",title:"Creating Animations",html:`<p>Many years ago, as a teenager, I used to make animated movies using various objects like Kraft caramels (\u201CCaramel Knowledge\u201D) and vegetables (\u201CThe Call of the Wild Vegetables\u201D). After I got my first computer, I wanted to develop a language to automate computer animation. As usual, things like jobs, family, etc got in the way and besides many far better programmers have created computer assisted animation programs. Thus, I abandoned this idea until I recently was simulating a simple toy as a project with an undergraduate. I thought it would be really cool if there were a way to pipe the output of a solution to the differential equation into some little cartoon of the toy. This would certainly make the visualization of the object much more intuitive. There were immediately many scientific reasons that would make such visualization useful as well. Watching the gaits of an animal or the waving of cilia or the synchronization of many oscillators could be done much better with animation than two and three dimensional plots of the state variables.</p>
<p>With this in mind, I have developed a simple scripting language that allows the user to make little cartoons and show them in a dedicated window. The following steps are required</p>
<ul>
<li><p>Run the numerical simulation for however long you need it.</p>
</li>
<li><p>Using a text editor create a description of the animation using the little scripting language described below.</p>
</li>
<li><p>Click on the <code>(V)iew axes</code> <code>(T)oon</code> menu item from the main XPP window.</p>
</li>
<li><p>Click on the <code>File</code> item in the animation window that pops up and give it the name of your script file.</p>
</li>
<li><p>If the file is OK, click on <code>Go</code> item and the animation will begin.</p>
</li>
</ul>
<h2 id="the-animation-view">The animation view</h2>
<p><strong>In web2</strong>, <code>(V)iew axes</code> <code>(T)oon</code> opens the <strong>Animation</strong> tab
(<code>ui/AniView.tsx</code>) instead of a separate resizeable window; there is no
pixmap-memory limit to run out of. The core computes each frame&#39;s
primitives in the <code>.ani</code> file&#39;s own unit coordinates (the <code>ani</code> <code>frame</code>
data event, <code>core/ani_data.cpp</code>) and the page draws them on a canvas
scaled to the tab&#39;s own size, so the picture fits any viewport from a
phone to a wide desktop. The controls are the same ideas as the X11
window&#39;s six buttons, as player controls:</p>
<ul>
<li><strong>File</strong>: loads a new animation file (usual extension <code>filename.ani</code>,
as below), through the browser&#39;s file dialog like any other Open.</li>
<li><strong>Go / Pause</strong>: plays and pauses; a step after Pause continues from the
frame shown, unlike the X11 window&#39;s slow round trip through Pause.</li>
<li><strong>Reset</strong>: moves the animation back to the beginning.</li>
<li><strong>the seek slider, and the frame step buttons</strong> (<code>&gt;&gt;&gt;&gt;</code>/<code>&lt;&lt;&lt;&lt;</code> in
X11): move to any frame or step one at a time; the delay between frames
sets the speed (Fast/Slow in X11 are this delay).</li>
<li><strong>Skip</strong>: sets the number of frames to skip while playing.</li>
<li>Saving frames: kinescope-style export (GIF/PNG made in the page from
the frames it already has) replaces <code>Mpeg</code> and the <code>anigif</code> option; see
&quot;Saving pictures and files&quot; in
<a href="04-using-the-interface.md">Using the interface</a>.</li>
</ul>
<p>At a phone width the tab is a full-screen sheet, with 44 px touch targets
on every control, and no sideways scroll.</p>
<h2 id="dasl-dynamical-animation-scripting-language">DASL: Dynamical Animation Scripting Language</h2>
<p>In order to use the animation components of XPP, you must first describe the animation that you want to do. This is done by creating a script with a text editor that describes the animation. You describe the coordinates and colors of a number of simple geometric objects. The coordinates and colors of these objects can depend on the values of the variables (both regular and fixed, but not auxiliary) at a given time. The animation then runs through the output of the numerical solution and draws the objects based on that output. I will first list all the commands and then give some examples.</p>
<p>Basically, there are two different types of objects: (i) transient and (ii) permanent. Transient objects have coordinates that are recomputed at every time as they are changing with the output of the simulation. Permanent objects are computed once and are fixed through the duration of the simulation. The objects themselves are simple geometric figures and text that can be put together to form the animation.</p>
<p>Each line in the script file consists of a command or object followed by a list of coordinates that must be separated by semicolons. For some objects, there are other descriptors which can be optional. Since color is important in visualization, there are two ways a color can be described. Either as a formula which is computed to yield a number between 0 and 1 or as an actual color name started with the dollar sign symbol, $. The <code>ani</code> file consists of a lines of commands and objects which are loaded into XPP and played back on the animation window. Here are the commands:</p>
<ul>
<li><strong><strong>dimension</strong></strong>: xlo;ylo;xhi;yho</li>
<li><strong><strong>speed</strong></strong>: delay</li>
<li><strong><strong>transient</strong></strong>:</li>
<li><strong><strong>permanent</strong></strong>:</li>
<li><strong><strong>line</strong></strong>: x1;y1;x2;y2;color;thickness</li>
<li><strong><strong>rline</strong></strong>: x1;y1;color;thickness</li>
<li><strong><strong>rect</strong></strong>: x1;y1;x2;y2;color;thickness</li>
<li><strong><strong>frect</strong></strong>: x1;y1;x2;y2;color</li>
<li><strong><strong>circ</strong></strong>: x1;y1;rad;color;thickness</li>
<li><strong><strong>fcirc</strong></strong>: x1;y1;rad;color</li>
<li><strong><strong>ellip</strong></strong>: x1;y1;rx;ry;color;thickness</li>
<li><strong><strong>fellip</strong></strong>: x1;y1;rx;ry;color</li>
<li><strong><strong>comet</strong></strong>: x1;y1;type;n;color</li>
<li><strong><strong>text</strong></strong>: x1;y1;s</li>
<li><strong><strong>vtext</strong></strong>: x1;y1;s;z</li>
<li><strong><strong>settext</strong></strong>: size;font;color</li>
<li><strong><strong>xnull</strong></strong>: x1;y1;x2;y2;color;id</li>
<li><strong><strong>ynull</strong></strong>: x1;y1;x2;y2;color;id</li>
<li><strong><strong>end</strong></strong>:</li>
</ul>
<p>All commands can be abbreviated to their first three letters and case is ignored. At startup the dimension of the animation window in user coordinates is (0,0) at the bottom left and (1,1) at the top right. Thus the point (0.5,0.5) is the center no matter what the actual size of the window on the screen. <strong>Color</strong> is described by either a floating point number between 0 and 1 with 0 corresponding to red and 1 to violet. When described as a floating point number, it can be a formula that depends on the variables. In all the commands, the color is optional <em>except</em> <strong>settext.</strong> The other way of describing color is to use names which all start with the $ symbol. The names are: <strong>$WHITE, $RED, $REDORANGE, $ORANGE, $YELLOWORANGE, $YELLOW, $YELLOWGREEN, $GREEN, $BLUEGREEN, $BLUE,$PURPLE, $BLACK</strong>.</p>
<p>The <strong>transient</strong> and <strong>permanent</strong> declarations tell the animator whether the coordinates have to be evaluated at every time or if they are fixed for all time. The default when the file is loaded is <strong>transient.</strong> Thus, these are just toggles between the two different types of objects.</p>
<p>The number following the <strong>speed</strong> declaration must be a nonnegative integer. It tells tha animator how many milliseconds to wait between pictures.</p>
<p>The <strong>dimension</strong> command requires 4 numbers following it. They are the coordinates of the lower left corner and the upper right. The defaults are (0,0) and (1,1).</p>
<p>The <strong>settext</strong> command tells the animator what size and color to make the next text output. The size must be an integer, <strong>{ 0,1,2,3,4 }</strong> with 0 the smallest and 4 the biggest. The font is either <strong>roman</strong> or <strong>symbol.</strong> The color must be a named color and not one that is evaluated.</p>
<p>The remaining ten commands all put something on the screen.</p>
<ul>
<li><p><strong><strong>line x1;y1;x2;y2;color;thickness</strong></strong> : draws a line from <strong>(x1,y1)</strong> to <strong>(x2,y2)</strong> in user coordinates. These four numbers can be any expression that involves variables and fixed variables from your simulation. They are evaluated at each time step (unless the line is <strong>permanent</strong>) and this is scaled to be drawn in the window. The <strong>color</strong> is optional and can either be a named color or an expression that is to be evaluated. The <strong>thickness</strong> is also optional but if you want to include this, you must include the <strong>color</strong> as well. <strong>thickness</strong> is any nonnegative integer and will result in a thicker line.</p>
</li>
<li><p><strong><strong>rline x1;y1;color;thickness</strong></strong> : is similar to the <strong>line</strong> command, but a line is drawn from the endpoints of the last line drawn to <strong>(xold+x1,yold+y1)</strong> which becomes then new last point. All other options are the same. This is thus a \u201Crelative\u201D line.</p>
</li>
<li><p><strong><strong>rect x1;y1;x2;y2;color;thickness</strong></strong> : draws a rectangle with lower corner <strong>(x1,y1)</strong> to upper corner <strong>(x2,y2)</strong> with optional color and thickness.</p>
</li>
<li><p><strong><strong>frect x1;y1;x2;y2;color</strong></strong> : draws a filled rectangle with lower corner <strong>(x1,y1)</strong> to upper corner <strong>(x2,y2)</strong> with optional color.</p>
</li>
<li><p><strong><strong>circ x1;y1;rad;color;thick</strong></strong> : draws a circle with radius <strong>rad</strong> centered at <strong>(x1,y1)</strong> with optional color and thickness.</p>
</li>
<li><p><strong><strong>fcirc x1;y1;rad;color</strong></strong> : draws a filled circle with radius <strong>rad</strong> centered at <strong>(x1,y1)</strong> with optional color.</p>
</li>
<li><p><strong><strong>ellip x1;y1;rx;ry;color</strong></strong> : draws an ellipse with radii <strong>rx,ry</strong> centered at <strong>(x1,y1)</strong> with optional color and thickness.</p>
</li>
<li><p><strong><strong>fellip x1;y1;rx;ry;color</strong></strong> : draws a filled ellipse with radii <strong>rx,ry</strong> centered at <strong>(x1,y1)</strong> with optional color.</p>
</li>
<li><p><strong><strong>comet x1;y1;type;n;color</strong></strong>: keeps a history of the last <code> n</code> points drawn and renders them in the optional <code>color</code>. If <code> type</code> is non-negative, then the last n points are drawn as a line with thickness in pixels of the magnitude of <code>type</code>. If <code>type</code> is negative, filled circles are drawn with a radius of <code>-thick</code> in pixels.</p>
</li>
<li><p><strong><strong>text x1;y1;s</strong></strong>: draws a string <strong>s</strong> at position <strong>(x1,y1)</strong> with the current color and text properties. Only the coordinates can depend on the current values.</p>
</li>
<li><p><strong><strong>vtext x1;y1;s;z</strong></strong>: draws a string <strong>s</strong> followed by the floating point value <strong>z</strong> at position <strong>(x1,y1)</strong> with the current color and text properties. Thus, you can print out the current time or value of a variable at any given time.</p>
</li>
<li><p><strong><strong>xnull x1;y1;x2;y2;color;id</strong></strong>: uses the nullclines that you have already computed in your animation. You can use the static nullclines by just choosing <strong>-1</strong> for the <strong>id</strong> parameter. To use dynamic nullclines, you must compute a range of nullclines using the Nullcline Freeze Range command. The parameter <strong>id</strong> runs from 0 to N where N is the number of nullclines that you have computed in the range dialog. The animator converts <strong>id</strong> to an integer and tests whether it is in the range and then loads the appropriate nullcline. There is an example shown below. The <strong>ynull</strong> command is identical. The parameters <strong>x1,y1,x2,y2</strong> tell the animator the window in which the nullclines are defined. These should be the lower-left and upper right corners of the phaseplane where the nullclines were computed.</p>
</li>
</ul>
<p><em>REMARK.</em> As with lines in ODE files, it is possible to create arrays of commands using the cobination of the <strong>[i1..i2]</strong> construction. Some examples are shown below.</p>
<h2 id="examples">Examples</h2>
<p>I will start out with a simple pendulum example and then a bunch of more interesting examples. Here is the old pendulum again:</p>
<pre><code># damped pendulum pend.ode
dx/dt = xp
dxp/dt = (-mu*xp-m*g*sin(x))/(m*l)
pe=m*g*(1-cos(x))
ke=.5*m*l*xp^2
aux P.E.=pe
aux K.E.=ke
aux T.E=pe+ke
x(0)=2
param m=10,mu=1,g=9.8,l=1
param scale=0.008333
@ bounds=1000
done
</code></pre>
<p>I have added an initial condition and another parameter used to scale the magnitude of the kinetic energy for a later animation. Fire up XPP and run this simulation. Now we will create a very simple animation:</p>
<pre><code># pend.ani
# simple animation file for pendulum
line .5;.5;.5+.4*sin(x);.5-.4*cos(x);$BLACK;3
fcircle .5+.4*sin(x);.5-.4*cos(x);.05;$RED
end
</code></pre>
<p>Notice that comments are allowed. This file is included with the distribution as is the ODE file so you don\u2019t have to type it in. There are only two lines of code. The first tells the animator to draw a line from the center of the screen at <strong>(.5,.5)</strong> to a point <strong>( 5+.4*sin(x), .5-.4*cos(x) )</strong>, where <strong>x</strong> is the variable in the ODE file for the pendulum. The line has thickness 3 and is black. The next line of code says to draw a filled circle centered at the same point as the line was with radius <strong>0.05</strong>. This will be colored red. Finally, we tell the interpreter that this is the end of the commands. In case you haven\u2019t already done it, click on (Initialconds) (Go) to solve the ODE. You should see a damped oscillation. Now click on (Viewaxes) (Toon). A new window will appear with the \u201Ctest pattern\u201D on the screen. In this window click on (File) and type in <strong>pend.ani</strong> at the prompt. XPP will tell you that two lines were loaded successfully. Comments are ignored. Click on (Go) in the animation window. You will see a pendulum appear and rock back and forth. It may be somewhat jerky depending on the server and graphics properties of your computer and graphics. You can stop it by clicking on (Pause), speed it up by clicking on (Fast) and slow it down by clicking on (Slow). The reaction to mouse clicks is terribly slow, so to test animation, I would integrate just for a short time at first until you are satisfied. You can edit the file and reload it with the (File) command.</p>
<p>Now we consider a much more complicated animation file:</p>
<pre><code># pend2.ani
# fancy animation file for pendulum
PERMANENT
settext 3;rom;$PURPLE
text .25;.9;Pendulum
line 0;.5;1;.5;$BLUE;4
SPEED 10
TRANSIENT
line .5;.5;.5+.4*sin(x);.5-.4*cos(x);$BLACK;3
fcircle .5+.4*sin(x);.5-.4*cos(x);.05;1.-scale*ke
settext 1;rom;$BLACK
vtext .05;.1;t=;t
settext 1;sym;$BLACK
vtext .05;.05;q=;x
end
</code></pre>
<p>The first noncomment tells the animator that what follows will be on every frame exactly as intially defined. The text is made fairly large and purple in Times-Roman font. The <strong>text</strong> command puts the text a quarter away across the screen near the top and write \u201CPendulum.\u201D Next a thick blue line is drawn across the middle of the screen to act as a \u201Ctether\u201D for the pendulum. We set the delay between frame to be 10 milliseconds with the <strong>SPEED</strong> command. Then all the remaining objects are to be <strong>TRANSIENT.</strong> The first line drawn is the arm of the pendulum. At the end, we place a filled circle, but the color of the circle is proportional to the kinetic energy. <strong>NOTE:</strong> We have used the <strong>fixed variable</strong> version of the kinetic energy, <strong>ke</strong> and not the <strong>auxiliary variable, K.E.</strong> since the latter is not \u201Cknown\u201D to the internal formula compiler but the former is. Next we set the text color and font stuff to small black roman letters and use the <strong>vtext</strong> command to tell the user the current time. Finally, we set the text to symbol and plot the value of the angle, $<code>\\theta</code>$ which is the same key as the letter \u201Cq.\u201D</p>
<p>Click on the (File) button in the animator. Load the file called <strong>pend2.ani</strong> and run it.</p>
<p>The next example is of a large dynamical system that represents a set of coupled excitable cells. The ODE file is called <code>wave.ode</code> and is included in the distribution. Here it is</p>
<pre><code># wave.ode
# pulse wave with diffusional coupling 
param a=.1,d=.2,eps=.05,gamma=0,i=0
vv0(0)=1
vv1(0)=1
f(v)=-v+heav(v-a)
#
vv0&#39;=i+f(vv0)-w0+d*(vv1-vv0)
vv[1..19]&#39;=i+f(vv[j])-w[j]+d*(vv[j-1]-2*vv[j]+vv[j+1])
vv20&#39;=i+f(vv20)-w20+d*(vv19-vv20)
#
w[0..20]&#39;=eps*(vv[j]-gamma*w[j])
@ meth=qualrk,dt=.25,total=150,xhi=150
done
</code></pre>
<p>Now we will create an animation file that plots the values of the voltages, <strong>v0, ..., v20</strong> as beads along the vertical axis whose horizontal height is proportional to their voltage and whose color is proportional to the value of the recovery variables, <strong>w0, ..., w20</strong>. Since I have run the simulation, I know that the recovery variables are between 0 and 1 and that the voltages are between -1 and 1. Here is the one-line animation file for this effect. It is called <code>wave.ani</code> and is included with the distribution:</p>
<pre><code># wave.ani
# animated wave 
fcircle .05+.04*[0..20];.5*(vv[j]+1);.02;1-w[j]
end
</code></pre>
<p>This file uses the \u201Carray\u201D capabilities of the XPP parser to expand the single line into 21 lines from 0 to 20. I just draw a filled circle at scaled vertical coordinate and horizontal coordinate with a small radius and colored according to the recovery variable. The horizontal coordinate is expanded to be <code>.05 + .04*0</code>, <code>.05 + .04*1</code>, etc; the vertical is <code>.5*(vv0+1)</code>, <code>.5*(vv1+1)</code>, etc; and the color is <code>1-w0</code>, 1-w1, etc. Thus this is interpreted as a 21 line animation file. Try it to see what it looks like. It is a simple matter to add a vertical scale and time ticker.</p>
<p>This next example illustrates the use of the relative line command. Here, the model is a chain of 20 oscillators representing the phases of spinal motoneurons which control the muscles of the lamprey, an eel-like animal. We will also compute a cumulative \u201Cbend\u201D angle which is dependent on the phase of each oscillation. Here is the ODE file, called <code>lamprey.ode</code></p>
<pre><code># example of a chain of coupled oscillators
par grad=0,phi=.1
h(u)=sin(u+phi)
par af=1,ar=1
par bend=.1
x1&#39;=1+grad+af*h(x2-x1)
x[2..19]&#39;=1+grad*[j]+ar*h(x[j-1]-x[j])+af*h(x[j+1]-x[j])
x20&#39;=1+20*grad+ar*h(x9-x10)
# here is cumulative bend
an1=bend*sin(x1)
an[2..20]=bend*sin(x[j])+an[j-1]
@ bound=1000
@ total=50
done
</code></pre>
<p>Fire up XPP and run this. (Note that this should be run on a torus phase-space, but since we are only looking at the animation, it is not important. ) Now get the animation window and load in the file <code> fish.ani</code> which looks like:</p>
<pre><code># fish.ani
# lamprey swimmer -- oscchain.ode
line 0;.5;.04*cos(an1);.5+.04*sin(an1);$BLACK;3
rline .04*cos(an[2..20]);.04*sin(an[j]);$BLACK;3
END
</code></pre>
<p>Click on (Go) to watch it swim! I draw a series of short line segments relative to the previous one and at an angle that is determined by the <strong>bend</strong> parameter in the ODE file and on the phase of the controlling oscillator. For fun, rerun the simulation with the parameter <strong>grad</strong> set to 0.1.</p>
<p>The penultimate example revisits the Lorenz equations. These equations can be derived by looking at a simple water wheel which consists of a circle of leaky cups with water dripping into them. (See Strogatz for a nice derivation of the equations.) The angle of one of the cups with respect to the viewer is found by integrating the angular velocity which is proportional to the <strong>x</strong> variable in the equations. Thus, as a final example, I present an animation with 8 cups of the Lorenz water wheel. I have created another ODE file that has additional info that I will use called <code>lorenz2.ode</code>. Here it is:</p>
<pre><code># the famous Lorenz equation set up for animated waterwheel and
# some delayed coordinates as well
init x=-7.5  y=-3.6  z=30
par r=27  s=10  b=2.66666
par c=.2  del=.1
x&#39;=s*(-x+y)
y&#39;=r*x-y-x*z
z&#39;=-b*z+x*y
# x is proportional to the angular velocity so integral is angle
theta&#39;=c*x
th[0..7]=theta+2*pi*[j]/8
# approximate the velocity vector in the butterfly coords
z1=z-del*(-b*z+x*y)
x1=x-del*(s*(-x+y))
@ dt=.025, total=40, xplot=x,yplot=y,zplot=z,axes=3d
@ xmin=-20,xmax=20,ymin=-30,ymax=30,zmin=0,zmax=50
@ xlo=-1.5,ylo=-2,xhi=1.5,yhi=2,bound=10000
done
</code></pre>
<p>Fire it up with XPP and integrate it. Then load the animation file called <code>lorenz.ani</code> and run it. Here is the file:</p>
<pre><code># shows the waterwheel using the integrated ang velocity
# see Strogatz book.  Use lorenz2.ode
PERMANENT 
circ .515;.515;.46;$BLACK;2
TRANSIENT
SPEED 20
frect .5+.45*sin(th[0..7]);.5+.45*cos(th[j]);.55+.45*sin(th[j]);.55+.45*cos(th[j]);$BLACK
# plotting the butterfly and a lagged version of it !!
fcirc .5+x/40;z/50;.02;$GREEN
fcirc .5+x1/40;z1/50;.02;$RED
end
</code></pre>
<p>The waterwheel can be seen when running the animation. In the center of the screen are two colored dots that represent the $<code>(x,z)</code>$ coordinates (green) of the attractor and the approximate velocity of these two variables in red.</p>
<p>The last example shows the use of dynamic nullclines. Here is the ODE file:</p>
<pre><code>init u=.0426,v=.0843
u&#39;=-u+f(aee*u-aie*v-te+stim(t))
v&#39;=(-v+f(aei*u-aii*v-ti))/tau
par aee=15,aie=9,te=3
par aei=20,aii=3,ti=3,tau=5
stim(t)=s0+s1*if(t&lt;tdone)then(t/tdone)else(0)
par s0=0,tdone=5,s1=1.2
f(u)=1/(1+exp(-u))
@ xp=u,yp=v,xlo=-.1,ylo=-.1,xhi=1.1,yhi=1.1,total=50
done
</code></pre>
<p>and here is the animation file</p>
<pre><code>xnull -.1;-.1;1.1;1.1;$RED;10*stim(t)
ynull -.1;-.1;1.1;1.1;$GREEN;10*stim(t)
fcircle (u+.1)/1.2;(v+.1)/1.2;.025;$BLACK
end
</code></pre>
<p>The stimulus goes from 0 to 1.2. Since nullclines are computed with $<code>t=0</code>$, the stimulus is essentially zero as far as XPP is concerned when the nullclines are computed. However, I have added a dummy parameter <code>s0</code> which we will vary between 0 and 1.2 to get a family of nullclines. Thus, click on Nullcline Freeze Range and use <code>s0</code> as the range parameter, 12 steps, with a Low value of 0 and a High value of 1.2. You will see 13 nullclines drawn. Think of them as nullclines $<code>0,1,2,\\ldots,12</code>$. They correspond to <code>s0</code>=$<code>0, 0.1,\\ldots, 1.2.</code>$ Integrate the ODEs. Click on Viewaxes Toon and load the above animation file. Click on Go and watch it animate the nullclines as well as show the solution. The first 4 terms in the nullcline directive scale the nullclines to fit on the animator. They are the same values as the phaseplane window. The next term is just the color. The final <strong>id</strong> parameter is coded as 10 times the stimulus. Thus, when the stimulus is at .85, this evaluates to 8.5 and XPP truncates this to the integer, 8, and draws nullcline \u201C8\u201D which is the nullcline for a stimulus of strength 0.8. If you draw enough of them, then it wont appear to jump much.</p>
<h2 id="saving-a-movie">Saving a movie</h2>
<p>X11 XPP&#39;s MPEG button wrote a <code>.ppm</code> file per frame and left you to run
the external <code>mpeg_encode</code> program on them yourself (a workflow this
section used to walk through step by step, disk space and all). <strong>In
web2</strong> there is no PPM-writing step and no external encoder: capture
frames with the kinescope (they are data \u2014 series, marks and the
viewport \u2014 not pixels) and export them as an animated GIF or PNG frames
from the page itself (docs/ui-v2.md T15). See &quot;Saving pictures and files&quot;
in <a href="04-using-the-interface.md">Using the interface</a>.</p>
`,headings:[{id:"the-animation-view",text:"The animation view",level:2},{id:"dasl-dynamical-animation-scripting-language",text:"DASL: Dynamical Animation Scripting Language",level:2},{id:"examples",text:"Examples",level:2},{id:"saving-a-movie",text:"Saving a movie",level:2}]},{id:"11-dll-libraries",title:"Creating C-files for faster simulations",html:`<h2 id="dynamically-linked-libraries">Dynamically linked libraries</h2>
<p>If your OS supports dynamically linked libraries, then it is easy to hook a right-hand side defined in C or even partially defined in C. You will first have to edit the Makefile so that it will use a dynamically linked library. I do not distribute it with this option turned on since it is seldom used. In the <code>CFLAGS</code> definition, add <code>-DHAVEDLL</code> and in the <code>LIBS</code> definition, add <code>-ldl</code>. Recompile XPP with these options.</p>
<p>Now you will be able to load libraries that contain one or more sets of right-hand sides. I will now present an elementary example. Consider the following ODE file <code>tstdll.ode</code></p>
<pre><code># test of dll
x&#39;=xp
y&#39;=yp
xp=0
yp=0
export {x,y,a,b,c,d,t} {xp,yp}
par a=1,b=1,c=1,d=1
done
</code></pre>
<p>The code beginning with <code>export</code> tells XPP we will call an external function routine, passing the 7 items <code>x,y,a,b,c,d,t</code> and returning the two items <code>xp,yp</code>. Note that they are fixed variables and set to zero at initialization, but XPP will override this when it is run if a library is loaded. They are just dummy place holders for the true right-hand sides. Now lets define the right-hand sides.</p>
<p>Here is the C-file called <code>funexample.c</code></p>
<pre><code>#include &lt;math.h&gt;
/*  
 some example functions
*/

lv(double *in,double *out,int nin,int nout,double *v,double *cn)
{
  double x=in[0],y=in[1];
  double a=in[2],b=in[3],c=in[4],d=in[5];
   double t=in[6];
  out[0]=a*x*(b-y);
  out[1]=c*y*(-d+x);
}

vdp(double *in,double *out,int nin,int nout,double *v,double *cn)
{
  double x=in[0],y=in[1];
  double a=in[2],b=in[3],c=in[4],d=in[5];
   double t=in[6];
  out[0]=y;
  out[1]=-x+a*y*(1-x*x);
}

duff(double *in,double *out,int nin,int nout,double *v,double *cn)
{
  double x=in[0],y=in[1];
  double a=in[2],b=in[3],c=in[4],d=in[5];
 double t=in[6];
  out[0]=y;
  out[1]=x*(1-x*x)+a*sin(b*t)-c*y;
}
</code></pre>
<p>This defines 3 different models, the Lotka-Volterra equation, the van der Pol equation, and the Duffing equation. Note that in addition to the parameters and variables that are passed by the user, XPP also passes all the variable and parameter information. The order is generally as follows. The array <code>v</code> contains <code>t</code>, the variables in the order they are defined followed by the fixed variables in the order defined. The array <code>cn</code> contains the parameters define in your model. <code>cn[2]</code> contains the first parameter and the others are defined in the order created. Thus, for the ode file <code>tstdll.ode</code>, we have the following identifications:</p>
<pre><code>cn[2]=a,cn[3]=b,cn[4]=c,cn[5]=d
v[0]=t,v[1]=x,v[2]=y,v[3]=xp,v[4]=yp
</code></pre>
<p>Edit and save the C file and type <code>make -f Makefile.lib</code> which will compile the module and then create a shared library. Run XPP using the ode file <code>tstdll.ode</code>. Now click on <code>File</code> <code>Edit</code> and choose the load dynamic library option. For the library name, use the full path, unless you have put the library in <code>usr/lib</code>. Or better yet, before you run XPP, type <code>export LD_LIBRARY_PATH=.</code> or <code> setenv LD_LIBRARY_PATH=.</code>, depending on your shell and then XPP will look in the current directory for the library. Then for the function, choose one of <code>lv, vdp, duff</code> which are the three right-hand sides define above. The main advantage of dynamically linked libraries is that if you have a right-hand side that is three pages of computer output, then you can still use XPP with no problems.</p>
<h2 id="completely-defining-the-right-hand-sides-in-c">Completely defining the right-hand sides in C</h2>
<p>This is probably not something you want to do very often. It is better to use the above approach. This method forces XPP to be a stand-alone problem for a single ODE. For very complicated models such as discretizations of a PDE you may want to compile the right-hand sides and run a dedicated program for that particular model. This is not as hard as it seems. You must first create a library, which is done by typing <code>make lib</code> after you have successfully compiled all of XPP: this builds <code>build/obj/libxppcore.a</code> (upstream&#39;s <code>make xpplib</code>/<code>libxpp.a</code>, before the numerics library and the X11 front end were split apart; there is no <code>-lX11</code> to link against any more, since xppautX has no X11 front end, so a program linked this way gets the headless core only \u2014 no browser page, no <code>-silent</code> batch entry point of its own). You only need to make this library once. You can then move it to where you usually keep libraries if you want. Now all you do is create a C file for the right-hand sides (I will show you below), say, it is called <code>lorenzrhs.c</code> and then type:</p>
<pre><code>gcc lorenzrhs.c -o lorenz build/obj/libxppcore.a -lm
</code></pre>
<p>and if all goes well, you will have an executable called \u201Clorenz\u201D. If the ODE file was called \u201Clorenz.ode\u201D then run this as follows:</p>
<pre><code>lorenz lorenz.ode
</code></pre>
<p>as XPP needs the information contained in the ODE file to tell it the names of variables and parameters, etc.</p>
<p>Since the names of the parameters are interpreted by XPP as references to a certain array, you must communicate their values to the C functions for your right-hand sides. XPP provides a little utility for this. In the (Files) submenu. click on (c-Hints) and a bunch of defines will be pronted to the console. You can use these in your program. All of your c-files must have the following skeleton:</p>
<pre><code>#include &lt;math.h&gt;

extern double constants[]; 
main(argc,argv)
 char **argv; 
 int argc;
{
 do_main(argc,argv);
 }



my_rhs(t,y,ydot,neq)
 double t,*y,*ydot; 
 int neq;
{

}

extra(y,t,nod,neq)
 double t,*y; 
 int nod,neq;
{

}
</code></pre>
<p>In addition, you must define the parameters for the problem. Clicking on the (c-Hints) will essentially write this skeleton along with the defines for the problem. Lets take the lorenz equation as an example. Here is the ODE file:</p>
<pre><code># the famous Lorenz equation set up for 3d view
init x=-7.5  y=-3.6  z=30
par r=27  s=10  b=2.66666  
x&#39;=s*(-x+y)
y&#39;=r*x-y-x*z
z&#39;=-b*z+x*y
@ dt=.025, total=40, xplot=x,yplot=y,zplot=z,axes=3d
@ xmin=-20,xmax=20,ymin=-30,ymax=30,zmin=0,zmax=50
@ xlo=-1.5,ylo=-2,xhi=1.5,yhi=2
done
</code></pre>
<p>Run XPP with this file as the input file and click on the (File) (c-Hints) and the following will be written to the console:</p>
<pre><code>#include &lt;math.h&gt;

 extern double constants[]; 
main(argc,argv)
 char **argv; 
 int argc;
{
 do_main(argc,argv);
 }
/* defines for lorenz.ode  */ 
#define r constants[2]
#define s constants[3]
#define b constants[4]
#define X y[0]
#define XDOT ydot[0]
#define Y y[1]
#define YDOT ydot[1]
#define Z y[2]
#define ZDOT ydot[2]
my_rhs(t,y,ydot,neq)
 double t,*y,*ydot; 
 int neq;
{
  }
extra(y,t,nod,neq)
 double t,*y; 
 int nod,neq;
{
  }
</code></pre>
<p>You just fill in the blanks as follows to produce the required C file:</p>
<pre><code>#include &lt;math.h&gt;

extern double constants[];

main(argc,argv)
     char **argv;
     int argc;
{
  do_main(argc,argv);
}

/* defines for lorenz.ode  */ 
#define r constants[2]
#define s constants[3]
#define b constants[4]
#define X y[0]
#define XDOT ydot[0]
#define Y y[1]
#define YDOT ydot[1]
#define Z y[2]
#define ZDOT ydot[2]

extra(y, t,nod,neq)
     double *y,t;
     int nod,neq;
{
 return; 
}

my_rhs( t,y,ydot,neq)
 double t,*y,*ydot;
 int neq;
{
 XDOT=s*(-X+Y);
YDOT=r*X-Y-X*Z;
ZDOT=-b*Z+X*Y;

}
</code></pre>
<p>Now just compile this and link it with the XPP library and run it with \u201Clorenz.ode\u201D as the input and it will be a dedicated solver of the lorenz equations.</p>
<p>Here is a final example that shows you how to include user-defined functions and auxiliary variables. Here is the ODE file:</p>
<pre><code># The Morris-Lecar model as in our chapter in Koch &amp; Segev
#  A simple membrane oscillator.  
#
params v1=-.01,v2=0.15,v3=0.1,v4=0.145,gca=1.33,phi=.333
params vk=-.7,vl=-.5,iapp=.08,gk=2.0,gl=.5,om=1
minf(v)=.5*(1+tanh((v-v1)/v2))
ninf(v)=.5*(1+tanh((v-v3)/v4))
lamn(v)= phi*cosh((v-v3)/(2*v4))
ica=gca*minf(v)*(v-1)
v&#39;=  (iapp+gl*(vl-v)+gk*w*(vk-v)-ica)*om
w&#39;= (lamn(v)*(ninf(v)-w))*om
aux I_ca=ica
b v-v&#39;
b w-w&#39;
@ TOTAL=30,DT=.05,xlo=-.6,xhi=.5,ylo=-.25,yhi=.75
@ xplot=v,yplot=w
set vvst {xplot=t,yplot=v,xlo=0,xhi=100,ylo=-.6,yhi=.5,total=100 \\
    dt=.5,meth=qualrk}
done
</code></pre>
<p>and here is the C-file you need:</p>
<pre><code>#include &lt;math.h&gt;

extern double constants[];

main(argc,argv)
     char **argv;
     int argc;
{
  do_main(argc,argv);
}


/* defines for lecar.ode  */ 
#define v1 constants[2]
#define v2 constants[3]
#define v3 constants[4]
#define v4 constants[5]
#define gca constants[6]
#define phi constants[7]
#define vk constants[8]
#define vl constants[9]
#define iapp constants[10]
#define gk constants[11]
#define gl constants[12]
#define om constants[13]
#define V y[0]
#define VDOT ydot[0]
#define W y[1]
#define WDOT ydot[1]
#define I_CA y[2]



double minf(v)
     double v;
{
  return .5*(1+tanh((v-v1)/v2));
}

double ninf(v)
     double v;
{
  return .5*(1+tanh((v-v3)/v4));
}

double lamn(v)
     double v;
{
  return phi*cosh((v-v3)/(2*v4));
}


extra(y, t,nod,neq)
     double *y,t;
     int nod,neq;
{
  I_CA=gca*minf(V)*(V-1.0);
}

my_rhs( t,y,ydot,neq)
 double t,*y,*ydot;
 int neq;
{
VDOT=om*(iapp+gl*(vl-V)+gk*W*(vk-V)+gca*minf(V)*(1-V));
WDOT=om*lamn(V)*(ninf(V)-W);
}
</code></pre>
<p>Note that the user-defined functions appear as C functions and the auxiliary variables are almost like regular ones and are defined in the \u201Cextra\u201D function.</p>
`,headings:[{id:"dynamically-linked-libraries",text:"Dynamically linked libraries",level:2},{id:"completely-defining-the-right-hand-sides-in-c",text:"Completely defining the right-hand sides in C",level:2}]},{id:"12-numerical-methods-notes",title:"Some comments on the numerical methods",html:`<p>Most are standard.</p>
<ol>
<li><p>The BVP solve works by shooting and using Newton\u2019s method. All Jacobi matrices are computed numerically.</p>
</li>
<li><p>The nullclines are found by dividing the window into a grid and evaluating the vector field at each point. Zero contours are found and plotted.</p>
</li>
<li><p>Equilibria are found with Newton\u2019s method and eigenvalues are found by the QR algorithm. Invariant sets are found by using initial data along an eigenvector for the corresponding eigenvalue. The eigenvector is computed by inverse iteration.</p>
</li>
<li><p>The Gear algorithm is out of Gear\u2019s text on numerical methods.</p>
</li>
<li><p>The two adaptive algorithms qualrk4 and stiff are from Numerical Recipes.</p>
</li>
<li><p>CVODE is based on a C-version of LSODE. It was written by Scott D. Cohen and Alan C. Hindmarsh, Numerical Mathematics Group, Center for Computational Sciences and Engineering, L-316, Lawrence Livermore National Lab, Livermore, CA 94551. email: <a href="mailto:alanh@llnl.gov">alanh@llnl.gov</a>. You can get full documentation for this powerful package <code>http://netlib.bell-labs.com/netlib/ode/index.html</code>.</p>
</li>
<li><p>Dormand/Prince are from their book</p>
</li>
<li><p>Rosenbrock is based on a Matlab version of the two step Rosenbrock algorithm (see Numerical Recipes again)</p>
</li>
<li><p>Delay equations are solved by storing previous data and quadratically interpolating from this data.</p>
</li>
<li><p>Stability of delay equations is computed by a method suggested by Tatyana Luzyanina. The linearized stability for a delay equation results in solving a transcendental equation $<code>f(z)=0.</code>$ The idea is to use the argument principle and compute the total change in the argument of $<code>f(z)</code>$ as $<code>z</code>$ goes around a a contour $<code>C.</code>$ The number of times divided by $<code>2\\pi</code>$ tells us the number of roots of $<code>f</code>$ inside the contour. Thus, XPP simply adds values of the argument of $<code>f(z)</code>$ at discrete points on a large contour defined by the user and which encloses a big chunk of the right-half plane. Obviously the best it can do is give sufficient conditions for instability as there could always be roots outside the contour. But it seems to work pretty well with modest contours except near changes in stability. In addition, XPP tries to find a specific eigenvalue by using Newton\u2019s method on the characteristic equation. Since there are infinitely many possible roots to these transcendental equations, the root found can be arbitrary. However, suppose there is a single pair of roots in the right-half plane. Then guessing a positive root will often land you on the desired root. Using the Singular Point Range option will follow this particular root as a parameter varies. This can often lead to a discovery of the value of the parameter for which there is a Hopf bifurcation.</p>
</li>
<li><p>Adjoints are computed for stable periodic orbits by integrating the negative transpose of the numerically computed variational equation backwards in time using backward Euler.</p>
</li>
<li><p>The Volterra solver uses essentially an integrator (second order) based on the implicit product scheme described in Peter Linz\u2019s book on Volterra equations (SIAM,1985). For ODEs implicit schemes take considerably more time than explicit ones, but since most of the compute time for Volterra equations is in approximating the integral, this time penalty is minimal. Performance is gained primarily by taking advantage of convolution type equations.</p>
</li>
<li><p>Normally distributed noise is computed by the Box-Muller transformation of uniform noise.</p>
</li>
<li><p>The curve-fitting is done by using a heavily customized version of the Marquardt-Levenberg algorithm taken from Numerical Recipes in C.</p>
</li>
<li><p>The FFT is through the usual means</p>
</li>
<li><p>The algorithms in the bifurcation package are described in the AUTO manual available from Eusebius Doedel. I just wrote the interface.</p>
</li>
<li><p>DAEs of the form $<code>F(X&#39;,X,W,t)=0</code>$ are solved by fixing $<code>X</code>$ and using Newton\u2019s method to compute $<code>(X&#39;,W)</code>$. The value of $<code>X&#39;</code>$ is sent to the integrator to update $<code>X.</code>$ This apparently is not how DASSL does it. It is essentially the method used by Rheinboldt et al in their package MANPAK. (I must confess that I invented my own way to solve them out of the inability to get DASSL to work.)</p>
</li>
</ol>
`,headings:[]},{id:"13-colors",title:"Colors",html:`<p>The colors used in individual curves take on numbers from 0 to 10. Here is the meaning of the numbers:</p>
<p><strong>0-Black/White; 1-Red; 2-Red Orange; 3-Orange; 4-Yellow Orange; 5-Yellow; 6-Yellow Green; 7-Green; 8-Blue Green; 9-Blue; 10-Purple.</strong></p>
<p><strong>In web2</strong> these eleven indices (0 the foreground colour, then red through purple) map to a palette per theme (<code>plot/colors.ts</code>), keeping the hue named above; see <a href="04-using-the-interface.md">Using the interface</a> for the light/dark/system theme.</p>
`,headings:[]},{id:"14-options-file",title:"The options file",html:`<p>You can have many options files. They are useful for initializing XPP. However, because you can now set options from within the ODE file, options files are probably obsolete. They have the following format:</p>
<pre><code>9x15    BIG_FONT_NAME   &lt;-- menu fonts
fixed   SMALL_FONT_NAME &lt;-- IC, browser, etc fonts
0       BACKGROUND (1=white,0=black)
0   IXPLT &lt;--  X-axis variable (0=time)
1   IYPLT &lt;--  Y-axis variable
1   IZPLT &lt;--  Z-axis variable
0   AXES &lt;-- type of axis (0-2d 5-3d)
1   NJMP &lt;-- nOutput
40  NMESH &lt;-- Nullcline mesh
4   METHOD &lt;-- Integration method
1   TIMEPLOT &lt;--- set to zero if one axis is not time
8000    MAXSTOR &lt;--- maximum rows stored
20.0    TEND &lt;-- total integration time
.05 DT &lt;--- time step
0.0 T0 &lt;-- start time
0.0 TRANS &lt;-- transient
100.    BOUND &lt;--- bounds
.0001   HMIN &lt;-- min step for GEAR
1.0 HMAX &lt;-- max \`\`   \`\`   \`\`
.00001  TOLER &lt;-- tolerance for GEAR
0.0 DELAY &lt;-- maximal delay
0.0 XLO  &lt;--- 2D window sizes
20.0    XHI
-2.0    YLO
2.0 YHI
</code></pre>
<p>Within an ODE file, you can call up a different options file by typing</p>
<pre><code>option &lt;filename&gt;
</code></pre>
<p>where <code>&lt;filename&gt;</code> is the name of an options file. The full list of
option names (also usable as <code>@ name=value</code> lines in an ODE file) is in
<a href="16-quick-reference.md#the-options-list">Quick reference</a>.
The appearance keys above (<code>BIG_FONT_NAME</code>, <code>SMALL_FONT_NAME</code>,
<code>BACKGROUND</code>) are X11 window settings; the browser front end (web2) has
its own theme (light/dark/system, docs/ui-v2.md section 6) and ignores
them. Everything else \u2014 the numerics, the plot axes, the storage size \u2014
still applies.</p>
`,headings:[]},{id:"15-generated-c-files",title:"C Files",html:`<p>For some types of problems that have lengthy and complicated right-hand sides, you would like to use directly compiled C code rather than using the built in interpreter. I have found that one generally does not get as much speed up as merits the extra work of recompilation for each problem. Nevertheless, I have included an option for doing that in XPP. If you want to do this for a particular ODE file, simply append the call to XPP with the option <code>-m</code> which means to make a C file. XPP goes on as before but when you exit the program, there will be two files called <code>my_cfile.c</code> and <code> my_hfile.h.</code> These files should help you in your creation of your own file. Basically, you should replace the call to <code>my_rhs()</code> in <code>my_rhs.c</code> with the contents of <code>my_cfile.c.</code> At this time, this is incompatible with functional equations, so dont use it if you are solving them. Nor is it compatible with tabulated stuff.</p>
<p>For example, the Morris-Lecar model</p>
<pre><code>2
variables v,w
fixed m
params v1=-.01,v2=0.15,v3=0.1,v4=0.145,gna=1.33,phi=.333
params vk=-.7,vl=-.5,iapp=.075,gk=2.0,gl=.5,om=1
user minf 1 .5*(1+tanh((arg1-v1)/v2))
user ninf 1 .5*(1+tanh((arg1-v3)/v4))
user lamn 1 phi*cosh((arg1-v3)/(2*v4))
odev  (iapp+gl*(vl-v)+gk*w*(vk-v)+gna*m*(1-v))*om
odew (lamn(v)*(ninf(v)-w))*om
odem minf(v)
b v-v&#39;
b w-w&#39;
done
</code></pre>
<p>produces the header file:</p>
<pre><code>#define v y__y[0]
#define w y__y[1]
double m;
#define v1 constants[1]
#define v2 constants[2]
#define v3 constants[3]
#define v4 constants[4]
#define gna constants[5]
#define phi constants[6]
#define vk constants[7]
#define vl constants[8]
#define iapp constants[9]
#define gk constants[10]
#define gl constants[11]
#define om constants[12]
double minf();
double ninf();
double lamn();
</code></pre>
<p>and the C file</p>
<pre><code>#include &quot;my_hfile.h&quot;
extern double constants[];
double minf(arg1)
double arg1;
{ 
 return(.5*(1+tanh((arg1-v1)/v2))
);
}

double ninf(arg1)
double arg1;
{ 
 return(.5*(1+tanh((arg1-v3)/v4))
);
}

double lamn(arg1)
double arg1;
{ 
 return(phi*cosh((arg1-v3)/(2*v4))
);
}

my_rhs(t,y__y,ydot,neq)
 double t,*y__y,*ydot;
int neq;
 { 

 do_fix();
ydot[0]= (iapp+gl*(vl-v)+gk*w*(vk-v)+gna*m*(1-v))*om;
ydot[1]=(lamn(v)*(ninf(v)-w))*om;

}
do_fix()
{
m=minf(v);

}
</code></pre>
<p>Copy the file, <code>my_rhs.c</code> to some backup. Delete the function called <code>my_rhs(...)</code> and substitute the contents of <code>my_cfile.c</code> putting the top declarations of <code> my_cfile.c</code> at the beginning of <code>my_rhs.c</code>. Then, remake XPP. Invoke it as before by typing <code>xpp</code> followed by the file name.</p>
<h2 id="warnings">Warnings</h2>
<p>This tool is only a guide to the production of C code. There are several things to watch out for:</p>
<ol>
<li><p>C is case sensitive and XPP is not so make sure you are consistent.</p>
</li>
<li><p>If your parameters are variables are the same as any of the local variables or key words in the C file <code>my_rhs.c</code> then there will be compilation errors. For example, don\u2019t use <code>i.</code> When in doubt, capitalize all your parameters and variables in the ODE file keeping the system functions such as <code>exp</code> in lower case.</p>
</li>
<li><p>Some XPP functions like <code>heav</code> and <code>max</code> are not known to C so you must define them. I will probably fix this later.</p>
</li>
<li><p>The expression <code>y**x</code> or <code>yx\u0302</code> makes no sense in C and should be replaced by <code>pow(y,x)</code>.</p>
</li>
</ol>
<h2 id="improved-ode-parser">Improved ODE parser</h2>
<p>The latest version of XPP/XPPAUT (1.6 and above) uses a somewhat different syntax for ODE files. No longer do you have to worry about the order in which things are described, nor do you have to worry about some names being defined before others. The right-hand sides, initial data, functions, etc are now much easier to input (in the sense that they are written almost as you would in a paper.) Many of the same commands used in the old style format remain unchanged, but the handling of fixed, auxiliary, and right-hand sides is much more intuitive. Finally, standard line continuation is allowed with the usual UNIX character, $<code>\\backslash</code>$. As this new stuff has not been tested thoroughly, let me know if you encounter problems. Also, the old style format still works so when in doubt use it.</p>
<p>The newstyle format for the parser has the following format:</p>
<p>The big difference is that the name of the variable, auxiliary quantity, fixed quantity, and Markov variable are kept with their right-hand sides. Thus there is no <code>o,i,r</code> stuff necessary anymore and the <code>fixed,variable,kernel</code> declarations are gone. Auxiliary variables always have user defined names and functions are put in as you would write them. (No longer is it necessary to use <code>arg1,arg2,</code> etc as the names of the arguments.) For variables that will satisfy differential equations, maps, or integral equations, you write the equation in the most obvious fashion. The order of statements is unimportant (except for fixed variables which are evaluated in the order they are defined). Finally the standard UNIX line continuation character $<code>\\backslash</code>$ is recognized so you can put statements on multiple lines. The exception to this is the Markov transition matrix. Each row of the matrix must be put on the same line.</p>
`,headings:[{id:"warnings",text:"Warnings",level:2},{id:"improved-ode-parser",text:"Improved ODE parser",level:2}]},{id:"16-quick-reference",title:"Quick reference",html:`<blockquote>
<p>XPP Commands<br>Bard Ermentrout \u2014 Dec 2012</p>
</blockquote>
<h2 id="ode-file-format">ODE File Format</h2>
<pre><code># comment line - name of file, etc
#include filename   
d&lt;name&gt;/dt=&lt;formula&gt;
&lt;name&gt;&#39;=&lt;formula&gt;
&lt;name&gt;(t)=&lt;formula&gt;
volt &lt;name&gt;=&lt;formula&gt;
&lt;name&gt;(t+1)=&lt;formula&gt;
x[n1..n2]&#39; = ...[j] [j-1] ... &lt;--  Arrays
%[i1..i2]
u[j]&#39;=...
v[j]&#39;=...
% 
markov &lt;name&gt; &lt;nstates&gt; &lt;init&gt;
  {t01} {t02} ... {t0k-1}
  {t10} ...
  ...
  {tk-1,0} ... {tk-1 k-1}

aux &lt;name&gt;=&lt;formula&gt;
!&lt;name&gt;=&lt;formula&gt; &lt;--  parameters defined as formulae
&lt;name&gt;=&lt;formula&gt;
parameter &lt;name1&gt;=&lt;value1&gt;,&lt;name2&gt;=&lt;value2&gt;, ...
wiener &lt;name1&gt;, &lt;name2&gt;, ...
number &lt;name1&gt;=&lt;value1&gt;,&lt;name2&gt;=&lt;value2&gt;, ...
&lt;name&gt;(&lt;x1&gt;,&lt;x2&gt;,...,&lt;xn&gt;)=&lt;formula&gt;
table &lt;name&gt; &lt;filename&gt;
table &lt;name&gt; % &lt;npts&gt; &lt;xlo&gt; &lt;xhi&gt; &lt;function(t)&gt;
global sign {condition} {name1=form1;...}
init &lt;name&gt;=&lt;value&gt;,...
&lt;name&gt;(0)=&lt;value&gt; or &lt;expr&gt; &lt;--  delay initial conditions
bdry &lt;expression&gt;
0= &lt;expression&gt;    &lt;---  For DAEs
solv &lt;name&gt;=&lt;expression&gt; &lt;------ For DAEs
special &lt;name&gt;=conv(type,npts,ncon,wgt,rootname)
           fconv(type,npts,ncon,wgt,rootname,root2,function)
           sparse(npts,ncon,wgt,index,rootname)
           fsparse(npts,ncon,wgt,index,rootname,root2,function)
               fftcon(type,npts,wgt,rootname)
               mmult(ncol,nrow,matrix,rootname)
               fmmult(ncol,nrow,matrix,root1,root2,f)
               findext(type,n,skip,root)
               gill(meth,rxn_list)
               delmmult(n,m,w,tau,root)
               delsparse(m,nc,w,index,tau,root)
          
export {x1,x2,....} {x1p,x2p,..}     
# comments
@ &lt;name&gt;=&lt;value&gt;, ...
set &lt;name&gt; {x1=z1,x2=z2,...}
only &lt;name1&gt;,&lt;name2&gt;,...
options &lt;filename&gt;
&quot; {z=3,b=3,...} Some nice text   &lt;---  Active comments 
done
</code></pre>
<p><strong>Remarks</strong></p>
<ul>
<li>ODEs are put in as <code>name&#39;=expression</code> or <code>dname/dt=expression</code>. Note you can use this notation for maps as well or use <code>name(t+1)=expression</code>.</li>
<li><code>name=expression</code> allows you to use <code>name</code> in many locations and to build up more complex expressions. The order in which they are written is the order in which they are evaluated, so don&#39;t use an expression using a name that hasn&#39;t yet been defined.</li>
<li>Array expressions such as <code>x[1..10]&#39;=...[j]...</code> are expanded as <code>x1&#39;=...1..</code> etc. The <code>[j]</code> is expanded into a number. You can use some minimal arithmetic as well such as <code>[j+1],[j-1],[j*2]</code> etc.</li>
<li><code>x[0..99](0)=sin(2*pi*[j]/100)</code> is a way to initialize an array ODE.</li>
<li><code>markov &lt;name&gt; &lt;n&gt; &lt;init&gt;</code> sets up a continuous Markov process with <code>n</code> states and whose $<code>n\\times n</code>$ transition matrix follows with entries delimited with the braces <code>{, }</code>. The diagonal entries should just be 0 as they are ignored. The starting state is given by <code>&lt;init&gt;</code>.</li>
<li><code>aux</code> quantities are extra stuff you might want to plot.</li>
<li><code>parameters</code> are named quantities that you can change within the program.</li>
<li><code>numbers</code> are named quantities that are invisible to the user and cannot be changed.</li>
<li><code>!name=...</code> defines a named quantity whose value depends on other numbers and parameters, but not variables, as these named quantities are only computed when you change parameters.</li>
<li><code>wiener</code> defines a set of Wiener processes.</li>
<li><code>table</code> reads in a function in the form of a table or you can define it within the ODE file; the file version of a table starts with three numbers, the number of values, the low, and the high, followed by the y-values. The x values are equally spaced from low to high. Tables are treated as functions of one variable with a linear interpolation as the default. If you want a cubic spline then the initial number in the file should have an <code>s</code> in front of it and if you want piecewise constant then put an <code>i</code> in front of the number. The function form of the table uses <code>%</code> followed by three numbers as with the file format and then a formula using the independent variable <code>t</code>. So <code>table s % 51 -25 25 exp(-abs(t))/2</code> will produce a tabular function such that <code>s(5)</code> would return $<code>\\exp(-5)/2.</code>$ Other values are interpolated. Tables are very useful for networks.</li>
<li>While rather clumsy in notation, you can initialize a delay equation via <code>x(0)=f(t)</code> to set the values of $<code>x</code>$ for $<code>-\\tau\\le t&lt;0.</code>$ Delay initial data is zero by default.</li>
<li>The <code>global</code> declaration includes two special quantities. Inside the braces, if you type <code>arret=value</code>, the integration will stop if the <code>value</code> is not zero. This way you can stop an integration if a particular event happens. The other declaration is <code>out_put=value</code> which will override the <code>transient</code> (see the numerics menu) and allow you to plot (when <code>value</code> is nonzero), but only when the events occur.</li>
<li>In the <code>global</code> declaration the <code>sign</code> is <code>{0,1,-1}</code>. 0 means the condition must be met exactly at the point it was checked. It is a good way to set initial conditions as expressions, e.g. <code>global 0 t {x=sin(1)}</code>.</li>
<li><code>bdry &lt;expression&gt;</code> is a way to set boundary conditions for the boundary value solver. The BVP solver tries to zero the expression. Use the names of your variables for the left end conditions and primed versions for the right ends (see <code>gberg.ode</code>).</li>
<li>The pair <code>0= &lt;expression&gt;</code> and <code>solve &lt;name&gt;=&lt;expression&gt;</code> set up differential algebraic equations. The lines starting with <code>0=</code> will solve for the variables in the <code>solv</code> lines to make the expressions zero. See <code>huygens.ode</code> for an example where three accelerations are solved for to get the dynamics of two pendulums on a cart.</li>
<li><code>export</code> is used to communicate with a dynamically loaded library compiled from some C code. See <code>tstdll.ode</code>.</li>
<li><code>only</code> is useful for silent mode (no GUI) as this puts out a bunch of data in a file. <code>only</code> restricts the output to a specified set of variables.</li>
<li><code>set</code> creates a bunch of settings for numerics, variables, parameters that you can call by name within XPP.</li>
<li>Active comments allow you to create a little tutorial where you take care of changing parameters, numerics, etc. You invoke this in XPP with the File Printsrc command. See <code>lecar.ode</code> for an example.</li>
</ul>
<p><strong>Order of evaluation.</strong></p>
<ol>
<li>Fixed variables</li>
<li><code>specials</code></li>
<li>DAEs</li>
<li>External C code</li>
<li>RHSs</li>
</ol>
<p>The general integral equation</p>
<pre><code class="language-math">u(t)=f(t)+\\int_0^t K(t,s,u(s))ds
</code></pre>
<p>becomes</p>
<pre><code>u = f(t) + int{K(t,t&#39;,u)}
</code></pre>
<p>The convolution equation:</p>
<pre><code class="language-math">v(t) = \\exp(-t) + \\int_0^t e^{-(t-s)^2}v(s) ds
</code></pre>
<p>would be written as:</p>
<pre><code>v(t) = exp(-t) + int{exp(-t^2)#v}
</code></pre>
<p>If one wants to solve, say,</p>
<pre><code class="language-math">u(t) = exp(-t) + \\int^t_0 (t-t&#39;)^{-mu} K(t,t&#39;,u(t&#39;))dt&#39;
</code></pre>
<p>the form is:</p>
<pre><code>u(t)= exp(-t) + int[mu]{K(t,t&#39;,u}
</code></pre>
<p>and for convolutions, use the form:</p>
<pre><code>u(t)= exp(-t) + int[mu]{w(t)#u}
</code></pre>
<p><strong>NETWORKS</strong></p>
<pre><code>special zip=conv(type,npts,ncon,wgt,root)
</code></pre>
<p>where <code>root</code> is the name of a variable and <code>wgt</code> is a table, produces an array <code>zip</code> with <code>npts</code>:</p>
<pre><code class="language-math">\\hbox{zip}[i] =\\sum_{j=-\\hbox{ncon}}^{\\hbox{ncon}}\\hbox{wgt}[j+ncon]
\\hbox{root}[i+j]
</code></pre>
<pre><code>special bob=fftconv(type,npts,wgt,root)
</code></pre>
<p>is similar to the <code>conv</code> operation, but uses the FFT to do it. The <code>type</code> should be <code>odd</code> or <code>periodic</code>. The size of the <code>wgt</code> table should be either <code>npts</code> or <code>2 npts</code>. The <code>sparse</code> network has the syntax:</p>
<pre><code>special zip=sparse(npts,ncon,wgt,index,root)
</code></pre>
<p>where <code>wgt</code> and <code>index</code> are tables with at least <code>npts * ncon</code> entries. The array <code>index</code> returns the indices of the offsets to with which to connect and the array <code>wgt</code> is the coupling strength. The return is</p>
<pre><code>zip[i] = sum(j=0;j&lt;ncon) w[i*ncon+j]*root[k]
k = index[i*ncon+j] 
</code></pre>
<p>The other two types of networks allow more complicated interactions:</p>
<pre><code>special zip=fconv(type,npts,ncon,wgt,root1,root2,f)
</code></pre>
<p>evaluates as</p>
<pre><code>zip[i]=sum(j=-ncon;j=ncon) wgt[ncon+j]*f(root1[i+j],root2[i])
</code></pre>
<p>and</p>
<pre><code>special zip=fsparse(npts,ncon,wgt,index,root1,root2,f)
</code></pre>
<p>evaluates as</p>
<pre><code>zip[i]=sum(j=0;j&lt;ncon) wgt[ncon*i+j]*f(root1[k],root2[i])
k = index[i*ncon+j] 
</code></pre>
<p>Matrix multiplication is also possible:</p>
<pre><code>special k=mmult(n,m,w,u)
</code></pre>
<p>returns a vector <code>k</code> of length <code>m </code> defined as</p>
<pre><code>k(j)=sum(i=0;i&lt;n)w(i+nj)u(i)
</code></pre>
<p>while</p>
<pre><code>special k=fmmult(n,m,w,u,v,f)
</code></pre>
<p>returns</p>
<pre><code>k(j)=sum(i=0;i&lt;n)w(i+nj)f(u(i),v(j))

special z=findext(type,n,skip,root)
</code></pre>
<p>finds the extreme values of a list of <code>n</code> variables starting at <code>root</code> and skipping every <code>skip</code> one. <code>type=1,-1,0</code> according as to whether you want the max, min, or both. In any case, <code>z(0), z(2)</code> are the max and min and <code>z(1),z(3)</code> are the index of the max and min. See <code>kohonen.ode</code> for an example of this.</p>
<pre><code>special k=delmmult(n,m,w,tau,u0)
</code></pre>
<p>returns the $<code>m</code>$ values</p>
<pre><code class="language-math">k(i)=\\sum_{j=0}^{m-1} \\mbox{w}[i m + j] u[j](t-\\mbox{tau}[i m + j])
</code></pre>
<pre><code>special k=delsparse(m,n,w,l,tau,root)
</code></pre>
<p>returns the $<code>m</code>$ values</p>
<pre><code class="language-math">k(i)=\\sum_{j=0}^{n-1} \\mbox{w}[i n  + j] u[l(i n +j)](t-\\mbox{tau}[i m + j])
</code></pre>
<pre><code>special ydot=import(soname,sofun,nret,root,w1,w2,...wm)
</code></pre>
<p>This is a conveneient way to load a large number of right-hand sides that have been coded in C. See the examples in the cuda folder.</p>
<h2 id="the-options-list">The options list</h2>
<p>The format for changing the options, either as <code>@</code> lines in an ODE file
or in an <a href="14-options-file.md">options file</a>, is:</p>
<pre><code>@ name1=value1, name2=value2, ...
</code></pre>
<p>where <code>name</code> is one of the following and <code>value</code> is either an integer, floating point, or string. (All names can be upper or lower case).</p>
<ul>
<li><p>QUIET=<code>0,1</code> dont print out stuff</p>
</li>
<li><p>LOGFILE=<code>filename</code> store prontouts in logfile</p>
</li>
<li><p>MAXSTOR=<code>integer</code> sets the total number of time steps that will be kept in memory. The default is 5000. If you want to perform very long integrations change this to some large number.</p>
</li>
<li><p>FORECOLOR=<code>rrggbb</code> sets the hexidecimal frame color, and text menu color.</p>
</li>
<li><p>BACKCOLOR=<code>rrggbb</code> sets the background color on the menus, sliders, dialogs, and buttons</p>
</li>
<li><p>MWCOLOR=<code>rrggbb</code> sets the background color for the main frame in the popups.</p>
</li>
<li><p>DWCOLOR=<code>rrggbb</code> sets the color of the drawing window and the browser numerical displays</p>
</li>
<li><p>BACKIMAGE=<code>file.xbm</code> sets the back image. The format is the not generally readily found X11-bitmap format. On the mac, you can use some unix tools like gimp, xv, or ImageMagick.</p>
</li>
<li><p>SMALLFONT=<code>fontname</code> where <code>fontname</code> is some font available to your X-server. This sets the \u201Csmall\u201D font which is used in the Data Browser and in some other windows.</p>
</li>
<li><p>BIGFONT=<code>fontname</code> sets the font for all the menus and popups.</p>
</li>
<li><p>SMC={0,...,10} sets the stable manifold color. These colors correspond to the colors available when plotting in XPP and are roughly, <code>\u201Cblack\u201D,red,redorange,orange,yelloworange,yello,yellowgreen,green,bluegreen,blue,purple</code> Here \u201Cblack\u201D means the FORECOLOR.</p>
</li>
<li><p>UMC={0,...,10} sets the unstable manifold color</p>
</li>
<li><p>XNC={0,...,10} sets the X-nullcline color</p>
</li>
<li><p>YNC={0,...,10} sets the Y-nullcline color</p>
</li>
<li><p>SEC,UEC,SPC,UPC=color set the colors for the screen on AUTO for stable eq, unstable eq, stable per, unstable per.</p>
</li>
<li><p>OUTPUT=filename sets the filename to which you want to write for \u201Csilent\u201D integration. The default is \u201Coutput.dat\u201D.</p>
</li>
<li><p>GRADS={0,1} turn off or on gradients in the XPP buttons &lt;/LI&gt;</p>
</li>
<li><p>HEIGHT=pixels,WIDTH=pixels, sets the height and width of the main window intially</p>
</li>
<li><p>RUNNOW=1 run the simulation as soon as the windows are up (Don\u2019t wait for Init conds Go, e.g.</p>
</li>
<li><p>BUT=name:kbs defines button on the top of the main window. You can define up to 20 such buttons. They will appear across the top when you press them, they will execute an XPP keyboard shortcut. For example, <strong>BUT=Fit:wf</strong> will create a button labeled <strong>Fit</strong> and when you press it, it will be as if you had clicked <strong>Window/zoom Fit</strong>. Most of the menu items are available.</p>
</li>
</ul>
<p>The remaining options can be set from within the program. They are</p>
<ul>
<li><p>LT=<code>int</code> sets the linetype. It should be less than 2 and greater than -6.</p>
</li>
<li><p>SEED=<code>int</code> sets the random number generator seed.</p>
</li>
<li><p>XP=name sets the name of the variable to plot on the x-axis. The default is <code>T</code>, the time-variable.</p>
</li>
<li><p>YP=name sets the name of the variable on the y-axis.</p>
</li>
<li><p>ZP=name sets the name of the variable on the z-axis (if the plot is 3D.)</p>
</li>
<li><p>COLORMAP=<code>0,1,2,3,4,5</code> sets the colormap</p>
</li>
<li><p>NPLOT=<code>int</code> tells XPP how many plots will be in the opening screen.</p>
</li>
<li><p>XP2=name,YP2=name,ZP2=name tells XPP the variables on the axes of the second curve; XP8 etc are for the 8th plot. Up to 8 total plots can be specified on opening. They will be given different colors.</p>
</li>
<li><p>MULTIWIN=<code>0,1</code>, puts multiple windows up and one each of the NPLOT curves in them.</p>
</li>
<li><p>SIMPLOT=<code>0,1</code> turns on the simultaneous plot flag for multiple windows</p>
</li>
<li><p>XHI2=value,YHI2=value,XLO2=value,YLO2=values dimensions the extra windows up to 8.</p>
</li>
<li><p>AXES=<code>{2,3}</code> determine whether a 2D or 3D plot will be displayed.</p>
</li>
<li><p>COLORIZE=1,COLORVIA=<code>name</code>, COLORLO=<code>value</code>, COLORHI=<code>value</code> all set the colorization of trajectories and in the <code>Numerics colorcode</code> command. If <code>name</code> is <code>speed</code>, then colorcoding will be via velocity.</p>
</li>
<li><p>TOTAL=value sets the total amount of time to integrate the equations (default is 20).</p>
</li>
<li><p>DT=value sets the time step for the integrator (default is 0.05).</p>
</li>
<li><p>NJMP=<code>integer</code> tells XPP how frequently to output the solution to the ODE. The default is 1, which means at each integration step.</p>
</li>
<li><p>T0=value sets the starting time (default is 0).</p>
</li>
<li><p>TRANS=value tells XPP to integrate until <code>T=TRANS</code> and then start plotting solutions (default is 0.)</p>
</li>
<li><p>NMESH=<code>integer</code> sets the mesh size for computing nullclines (default is 40).</p>
</li>
<li><p>DFGRID=<code>integer</code> sets the grid size for direction fields, flows etc (default is 10);</p>
</li>
<li><p>METH=<code>{ discrete,euler,modeuler,rungekutta,adams,gear,volterra, backeul, qualrk,stiff,cvode,5dp,83dp,2rb,ymp}</code> sets the integration method (default is Runge-Kutta.)</p>
</li>
<li><p>DTMIN=value sets the minimum allowable timestep for the Gear integrator.</p>
</li>
<li><p>DTMAX=value sets the maximum allowable timestep for the Gear integrator</p>
</li>
<li><p>VMAXPTS=value sets the number of points maintained in for the Volterra integral solver. The default is 4000.</p>
</li>
<li><p>{ JAC_EPS=value, NEWT_TOL=value, NEWT_ITER=value} set parameters for the root finders.</p>
</li>
<li><p>ATOLER=value sets the absolute tolerance for CVODE.</p>
</li>
<li><p>TOLER=value sets the error tolerance for the Gear, adaptive RK, and stiff integrators. It is the relative tolerance for CVODE.</p>
</li>
<li><p>BOUND=value sets the maximum bound any plotted variable can reach in magnitude. If any plottable quantity exceeds this, the integrator will halt with a warning. The program will not stop however (default is 100.)</p>
</li>
<li><p>DELAY=value sets the maximum delay allowed in the integration (default is 0.)</p>
</li>
<li><p>BANDUP=<code>int</code>, BANDLO=<code>int</code> bandwidths for the Jacobian computation.</p>
</li>
<li><p>PHI=value,THETA=value set the angles for the three-dimensional plots.</p>
</li>
<li><p>XLO=value,YLO=value,XHI=value,YHI=value set the limits for two-dimensional plots (defaults are 0,-2,20,2 respectively.) Note that for three-dimensional plots, the plot is scaled to a cube with vertices that are $<code>\\pm1</code>$ and this cube is rotated and projected onto the plane so setting these to $<code>\\pm2</code>$ works well for 3D plots.</p>
</li>
<li><p>XMAX=value, XMIN=value, YMAX=value, YMIN=value, ZMAX=value, ZMIN=value set the scaling for three-d plots.</p>
</li>
<li><p>POIMAP=<code>{ section,maxmin, period} </code> sets up a Poincare map for either sections of a variable, the extrema, or period.</p>
</li>
<li><p>POIVAR=name sets the variable name whose section you are interested in finding.</p>
</li>
<li><p>POIPLN=value is the value of the section; it is a floating point.</p>
</li>
<li><p>POISGN=<code>{ 1, -1, 0 }</code> determines the direction of the section.</p>
</li>
<li><p>POISTOP=1 means to stop the integration when the section is reached.</p>
</li>
<li><p>RANGE=1 means that you want to run a range integration (in batch mode).</p>
</li>
<li><p>RANGEOVER=name, RANGESTEP=number, RANGELOW=number, RANGEHIGH=number, RANGERESET=<code>Yes,No</code>, RANGEOLDIC=<code>Yes,No</code> all correspond to the entries in the range integration option.</p>
</li>
<li><p>TOR_PER=value, defined the period for a toroidal phasespace and tellx XPP that there will be some variables on the circle.</p>
</li>
<li><p>FOLD=name, tells XPP that the variable &lt;name&gt; is to be considered modulo the period. You can repeat this for many variables.</p>
</li>
<li><p>PS_Font=<code>fontname</code>,PS_LW=<code>linewidth</code>,PS_FSIZE=<code>fontsize</code>,PS_COLOR=<code>0,1</code> sets up postscript options.</p>
</li>
<li><p>S1=<code>name</code>, SLO1=<code>number</code>,SHI1=<code>number</code> sets the variables, parameters associated with a slider and their low and high values. Use S2,S3 for the other sliders (This is different for the iPad/iPhone.)</p>
</li>
<li><p>STOCH=<code>1,2</code> set the stochastic flag. Set up a range and use this to compute the mean etc of an ensemble of trajectories for batch mode integration. 1 returns the mean and 2 the variance.</p>
</li>
<li><p>POSTPROCESS=<code>1,2,3,4,5,6</code> is for batch integration and allows you to process your data as follows: 1-histogram, 2-Fourier,3-Power,4-Power spectral density, 5-cross spectrum, 6-coherence.</p>
</li>
<li><p>HISTHI=<code>number</code>,HISTLO=<code>number</code>,HISTBINS=<code>number</code>, HISTCOL=<code>variable name</code>, sets up the relevant quantitues for a batch histogram; used in conjunction with POSTPROCESS.</p>
</li>
<li><p>SPECCOL=<code>name</code>, SPECCOL2=<code>name</code>, SPECWIDTH=<code>number</code>, SPECWIN=<code>0,1,2,3,4</code> (corresponding to square,parabolic,hamming,bartlett, or hanning windows), sets up relevant spectral stuff for use in conjunction with postprocessing.</p>
</li>
<li><p>AUTO-stuff. The following AUTO-specific variables can also be set: <code>NTST, NMAX, NPR, DSMIN, DSMAX, DS, PARMIN, PARMAX, NORMMIN, NORMMAX, AUTOXMIN, AUTOXMAX, AUTOYMIN, AUTOYMAX, AUTOVAR</code>. The last is the variable to plot on the y-axis. The x-axis variable is always the first parameter in the ODE file unless you change it within AUTO.</p>
</li>
<li><p>DLL_LIB=<code>file</code> Dynamically linked library</p>
</li>
<li><p>DLL_FUN=<code>name</code> Dynamically linked function.</p>
</li>
<li><p>DFDRAW =<code>1, 2, or 3</code> will force the drawing of direction fields on batch plots; 1 is unscaled, 2 is scaled, and 3 is colorized.</p>
</li>
<li><p>NCDRAW=<code>1</code> forces the drawing of nullclines in batch plots</p>
</li>
</ul>
<p>0-Foreground color; 1-Red; 2-Red Orange; 3-Orange; 4-Yellow Orange; 5-Yellow; 6-Yellow Green; 7-Green; 8-Blue Green; 9-Blue; 10-Purple.</p>
<p>You should be aware of the following keywords that should not be used in your ODE files for anything other than their meaning here.</p>
<pre><code>sin cos tan atan atan2 sinh cosh tanh
exp delay ln log log10 t pi if then else
asin acos heav sign mod flr ran abs del\\_shft 
max min normal besselj bessely besseli erf erfc poisson
lgamma arg1 ... arg9  @ $ + - / * ^ ** shift
| &gt; &lt; == &gt;= &lt;= != not \\# int sum of i&#39;
</code></pre>
<p>These are mainly self-explanatory. The nonobvious ones are:</p>
<ul>
<li><p><strong><code>heav(arg1)</code></strong>: the step function, zero if <code>arg1&lt;0</code> and 1 otherwise.</p>
</li>
<li><p><strong><code>sign(arg)</code></strong>: which is the sign of the argument (zero has sign 0)</p>
</li>
<li><p><strong><code>ran(arg)</code></strong>: produces a uniformly distributed random number between 0 and <code>arg.</code></p>
</li>
<li><p><strong><code>besselj, bessely, besseli </code></strong>: take two arguments, $<code>n,x</code>$ and return $<code>J_n(x)</code>$ $<code>Y_n(x),I_n(x)</code>$ the Bessel functions.</p>
</li>
<li><p><strong><code>erf(x), erfc(x)</code></strong>: are the error function and the complementary function.</p>
</li>
<li><p><strong><code>lgamma(x)</code></strong>: is the log of the gamma function.</p>
</li>
<li><p><strong><code>normal(arg1,arg2)</code></strong>: produces a normally distributed random number with mean <code>arg1</code> and variance <code>arg2</code>.</p>
</li>
<li><p><strong><code>poisson(arg)</code></strong>: produces the number of events from a Poisson process with parameter <code>arg</code>. It is a random number.</p>
</li>
<li><p><strong><code>max(arg1,arg2)</code></strong>: produces the maximum of the two arguments and <code>min</code> is the minimum of them.</p>
</li>
<li><p><strong><code>if(&lt;exp1&gt;)then(&lt;exp2&gt;)else(&lt;exp3&gt;)</code></strong>: evaluates <code> </code> If it is nonzero it evaluates to otherwise it is . E.g. <code>if(x&gt;1)then(ln(x))else(x-1)</code> will lead to <code>ln(2)</code> if <code>x=2</code> and <code>-1 if x=0.</code></p>
</li>
<li><p><strong><code>delay(&lt;var&gt;,&lt;exp&gt;)</code></strong>: returns variable <code>&lt;var&gt;</code> delayed by the result of evaluating <code>&lt;exp&gt;</code>. In order to use the delay you must inform the program of the maximal possible delay so it can allocate storage.</p>
</li>
<li><p><strong><code>del_shft(&lt;var&gt;,&lt;shft&gt;,&lt;delay&gt;).</code></strong>: This operator combines the <code>delay</code> and the <code>shift</code> operators and returns the value of the variable <code>&lt;var&gt;</code> shifted by <code>&lt;shft&gt;</code> at the delayed time given by <code>&lt;delay&gt;</code>. (See <code>sine-circle.ode</code> for an example.)</p>
</li>
<li><p><strong><code>mod(arg1,arg2)</code></strong>: is <code>arg1</code> modulo <code>arg2</code></p>
</li>
<li><p><strong><code>flr(arg)</code></strong>: is the integer part of<code>&lt;arg&gt;</code> returning the largest integer less than <code>&lt;arg&gt;</code>.</p>
</li>
<li><p><strong><code>t </code></strong>: is the current time in the integration of the differential equation.</p>
</li>
<li><p><strong><code>pi</code></strong>: is $<code>\\pi.</code>$</p>
</li>
<li><p><strong><code>arg1, ..., arg9</code></strong>: are the formal arguments for functions. You never will actually see them, but they are used internally in the parser.</p>
</li>
<li><p><strong><code>int, #</code></strong>: concern Volterra equations.</p>
</li>
<li><p><strong><code>shift(&lt;var&gt;,&lt;exp&gt;)</code></strong>: This operator evaluates the expression <code>&lt;exp&gt;</code> converts it to an integer and then uses this to indirectly address a variable whose address is that of <code>&lt;var&gt;</code> plus the integer value of the expression. This is a way to imitate arrays in XPP. For example if you defined the sequence of 5 variables, <code> u0,u1,u2,u3,u4</code> one right after another, then <code>shift(u0,2)</code> would return the value of <code>u2.</code></p>
</li>
<li><p><strong><code>set(&lt;var&gt;,int,value)</code></strong>: is a weird little function that will assign the variable <code>&lt;var&gt; </code> shifted by <code>int </code> the value <code>value</code>. It would be used for example to reset some random variable in a <code>global</code> statement. Since <em>all</em> XPP functions return values, this needs an assignment and will retun <code>value</code>. For example:</p>
<pre><code>global 1 t-t0 {u=set(x0,50*ran(1),3.14159}
</code></pre>
<p>will pick a random index <code>int</code> and assign <code>shift(x0,int)</code> the value 3.14159. Weird, but I needed it for a problem, so there it is.</p>
</li>
<li><p><strong><code>sum(&lt;ex1&gt;,&lt;ex2&gt;)of(&lt;ex3&gt;)</code></strong>: is a way of summing up things. The expressions <code>,&lt;ex1&gt;</code> are evaluated and their integer parts are used as the lower and upper limits of the sum. The index of the sum is <code>i\u2019</code> so that you cannot have double sums since there is only one index. is the expression to be summed and will generally involve <code>i\u2019.</code> For example <code>sum(1,10)of(i\u2019)</code> will be evaluated to 55. Another example combines the sum with the shift operator. <code>sum(0,4)of(shift(u0,i\u2019))</code> will sum up <code>u0</code> and the next four variables that were defined after it.</p>
</li>
</ul>
<p>Add a <code>.xpprc</code> file to set your favorite options, e.g</p>
<pre><code>@ bell=0,grads=0,dwcolor=eeddff
@ bigfont=lucidasanstypewriter-bold-14
</code></pre>
<h2 id="command-line-arguments">Command line arguments</h2>
<p>xppautX adds its own front-end flags ahead of xpp&#39;s original ones (see
<a href="04-using-the-interface.md#starting-xppautx">Using the interface</a>):</p>
<pre><code>xppautX [--server|--web|--script FILE] [--port N] [--no-open]
        [--verbose|--debug] [--version] file.ode [xpp options]
</code></pre>
<p>xpp&#39;s own options still apply after the file name (or anywhere, for the
ones below that predate this ordering). Many of them provide an API:
other programs or scripts can interact with <code>xppautX -silent</code> in a batch
mode, useful for processing many files or runs.</p>
<p><strong>Appearance flags with no effect any more.</strong> These sized, coloured or
fonted the X11 windows; web2 has its own light/dark/system theme
(docs/ui-v2.md section 6) and a responsive layout instead, so xppautX
accepts and ignores them: <code>-xorfix</code>, <code>-allwin</code>, <code>-white</code>, <code>-bigfont *font*</code>, <code>-smallfont *font*</code>, <code>-forecolor *color*</code>, <code>-backcolor *color*</code>,
<code>-mwcolor *color*</code>, <code>-dwcolor *color*</code>, <code>-backimage *filename*</code>, <code>-grads *B*</code>, <code>-width *N*</code>, <code>-height *N*</code>, <code>-bell *B*</code>, <code>-ee</code>.</p>
<p><strong>Flags that still work</strong>, because they are about the model, the batch
run or the files, not the X11 windows:</p>
<ul>
<li><p><strong>-silent</strong>: Runs XPP&#39;s integrators without opening the front end. The result of the integration is saved to a file called <code>output.dat</code> (see <code>-outfile</code>) but this can be changed. The length of integration, methods, Poincare sections, etc, are all specified in either the <a href="14-options-file.md">options file</a> or in the internal options. When you run a range integration in silent mode, if the parameter <code>RANGERESET</code> is <code>yes</code> (the default) then a new output file will be opened for each integration: ranging over 50 values gives 50 output files named <code>output.dat.0</code>, <code>output.dat.1</code>, etc. If you have set <code>RANGERESET=no</code>, then only one file is produced.</p>
</li>
<li><p><strong>-convert</strong>: Converts the old-style parser format to the new style, writing <code>&lt;file&gt;.new</code>.</p>
</li>
<li><p><strong>-setfile <em>filename</em></strong>: loads the named <code>.set</code> file after loading up the ODE file.</p>
</li>
<li><p><strong>-newseed</strong>: uses the machine time to re-seed the random number generator.</p>
</li>
<li><p><strong>-runnow</strong>: runs the ODE file immediately on startup (implied by <code>-silent</code>).</p>
</li>
<li><p><strong>-parfile <em>filename</em></strong>: loads parameters from the named file. The first line gives the number of parameters, then one value per line followed by its name (must match a parameter name in the ODE file), e.g. (<code>lecar.par</code>):</p>
<pre><code>12 Number params
0.0    iapp
.333   phi
-.01   v1
0.15   v2
0.1    v3
0.145  v4
1.33   gca
-.7    vk
-.5    vl
2.0    gk
.5     gl
1      om
</code></pre>
</li>
<li><p><strong>-outfile <em>filename</em></strong>: sends output to this file (default <code>output.dat</code>). The first column is time, the rest are the ordered variables, e.g. (<code>lecar.out</code>, variables <code>V</code>, <code>W</code>):</p>
<table>
<thead>
<tr>
<th>t</th>
<th>V</th>
<th>W</th>
</tr>
</thead>
<tbody><tr>
<td>0</td>
<td>-0.36059999</td>
<td>0.0911</td>
</tr>
<tr>
<td>0.050000001</td>
<td>-0.36620989</td>
<td>0.087350026</td>
</tr>
<tr>
<td>0.1</td>
<td>-0.3715646</td>
<td>0.083690271</td>
</tr>
<tr>
<td>\u22EE</td>
<td>\u22EE</td>
<td>\u22EE</td>
</tr>
</tbody></table>
</li>
<li><p><strong>-icfile <em>filename</em></strong>: loads initial conditions from the named file, one value per line mapped to the variables in file order (variables with a differential equation, Wiener and Markov variables), e.g. (<code>lecar.ic</code>, for <code>V</code>, <code>W</code>): <code>-0.3606</code> then <code>0.0911</code>.</p>
</li>
<li><p><strong>-internset <em>B</em></strong>: run (<code>1</code>) or not (<code>0</code>) internal sets during a batch run.</p>
</li>
<li><p><strong>-uset <em>setname</em></strong>: names an internal set to run during a batch run (repeatable).</p>
</li>
<li><p><strong>-rset <em>setname</em></strong>: names an internal set <em>not</em> to run during a batch run (repeatable).</p>
</li>
<li><p><strong>-include <em>filename</em></strong>: names a file to include along with the selected file (the <code>#include</code> directive, from the command line).</p>
</li>
<li><p><strong>-qsets / -qpars / -qics</strong>: query the names of internal sets, the parameters, or the initial conditions and save the results to <code>-outfile</code>; lets an external program or script inspect an ODE model.</p>
</li>
<li><p><strong>-quiet <em>B</em></strong>: verbose log messages will (<code>B=0</code>) or will not (<code>B=1</code>) be written; xppautX&#39;s own <code>--verbose</code>/<code>--debug</code> raise the level further (core/xpp_log.h).</p>
</li>
<li><p><strong>-logfile <em>filename</em></strong>: the file to which log messages are written (also xppautX&#39;s <code>-logfile</code>).</p>
</li>
<li><p><strong>-anifile <em>filename</em></strong>: loads an animation (<code>.ani</code>) from the named file at start-up; useful for teaching demonstrations.</p>
</li>
<li><p><strong>-mkplot</strong>: with <code>-silent</code>, writes a plot named after the ODE file (or numbered, for several); SVG or PS (<code>-plotfmt</code>).</p>
</li>
<li><p><strong>-noout</strong>: does not write an output data file in batch mode; combine with <code>-mkplot</code> to suppress the data.</p>
</li>
<li><p><strong>-plotfmt <em>ps|svg</em></strong>: sets the plot format for <code>-mkplot</code>.</p>
</li>
<li><p><strong>-version</strong>: prints xpp&#39;s version (<code>xppautX --version</code> prints this fork&#39;s).</p>
</li>
<li><p><strong>-ncdraw <em>k</em></strong>: for a figure with nullclines: <code>k=1</code> draws them on the plot; <code>k=2</code> (with <code>-silent</code>) dumps them to <code>nullclines.dat</code> instead, X-nullcline first then Y (format for gnuplot). For example, to compute the nullclines, dump them to a file and not save the run&#39;s output:</p>
<pre><code>xppautX lecar.ode -noout -silent -ncdraw 2
</code></pre>
<p>or to make a plot:</p>
<pre><code>xppautX lecar.ode -noout -silent -ncdraw 1 -dfdraw 1 -mkplot
</code></pre>
</li>
<li><p><strong>-dfdraw <em>k</em></strong>: draws direction fields the same way: <code>k=1,2,3</code> unscaled, scaled or colorized on the plot; <code>k=4,5</code> dumps the scaled/unscaled coordinates to <code>dirfields.dat</code>.</p>
</li>
<li><p><strong>-readset <em>filename</em></strong>: loads anything an internal set file can hold (options, parameters, initial conditions) from one line (up to 1024 characters), e.g. a file <code>tst.opt</code> containing <code>iapp=0.1;phi=.05;total=500</code>, run as <code>xppautX lecar.ode -readset tst.opt</code> (works with <code>-silent</code> and <code>-mkplot</code> too).</p>
</li>
<li><p><strong>-with <em>string</em></strong>: the same as <code>-readset</code>, with the options inline in a quoted string (no spaces), e.g. <code>xppautX lecar.ode -with &quot;iapp=0.1;phi=0.05;total=1000&quot; -runnow</code>.</p>
</li>
</ul>
<p>The only other thing on the command line should be the file name. Thus,</p>
<pre><code>xppautX test.ode -convert
</code></pre>
<p>will convert <code>test.ode</code> to the new format and run it.</p>
`,headings:[{id:"ode-file-format",text:"ODE File Format",level:2},{id:"the-options-list",text:"The options list",level:2},{id:"command-line-arguments",text:"Command line arguments",level:2}]}];var _v={amp:"&",lt:"<",gt:">",quot:'"',"#39":"'",apos:"'"};function Mv(e){return e.replace(/&(#39|amp|lt|gt|quot|apos);/g,(t,n)=>_v[n])}function Kh(e){return Mv(e.replace(/<[^>]+>/g," ").replace(/\s+/g," ")).trim()}var Cv=/<h([1-6])\s+id="([^"]*)"[^>]*>([\s\S]*?)<\/h\1>|<(p|li|pre|blockquote)[^>]*>([\s\S]*?)<\/\4>/g;function Dv(e){let t=[],n="",o=e.title;for(let i of e.html.matchAll(Cv)){if(i[1]!==void 0){n=i[2],o=Kh(i[3]),o&&t.push({chapter:e.id,chapterTitle:e.title,anchor:n,heading:o,text:o});continue}let r=Kh(i[5]);r&&t.push({chapter:e.id,chapterTitle:e.title,anchor:n,heading:o,text:r})}return t}var Bh=null,jh=[];function Lv(e){return Bh!==e&&(jh=e.flatMap(Dv),Bh=e),jh}var Ov=40;function Wh(e,t,n=Ov){let o=t.trim().toLowerCase();if(!o)return[];let i=[];for(let r of Lv(e))if((r.text.toLowerCase().includes(o)||r.heading.toLowerCase().includes(o))&&(i.push(r),i.length>=n))break;return i}var Iv='button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])',$v="input, textarea, select, [contenteditable]";function Nv(e){return pi.find(t=>t.id===e)??pi[0]}function Yh(){let e=te(),t=M(c=>c.help),n=J(null),o=J(null),i=()=>e.store.dispatch({type:"help",action:{type:"close"}}),r=c=>e.store.dispatch({type:"help",action:{type:"open",target:c}});G(()=>{let c=p=>{p.key!=="F1"||p.defaultPrevented||p.target?.closest?.($v)||(p.preventDefault(),e.store.dispatch({type:"help",action:{type:"open"}}))};return window.addEventListener("keydown",c),()=>window.removeEventListener("keydown",c)},[e]),G(()=>{if(!t.open){n.current?.contains(document.activeElement)&&document.querySelector(".help-toggle")?.focus();return}n.current?.querySelector(Iv)?.focus();let c=p=>{p.key!=="Escape"||e.store.getState().ask||(p.preventDefault(),p.stopPropagation(),i())};return window.addEventListener("keydown",c,!0),()=>window.removeEventListener("keydown",c,!0)},[t.open]),G(()=>{if(!t.open)return;let c=t.anchor&&o.current?.querySelector(`#${CSS.escape(t.anchor)}`);c?c.scrollIntoView({block:"start"}):o.current?.scrollTo(0,0)},[t.open,t.chapter,t.anchor]);let a=Nv(t.chapter),l=Ye(()=>Wh(pi,t.query),[t.query]),d=c=>{let p=c.target.closest?.("a[href]");if(!p)return;let m=ah(p.getAttribute("href")??"",a.id);m&&(c.preventDefault(),r(m))};return s("section",{id:"help-panel",ref:n,class:"help-panel"+(t.open?" open":""),"aria-label":"Help",children:[s("div",{class:"help-header",children:[s("button",{class:"help-back",onClick:i,children:"Back"}),s("h2",{children:"Help"})]}),s("label",{class:"help-search",children:[s("span",{class:"visually-hidden",children:"Search the manual"}),s("input",{type:"search",placeholder:"Search the manual",value:t.query,onInput:c=>e.store.dispatch({type:"help",action:{type:"query",query:c.target.value}})})]}),t.query.trim()!==""&&s("ul",{class:"help-results","aria-label":"Search results",children:[l.length===0&&s("li",{class:"muted help-no-results",children:"No match."}),l.map((c,p)=>s("li",{children:s("button",{type:"button",class:"help-result",onClick:()=>r({chapter:c.chapter,anchor:c.anchor}),children:[s("span",{class:"help-result-heading",children:[c.chapterTitle," \xB7 ",c.heading]}),s("span",{class:"help-result-text",children:c.text})]})},p))]}),s("div",{class:"help-body",children:[s("nav",{class:"help-toc","aria-label":"Chapters",children:s("ol",{children:pi.map((c,p)=>s("li",{children:s("button",{type:"button",class:"help-toc-item"+(c.id===a.id?" current":""),"aria-current":c.id===a.id?"page":void 0,onClick:()=>r({chapter:c.id}),children:[p+1,". ",c.title]})},c.id))})}),s("div",{class:"help-content",ref:o,onClick:d,children:[s("h1",{children:a.title}),s("div",{dangerouslySetInnerHTML:{__html:a.html}})]})]})]})}var Rv=new Set(["Escape","Enter","Backspace","Delete","Home","End","ArrowLeft","ArrowRight","ArrowUp","ArrowDown","PageUp","PageDown"]),Fv='input, textarea, select, [contenteditable], [role="dialog"], [role="menu"], [role="listbox"]',qv='button, a, summary, [role="button"], [role="tab"], [role="slider"]';function Hv(e){return e.ctrlKey||e.metaKey||e.altKey?null:e.key.length===1||Rv.has(e.key)?e.key:null}function zv(e,t){return e?.closest?e.closest(Fv)?!1:e.closest(qv)?t.length===1&&t!==" "||t==="Escape":!0:!0}function Gh(e){G(()=>{let t=n=>{if(n.defaultPrevented)return;let o=Hv(n);!o||!zv(n.target,o)||(n.preventDefault(),e.typeKey(o))};return window.addEventListener("keydown",t),()=>window.removeEventListener("keydown",t)},[e])}var Xv=["main","file","num"];function Zh(){let e=te(),t=M(m=>m.hello?.menus),n=M(m=>m.core?.menu??0),o=M(m=>m.drawerOpen),i=M(m=>m.busy),r=J(null),a=()=>e.store.dispatch({type:"drawer",open:!1});G(()=>{if(!o){r.current?.contains(document.activeElement)&&document.querySelector(".menu-toggle")?.focus();return}r.current?.querySelector("button")?.focus();let m=u=>{u.key!=="Escape"||e.store.getState().ask||(u.preventDefault(),u.stopPropagation(),a())};return window.addEventListener("keydown",m,!0),()=>window.removeEventListener("keydown",m,!0)},[o]);let l=Xv[n]??"main",d=t?.[l]??[],c=t?.[`${l}_keys`]??"",p=t?.[`${l}_hints`]??[];return s(ve,{children:[s("nav",{id:"command-menu",ref:r,class:"menu-panel"+(o?" open":""),"aria-label":"Commands",children:[s("div",{class:"menu-title-row",children:[s("h2",{class:"menu-title",children:l==="main"?"Commands":l==="file"?"File":"Numerics"}),s(st,{target:Ss(n),label:l==="main"?"the main commands":l==="file"?"the File menu":"Numerics"})]}),s("ul",{children:d.map((m,u)=>s("li",{children:s("button",{class:"menu-item",title:i?Mo:p[u],"aria-keyshortcuts":c[u],disabled:i,onClick:()=>{a(),e.key(c[u])},children:[s("kbd",{"aria-hidden":"true",children:c[u]?.toUpperCase()}),s("span",{children:m})]})},`${l}${u}`))})]}),o&&s("div",{class:"drawer-scrim",onClick:a,"aria-hidden":"true"})]})}var Jh={error:"Error",auto:"AUTO",log:"Log",info:"Info"},Uv=["all","error","auto","log","info"];function Qh(){let e=M(d=>d.log),[t,n]=ie("all"),[o,i]=ie(""),r=Ye(()=>{let d={error:0,auto:0,log:0,info:0};for(let c of e)d[c.kind]++;return d},[e]),a=o.trim().toLowerCase(),l=e.filter(d=>(t==="all"||d.kind===t)&&(!a||d.text.toLowerCase().includes(a)));return s("details",{class:"messages",children:[s("summary",{children:["Messages (",e.length,")",r.error>0&&s("span",{class:"error-count",children:[" \xB7 ",r.error," error",r.error>1?"s":""]})]}),s("div",{class:"messages-tools",children:[s("div",{class:"messages-filters",role:"group","aria-label":"Filter messages by kind",children:Uv.map(d=>s("button",{class:"small"+(t===d?" active":""),"aria-pressed":t===d,onClick:()=>n(d),children:[d==="all"?"All":Jh[d],d!=="all"&&r[d]>0?` (${r[d]})`:""]},d))}),s("label",{class:"messages-search",children:[s("span",{class:"visually-hidden",children:"Search messages"}),s("input",{type:"search",placeholder:"Search\u2026",value:o,onInput:d=>i(d.target.value)})]})]}),l.length===0?s("p",{class:"text-empty",children:e.length===0?"Nothing yet.":"No message matches."}):s("ul",{class:"messages-list",children:l.map((d,c)=>s("li",{class:"message-line message-"+d.kind,children:[s("span",{class:"message-kind",children:Jh[d.kind]}),s("span",{class:"message-text",children:d.text})]},c))})]})}var Mr=Math.PI/180;function Vv(e,t){let n=Math.cos(e*Mr),o=Math.sin(e*Mr),i=Math.sin(t*Mr),r=Math.cos(t*Mr);return[[n,o,0],[-r*o,r*n,i],[o*i,-i*n,r]]}function Kv(e,t,n,o){let i=e.xmax>e.xmin?2/(e.xmax-e.xmin):0,r=e.ymax>e.ymin?2/(e.ymax-e.ymin):0,a=e.zmax>e.zmin?2/(e.zmax-e.zmin):0;return[(t-(e.xmin+e.xmax)/2)*i,(n-(e.ymin+e.ymax)/2)*r,(o-(e.zmin+e.zmax)/2)*a]}function Bv(e,t,n,o){return[e[0][0]*t+e[0][1]*n+e[0][2]*o,e[1][0]*t+e[1][1]*n+e[1][2]*o,e[2][0]*t+e[2][1]*n+e[2][2]*o]}function zs(e,t,n,o,i,r,a,l,d){let[c,p,m]=Kv(e,a,l,d),[u,f,w]=Bv(Vv(t,n),c,p,m);if(!o)return{x:u,y:f};if(w>=r||w<i)return null;let k=(r-i)/(r-w);return{x:k*u,y:k*f}}var jv=(()=>{let e=[];for(let t of[0,1])for(let n of[0,1])for(let o of[0,1])e.push([t,n,o]);return e})();function em(e,t,n,o,i,r){return jv.map(([a,l,d])=>zs(e,t,n,o,i,r,a?e.xmax:e.xmin,l?e.ymax:e.ymin,d?e.zmax:e.zmin))}var tm=(()=>{let e=[];for(let t=0;t<8;t++)for(let n=0;n<3;n++){let o=t^1<<n;o>t&&e.push([t,o])}return e})();function nm(e,t,n,o){return{theta:e-n,phi:t-o}}var Xs=5,Us=30;function om(e,t,n,o){let i=o?Us:Xs;switch(n){case"ArrowLeft":return{theta:e-i,phi:t};case"ArrowRight":return{theta:e+i,phi:t};case"ArrowUp":return{theta:e,phi:t+i};case"ArrowDown":return{theta:e,phi:t-i};default:return null}}function Wv(e,t,n,o){let i=1/0,r=-1/0,a=1/0,l=-1/0;for(let f of n)f&&(f.x<i&&(i=f.x),f.x>r&&(r=f.x),f.y<a&&(a=f.y),f.y>l&&(l=f.y));if(!(i<=r)||!(a<=l))return{cx:e/2,cy:t/2,scale:1};let d=Math.max(1e-9,r-i),c=Math.max(1e-9,l-a),p=Math.max(1,e-2*o),m=Math.max(1,t-2*o),u=Math.min(p/d,m/c);return{cx:e/2-(i+r)/2*u,cy:t/2+(a+l)/2*u,scale:u}}function Oo(e,t){return[e.cx+t.x*e.scale,e.cy-t.y*e.scale]}function im(e,t,n,o,i,r,a,l){let d=window.devicePixelRatio||1,c=Math.max(1,Math.round(t*d)),p=Math.max(1,Math.round(n*d));e.width!==c&&(e.width=c),e.height!==p&&(e.height=p);let m=e.getContext("2d");if(!m||(m.setTransform(d,0,0,d,0,0),m.clearRect(0,0,t,n),!o))return null;let u=Wv(t,n,o.box,24);m.strokeStyle=l,m.lineWidth=1,m.beginPath();for(let[f,w]of tm){let k=o.box[f],A=o.box[w];if(!k||!A)continue;let[C,P]=Oo(u,k),[$,E]=Oo(u,A);m.moveTo(C,P),m.lineTo($,E)}m.stroke(),m.lineCap="round",m.lineJoin="round";for(let f of o.curves)if(m.strokeStyle=Be(f.color,a),m.fillStyle=m.strokeStyle,f.line){m.lineWidth=1.5,m.beginPath();let w=!1;for(let k of f.points){if(!k){w=!1;continue}let[A,C]=Oo(u,k);w?m.lineTo(A,C):m.moveTo(A,C),w=!0}m.stroke()}else for(let w of f.points){if(!w)continue;let[k,A]=Oo(u,w);m.beginPath(),m.arc(k,A,f.radius,0,2*Math.PI),m.fill()}return{width:t,height:n,theta:i,phi:r,curves:o.curves.map(f=>({label:f.label,points:f.points.filter(w=>w).length})),box:o.box.map(f=>f?{x:Oo(u,f)[0],y:Oo(u,f)[1]}:null),at:performance.now()}}var Cr=class{constructor(t){this.model=null;this.theta=0;this.phi=0;this.dark=!1;this.axisColor="#888";this.lastInfo=null;this.canvas=t}set(t,n,o,i,r){this.model=t,this.theta=n,this.phi=o,this.dark=i,this.axisColor=r,this.redraw()}resize(){this.redraw()}redraw(){let t=this.canvas.clientWidth,n=this.canvas.clientHeight;!t||!n||(this.lastInfo=im(this.canvas,t,n,this.model,this.theta,this.phi,this.dark,this.axisColor))}pixels(){return er(this.canvas)}info(){return this.lastInfo}destroy(){this.model=null}};var Vs=new Float32Array(0);function rm(e,t,n,o,i,r,a){let[l,d,c]=e.shift,p=Math.max(l,d,c,0);return{curves:e.curves.map(u=>{let f=e.columns.get(u.x)??Vs,w=e.columns.get(u.y)??Vs,k=e.columns.get(u.z)??Vs,A=Math.max(0,Math.min(f.length,w.length,k.length)-p),C=new Array(A);for(let P=0;P<A;P++)C[P]=zs(t,n,o,i,r,a,f[p-l+P],w[p-d+P],k[p-c+P]);return{label:Fi(e,u),color:u.color,line:u.line>0,radius:u.line>0?0:Math.max(1,-u.line),points:C,row0:p}}),box:em(t,n,o,i,r,a)}}function Dr(e){return e===void 0?"":String(Math.round(e))}function am({win:e,dark:t,shown:n,tabbed:o}){let i=te(),r=M(N=>et(N.plots,e)),a=r?.info??null,l=r?.series??null,d=r?.view3d??null,c=M(N=>N.busy),p=J(null),m=J(null),u=J(null),f=J(null);G(()=>{if(!m.current)return;let N=new Cr(m.current);u.current=N,wo(e,N);let x=new ResizeObserver(()=>{p.current?.clientWidth&&N.resize()});return x.observe(p.current),()=>{x.disconnect(),wo(e,null),N.destroy()}},[e]);let w=Ye(()=>l&&a?.three&&d?rm(l,a.box,d.theta,d.phi,a.persp,a.zplane,a.zview):null,[l,a,d]);G(()=>{if(!n||!u.current||!d)return;let N=getComputedStyle(document.documentElement).getPropertyValue("--fg-muted").trim()||"#888";u.current.set(w,d.theta,d.phi,t,N)},[w,d,t,n]);let k=(N,x)=>i.rotate3d(e,N,x),A=N=>{!d||N.button!==0||(N.currentTarget.setPointerCapture?.(N.pointerId),f.current={pointerId:N.pointerId,x0:N.clientX,y0:N.clientY,theta0:d.theta,phi0:d.phi})},C=N=>{let x=f.current;if(!x||x.pointerId!==N.pointerId)return;let v=nm(x.theta0,x.phi0,N.clientX-x.x0,N.clientY-x.y0);k(v.theta,v.phi)},P=N=>{f.current?.pointerId===N.pointerId&&(f.current=null)},$=N=>{if(!d||N.ctrlKey||N.metaKey||N.altKey)return;let x=om(d.theta,d.phi,N.key,N.shiftKey);x&&(N.preventDefault(),N.stopPropagation(),k(x.theta,x.phi))},E=!w||w.curves.every(N=>N.points.length===0),U=`3D plot${a?` of ${a.title}`:""}${d?`, theta ${Dr(d.theta)}, phi ${Dr(d.phi)}`:""}`,R=o?{role:"tabpanel",id:`plot-panel-${e}`,"aria-labelledby":`plot-tab-${e}`}:{"aria-label":"Plot"};return s("section",{class:"plot-view",hidden:!n,...R,children:[s("header",{class:"plot-bar",children:s("div",{class:"legend",role:"group","aria-label":"Curves",children:w?.curves.map((N,x)=>s("span",{class:"legend-item",children:[s("span",{class:"swatch",style:{background:Be(N.color,t)},"aria-hidden":"true"}),N.label]},x))})}),s("div",{class:"plot-host",ref:p,tabIndex:0,role:"application","aria-roledescription":"3D plot","aria-label":U,"aria-describedby":"plot-3d-keys-help",onKeyDown:$,onPointerDown:A,onPointerMove:C,onPointerUp:P,onPointerCancel:P,children:[s("canvas",{ref:m,class:"plot-canvas-3d","aria-hidden":"true"}),E&&s("div",{class:"plot-empty",children:[s("p",{children:c?"Integrating\u2026":"No trajectory yet."}),!c&&s("button",{class:"primary",onClick:()=>i.keys("i","g"),children:"Integrate (I, G)"})]})]}),s("p",{id:"plot-3d-keys-help",class:"visually-hidden",children:`Drag turns the view; the arrow keys turn it ${Xs} degrees at a time, Shift ${Us}.`}),s("footer",{class:"readout",role:"status","aria-live":"polite",children:s("span",{class:"muted",children:[d?s(ve,{children:["theta ",Dr(d.theta)," \xB7 phi ",Dr(d.phi)," \u2014 "]}):null,s("span",{class:"hint-mouse",children:"Drag to turn"}),s("span",{class:"hint-touch",children:"Drag to turn"})," \xB7 arrow keys turn it too"]})})]})}function Yv(e){if(e.info)return e.info.title;let t=e.series?.curves[0],n=o=>e.series?.names.get(o)??(o===0?"T":"");return t?`${n(t.y)} vs ${n(t.x)}`:""}function Gv(){let e=te(),{frames:t,playing:n,shown:o}=M(r=>r.kinescope),i=M(r=>r.busy);return s("div",{class:"kinescope-bar",role:"group","aria-label":"Kinescope",children:[s("button",{class:"small",disabled:i,onClick:()=>e.kinescopeCapture(),title:"Kinescope/Capture: keep this plot as a frame (k, c)",children:"Capture"}),t.length>0&&s(ve,{children:[s("span",{class:"kinescope-count",children:[t.length," frame",t.length===1?"":"s",n&&o!==null?`, showing ${o+1}`:""]}),s("button",{class:"small",disabled:i||n,onClick:()=>e.kinescopePlay(),title:"Kinescope/Playback: show the captured frames (k, p)",children:"Play"}),s("button",{class:"small",disabled:!n,onClick:()=>e.kinescopeStop(),children:"Stop"}),s("button",{class:"small",disabled:n,onClick:()=>e.downloadKinescopeGif(),title:"An animated GIF of the captured frames, built here and downloaded",children:"Export GIF"}),s("button",{class:"small",disabled:i,onClick:()=>e.kinescopeReset(),title:"Kinescope/Reset: clear the captured frames (k, r)",children:"Reset"})]})]})}function sm({dark:e}){let t=te(),n=M(c=>c.plots.windows),o=M(c=>c.plots.active),i=M(c=>c.busy),r=J(null),a=n.length?n.map(c=>c.win):[1],l=n.length>1;return s("div",{class:"plots",children:[s("div",{class:"plot-windows",children:[l&&s("div",{class:"plot-tabs",role:"tablist","aria-label":"Plot windows",ref:r,onKeyDown:c=>{let p=n.findIndex(w=>w.win===o),m=n.length,u=c.key==="ArrowRight"||c.key==="ArrowDown"?(p+1)%m:c.key==="ArrowLeft"||c.key==="ArrowUp"?(p-1+m)%m:c.key==="Home"?0:c.key==="End"?m-1:-1;if(u<0||p<0)return;c.preventDefault(),c.stopPropagation();let f=n[u].win;t.selectWindow(f),r.current?.querySelector(`#plot-tab-${f}`)?.focus()},children:n.map(c=>s("button",{id:`plot-tab-${c.win}`,class:"plot-tab",role:"tab","aria-selected":c.win===o,"aria-controls":`plot-panel-${c.win}`,tabIndex:c.win===o?0:-1,onClick:()=>t.selectWindow(c.win),children:[s("span",{class:"plot-tab-num",children:c.win}),s("span",{class:"plot-tab-title",children:Yv(c)})]},c.win))}),s("div",{class:"plot-window-tools",children:[s("button",{class:"small",disabled:i,onClick:()=>t.newWindow(),title:"Makewindow/Create: a new plot window, a copy of this one (M, C)",children:"New window"}),l&&s("button",{class:"small",disabled:i||o===1,onClick:()=>t.closeWindow(),title:"Makewindow/Destroy: close this plot window (M, D); window 1 stays",children:"Close window"})]}),s(Gv,{})]}),a.map(c=>{let p=c===o||a.length===1;return n.find(u=>u.win===c)?.info?.three?s(am,{win:c,dark:e,shown:p,tabbed:l},c):s(zh,{win:c,dark:e,shown:p,tabbed:l},c)}),s("p",{id:"plot-keys-help",class:"visually-hidden",children:kh})]})}function lm(){let e=te(),t=M(p=>p.connected),n=M(p=>p.exited),o=M(p=>p.busy),i=M(p=>p.stopping),r=M(p=>p.progress),a=M(p=>p.bottom),l=M(p=>p.core?.rows??0);return s("footer",{class:"status-bar",children:[s("span",{class:`status-dot ${n!==null?"down":t?o?"busy":"up":""}`,"aria-hidden":"true"}),s("span",{role:"status","data-testid":"status",children:n!==null?"XPP has stopped":t?i?"Stopping\u2026":o?"Working\u2026":"Ready":"Connecting\u2026"}),r&&s("progress",{max:r.of,value:r.n,"aria-label":"Progress",children:[Math.round(100*r.n/r.of),"%"]}),o&&s("button",{class:"small danger",disabled:i,onClick:()=>e.abort(),title:"Stop the running command (Escape does the same)",children:i?"Stopping\u2026":"Stop"}),s("span",{class:"status-message",children:a}),s("span",{class:"muted rows",children:[l," rows"]})]})}var Zv='button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])',Jv=[["Find","find","Find the row where a column is closest to a value"],["Get","get","Make the selected row the initial conditions"],["Replace","replace","Replace a column by a formula"],["Unrepl","unreplace","Undo the last Replace"],["Table","table","Write a column as a function table file"],["Load","load","Load data from a file"],["Write","write","Write the rows from First to Last to a file"],["First","first","Start the range at the selected row"],["Last","last","End the range at the selected row"],["Restore","restore","Redraw the plot from the rows First to Last"],["Add col","addcol","Add a column computed from a formula"],["Del col","delcol","Delete a column"]];function Qv(e){return e===null?"NaN":e===void 0?"":String(e)}function ew(e){let[t,n]=ie(32);return G(()=>{let o=()=>{let a=e.current?.getBoundingClientRect().height;a&&n(a)};o();let i=matchMedia("(pointer: coarse)"),r=()=>requestAnimationFrame(o);return i.addEventListener("change",r),window.addEventListener("resize",r),()=>{i.removeEventListener("change",r),window.removeEventListener("resize",r)}},[]),t}function cm(){let e=te(),t=M(v=>v.table.open),n=M(v=>v.table.page),o=M(v=>v.table.exporting),i=M(v=>v.table.selected),r=M(v=>v.core?.rows??0),a=Math.max(n?.rows??0,r),l=n?.cols??["T"],d=J(null),c=J(null),p=J(null),m=ew(p),[u,f]=ie(0),[w,k]=ie(0),A=()=>e.closeTable();G(()=>{if(!t){d.current?.contains(document.activeElement)&&document.querySelector(".table-toggle")?.focus();return}d.current?.querySelector(Zv)?.focus();let v=T=>{T.key!=="Escape"||e.store.getState().ask||(T.preventDefault(),T.stopPropagation(),A())};return window.addEventListener("keydown",v,!0),()=>window.removeEventListener("keydown",v,!0)},[t]),G(()=>{let v=c.current;if(!t||!v)return;let T=()=>f(v.scrollTop),L=new ResizeObserver(()=>k(v.clientHeight));return L.observe(v),v.addEventListener("scroll",T),k(v.clientHeight),f(v.scrollTop),()=>{L.disconnect(),v.removeEventListener("scroll",T)}},[t]);let C=Math.max(1,Math.ceil((w||1)/m)+1),P=Math.max(0,Math.floor(u/m));G(()=>{!t||!a||o||e.fetchTableRows(P,C)},[t,a,P,C,n,o]);let $=v=>{let T=c.current;if(!T)return;let L=v*m,H=L+m,O=w||T.clientHeight;L<T.scrollTop?T.scrollTop=L:H>T.scrollTop+O&&(T.scrollTop=H-O)},E=v=>{let T=Math.max(0,Math.min(a>0?a-1:0,v));e.selectTableRow(T),$(T)},U=v=>{let T=Math.max(1,C-1),L={ArrowUp:-1,ArrowDown:1,PageUp:-T,PageDown:T};v.key in L?(v.preventDefault(),v.stopPropagation(),E(i+L[v.key])):v.key==="Home"?(v.preventDefault(),v.stopPropagation(),E(0)):v.key==="End"?(v.preventDefault(),v.stopPropagation(),E(a-1)):v.key==="Enter"&&(v.preventDefault(),v.stopPropagation(),e.getRow())},R=Math.min(a,P+C),N=[];for(let v=P;v<R;v++)N.push({row:v,values:Yu(n,v)});let x=async()=>{let v=await e.exportTableCsv(),T=URL.createObjectURL(new Blob([v],{type:"text/csv"}));Bt("data.csv",T),setTimeout(()=>URL.revokeObjectURL(T),1e3)};return s("section",{id:"table-panel",ref:d,class:"table-panel"+(t?" open":""),"aria-label":"Data table",children:[s("div",{class:"table-header",children:[s("button",{class:"table-back",onClick:A,children:"Back"}),s("h2",{children:"Data"}),s(st,{target:Ue.dataTab,label:"the Data tab"}),s("button",{class:"small",onClick:x,disabled:!n?.data.length||o,title:"Save every stored row as a CSV file",children:o?"Exporting\u2026":"Export CSV"})]}),s("div",{class:"table-tools",children:Jv.map(([v,T,L])=>s("button",{title:L,onClick:()=>e.browserOp(T),children:v},T))}),s("p",{class:"table-info",role:"status",children:a?`${a} rows. Selected row ${i}.`+(n?` First..Last: ${n.start}..${Math.max(n.start,n.end-1)}.`:""):"No data yet: integrate first."}),s("div",{class:"table-grid-wrap",ref:c,role:"grid","aria-label":"Stored data","aria-rowcount":a+1,"aria-colcount":l.length,"aria-activedescendant":a?`table-row-${i}`:void 0,tabIndex:0,onKeyDown:U,children:[s("div",{class:"table-row table-row-head",role:"row","aria-rowindex":1,ref:p,children:l.map((v,T)=>s("span",{class:"table-cell",role:"columnheader",title:v,children:v},T))}),s("div",{class:"table-body",style:{height:`${a*m}px`},children:N.map(({row:v,values:T})=>s("div",{id:`table-row-${v}`,class:"table-row"+(v===i?" selected":""),role:"row","aria-rowindex":v+2,"aria-selected":v===i,style:{top:`${v*m}px`},onClick:()=>E(v),children:l.map((L,H)=>s("span",{class:"table-cell",role:"gridcell",children:Qv(T?.[H])},H))},v))})]})]})}var tw='button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])',dm=[{id:"equations",label:"Equations"},{id:"source",label:"Source"},{id:"equilibrium",label:"Equilibrium"}];function nw(){let e=M(t=>t.text.equations);return e?e.length?s("pre",{class:"text-equations",tabIndex:0,"aria-label":"The model's equations",children:e.join(`
`)}):s("p",{class:"text-empty",children:"The model has no equations."}):s("p",{class:"text-empty",children:"No equations yet."})}function ow({line:e,session:t}){return s("div",{class:"source-line",children:[s("span",{class:"source-text",children:e.text||"\xA0"}),e.comment?.hasAction&&s("button",{class:"source-action",onClick:()=>t.runAction(e.comment.index),title:"A comment action: sets the parameters it names",children:e.comment.text.trim()||`Action ${e.comment.index+1}`})]})}function iw(){let e=te(),t=M(o=>o.text.source);if(!t)return s("p",{class:"text-empty",children:"No source yet."});let n=t.comments.filter(o=>o.hasAction).length;return s("div",{class:"text-source",children:[n>0&&s("p",{class:"text-hint",role:"status",children:[n," action",n>1?"s":""," on the lines marked with a button below."]}),s("div",{class:"source-lines","aria-label":"The model's source, with line numbers",children:t.lines.map((o,i)=>s("div",{class:"source-row",children:[s("span",{class:"source-lineno","aria-hidden":"true",children:i+1}),s(ow,{line:o,session:e})]},i))})]})}function rw(e){let t=e.toLowerCase();return t==="stable"?"eq-stable":t==="unstable"?"eq-unstable":"eq-neutral"}function aw(){let e=te(),t=M(n=>n.text.equilibrium);return s("div",{class:"text-equilibrium",children:[s("div",{class:"text-tools",children:[s("button",{onClick:()=>e.findEquilibrium(),title:"Sing pts / Go: find the equilibrium closest to the current initial conditions",children:"Find equilibrium"}),s("button",{onClick:()=>e.importEquilibrium(),disabled:!t,title:"Make this equilibrium the initial conditions",children:"Import"})]}),t?s(ve,{children:[s("p",{class:"eq-type "+rw(t.type),role:"status",children:t.type}),s("table",{class:"eq-counts",children:[s("caption",{class:"visually-hidden",children:"Eigenvalue counts: complex/real with positive/negative real part, purely imaginary"}),s("tbody",{children:[s("tr",{children:[s("th",{scope:"row",children:"c+"}),s("td",{children:t.cplus}),s("th",{scope:"row",children:"c-"}),s("td",{children:t.cminus})]}),s("tr",{children:[s("th",{scope:"row",children:"r+"}),s("td",{children:t.rplus}),s("th",{scope:"row",children:"r-"}),s("td",{children:t.rminus})]}),s("tr",{children:[s("th",{scope:"row",children:"im"}),s("td",{children:t.im}),s("td",{}),s("td",{})]})]})]}),s("table",{class:"eq-values",children:[s("caption",{children:"Values"}),s("thead",{children:s("tr",{children:[s("th",{scope:"col",children:"Name"}),s("th",{scope:"col",children:"Value"})]})}),s("tbody",{children:t.values.map(([n,o])=>s("tr",{children:[s("td",{children:n}),s("td",{title:String(o),children:je(o)})]},n))})]}),s("table",{class:"eq-values",children:[s("caption",{children:"Eigenvalues"}),s("thead",{children:s("tr",{children:[s("th",{scope:"col",children:"Real"}),s("th",{scope:"col",children:"Imaginary"})]})}),s("tbody",{children:t.eigenvalues?.length?t.eigenvalues.map(([n,o],i)=>s("tr",{children:[s("td",{title:String(n),children:je(n)}),s("td",{title:String(o),children:je(o)})]},i)):s("tr",{children:s("td",{colSpan:2,class:"text-hint",children:"Not known for this equilibrium (a delay equation)."})})})]})]}):s("p",{class:"text-empty",children:"No equilibrium yet: choose Find equilibrium, or Sing pts from the menu."})]})}function um(){let e=te(),t=M(r=>r.text.open),n=M(r=>r.text.tab),o=J(null),i=()=>e.closeText();return G(()=>{if(!t){o.current?.contains(document.activeElement)&&document.querySelector(".text-toggle")?.focus();return}o.current?.querySelector(tw)?.focus();let r=a=>{a.key!=="Escape"||e.store.getState().ask||(a.preventDefault(),a.stopPropagation(),i())};return window.addEventListener("keydown",r,!0),()=>window.removeEventListener("keydown",r,!0)},[t]),s("section",{id:"text-panel",ref:o,class:"text-panel"+(t?" open":""),"aria-label":"Text views",children:[s("div",{class:"text-header",children:[s("button",{class:"text-back",onClick:i,children:"Back"}),s("h2",{children:"Text"})]}),s("div",{class:"text-tabs",role:"group","aria-label":"Which text view to show",children:dm.map(r=>s("button",{"aria-pressed":n===r.id,class:"text-tab"+(n===r.id?" active":""),onClick:()=>e.selectTextTab(r.id),children:r.label},r.id))}),s("div",{class:"text-body","aria-label":`${dm.find(r=>r.id===n).label} view`,children:[n==="equations"&&s(nw,{}),n==="source"&&s(iw,{}),n==="equilibrium"&&s(aw,{})]})]})}var pm={system:"light",light:"dark",dark:"system"},Ks={system:"follow system",light:"light",dark:"dark"};function sw({theme:e}){let t={width:18,height:18,viewBox:"0 0 24 24",fill:"none",stroke:"currentColor","stroke-width":2,"stroke-linecap":"round","stroke-linejoin":"round","aria-hidden":"true",class:"icon"};return e==="light"?s("svg",{...t,children:[s("circle",{cx:"12",cy:"12",r:"4"}),s("path",{d:"M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"})]}):e==="dark"?s("svg",{...t,children:s("path",{d:"M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"})}):s("svg",{...t,children:[s("circle",{cx:"12",cy:"12",r:"9"}),s("path",{d:"M12 3a9 9 0 0 1 0 18z",fill:"currentColor"})]})}function hm(){let e=te(),t=M(f=>f.title),n=M(f=>f.hello?.file??""),o=M(f=>f.theme),i=M(f=>f.drawerOpen),r=M(f=>f.valuesOpen),a=M(f=>f.table.open),l=M(f=>f.text.open),d=M(f=>f.aplot.open),c=M(f=>f.ani.open),p=M(f=>f.help.open),m=M(f=>f.busy);return s("header",{class:"title-bar",children:[s("button",{class:"menu-toggle","aria-controls":"command-menu","aria-expanded":i,onClick:()=>e.store.dispatch({type:"drawer",open:!i}),children:"Menu"}),s("h1",{children:t||"XPP"}),s("span",{class:"muted file",children:n}),s("span",{class:"spacer"}),s("button",{class:"primary",disabled:m,onClick:()=>e.keys("i","g"),title:m?Mo:"Initialconds / Go (I, G)",children:"Integrate"}),s("button",{class:"theme-toggle icon-button",onClick:()=>{let f=pm[o];ch(f),e.store.dispatch({type:"theme",theme:f})},"data-theme-choice":o,"aria-label":`Theme: ${Ks[o]}`,title:`Theme: ${Ks[o]} (click for ${Ks[pm[o]]})`,children:s(sw,{theme:o})}),s("button",{class:"values-toggle","aria-controls":"values-panel","aria-expanded":r,onClick:()=>e.store.dispatch({type:"valuesPanel",open:!r}),children:"Values"}),s("button",{class:"table-toggle","aria-controls":"table-panel","aria-expanded":a,onClick:()=>a?e.closeTable():e.openTable(),children:"Data"}),s("button",{class:"text-toggle","aria-controls":"text-panel","aria-expanded":l,onClick:()=>l?e.closeText():e.openText(),title:"Equations, source and the last equilibrium",children:"Text"}),s("button",{class:"aplot-toggle","aria-controls":"aplot-panel","aria-expanded":d,onClick:()=>d?e.closeAplot():e.openAplot(),title:"The array plot: XPP's grid of a range of columns and rows, coloured by value",children:"Array"}),s("button",{class:"ani-toggle","aria-controls":"ani-panel","aria-expanded":c,onClick:()=>c?e.closeAni():e.openAni(),title:"The animation (Viewaxes/Toon): play, step and seek its frames",children:"Animation"}),s("button",{class:"help-toggle","aria-controls":"help-panel","aria-expanded":p,onClick:()=>e.store.dispatch({type:"help",action:{type:"open"}}),title:"The manual (F1)",children:"Help"})]})}var lw=6e3;function cw({toast:e}){let t=te(),n=J(null),o=r=>{r?.length&&t.addMissingFile(e.id,r[0])},i=async()=>{if(!Ii()){n.current.value="",n.current.click();return}o(await $i(!1).catch(()=>null))};return s(ve,{children:[s("input",{ref:n,type:"file",class:"visually-hidden",tabIndex:-1,"aria-hidden":"true","data-file-input":"add",onChange:r=>{let a=r.target,l=[...a.files??[]];a.value="",o(l)}}),s("button",{class:"small","data-add-file":e.action.name,onClick:()=>{i()},title:`Copy a file into the model's folder as ${e.action.name} and run the command again`,children:"Add file\u2026"})]})}function dw({toast:e}){let t=te(),n=()=>t.store.dispatch({type:"dismiss",id:e.id});return G(()=>{if(e.kind==="error")return;let o=setTimeout(n,lw);return()=>clearTimeout(o)},[e.id]),s("li",{class:`toast ${e.kind}`,role:e.kind==="error"?"alert":"status",children:[s("span",{children:[e.text,e.action&&!e.text.includes(e.action.name)&&s(ve,{children:[" (",e.action.name,")"]})]}),e.action?.kind==="addFile"&&s(cw,{toast:e}),s("button",{class:"icon",onClick:n,"aria-label":"Dismiss",children:"\xD7"})]})}function mm(){let e=M(t=>t.toasts);return s("ul",{class:"toasts","aria-label":"Notifications",children:e.map(t=>s(dw,{toast:t},t.id))})}var uw='button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])';function Lr(e){return`${e.kind}:${e.name.toLowerCase()}`}function fm({target:e,onClose:t}){let n=te(),o=M(O=>O.core?.pars)??[],i=M(O=>O.core?.ics)??[],r=e!=="new",a=Ye(()=>[...o.map(([O,X])=>({kind:"par",name:O,value:X})),...i.map(([O,X])=>({kind:"ic",name:O,value:X}))],[o,i]),l=r?a.find(O=>O.name.toLowerCase()===e.name.toLowerCase())??null:null,[d,c]=ie(""),[p,m]=ie(l),[u,f]=ie(r?e.lo:""),[w,k]=ie(r?e.hi:""),[A,C]=ie(r?e.step:""),[P,$]=ie(0),E=Ye(()=>ap(a,d),[a,d]),U=O=>{let X=p&&Lr(p)===Lr(O);if(m(O),!X){let _=ep(O.value);f(String(_.lo)),k(String(_.hi)),C(String(rs(_.lo,_.hi)))}},R=rp(u,w,A),N=!!p&&!R.lo&&!R.hi&&!R.step,x=J(null);G(()=>{let O=document.activeElement,X=x.current?.querySelector("[data-autofocus]");return X?.focus(),X instanceof HTMLInputElement&&X.select(),()=>O?.focus?.()},[]);let v=()=>{if(!N||!p)return;let O={name:p.name,lo:u,hi:w,step:A};r?n.store.dispatch({type:"values",action:{type:"setSlider",id:e.id,patch:O}}):n.store.dispatch({type:"values",action:{type:"addSliderWith",def:O}}),t()},T=O=>{if(O.key==="ArrowDown"||O.key==="ArrowUp"){if(O.preventDefault(),!E.length)return;let X=O.key==="ArrowDown"?1:-1;$(_=>(_+X+E.length)%E.length)}else O.key==="Enter"&&(O.preventDefault(),E[P]&&U(E[P]))},L=O=>{if(O.key==="Escape")O.preventDefault(),O.stopPropagation(),t();else if(O.key==="Tab"){let X=[...x.current.querySelectorAll(uw)];if(!X.length)return;let _=X.indexOf(document.activeElement),j=O.shiftKey?_<=0?X.length-1:_-1:_===X.length-1?0:_+1;O.preventDefault(),X[j].focus()}},H=r?`Edit slider ${e.name}`:"Add slider";return s("div",{class:"dialog-backdrop",children:s("div",{class:"dialog",ref:x,role:"dialog","aria-modal":"true","aria-labelledby":"slider-dialog-title",onKeyDown:L,children:[s("div",{class:"dialog-title-row",children:[s("h2",{id:"slider-dialog-title",children:H}),s(st,{target:Ue.valuesPanel,label:H})]}),s("label",{class:"slider-picker-search",children:[s("span",{class:"visually-hidden",children:"Search parameters and variables"}),s("input",{type:"text",placeholder:"Search parameters and variables",value:d,"data-autofocus":"",onInput:O=>{c(O.target.value),$(0)},onKeyDown:T})]}),s("ul",{class:"slider-picker-list",role:"listbox","aria-label":"Parameters and variables",tabIndex:-1,onKeyDown:T,children:[E.length===0&&s("li",{class:"slider-picker-empty muted",children:"No match"}),E.map((O,X)=>{let _=Lr(O),j=p&&Lr(p)===_;return s("li",{role:"option","aria-selected":!!j,class:"slider-picker-item"+(X===P?" active":"")+(j?" picked":""),onMouseEnter:()=>$(X),onClick:()=>U(O),children:[s("span",{class:"slider-picker-name",children:O.name}),s("span",{class:"slider-picker-value muted",children:je(O.value)})]},_)})]}),p&&s("p",{class:"slider-picker-picked",children:["Picked: ",s("b",{children:p.name})," (currently ",je(p.value),")"]}),s("div",{class:"form-grid slider-dialog-fields",children:[s("label",{children:[s("span",{children:"Min"}),s("input",{type:"number",inputMode:"decimal",value:u,"aria-invalid":R.lo?"true":void 0,"aria-describedby":R.lo?"slider-lo-err":void 0,onInput:O=>f(O.target.value)}),R.lo&&s("p",{class:"field-error",id:"slider-lo-err",role:"alert",children:R.lo})]}),s("label",{children:[s("span",{children:"Max"}),s("input",{type:"number",inputMode:"decimal",value:w,"aria-invalid":R.hi?"true":void 0,"aria-describedby":R.hi?"slider-hi-err":void 0,onInput:O=>k(O.target.value)}),R.hi&&s("p",{class:"field-error",id:"slider-hi-err",role:"alert",children:R.hi})]}),s("label",{children:[s("span",{children:"Step"}),s("input",{type:"number",inputMode:"decimal",min:"0",value:A,"aria-invalid":R.step?"true":void 0,"aria-describedby":R.step?"slider-step-err":void 0,onInput:O=>C(O.target.value)}),R.step&&s("p",{class:"field-error",id:"slider-step-err",role:"alert",children:R.step})]})]}),s("div",{class:"dialog-actions",children:[s("button",{type:"button",onClick:t,children:"Cancel"}),s("button",{type:"button",class:"primary",disabled:!N,onClick:v,children:"OK"})]})]})})}var pw=400;function hw({def:e,index:t,onEdit:n}){let o=te(),i=M(v=>v.core?.pars),r=M(v=>v.core?.ics),a=M(v=>v.values.queue),d=Ye(()=>[...i??[],...r??[]].map(([v])=>v),[i,r]).find(v=>v.toLowerCase()===e.name.toLowerCase())??"",c=i?.some(([v])=>v.toLowerCase()===d.toLowerCase())?"par":"ic",p=d?[...i??[],...r??[]].find(([v])=>v.toLowerCase()===d.toLowerCase())?.[1]:void 0,m=d?a.find(v=>v.name!==void 0&&zt(v.kind,v.name)===zt(c,d)):void 0,u=m?Number(m.text):p,f=np(e),w=tp(e),k=J(null),A=J(null),C=J(null),[P,$]=ie(null),E=J(null);G(()=>{k.current=null},[d]),G(()=>()=>{E.current&&clearTimeout(E.current)},[]);let U=v=>{if(!d)return;let T=o.store.getState();v===k.current&&!m||(T.busy||(k.current=v),o.slide(c,d,v))},R=v=>{E.current&&(clearTimeout(E.current),E.current=null);let T=Number(v);if(!d||v.trim()===""||!Number.isFinite(T))return;let L=f?Math.max(Math.min(f.lo,f.hi),Math.min(Math.max(f.lo,f.hi),T)):T,H=C.current;H!==null&&L===Number(H)||(U(L),H!==null&&o.recordEdit({kind:c,name:d,previous:H}),C.current=String(L))},N=e.name||`Slider ${t+1}`,x=v=>`slider-${v}-${e.id}`;return s("div",{class:"slider-card"+(m?" queued":""),"data-slider":e.id,children:[s("div",{class:"slider-card-head",children:[s("span",{class:"slider-card-name",title:N,children:N}),s("label",{class:"visually-hidden",htmlFor:x("val"),children:[N,": value"]}),s("input",{id:x("val"),class:"slider-card-value",type:"number",inputMode:"decimal",disabled:!d,step:w??"any",min:f?Math.min(f.lo,f.hi):void 0,max:f?Math.max(f.lo,f.hi):void 0,title:"The value: type or spin it to set it (sends after a short pause, or at once on Enter/blur)",value:P??(u!==void 0?je(u):""),onFocus:()=>{$(p!==void 0?String(p):""),C.current=p!==void 0?String(p):null},onInput:v=>{let T=v.target.value;$(T),E.current&&clearTimeout(E.current),E.current=setTimeout(()=>{E.current=null,R(T)},pw)},onKeyDown:v=>{v.key==="Enter"?(R(v.target.value),v.target.blur()):v.key==="Escape"&&(v.stopPropagation(),E.current&&clearTimeout(E.current),$(null))},onBlur:v=>{R(v.target.value),$(null)}}),s("button",{class:"icon slider-card-edit","aria-label":`Edit slider ${N}`,title:"Change the parameter or variable, min, max or step",onClick:n,children:"\u270E"}),s("button",{class:"icon slider-card-remove","aria-label":`Remove slider ${N}`,title:"Remove this slider",onClick:()=>o.store.dispatch({type:"values",action:{type:"removeSlider",id:e.id}}),children:"\u2715"})]}),s("div",{class:"slider-card-track",children:[s("span",{class:"slider-card-lim muted",children:f?je(Math.min(f.lo,f.hi)):""}),s("label",{class:"visually-hidden",htmlFor:x("range"),children:[N,": drag to change the value"]}),s("input",{type:"range",id:x("range"),class:"slider-card-range",min:0,max:So,step:1,value:u!==void 0&&f?op(u,f):So/2,disabled:!d||!f,"aria-label":`${N} slider`,"aria-valuetext":u!==void 0?je(u):void 0,title:"Drag, or use the arrow keys, to change the value and integrate again",onInput:v=>{!d||!f||(A.current===null&&(A.current=p!==void 0?String(p):null),U(as(Number(v.target.value),f)))},onChange:v=>{d&&f&&U(as(Number(v.target.value),f)),d&&A.current!==null&&o.recordEdit({kind:c,name:d,previous:A.current}),A.current=null}}),s("span",{class:"slider-card-lim muted",children:f?je(Math.max(f.lo,f.hi)):""})]})]})}function gm(){let e=M(i=>i.values.sliders),t=M(i=>!!i.core),[n,o]=ie(null);return t?s(ve,{children:[s("section",{class:"slider-strip","aria-label":"Sliders",children:[e.map((i,r)=>s(hw,{def:i,index:r,onEdit:()=>o(i)},i.id)),s("button",{class:"small slider-add",onClick:()=>o("new"),title:"Add a slider on a parameter or variable",children:"Add slider"})]}),n!==null&&s(fm,{target:n,onClose:()=>o(null)},n==="new"?"new":n.id)]}):null}var vm="A number, or %formula such as %2*pi",mw='button:not([disabled]), input, select, [tabindex]:not([tabindex="-1"])',fw="Go runs from Initial; Last copies Now into Initial, then runs.",wm="xpp.values.folded";function bm(){try{let e=JSON.parse(localStorage.getItem(wm)??"[]");return Array.isArray(e)?e.filter(t=>typeof t=="string"):[]}catch{return[]}}function gw(e){let[t,n]=ie(()=>bm().includes(e));return[t,()=>{let i=!t;n(i);try{let r=bm().filter(a=>a!==e);localStorage.setItem(wm,JSON.stringify(i?[...r,e]:r))}catch{}}]}function Bs({id:e,title:t,hint:n,tools:o,children:i}){let[r,a]=gw(e),l=`values-sec-${e}`;return s("section",{class:"value-group"+(r?" folded":""),"aria-label":t,"data-section":e,children:[s("div",{class:"value-group-head",children:[s("h3",{children:s("button",{class:"value-fold","aria-expanded":!r,"aria-controls":l,onClick:a,title:n??`Show or hide ${t.toLowerCase()}`,children:[s("span",{class:"value-fold-mark","aria-hidden":"true",children:r?"\u25B8":"\u25BE"}),t]})}),s(st,{target:Ue.valuesPanel,label:t})]}),s("div",{id:l,hidden:r,children:[n&&s("p",{class:"value-hint",children:n}),o&&s("div",{class:"value-tools",children:o}),i]})]})}function js({kind:e,label:t,name:n,index:o,display:i,full:r,hint:a,numeric:l,extra:d}){let c=te(),p=zt(e,o??n),m=M(T=>T.values.errors[p]),u=M(T=>dp(T.values.queue,p)),f=M(T=>e==="par"||e==="ic"?T.values.defaults?.[p]??null:null),[w,k]=ie(!1),[A,C]=ie(i);G(()=>{w||C(i)},[i,w]);let P=`value-${p}`.replace(/[^\w-]/g,"_"),$=m?`${P}-err`:void 0,E=J(!1),U=J(r),R=()=>C(i),N=()=>{if(k(!1),E.current){E.current=!1,R();return}let T=A.trim();if(T===""||T===U.current){R();return}if(l&&!T.startsWith("%")&&!Number.isFinite(Number(T))){R();return}o!==void 0?c.setValueByIndex(e,o,T,r):c.setValue(e,n,T,r)},x=f!==null&&Number(r)!==f,v=u?"Sent when the running command ends":f!==null?`${a}; default: ${je(f)}`:a;return s("div",{class:"value-field"+(u?" queued":"")+(x?" changed":""),children:[s("label",{htmlFor:P,class:"value-name",title:t,children:t}),s("input",{id:P,value:w?A:i,title:v,spellcheck:!1,autocomplete:"off","aria-invalid":m?"true":void 0,"aria-describedby":$,"data-queued":u?"1":void 0,onFocus:()=>{k(!0),C(r),U.current=r},onInput:T=>C(T.target.value),onBlur:N,onKeyDown:T=>{T.key==="Enter"?(T.preventDefault(),T.target.blur()):T.key==="Escape"&&(T.stopPropagation(),E.current=!0,T.target.blur())}}),d,f!==null&&n!==void 0&&s("button",{class:"value-reset icon",title:`default: ${je(f)}`,disabled:!x,"aria-label":`Reset ${t} to its default, ${je(f)}`,onClick:()=>c.resetValue(e,n,r),children:"\u21BA"}),m&&s("p",{class:"field-error",id:$,role:"alert",children:m})]})}function Ws(e){return{display:typeof e=="number"?je(e):e,full:typeof e=="number"?String(e):e}}function xm({kind:e}){let t=te(),n=J(null),o=e==="par"?"parameters":"initial conditions";return s(ve,{children:[s("button",{class:"small",onClick:()=>t.saveValues(e),title:`Save the ${o} as a file XPP reads (${e==="par"?"File/Read par":"Initialconds/File, -icfile"})`,children:"Save"}),s("button",{class:"small",onClick:()=>n.current?.click(),title:`Load ${o} from a saved file (XPP's, or "name value" lines)`,children:"Load"}),s("input",{ref:n,id:`values-load-${e}`,type:"file",hidden:!0,onChange:async i=>{let r=i.target,a=r.files?.[0];a&&t.loadValues(e,await a.text()),r.value=""}})]})}function bw(){let e=te(),t=M(n=>n.core?.pars);return t?.length?s(Bs,{id:"par",title:"Parameters",tools:s(ve,{children:[s(xm,{kind:"par"}),s("button",{class:"small",onClick:()=>e.defaultValues("par"),title:"Every parameter from the ODE file",children:"Reset all"})]}),children:s("div",{class:"value-list",children:t.map(([n,o])=>s(js,{kind:"par",label:n,name:n,hint:vm,numeric:!0,...Ws(o)},n.toLowerCase()))})}):null}function yw(){let e=M(i=>i.core?.ics),t=M(i=>i.core?.now),n=M(i=>i.busy),o=M(i=>i.busy?i.plots.windows.find(r=>r.win===i.plots.active)?.series??null:null);return(e??[]).map(([i],r)=>{let a=n&&o&&o.rows>0?o.columns.get(r+1):void 0;return a&&o.names.get(r+1)?.toLowerCase()===i.toLowerCase()?a[o.rows-1]:t?.[r]??null})}function vw(){let e=te(),t=M(r=>r.core?.ics),n=M(r=>!!r.core?.now),o=M(r=>r.busy),i=yw();return t?.length?s(Bs,{id:"ic",title:"State",hint:fw,tools:s(ve,{children:[s("button",{class:"small",disabled:o||!n,onClick:()=>e.useCurrentState(),title:"Copy Now into Initial (as Initialconds/Last does), without running",children:"\u2190 Use current state"}),s(xm,{kind:"ic"}),s("button",{class:"small",onClick:()=>e.defaultValues("ic"),title:"Every initial condition from the ODE file",children:"Reset all"})]}),children:[s("div",{class:"value-cols","aria-hidden":"true",children:[s("span",{}),s("span",{children:"Initial"}),s("span",{children:"Now"})]}),s("div",{class:"value-list value-state",children:t.map(([r,a],l)=>s(js,{kind:"ic",label:r,name:r,hint:vm,numeric:!0,...Ws(a),extra:s("output",{class:"value-now"+(i[l]===null?" none":""),"data-name":r,"aria-label":`${r} now`,title:i[l]===null?"No run yet":`Now: ${i[l]}`,children:i[l]===null?"\u2013":je(i[l])})},r.toLowerCase()))})]}):null}function ym({id:e,title:t,kind:n,entries:o,hint:i}){return o.length?s(Bs,{id:e,title:t,children:s("div",{class:"value-list",children:o.map(([,r],a)=>s(js,{kind:n,label:`${t} ${a+1}`,index:a,hint:i,numeric:!1,...Ws(r)},a))})}):null}function ww(){let e=te(),t=M(n=>n.hello?.userbuttons??[]);return t.length?s("section",{class:"value-group","aria-label":"Buttons",children:s("div",{class:"value-list value-buttons",children:t.map((n,o)=>s("button",{onClick:()=>e.userButton(o),title:"A button defined in the ODE file",children:n},o))})}):null}function xw({session:e}){let t=M(n=>n.values.runOnChange);return s("label",{class:"value-runs",title:"Integrate again after a parameter or initial condition changes (like a slider)",children:[s("input",{type:"checkbox",checked:t,onChange:n=>e.setRunOnChange(n.target.checked)}),"Run on change"]})}function km(){let e=te(),t=M(p=>p.valuesOpen),n=M(p=>p.values.history),o=M(p=>p.values.queue.length),i=M(p=>p.core?.bcs??[]),r=M(p=>p.core?.delays??[]),a=J(null),l=()=>e.store.dispatch({type:"valuesPanel",open:!1});G(()=>{if(!t){a.current?.contains(document.activeElement)&&document.querySelector(".values-toggle")?.focus();return}a.current?.querySelector(mw)?.focus();let p=m=>{m.key!=="Escape"||e.store.getState().ask||(m.preventDefault(),m.stopPropagation(),l())};return window.addEventListener("keydown",p,!0),()=>window.removeEventListener("keydown",p,!0)},[t]);let d=n[n.length-1],c=d?`Undo: restore ${d.name??`${d.kind} ${(d.index??0)+1}`}`:"Nothing to undo";return s("section",{id:"values-panel",ref:a,class:"values-panel"+(t?" open":""),"aria-label":"Values",onKeyDown:p=>{(p.ctrlKey||p.metaKey)&&(p.key==="z"||p.key==="Z")&&(p.preventDefault(),e.undoValue())},children:[s("div",{class:"values-header",children:[s("button",{class:"values-back",onClick:l,children:"Back"}),s("h2",{children:"Values"}),s(xw,{session:e}),s("button",{class:"small",disabled:!n.length,title:c,onClick:()=>e.undoValue(),children:"Undo"})]}),o>0&&s("p",{class:"values-queued",role:"status",children:[o===1?"1 change waits":`${o} changes wait`," for the running command to end."]}),s("div",{class:"values-body",children:[s(ww,{}),s(bw,{}),s(vw,{}),s(ym,{id:"bc",title:"Boundary conditions",kind:"bc",entries:i,hint:"An expression that is zero at the boundary"}),s(ym,{id:"delay",title:"Delay initial data",kind:"delay",entries:r,hint:"An expression in t for t < 0"})]})]})}function kw(){let e=M(o=>o.connected),t=M(o=>o.exited),n=M(o=>o.hello);return t!==null?s("div",{class:"banner error",role:"alert",children:["XPP has stopped",t?" with an error":"",". Messages below show what it printed; start it again to continue."]}):!e&&n?s("div",{class:"banner",role:"status",children:"Connection lost. Reconnecting\u2026"}):null}function Tw(){let e=M(n=>n.theme),t=Pr(e);return s("div",{class:"shell",children:[s("a",{class:"skip-link",href:"#main",children:"Skip to the plot"}),s(hm,{}),s(Zh,{}),s("main",{id:"main",class:"workspace",children:[s(kw,{}),s(sm,{dark:t}),s(gm,{}),s(Qh,{})]}),s(km,{}),s(cm,{}),s(um,{}),s(Vh,{dark:t}),s(ih,{}),s(dh,{}),s(Yh,{}),s(lm,{}),s(mm,{}),s(yh,{})]})}function Tm({session:e}){return Gh(e),s(Ts.Provider,{value:e,children:s(Tw,{})})}var Or=new br(new Li,new Di);Or.store.dispatch({type:"theme",theme:lh()});Hp(Or);Hl(s(Tm,{session:Or}),document.getElementById("app"));Or.start();})();
