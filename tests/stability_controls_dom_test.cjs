const {JSDOM}=require(process.env.LP_JSDOM_MODULE||'jsdom'),fs=require('fs'),path=require('path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),w=new JSDOM(fs.readFileSync(root+'/vision-ui/index.html','utf8'),{runScripts:'outside-only',pretendToBeVisual:true}).window;
w.matchMedia=()=>({matches:true});w.CSS={escape:x=>x};w.HTMLElement.prototype.scrollIntoView=function(){};w.HTMLElement.prototype.scrollBy=function(){};
const fixture=fs.readFileSync(__dirname+'/vision_ui_test.cjs','utf8').split('await page.addInitScript(()=>{')[1].split('\n });\n await page.goto')[0];w.eval(fixture);w.eval(fs.readFileSync(root+'/vision-ui/vision.js','utf8'));w.__setLoading(false);w.document.getElementById('opening').click();
const send=(a,x=0,y=0,t='')=>w.LPVision.send(a,x,y,t),key=(target,name,extra={})=>target.dispatchEvent(new w.KeyboardEvent('keydown',{key:name,bubbles:true,cancelable:true,...extra}));
setTimeout(()=>{try{
 send('screen',2);const search=w.document.getElementById('part-search-0');search.focus();key(search,'s',{ctrlKey:true});assert.equal(w.__commands.at(-1).action,'save');
 const third=w.document.getElementById('part-2');third.focus();key(third,'Delete');assert.equal(w.__commands.at(-1).action,'clearPart');assert.equal(w.__commands.at(-1).a,2);
 w.document.getElementById('back').focus();key(w.document.getElementById('back'),'ArrowLeft');assert.equal(w.__commands.at(-1).action,'masterStep');assert.equal(w.__commands.at(-1).a,0);
 let state=JSON.parse(JSON.stringify(w.LPVision.state));state.performance.parts=[0,0,0,0];state.performance.instrumentVisible=[true,false,true,true];w.LPVision.receive(state);assert.equal(w.document.querySelector('#part-0 option[value="1"]'),null);
 const instruments=w.LPVision.state.performance.instruments;w.LPVision.receive({type:'telemetry',version:1,request:500,error:'',peak:.95,clipping:false,status:'Ready',loadedName:'Piano',loadedCatalogIndex:1,selected:1,currentFavorite:-1,preparing:false,performanceName:'Draft name',volumes:[450,1000,1000,1000],controls:w.LPVision.state.controls,diagnostics:{cpu:12,xruns:2,frames:128,synths:4,memoryMB:150}});
 assert.equal(w.LPVision.state.performance.instruments,instruments);assert.equal(w.LPVision.state.performance.volumes[0],450);assert(w.document.getElementById('peak-label').textContent.includes('Pouca folga'));
 send('screen',3);state=JSON.parse(JSON.stringify(w.LPVision.state));state.diagnostics={cpu:12,xruns:2,frames:128,synths:4,memoryMB:150};w.LPVision.receive(state);assert(w.document.getElementById('audio-diagnostics').textContent.includes('Interrupções: 2'));
 console.log('PASS: production Ctrl+S, correct-layer Delete, master arrows, hidden-choice filtering, telemetry merge, headroom and diagnostics');
}finally{w.close();}},30);
