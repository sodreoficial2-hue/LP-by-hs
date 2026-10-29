const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../vision-ui/vision.js'),'utf8');
const render=source.slice(source.indexOf(' function instruments(){'),source.indexOf(' function audio(){'));
function fixture(category=0,restoring=false){
 const all=[],sent=[];
 function node(tag,attrs={},...children){const el={tag,attrs,children,events:{},append(...c){this.children.push(...c);},addEventListener(k,fn){this.events[k]=fn;},querySelector(tag){return all.find(x=>x.tag===tag);}};all.push(el);return el;}
 const context={state:{category,restoring,categories:['Piano'],catalog:[{id:7,name:'Concert Grand'}],favorites:[{id:0,name:'Piano favorito',occupied:true}]},title(){},node,button(label,click,attrs){const e=node('button',attrs,label);e.events.click=click;return e;},crop(){return node('svg');},art(){return node('img');},send(...c){sent.push(c);},document:{querySelector(){return null;}}};
 vm.createContext(context);vm.runInContext(render,context);context.instruments();return {all,sent};
}
function event(key){return {key,shiftKey:true,preventDefault(){this.prevented=true;},stopPropagation(){this.stopped=true;}};}
let f=fixture();let row=f.all.find(x=>x.attrs?.['data-row']===7),choose=f.all.find(x=>x.attrs?.['data-select']===7);
let e=event();row.events.contextmenu(e);assert(e.prevented);assert.deepEqual(f.sent,[['soundMenu',7]]);
e=event('F10');choose.events.keydown(e);assert(e.prevented&&e.stopped);assert.deepEqual(f.sent[1],['soundMenu',7]);
f=fixture(15);row=f.all.find(x=>x.attrs?.['data-row']===0);row.events.contextmenu(event());assert.deepEqual(f.sent,[['learnFavorite',0]]);
f=fixture(0,true);row=f.all.find(x=>x.attrs?.['data-row']===7);assert.equal(row.events.contextmenu,undefined);
console.log('PASS: production category context menu, keyboard access, favorite Program Change Learn and restore isolation');
