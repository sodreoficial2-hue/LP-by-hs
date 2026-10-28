// Exercise the production render function with a minimal DOM test double.
// Browser/WebView2 integration is a separate validation step.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../vision-ui/vision.js'),'utf8');
const renderSource=source.slice(source.indexOf(' function render(){'),source.indexOf(' function receive('));
function fixture(id){
 const elements=new Map();
 const document={activeElement:null,querySelector:()=>elements.get(id),querySelectorAll:()=>[]};
 function element(name,value=''){
  return {value,selectionStart:0,selectionEnd:0,selectionDirection:'none',scrollTop:87,
   getAttribute:()=>name,matches:()=>name===id,
   focus(){document.activeElement=this;},
   setSelectionRange(start,end,direction){this.selectionStart=start;this.selectionEnd=end;this.selectionDirection=direction;},
   setAttribute(){},classList:{toggle(){}},
   replaceChildren(){elements.set(id,element(id,'stale server text'));document.activeElement=null;}};
 }
 const $=name=>{if(!elements.has(name))elements.set(name,element(name));return elements.get(name);};
 const input=$(id);input.focus();
 const state={screen:id==='sound-search'?'instruments':'performance',effects:false,category:0,catalog:[],controls:[],audio:{drivers:[],midiInputs:[],latencyMs:0},performance:{instruments:[],instrumentCategories:[],parts:[]}};
 const context={document,$,state,structure:'',CSS:{escape:x=>x},instruments(){},performance(){},home(){},audio(){},profiles(){},keyboard(){},parameters(){},title(){},node(){}};
 vm.createContext(context);vm.runInContext(renderSource,context);
 return {context,$,document,state};
}
for(const id of ['sound-search','part-search-0','part-search-1','part-search-2','part-search-3','performance-name']){
 const f=fixture(id);
 // Every character forces the same rebuild that filtering causes in production.
 for(const character of 'Piano elétrico'){
  const old=f.document.activeElement,at=old.selectionStart;
  old.value=old.value.slice(0,at)+character+old.value.slice(old.selectionEnd);
  old.setSelectionRange(at+1,at+1,'none');f.context.structure='';f.context.render();
 }
 assert.equal(f.document.activeElement.value,'Piano elétrico',id);
 assert.equal(f.document.activeElement.selectionStart,14,id);
 const input=f.document.activeElement;input.setSelectionRange(2,8,'backward');
 f.context.structure='';f.context.render();
 assert.equal(f.document.activeElement.value,'Piano elétrico');
 assert.equal(f.document.activeElement.selectionStart,2);
 assert.equal(f.document.activeElement.selectionEnd,8);
 assert.equal(f.document.activeElement.selectionDirection,'backward');
 assert.equal(f.$('content').scrollTop,87);
 // A state update with no structural change must also preserve the caret.
 f.context.render();assert.equal(f.document.activeElement.selectionStart,2);
 // Clearing to zero results and typing again must keep the search focused.
 f.document.activeElement.value='';f.document.activeElement.setSelectionRange(0,0,'none');
 f.state.catalog=[];f.context.structure='';f.context.render();
 assert.equal(f.document.activeElement.value,'');assert.equal(f.document.activeElement.selectionStart,0);
}
console.log('PASS: production render preserves text, caret, backward selections, empty searches and scroll in catalog and four Performance searches');
