// Renderer contract and offline/accessibility checks. This fixture is NOT an audio host.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const http=require('node:http');
let browser,server;
const path=require('node:path');
const {chromium}=require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES?path.join(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES,'playwright'):'playwright');
const output=process.env.LP_VISION_TEST_OUTPUT||path.resolve(__dirname,'../../vision-analysis/ui-validation');
fs.mkdirSync(output,{recursive:true});
(async()=>{
 server=http.createServer((req,res)=>{const name=decodeURIComponent(req.url.split('?')[0]);const file=path.join(__dirname,'../vision-ui',name==='/'?'index.html':name);if(!file.startsWith(path.resolve(__dirname,'../vision-ui')+path.sep)){res.writeHead(403);return res.end();}fs.readFile(file,(err,data)=>{if(err){res.writeHead(404);return res.end();}res.setHeader('Content-Type',file.endsWith('.css')?'text/css':file.endsWith('.js')?'application/javascript':'text/html');res.end(data);});});await new Promise(resolve=>server.listen(8766,'127.0.0.1',resolve));
 browser=await chromium.launch({headless:true,executablePath:process.env.LP_TEST_CHROMIUM,args:['--no-sandbox','--single-process','--no-zygote','--disable-gpu','--in-process-gpu']});
 const page=await browser.newPage({viewport:{width:1360,height:900},reducedMotion:'reduce'});
 const failures=[],external=[];page.on('pageerror',e=>failures.push(e.message));
 await page.route('**/*',r=>{const u=r.request().url();if(!u.startsWith('http://127.0.0.1:8766')&&!u.startsWith('data:')){external.push(u);return r.abort();}return r.continue();});
 await page.addInitScript(()=>{
  const categories=['Pianos acústicos','Pianos elétricos','Órgãos','Cordas','Metais','Leads','Pads','Percussão','Bateria','Acordeons','Outros','Contrabaixos','Guitarras','Performance','Meus Sons','Favoritos'];
  const parameters=['Volume','Attack','Decay','Sustain','Release','Filtro','Reverb','Delay','Chorus','Tremolo','Compressor','Grave','Médio','Agudo','Glide','Master'];
  let receiver=null;window.__commands=[];
  const state={type:'state',version:1,request:0,error:'',screen:'home',effects:false,loading:true,progress:35,status:'ASIO desconectado. Escolha um driver.',loadedName:'',category:0,selected:0,categories,
   catalog:[{id:0,name:'Concert Grand',favorite:false,loaded:false},{id:1,name:'Piano "Soul" <b>Seguro</b>',favorite:false,loaded:false}],
   favorites:Array.from({length:20},(_,id)=>({id,name:id===0?'Concert Grand':id===19?'Rhodes':'Vazio',occupied:id===0||id===19,loaded:false})),
   controls:parameters.map((name,id)=>({id,name,label:name+': 50%',value:500,enabled:id===15})),partMix:false,
   audio:{drivers:['Selecione driver ASIO','ASIO4ALL v2','TASCAM US-1800'],driver:0,buffers:['Buffer do driver','64','128','256','512','1024','2048'],buffer:0,midiInputs:['MIDI desligado','A-PRO'],midi:0,connected:false,sampleRate:48000,latencyMs:5.3},
   performance:{mode:0,name:'Minha Performance',parts:[0,0,0,0],editingPart:0,instrumentCategories:[-1,0,1,2],modes:['Camadas: quatro timbres','Dividir 2 + 2','Dividir 3 + 1','Dividir 2 + 1 + 1','Dividir quatro partes'],instruments:['Desativado','Concert Grand','Rhodes','Órgão']},about:'Hugo Sodré\nMúsica, visão e propósito.\n\nCréditos dos instrumentos.'};
  const emit=()=>receiver?.({data:JSON.parse(JSON.stringify(state))});window.__setLoading=value=>{state.loading=value;state.progress=value?35:100;emit();};
  window.chrome={webview:{addEventListener:(_,fn)=>{receiver=fn;},postMessage:wire=>{
   const [version,id,action,aa,bb,text]=wire.split('\t'),a=Number(aa),b=Number(bb);if(version!=='LP1')throw new Error('Wrong protocol');window.__commands.push({action,a,b,text:decodeURIComponent(text)});state.request=Number(id);state.error='';
   if(action==='category'){state.category=a;state.screen='instruments';state.effects=false;state.selected=0;state.partMix=a===13;}
   if(action==='select'){if(state.category!==13)state.partMix=false;state.selected=a;state.loadedName=state.catalog[a].name;state.catalog.forEach(r=>r.loaded=r.id===a);state.controls.forEach(c=>c.enabled=true);}
   if(action==='favorite'){const r=state.catalog[a];r.favorite=!r.favorite;state.favorites[0]={id:0,name:r.favorite?r.name:'Vazio',occupied:r.favorite,loaded:r.loaded};}
   if(action==='activateFavorite'){state.selected=a;if(state.favorites[a].occupied)state.loadedName=state.favorites[a].name;}
   if(action==='removeFavorite')state.favorites[a]={id:a,name:'Vazio',occupied:false,loaded:false};
   if(action==='effects'){state.effects=!state.effects;if(!state.effects&&state.screen!=='performance')state.screen='home';}
   if(action==='escape'){state.effects=false;if(state.screen!=='performance')state.screen='home';}
   if(action==='screen'){state.screen=['home','instruments','performance','audio','profiles','about','keyboard'][a];state.effects=false;}
   if(action==='parameter'){state.controls[a].value=b;state.controls[a].label=state.controls[a].name+': '+Math.round(b/10)+'%';}
   if(action==='driver'){state.audio.driver=a;state.audio.connected=a>0;}
   if(action==='buffer')state.audio.buffer=a;
   if(action==='midi')state.audio.midi=a;
   if(action==='editPart')state.performance.editingPart=a;
   if(action==='performanceName')state.performance.name=decodeURIComponent(text);
   if(action==='performancePart')state.performance.parts[a]=b;
   if(action==='performanceMode')state.performance.mode=a;
   if(action==='disconnect')state.audio.connected=false;
   emit();
  }}};
 });
 await page.goto('http://127.0.0.1:8766',{waitUntil:'networkidle'});await page.evaluate(()=>document.fonts.ready);
 await page.setViewportSize({width:1536,height:864});await page.screenshot({path:path.join(output,'opening-1536.png')});
 assert.equal(await page.locator('#opening').isVisible(),true);assert.equal(await page.locator('main').evaluate(el=>el.inert),true);
 await page.keyboard.press('Enter');assert.equal(await page.locator('#opening').isVisible(),true,'opening cannot skip unfinished loading');
 await page.evaluate(()=>window.__setLoading(false));await page.locator('#opening').waitFor({state:'hidden'});assert.equal(await page.locator('main').evaluate(el=>el.inert),false);
 assert.equal(await page.locator('.category-card').count(),16);
 for(const size of [{width:1360,height:900},{width:1024,height:600}]){
  await page.setViewportSize(size);
  const bounds=await page.locator('.category-card').evaluateAll(cards=>cards.map(c=>{let r=c.getBoundingClientRect();return {top:r.top,bottom:r.bottom,width:r.width,height:r.height};}));
  for(const r of bounds){assert.ok(r.height>=44&&r.width>=100);assert.ok(r.top>=0&&r.bottom<=size.height-65);}
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.screenshot({path:path.join(output,`home-${size.width}.png`)});
 }
 await page.setViewportSize({width:1360,height:900});
 await page.locator('[data-category="0"]').focus();await page.keyboard.press('ArrowRight');assert.equal(await page.locator(':focus').getAttribute('data-category'),'1');
 assert.equal(await page.evaluate(()=>window.__commands.filter(c=>c.action==='select').length),0,'category arrows never load audio');
 await page.keyboard.press('Enter');assert.equal(await page.locator('.sound-row').count(),2);assert.equal(await page.locator('#loaded-name').textContent(),'Nenhum timbre carregado');
 await page.locator('[data-select="0"]').click();assert.equal(await page.locator('#loaded-name').textContent(),'Concert Grand');
 await page.keyboard.press('ArrowDown');assert.equal(await page.locator('#loaded-name').textContent(),'Piano "Soul" <b>Seguro</b>');assert.equal(await page.locator('.sound-select b').count(),0,'filenames must not inject HTML');
 await page.keyboard.press('Space');assert.equal(await page.locator('[data-star="1"]').textContent(),'★');
 await page.screenshot({path:path.join(output,'instruments.png')});
 await page.keyboard.press('e');assert.equal(await page.locator('.parameter').count(),16);
 await page.locator('#parameter-1').focus();await page.keyboard.press('ArrowRight');await page.waitForTimeout(80);
 assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='parameter'&&c.a===1&&c.b===501)));
 // Real pointer events on the painted knob, not synthetic range input only.
 const mouseKnob=async(id,dx,dy,shift=false)=>{
  const knob=page.locator(`[data-parameter="${id}"] .knob`);await knob.scrollIntoViewIfNeeded();const r=await knob.boundingBox();
  await page.mouse.move(r.x+r.width/2,r.y+r.height/2);if(shift)await page.keyboard.down('Shift');await page.mouse.down();
  await page.mouse.move(r.x+r.width/2+dx,r.y+r.height/2+dy,{steps:4});await page.mouse.up();if(shift)await page.keyboard.up('Shift');
 };
 await mouseKnob(1,0,-20);assert.equal(await page.locator('#parameter-1').inputValue(),'601');
 await mouseKnob(1,0,10,true);assert.equal(await page.locator('#parameter-1').inputValue(),'591');
 await mouseKnob(1,400,0);assert.equal(await page.locator('#parameter-1').inputValue(),'1000');
 await mouseKnob(1,-400,0);assert.equal(await page.locator('#parameter-1').inputValue(),'0');
 const knob1=page.locator('[data-parameter="1"] .knob');await knob1.hover();await page.mouse.wheel(0,-100);await page.waitForTimeout(80);assert.equal(await page.locator('#parameter-1').inputValue(),'10');
 // A controller update must repaint a focused range once mouse editing stops.
 await page.evaluate(()=>{const s=structuredClone(window.LPVision.state);s.controls[1].value=730;window.LPVision.receive(s);});assert.equal(await page.locator('#parameter-1').inputValue(),'730');assert.equal(await page.locator('#value-1').textContent(),'73%');
 await page.evaluate(()=>{const s=structuredClone(window.LPVision.state);s.controls[2].enabled=false;window.LPVision.receive(s);});
 const beforeDisabled=await page.evaluate(()=>window.__commands.filter(c=>c.action==='parameter'&&c.a===2).length);await mouseKnob(2,0,-20);assert.equal(await page.evaluate(()=>window.__commands.filter(c=>c.action==='parameter'&&c.a===2).length),beforeDisabled,'disabled knobs cannot send mouse changes');
 // Ending the panel mid-drag must not leak subsequent pointer changes to another sound.
 const interrupted=await knob1.boundingBox();await page.mouse.move(interrupted.x+40,interrupted.y+40);await page.mouse.down();await page.mouse.move(interrupted.x+40,interrupted.y+30);
 await page.keyboard.press('e');const afterClose=await page.evaluate(()=>window.__commands.filter(c=>c.action==='parameter').length);
 await page.mouse.move(interrupted.x+40,interrupted.y+10);await page.mouse.up();assert.equal(await page.evaluate(()=>window.__commands.filter(c=>c.action==='parameter').length),afterClose);
 await page.keyboard.press('e');
 await knob1.click({button:'right'});assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='learnParameter'&&c.a===1)));
 await page.locator('#parameter-1').click({button:'right'});assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='learnParameter'&&c.a===1)));
 await page.screenshot({path:path.join(output,'parameters.png')});
 await page.keyboard.press('Escape');await page.locator('[data-category="15"]').click();assert.equal(await page.locator('.sound-row').count(),20);
 await page.locator('[data-select="0"]').click({button:'right'});assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='learnFavorite'&&c.a===0)));
 await page.locator('[data-select="19"]').click({button:'right'});assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='learnFavorite'&&c.a===19)));
 await page.screenshot({path:path.join(output,'favorites.png')});
 await page.locator('[data-screen="3"]').click();await page.locator('#driver').selectOption('1');await page.locator('#midi').selectOption('1');await page.locator('#buffer').selectOption('3');
 assert.equal(await page.locator('#audio-dot').getAttribute('class'),'dot connected');
 await page.getByRole('button',{name:'ASIO Control',exact:true}).click();assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='controlPanel')));
 await page.screenshot({path:path.join(output,'audio.png')});
 await page.locator('[data-screen="2"]').click();await page.locator('#performance-name').fill('Soul — apresentação');await page.keyboard.press('e');assert.ok((await page.locator('#performance-name').inputValue()).endsWith('e'),'E must type normally in text fields');
 assert.equal(await page.locator('#favorites-nav').count(),0);assert.equal(await page.locator('[id^=part-category-]').count(),4);
 await page.locator('#part-category-0').selectOption('0');assert.equal(await page.locator('#part-0 option[value="2"]').count(),0);
 await page.locator('#part-0').selectOption('1');await page.locator('#part-category-1').selectOption('1');await page.locator('#part-1').selectOption('2');
 await page.locator('#part-1').focus();await page.keyboard.press('e');assert.equal(await page.locator('.parameter').count(),16);assert.equal(await page.locator('#heading').textContent(),'Parâmetros da parte 2');await mouseKnob(6,0,-10);assert.equal(await page.locator('#parameter-6').inputValue(),'550');await page.keyboard.press('Escape');assert.equal(await page.locator('#part-1').inputValue(),'2');await page.locator('#performance-mode').selectOption('1');await page.getByRole('button',{name:'Ativar Performance',exact:true}).click();
 assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='performancePlay')));
 await page.screenshot({path:path.join(output,'performance.png')});
 await page.locator('#brand').click();await page.locator('[data-category="13"]').click();await page.locator('[data-select="0"]').click();await page.keyboard.press('e');assert.equal(await page.locator('.parameter').count(),4);assert.equal(await page.locator('#heading').textContent(),'Mix da Performance');await mouseKnob(0,0,-10);assert.equal(await page.locator('#parameter-0').inputValue(),'550');await page.keyboard.press('Escape');
 await page.locator('[data-screen="4"]').click();await page.getByRole('button',{name:/Salvar perfil Escolha/}).click();await page.getByRole('button',{name:/Abrir perfil Carregue/}).click();assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='save')&&window.__commands.some(c=>c.action==='open')));
 await page.screenshot({path:path.join(output,'profiles.png')});
 await page.locator('#keyboard-nav').click();await page.locator('[data-note="60"]').focus();await page.keyboard.down('Space');await page.keyboard.up('Space');
 assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='noteOn'&&c.a===60)&&window.__commands.some(c=>c.action==='noteOff'&&c.a===60)));
 await page.keyboard.down('Space');await page.keyboard.press('F12');await page.keyboard.up('Space');assert.ok(await page.evaluate(()=>window.__commands.some(c=>c.action==='releaseNotes')&&window.__commands.some(c=>c.action==='panic')));
 await page.locator('[data-note="60"]').focus();await page.keyboard.down('Space');await page.keyboard.press('e');await page.keyboard.up('Space');assert.equal(await page.locator('.key.down').count(),0);assert.ok(await page.evaluate(()=>window.__commands.filter(c=>c.action==='releaseNotes').length>=2));
 await page.keyboard.press('Escape');await page.locator('#keyboard-nav').click();await page.screenshot({path:path.join(output,'keyboard.png')});
 const priorCommands=await page.evaluate(()=>window.__commands.length);await page.emulateMedia({reducedMotion:'no-preference'});await page.reload({waitUntil:'networkidle'});await page.evaluate(()=>window.__setLoading(false));const openingReadyAt=await page.evaluate(()=>performance.now());await page.locator('#opening').waitFor({state:'hidden',timeout:6000});assert.ok((await page.evaluate(()=>performance.now()))-openingReadyAt>=2300,'automatic opening retains the original display time');
 assert.deepEqual(failures,[]);assert.deepEqual(external,[],'all assets must work offline');
 assert.equal(await page.evaluate(()=>Boolean(window.__vli||window.__vliAudioEffects)),false,'old browser synthesizer must not be loaded');
 const report={result:'PASS',scope:'Chromium renderer with a control-state fixture; not Windows/ASIO hardware',checks:['original full-screen opening with title-only composition, loading gate, keyboard skip and automatic display time','16 categories at 1360×900 and 1024×600','offline original artwork and fonts','arrows/Enter and automatic sound selection commands','safe instrument names','Space favorites and 20 numbered slots','E/ESC and text entry','Mouse drag, Shift precision, wheel, bounds, disabled knobs, live controller repaint, per-part parameters, performance mix and preserved MIDI Learn','ASIO/MIDI selectors and ASIO Control command','Four independent category filters, native instrument IDs, contextual part editing with E and four-volume playback mix','native save/open command routing','virtual keyboard releases and F12'],externalRequests:external,pageErrors:failures,commands:priorCommands+await page.evaluate(()=>window.__commands.length)};
 fs.writeFileSync(path.join(output,'renderer-results.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));await browser.close();server.close();
})().catch(async e=>{console.error(e);await browser?.close();server?.close();process.exitCode=1;});
