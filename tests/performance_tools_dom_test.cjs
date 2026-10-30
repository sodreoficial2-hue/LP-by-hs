// Full production renderer with a simulated native bridge. No audio/layout claim.
const {JSDOM}=require(process.env.LP_JSDOM_MODULE||'jsdom'),fs=require('fs'),path=require('path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');const w=new JSDOM(fs.readFileSync(root+'/vision-ui/index.html','utf8'),{runScripts:'outside-only',pretendToBeVisual:true}).window;
w.matchMedia=()=>({matches:true});w.CSS={escape:x=>x};w.HTMLElement.prototype.scrollIntoView=function(){};w.HTMLElement.prototype.scrollBy=function(){};
const errors=[];w.addEventListener('error',e=>errors.push(String(e.error)));
const fixture=fs.readFileSync(__dirname+'/vision_ui_test.cjs','utf8').split('await page.addInitScript(()=>{')[1].split('\n });\n await page.goto')[0];w.eval(fixture);
const baseBridge=w.chrome.webview.postMessage;const clone=o=>JSON.parse(JSON.stringify(o));let savedWorkspace;
w.chrome.webview.postMessage=wire=>{
 const [,id,action,aa,bb,text]=wire.split('\t'),a=Number(aa),b=Number(bb),p=w.LPVision?.state.performance;
 if(['partVolume','partSolo','partMute','partCategory','partSearch','copyPart','swapParts','learnLow','learnHigh','performanceSaveNew'].includes(action)){
  const s=clone(w.LPVision.state);s.request=Number(id);const p=s.performance;
  if(action==='partVolume')p.volumes[a]=b;if(action==='partSolo')p.solo=p.solo===a?-1:a;if(action==='partMute')p.muted[a]=!p.muted[a];if(action==='partCategory')p.browseCategories[a]=b;if(action==='partSearch')p.searches[a]=decodeURIComponent(text);if(action==='copyPart'){p.parts[b]=p.parts[a];p.volumes[b]=p.volumes[a];}if(action==='swapParts')[p.parts[a],p.parts[b]]=[p.parts[b],p.parts[a]];if(action==='learnLow')p.splitLearning=a*2;if(action==='learnHigh')p.splitLearning=a*2+1;
  w.__commands.push({action,a,b,text:decodeURIComponent(text)});savedWorkspace=s;w.LPVision.receive(s);return;
 }
 baseBridge(wire);
};
w.eval(fs.readFileSync(root+'/vision-ui/vision.js','utf8'));w.__setLoading(false);w.document.getElementById('opening').click();
const send=(a,x=0,y=0,t='')=>w.LPVision.send(a,x,y,t);
setTimeout(()=>{try{
 send('screen',2);let s=clone(w.LPVision.state);Object.assign(s.performance,{parts:[1,2,0,0],volumes:[1000,800,1000,1000],muted:[false,false,false,false],solo:-1,browseCategories:[0,1,0,0],searches:['','', '', ''],zones:['Dó-1 até Sol9','Dó3 até Dó6','Todo o teclado','Todo o teclado'],splitLearning:-1,editingSaved:true});w.LPVision.receive(s);
 const button=(name,row=0)=>[...w.document.querySelector(`[data-part="${row}"]`).querySelectorAll('button')].find(b=>b.textContent===name);
 let volume=w.document.getElementById('part-volume-0');volume.value='450';volume.dispatchEvent(new w.Event('input'));assert.equal(w.LPVision.state.performance.volumes[0],450);
 button('Ouvir somente esta camada').click();assert.equal(w.LPVision.state.performance.solo,0);button('Ouvir todas').click();assert.equal(w.LPVision.state.performance.solo,-1);
 button('Silenciar camada',1).click();assert.equal(w.LPVision.state.performance.muted[1],true);
 const target=w.document.getElementById('part-target-0');target.value='2';target.dispatchEvent(new w.Event('change'));button('Copiar para o destino').click();assert.equal(w.LPVision.state.performance.parts[2],1);
 button('Aprender início pelo MIDI',1).click();assert.equal(w.LPVision.state.performance.splitLearning,2);
 const search=w.document.getElementById('part-search-0');search.value='Grand';search.dispatchEvent(new w.Event('input'));assert.equal(w.LPVision.state.performance.searches[0],'Grand');
 // Re-enter with the native saved workspace and verify per-layer filters/search.
 const restored=clone(w.LPVision.state);send('screen',0);restored.screen='performance';w.LPVision.receive(restored);assert.equal(w.document.getElementById('part-search-0').value,'Grand');assert.equal(w.document.getElementById('part-category-1').value,'1');
 assert([...w.document.querySelectorAll('button')].some(b=>b.textContent==='Salvar alterações'));assert([...w.document.querySelectorAll('button')].some(b=>b.textContent==='Salvar como nova'));assert.equal(errors.length,0);
 console.log('PASS: complete renderer volume, solo/mute, copy destination, split Learn, per-layer search recovery and distinct edit/save-as-new controls');
 }finally{w.close();}},30);
