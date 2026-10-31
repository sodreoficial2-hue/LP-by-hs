const {JSDOM}=require('jsdom'),fs=require('fs'),path=require('path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),w=new JSDOM(fs.readFileSync(root+'/vision-ui/index.html','utf8'),{runScripts:'outside-only',pretendToBeVisual:true}).window;
w.matchMedia=()=>({matches:true});w.CSS={escape:x=>x};w.HTMLElement.prototype.scrollIntoView=function(){};w.HTMLElement.prototype.scrollBy=function(){};
const fixture=fs.readFileSync(__dirname+'/vision_ui_test.cjs','utf8').split('await page.addInitScript(()=>{')[1].split('\n });\n await page.goto')[0];w.eval(fixture);w.eval(fs.readFileSync(root+'/vision-ui/vision.js','utf8'));w.__setLoading(false);w.document.getElementById('opening').click();
setTimeout(()=>{try{
 const doc=w.document,key=(target,name,extra={})=>target.dispatchEvent(new w.KeyboardEvent('keydown',{key:name,bubbles:true,cancelable:true,...extra}));
 let state=JSON.parse(JSON.stringify(w.LPVision.state));state.master=.42;state.masterCC=11;w.LPVision.receive(state);
 assert.equal(doc.getElementById('master-volume').value,'420');assert.equal(doc.getElementById('master-value').textContent,'42%');
 const cards=[...doc.querySelectorAll('[data-category]')];cards[0].focus();key(cards[0],'ArrowRight');assert.equal(w.__commands.at(-1).action,'masterStep');assert.equal(w.__commands.at(-1).a,1);
 key(cards[0],'ArrowDown');assert.equal(doc.activeElement,cards[1]);
 const range=doc.getElementById('master-volume');range.value='610';range.dispatchEvent(new w.Event('input',{bubbles:true}));assert.equal(w.__commands.at(-1).action,'master');assert.equal(w.__commands.at(-1).a,610);
 doc.getElementById('master-learn').click();assert.equal(w.__commands.at(-1).action,'learnMaster');
 w.LPVision.send('category',0);w.LPVision.send('select',0);w.LPVision.send('effects');assert.equal(doc.querySelector('[data-parameter="15"] h2').textContent,'Phaser');assert.equal(doc.querySelectorAll('.parameter').length,16);assert(doc.getElementById('master-volume'));
 w.LPVision.send('screen',2);const select=doc.getElementById('part-0');let count=w.__commands.length;key(select,'ArrowRight');assert.equal(w.__commands.length,count,'MIDI/sound dropdown navigation must remain local');
 console.log('PASS: home master arrows, category navigation, master mouse/MIDI, persisted display, Phaser replacement and dropdown focus');
}finally{w.close();}},30);
