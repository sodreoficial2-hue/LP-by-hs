const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../vision-ui/vision.js'),'utf8');
const all=[],sent=[];
function node(tag,attrs={},...children){
 const e={tag,attrs,children:[],events:{},append(...items){this.children.push(...items);},addEventListener(name,fn){(this.events[name]??=[]).push(fn);},setAttribute(k,v){this.attrs[k]=v;},querySelector(tag){return this.children.find(x=>x.tag===tag);},scrollIntoView(){},dispatch(name,event={}){for(const fn of this.events[name]||[])fn(event);}};
 if(tag==='select'){e._index=-1;Object.defineProperties(e,{options:{get(){return this.children;}},value:{get(){return String(this.children[this._index]?.attrs.value??'');},set(v){this._index=this.children.findIndex(x=>String(x.attrs.value)===String(v));}},selectedIndex:{get(){return this._index;},set(v){this._index=v;}}});}
 else if(attrs.value!==undefined)e.value=attrs.value;
 e.append(...children.flat());all.push(e);return e;
}
const context={node,state:{categories:Array.from({length:16},(_,i)=>'Categoria '+i),performance:{name:'Meu som',parts:[0,0,0,0],editingPart:0,modes:['Layer','Split'],mode:0,instrumentCategories:[-1,0,0],instruments:['Desativado','Piano','Cordas']}},send(...args){sent.push(args);},flushParameters(){},title(){},render(){},structure:'',button(label,fn,attrs={}){const e=node('button',attrs,label);e.addEventListener('click',fn);return e;}};
vm.createContext(context);vm.runInContext(source.slice(source.indexOf(' function field('),source.indexOf(' function title('))+source.slice(source.indexOf(' let partCategories='),source.indexOf(' function parameters()')),context);context.performance();
assert(!all.some(e=>e.tag==='button'&&e.children.includes('Ativar Performance')));
for(let i=0;i<4;i++){
 const select=all.find(e=>e.attrs.id===`part-${i}`);assert.equal(select.size,6);
 let prevented=false;select.dispatch('keydown',{key:'ArrowDown',preventDefault(){prevented=true;}});assert(prevented);assert.equal(select.value,'1');assert.deepEqual(sent.at(-1),['performancePart',i,1]);
 select.dispatch('keydown',{key:'ArrowDown',preventDefault(){}});assert.equal(select.value,'2');assert.deepEqual(sent.at(-1),['performancePart',i,2]);
 select.dispatch('change');assert.deepEqual(sent.at(-1),['performancePart',i,2]); // Duplicate input/change does not reload.
 select.dispatch('keydown',{key:'Home',preventDefault(){}});assert.equal(select.value,'0');assert.deepEqual(sent.at(-1),['performancePart',i,0]);
}
const save=all.find(e=>e.tag==='button'&&e.children.includes('Salvar Performance'));save.dispatch('click');assert.deepEqual(sent.at(-2),['performanceName',0,0,'Meu som']);assert.deepEqual(sent.at(-1),['performanceSave']);assert(!sent.some(c=>c[0]==='performancePlay'));
console.log('PASS: production four-layer keyboard audition, immediate selection, duplicate suppression, automatic activation and explicit Save');
