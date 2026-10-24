/* Vision Laiberry visual adaptation for the existing Live Performance host.
   This renderer has NO synthesizer, audio device, Web MIDI or profile storage. */
(() => {
 'use strict';
 const $ = id => document.getElementById(id);
 const assets = window.LPVisionAssets || {};
 const native = window.chrome?.webview;
 let state = null, serial = 0, structure = '', errorTimer = null;
 let openingVisible=true,openingClosing=false,openingSkip=false,openingTimer=null;
 const openingStarted=window.performance.now();
 const held = new Set(), pendingParameters = new Map();
 let parameterTimer = null, parameterDrag = null;
 const svgNS = 'http://www.w3.org/2000/svg';
 function mountOpening(){
  const splash=window.LPVisionSplash,host=$('opening-art');
  document.querySelectorAll('.topbar,.page-heading,main,footer').forEach(el=>el.inert=true);
  if(splash){
   const svg=document.createElementNS(svgNS,'svg');svg.setAttribute('viewBox','0 0 1536 864');svg.setAttribute('preserveAspectRatio','xMidYMid meet');svg.setAttribute('aria-hidden','true');
   const defs=document.createElementNS(svgNS,'defs');
   const clip=(id,rectangles)=>{const path=document.createElementNS(svgNS,'clipPath');path.id=id;for(const [x,y,width,height]of rectangles){const rect=document.createElementNS(svgNS,'rect');for(const [key,value]of Object.entries({x,y,width,height}))rect.setAttribute(key,String(value));path.append(rect);}defs.append(path);};
   clip('opening-title-clip',[[550,140,770,120],[565,250,875,95]]);
   clip('opening-wave-clip',[[1320,140,120,120]]);svg.append(defs);
   const layer=(href,clipId)=>{const image=document.createElementNS(svgNS,'image');image.setAttribute('href',href);image.setAttribute('width','1536');image.setAttribute('height','864');image.setAttribute('preserveAspectRatio','none');if(clipId)image.setAttribute('clip-path',`url(#${clipId})`);svg.append(image);};
   layer(splash.original);layer(splash.titleEdit,'opening-title-clip');layer(splash.original,'opening-wave-clip');host.append(svg);
  }else host.append(node('h1',{},'Live Performance'));
  $('opening').addEventListener('click',()=>{openingSkip=true;finishOpening();});$('opening').focus();
 }
 function finishOpening(){
  if(!openingVisible||openingClosing||!state)return;
  if(state.loading){clearTimeout(openingTimer);return;}
  $('opening-status').textContent='Instrumentos prontos. Pressione Enter para continuar.';
  clearTimeout(openingTimer);openingTimer=setTimeout(()=>{
   openingClosing=true;$('opening').classList.add('leaving');
   const delay=matchMedia('(prefers-reduced-motion: reduce)').matches?0:800;
   setTimeout(()=>{openingVisible=false;$('opening').hidden=true;document.querySelectorAll('.topbar,.page-heading,main,footer').forEach(el=>el.inert=false);if(state.screen==='home')document.querySelector(`[data-category="${state.category}"]`)?.focus({preventScroll:true});else $('content').focus();},delay);
  },openingSkip?0:Math.max(0,3500-(window.performance.now()-openingStarted)));
 }
 function node(tag, attrs = {}, ...children) {
  const result = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs)) {
   if (value == null || value === false) continue;
   if (key === 'class') result.className = value;
   else if (key === 'text') result.textContent = value;
   else if (key === 'disabled') result.disabled = Boolean(value);
   else result.setAttribute(key, String(value));
  }
  for (const child of children.flat()) if (child != null) result.append(typeof child === 'string' ? document.createTextNode(child) : child);
  return result;
 }
 function button(label, action, attrs = {}) {const b = node('button', {type:'button', ...attrs}, label);b.addEventListener('click', action);return b;}
 function notice(text) {if(!text)return;clearTimeout(errorTimer);$('error').textContent=text;$('error').hidden=false;errorTimer=setTimeout(()=>$('error').hidden=true,10000);}
 function send(action, a=0, b=0, text='') {
  if(!native?.postMessage){notice('Abra esta interface pelo Live Performance Vision para controlar seus instrumentos.');return 0;}
  serial = serial >= 1000000000 ? 1 : serial + 1;
  native.postMessage(['LP1',serial,action,a,b,encodeURIComponent(text)].join('\t'));
  return serial;
 }
 function flushParameters(){clearTimeout(parameterTimer);parameterTimer=null;for(const [id,value] of pendingParameters)send('parameter',id,value);pendingParameters.clear();}
 function setParameter(id,value,immediate=false){pendingParameters.set(id,Number(value));if(immediate)flushParameters();else if(!parameterTimer)parameterTimer=setTimeout(flushParameters,25);}
 // The painted knob and the native range share the same host parameter route.
 function bindMouseKnob(knob,range){
  const update=value=>{range.value=String(Math.round(Math.max(0,Math.min(1000,value))));range.dispatchEvent(new Event('input'));};
  const finish=(flush=true)=>{
   if(!parameterDrag||parameterDrag.knob!==knob)return;
   const pointer=parameterDrag.pointer;parameterDrag=null;knob.classList.remove('dragging');
   if(knob.hasPointerCapture(pointer))knob.releasePointerCapture(pointer);
   if(flush)flushParameters();
  };
  knob.title='Arraste para cima ou para a direita para aumentar; para baixo ou esquerda para diminuir. Shift: ajuste fino. Clique direito: MIDI Learn.';
  knob.addEventListener('pointerdown',event=>{
   if(event.button!==0||!event.isPrimary||range.disabled)return;
   event.preventDefault();parameterDrag?.finish();range.focus({preventScroll:true});
   parameterDrag={knob,range,pointer:event.pointerId,x:event.clientX,y:event.clientY,value:Number(range.value),finish};
   knob.setPointerCapture(event.pointerId);knob.classList.add('dragging');
  });
  knob.addEventListener('pointermove',event=>{
   const drag=parameterDrag;if(!drag||drag.knob!==knob||drag.pointer!==event.pointerId)return;
   if(range.disabled){finish();return;}
   const delta=(event.clientX-drag.x)+(drag.y-event.clientY);drag.x=event.clientX;drag.y=event.clientY;
   drag.value=Math.max(0,Math.min(1000,drag.value+delta*(event.shiftKey?1:5)));update(drag.value);
  });
  for(const type of ['pointerup','pointercancel','lostpointercapture'])knob.addEventListener(type,event=>{if(parameterDrag?.pointer===event.pointerId)finish();});
  knob.addEventListener('wheel',event=>{
   if(range.disabled||parameterDrag||event.ctrlKey)return;
   const delta=event.deltaY||event.deltaX;if(!delta)return;
   event.preventDefault();range.focus({preventScroll:true});update(Number(range.value)-Math.sign(delta)*(event.shiftKey?1:10));flushParameters();
  },{passive:false});
 }
 function releaseNotes(){if(!held.size)return;held.clear();document.querySelectorAll('.key.down').forEach(e=>e.classList.remove('down'));send('releaseNotes');}
 function crop(asset, x, y, width, height) {
  const svg = document.createElementNS(svgNS,'svg');svg.setAttribute('viewBox',`${x} ${y} ${width} ${height}`);svg.setAttribute('preserveAspectRatio','xMidYMid meet');svg.setAttribute('aria-hidden','true');
  const defs=document.createElementNS(svgNS,'defs'),clip=document.createElementNS(svgNS,'clipPath'),rect=document.createElementNS(svgNS,'rect');const clipId='art-'+crypto.randomUUID();clip.id=clipId;for(const [key,value] of Object.entries({x,y,width,height}))rect.setAttribute(key,String(value));clip.append(rect);defs.append(clip);svg.append(defs);
  const image=document.createElementNS(svgNS,'image');image.setAttribute('clip-path',`url(#${clipId})`);image.setAttribute('href',assets[asset]||'');image.setAttribute('width','1672');image.setAttribute('height','941');svg.append(image);return svg;
 }
 const artCells=[0,1,2,4,5,6,6,10,11,3,9,8,7];
 function art(category){
  const box=node('div',{class:'category-art','aria-hidden':'true'});
  if(category>=13){box.classList.add('symbol');if(category===15)box.classList.add('star');box.textContent=category===13?'▤':category===14?'+':'★';return box;}
  const i=artCells[category]??0, x=156+(i%4)*346, y=255+Math.floor(i/4)*204;
  box.append(crop('categories',x,y,320,140));return box;
 }
 function field(label, id, values, selected, onChange){
  const select=node('select',{id,'data-focus':id});values.forEach((name,i)=>select.append(node('option',{value:i},name)));select.value=String(selected);
  select.addEventListener('change',()=>onChange(Number(select.value)));
  return node('label',{class:'field',for:id},label,select);
 }
 function title(text, kicker='LIVE PERFORMANCE'){const accent=text==='Escolha a categoria do instrumento'?'do instrumento':text==='Escolha o timbre'?'timbre':'';if(accent){const i=text.indexOf(accent);$('heading').replaceChildren(document.createTextNode(text.slice(0,i)),node('em',{},accent));}else $('heading').textContent=text;$('eyebrow').textContent=kicker;}
 function home(){
  title('Escolha a categoria do instrumento','SUA BIBLIOTECA DE PALCO');
  const grid=node('div',{class:'home-grid','aria-label':'Categorias de instrumentos'});
  state.categories.forEach((name,i)=>{const card=button('',()=>send('category',i),{class:'category-card','data-category':i,'aria-label':`Categoria: ${name}`});card.append(art(i),node('span',{class:'category-label'},name));grid.append(card);});return grid;
 }
 function instruments(){
  const favorites=state.category===15, category=state.categories[state.category];title('Escolha o timbre',favorites?'FAVORITOS · PROGRAM CHANGE 1–20':category.toUpperCase());
  const hero=node('section',{class:'instrument-hero panel','aria-label':category});
  if(favorites){const picture=node('div',{class:'hero-art'});picture.append(crop('favorites',190,300,540,450));hero.append(picture);}
  else if(state.category===0){const picture=node('div',{class:'hero-art'});picture.append(crop('piano',125,300,680,450));hero.append(picture);}
  else hero.append(art(state.category));
  hero.append(node('h2',{},category),node('p',{},favorites?'Clique direito: aprender Program Change.':'↑ ↓ Selecionar e carregar · Espaço Favoritar'));
  const list=node('ul',{class:'sound-list',id:'sound-list','aria-label':favorites?'Vinte posições de favoritos':'Timbres da categoria'}),rows=favorites?state.favorites:state.catalog;
  if(!rows.length)list.append(node('li',{class:'empty'},node('h2',{},'Nenhum timbre nesta categoria'),node('p',{},'Importe seus SFZ ou SF2 em Perfis. Os instrumentos serão adicionados a Meus Sons.')));
  rows.forEach(row=>{
   const item=node('li',{class:'sound-row','data-row':row.id});
   const choose=button('',()=>send(favorites?'activateFavorite':'select',row.id),{class:'sound-select','data-select':row.id,'data-focus':`row-${row.id}`,'aria-label':favorites?`Favorito ${row.id+1}: ${row.name}`:row.name,disabled:favorites&&!row.occupied});
   if(favorites)choose.append(node('span',{class:'slot-number','aria-hidden':'true'},String(row.id+1).padStart(2,'0')));
   choose.append(node('span',{},row.name));
   const star=button(favorites?'★':(row.favorite?'★':'☆'),()=>send(favorites?'removeFavorite':'favorite',row.id),{class:'favorite-toggle','data-star':row.id,'data-focus':`star-${row.id}`,'aria-label':favorites?`Remover favorito ${row.id+1}`:`Favoritar ${row.name}`,disabled:favorites&&!row.occupied});
   item.append(choose,star);
   if(favorites){const learn=()=>send('learnFavorite',row.id);item.addEventListener('contextmenu',e=>{e.preventDefault();learn();});choose.addEventListener('keydown',e=>{if(e.key==='ContextMenu'||(e.shiftKey&&e.key==='F10')){e.preventDefault();learn();}});}
   list.append(item);
  });return node('div',{class:'instrument-layout'},hero,list);
 }
 function audio(){
  title('Áudio e MIDI','SUA CONEXÃO DE PALCO');const a=state.audio;
  const io=node('section',{class:'panel wide'},node('h2',{},'Driver ASIO e controlador MIDI'),field('Driver ASIO','driver',a.drivers,a.driver,i=>send('driver',i)),field('Buffer de áudio','buffer',a.buffers,a.buffer,i=>send('buffer',i)),field('Entrada MIDI','midi',a.midiInputs,a.midi,i=>send('midi',i)),node('div',{class:'actions'},button('Conectar ASIO',()=>send('connect'),{class:'primary'}),button('Desconectar',()=>send('disconnect')),button('ASIO Control',()=>send('controlPanel'))));
  const info=node('section',{class:'panel wide'},node('h2',{},'Estado da conexão'),node('p',{class:'metric',id:'audio-detail'}),node('p',{class:'note'},'Áudio e MIDI são processados pelo motor nativo. As configurações de áudio são salvas automaticamente.'));
  return node('div',{class:'form-grid'},io,info);
 }
 function profiles(){
  title('Perfis de show e instrumentos','SEU REPERTÓRIO, PRONTO PARA VOLTAR');
  const card=(name,description,action)=>button([node('strong',{},name),node('span',{},description)],()=>send(action),{class:'panel profile-card'});
  return node('div',{class:'form-grid'},card('Abrir perfil','Carregue os favoritos, parâmetros e mapeamentos de um show.','open'),card('Salvar perfil','Escolha o local do arquivo pelo diálogo do Windows.','save'),card('Importar SFZ','Adicione um instrumento à categoria Meus Sons.','importSfz'),card('Importar SF2','Adicione os presets de um SoundFont à sua biblioteca.','importSf2'),node('section',{class:'panel wide'},node('h2',{},'Um perfil para cada apresentação'),node('p',{class:'details'},'Use Salvar perfil para guardar favoritos, ajustes, performances e MIDI Learn. Cada arquivo .lphs pode conter o repertório de um artista ou show.')));
 }
 let partCategories=[];
 function performance(){
  title('Monte sua Performance','ATÉ QUATRO PARTES · CAMADAS E DIVISÕES');const p=state.performance;
  const zones=field('Distribuição do teclado','performance-mode',p.modes,p.mode,i=>send('performanceMode',i));
  const parts=node('section',{class:'panel wide'},node('h2',{},'Instrumentos das quatro partes'));
  p.parts.forEach((selected,i)=>{
   const categoryIds=state.categories.map((_,n)=>n).filter(n=>n!==13&&n!==15);
   let category=partCategories[i]??p.instrumentCategories[selected];if(!categoryIds.includes(category))category=categoryIds[0];partCategories[i]=category;
   const categoryField=field(`Categoria da parte ${i+1}`,`part-category-${i}`,categoryIds.map(n=>state.categories[n]),categoryIds.indexOf(category),index=>{partCategories[i]=categoryIds[index];structure='';render();});
   const soundField=field(`Timbre da parte ${i+1}`,`part-${i}`,[],selected,index=>send('performancePart',i,index));
   const select=soundField.querySelector('select');
   p.instruments.forEach((name,n)=>{if(n===0||n===selected||p.instrumentCategories[n]===category)select.append(node('option',{value:n},n===selected&&n>0&&p.instrumentCategories[n]!==category?`Atual: ${name}`:name));});select.value=String(selected);
   const row=node('section',{class:'part-row'+(p.editingPart===i?' editing':''),'data-part':i},node('span',{class:'part-number'},String(i+1)),node('div',{class:'part-fields'},categoryField,soundField),node('div',{class:'part-actions'},button('Parâmetros · E',()=>{flushParameters();send('editPart',i);send('effects');},{'aria-label':`Editar parâmetros da parte ${i+1}`}),button('Limpar',()=>send('clearPart',i))));
   row.addEventListener('focusin',()=>{if(state.performance.editingPart!==i)send('editPart',i);});parts.append(row);
  });
  const name=node('input',{id:'performance-name',type:'text',maxlength:255,value:p.name,'data-focus':'performance-name'});name.addEventListener('change',()=>send('performanceName',0,0,name.value));
  return node('div',{class:'form-grid'},node('section',{class:'panel wide'},zones),parts,node('section',{class:'panel wide'},node('label',{class:'field',for:'performance-name'},'Nome da Performance',name),node('div',{class:'actions'},button('Ativar Performance',()=>{send('performanceName',0,0,name.value);send('performancePlay');},{class:'primary'}),button('Salvar Performance',()=>{send('performanceName',0,0,name.value);send('performanceSave');})),node('p',{class:'note'},'Ative para aplicar as partes ao áudio. Depois, salve o perfil para guardar a Performance em disco.')));
 }
 function parameters(){
  title(state.partMix?'Mix da Performance':state.screen==='performance'?`Parâmetros da parte ${state.performance.editingPart+1}`:'Parâmetros do instrumento',state.partMix?'VOLUMES DAS QUATRO PARTES':'EXPRESSÃO · TEXTURA · AMBIENTE');
  const grid=node('div',{class:`parameter-grid${state.partMix?' mix':''}`});
  state.controls.slice(0,state.partMix?4:16).forEach(c=>{
   const range=node('input',{type:'range',min:0,max:1000,step:1,value:c.value,id:`parameter-${c.id}`,'aria-label':c.name,'data-focus':`parameter-${c.id}`,disabled:!c.enabled});
   const knob=node('div',{class:'knob','aria-hidden':'true'}),out=node('output',{for:`parameter-${c.id}`,id:`value-${c.id}`},`${Math.round(c.value/10)}%`);
   range.addEventListener('input',()=>{out.textContent=`${Math.round(range.value/10)}%`;knob.style.setProperty('--angle',`${Number(range.value)*.27}deg`);setParameter(c.id,range.value);});range.addEventListener('change',()=>setParameter(c.id,range.value,true));
   bindMouseKnob(knob,range);
   const learn=()=>send('learnParameter',c.id),cell=node('section',{class:'panel parameter','data-parameter':c.id},node('h2',{},c.name),knob,range,out,node('span',{class:'mapping',id:`mapping-${c.id}`}),button('MIDI Learn',learn,{class:'learn',disabled:!c.enabled,'aria-label':`Aprender controle MIDI: ${c.name}`}));
   cell.addEventListener('contextmenu',e=>{e.preventDefault();learn();});range.addEventListener('keydown',e=>{if(e.key==='ContextMenu'||(e.shiftKey&&e.key==='F10')||(e.ctrlKey&&e.key.toLowerCase()==='l')){e.preventDefault();learn();}});grid.append(cell);
  });return grid;
 }
 function keyboard(){
  title('Toque o instrumento selecionado','TECLADO VIRTUAL · C3 A C6');
  const keys=node('div',{class:'keys','aria-label':'Teclado virtual'}),black=new Set([1,3,6,8,10]),whites=[];
  for(let n=48;n<=84;n++)if(!black.has(n%12))whites.push(n);
  const names=['Dó','Dó sustenido','Ré','Ré sustenido','Mi','Fá','Fá sustenido','Sol','Sol sustenido','Lá','Lá sustenido','Si'];
  function key(n,dark){
   const b=button(n%12===0?`Dó${Math.floor(n/12)-1}`:'',()=>{}, {class:`key ${dark?'black':'white'}`,'data-note':n,'aria-label':`${names[n%12]} ${Math.floor(n/12)-1}`});
   const down=()=>{if(held.has(n)||!state.loadedName)return;held.add(n);b.classList.add('down');send('noteOn',n,100);};
   const up=()=>{if(held.delete(n)){b.classList.remove('down');send('noteOff',n);}};
   b.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();b.setPointerCapture(e.pointerId);down();});for(const kind of ['pointerup','pointercancel','lostpointercapture'])b.addEventListener(kind,up);
   b.addEventListener('keydown',e=>{if((e.key===' '||e.key==='Enter')&&!e.repeat){e.preventDefault();down();}});b.addEventListener('keyup',e=>{if(e.key===' '||e.key==='Enter'){e.preventDefault();up();}});b.addEventListener('blur',up);return b;
  }
  whites.forEach(n=>keys.append(key(n,false)));
  for(let n=48;n<=84;n++)if(black.has(n%12)){const b=key(n,true),left=whites.filter(w=>w<n).length;b.style.left=`${left/whites.length*100}%`;keys.append(b);}
  return node('section',{class:'keyboard-panel'},node('p',{class:'details'},state.loadedName||'Escolha um timbre na biblioteca antes de tocar.'),keys,node('p',{class:'note'},'Clique nas teclas ou use Tab e mantenha Espaço ou Enter pressionado. F12 interrompe todas as notas.'));
 }
 function render(){
  if(!state)return;
  const focus=document.activeElement?.getAttribute('data-focus'),scroll=$('content').scrollTop;
  const key=JSON.stringify([state.screen,state.effects,state.category,state.loading,state.partMix,state.catalog.map(r=>r.name),state.category===15?state.favorites.map(r=>[r.name,r.occupied]):null,state.audio.drivers,state.audio.midiInputs,state.performance.instruments,state.performance.instrumentCategories,state.screen==='performance'?state.performance.parts:null,state.screen==='performance'&&state.effects?state.performance.editingPart:null]);
  if(key!==structure){
   structure=key;let body;
   if(state.loading){title('Preparando seus instrumentos');body=node('section',{class:'waiting panel'},node('h2',{},'Carregando a biblioteca'),node('progress',{id:'progress',max:100,value:state.progress}),node('p',{class:'details'},state.status));}
   else if(state.effects)body=parameters();
   else body=({home,instruments,performance,audio,profiles,keyboard,about:()=>{title('Música, visão e propósito','HUGO SODRÉ · ARTISTA E CRIADOR');return node('article',{class:'panel about-copy'},state.about);}}[state.screen]||home)();
   $('content').replaceChildren(body);
   const next=focus?document.querySelector(`[data-focus="${CSS.escape(focus)}"]`):null;
   if(next){next.focus({preventScroll:true});$('content').scrollTop=scroll;}
   else if(state.screen==='instruments'&&!state.effects){const selected=document.querySelector(`[data-select="${state.selected}"]`);selected?.focus({preventScroll:true});}
   else {$('content').scrollTop=0;if(state.screen==='home'&&!state.effects)document.querySelector(`[data-category="${state.category}"]`)?.focus({preventScroll:true});}
  }
  $('loaded-name').textContent=state.loadedName||'Nenhum timbre carregado';$('status').textContent=state.status;$('audio-dot').classList.toggle('connected',state.audio.connected);
  $('back').hidden=state.screen==='home'&&!state.effects;$('keyboard-nav').hidden=state.screen==='keyboard'||state.effects;
  $('effects-nav').setAttribute('aria-pressed',String(state.effects));
  document.querySelectorAll('nav [data-screen]').forEach(b=>b.setAttribute('aria-current',String(!state.effects&&Number(b.dataset.screen)===['home','instruments','performance','audio','profiles','about','keyboard'].indexOf(state.screen))));
  const rows=state.category===15?state.favorites:state.catalog;
  for(const row of document.querySelectorAll('.sound-row')){const r=rows[Number(row.dataset.row)];if(!r)continue;row.classList.toggle('selected',Number(row.dataset.row)===state.selected);row.classList.toggle('loaded',Boolean(r.loaded));row.querySelector('.sound-select')?.setAttribute('aria-pressed',String(Boolean(r.loaded)));const star=row.querySelector('.favorite-toggle');if(star&&state.category!==15){star.textContent=r.favorite?'★':'☆';star.setAttribute('aria-pressed',String(r.favorite));star.setAttribute('aria-label',`${r.favorite?'Remover dos favoritos:':'Favoritar'} ${r.name}`);}}
  for(const c of state.controls){const range=$(`parameter-${c.id}`),cell=document.querySelector(`[data-parameter="${c.id}"]`);if(!range||!cell)continue;if(parameterDrag?.range!==range&&!range.matches(':active')&&!pendingParameters.has(c.id)){range.value=String(c.value);$(`value-${c.id}`).textContent=`${Math.round(c.value/10)}%`;cell.querySelector('.knob').style.setProperty('--angle',`${c.value*.27}deg`);}range.disabled=!c.enabled;cell.classList.toggle('disabled',!c.enabled);cell.querySelector('.learn').disabled=!c.enabled;$(`mapping-${c.id}`).textContent=c.label.includes(' · ')?c.label.split(' · ').slice(1).join(' · '):'Clique direito para mapear';}
  if($('audio-detail'))$('audio-detail').textContent=state.audio.connected?`ASIO conectado · ${state.audio.sampleRate} Hz · ${state.audio.latencyMs.toFixed(1)} ms de saída`:'ASIO desconectado';
  for(const [id,value] of [['driver',state.audio.driver],['buffer',state.audio.buffer],['midi',state.audio.midi],['performance-mode',state.performance.mode],['performance-name',state.performance.name],...state.performance.parts.map((p,i)=>[`part-${i}`,p])]){const target=$(id);if(target&&document.activeElement!==target)target.value=String(value);}
  document.querySelectorAll('[data-part]').forEach(row=>row.classList.toggle('editing',Number(row.dataset.part)===state.performance.editingPart));
  if($('progress'))$('progress').value=state.progress;
 }
 function receive(event){const next=event?.data??event;if(!next||next.type!=='state'||next.version!==1)return;if(state?.screen==='keyboard'&&(next.screen!=='keyboard'||next.effects))releaseNotes();if(state&&JSON.stringify([state.screen,state.effects,state.loadedName,state.performance.editingPart])!==JSON.stringify([next.screen,next.effects,next.loadedName,next.performance.editingPart])){parameterDrag?.finish(false);clearTimeout(parameterTimer);parameterTimer=null;pendingParameters.clear();}if(state?.screen!=='performance'&&next.screen==='performance')partCategories=[];state=next;if(next.error)notice(next.error);render();finishOpening();}
 $('brand').addEventListener('click',()=>{flushParameters();send('screen',0);});$('back').addEventListener('click',()=>{flushParameters();send('screen',0);});
 document.querySelectorAll('[data-screen]').forEach(b=>b.addEventListener('click',()=>{flushParameters();send('screen',Number(b.dataset.screen));}));
 $('keyboard-nav').addEventListener('click',()=>send('screen',6));$('effects-nav').addEventListener('click',()=>{flushParameters();send('effects');});$('panic').addEventListener('click',()=>{releaseNotes();send('panic');});
 document.addEventListener('keydown',event=>{
  if(openingVisible&&event.key!=='F12'){
   if(['Enter',' ','Escape'].includes(event.key)){event.preventDefault();openingSkip=true;finishOpening();}
   else if(!event.altKey&&!event.ctrlKey&&!event.metaKey)event.preventDefault();
   return;
  }
  if(!state||state.loading)return;
  if(event.key==='F12'){event.preventDefault();releaseNotes();send('panic');return;}
  if(event.key==='Escape'){event.preventDefault();flushParameters();releaseNotes();send('escape');return;}
  const target=event.target,entry=target.matches('select,textarea,input:not([type=range])');
  if(state.screen==='performance'&&target.matches('select')&&event.key.toLowerCase()==='e'&&!event.ctrlKey&&!event.altKey&&!event.metaKey){event.preventDefault();flushParameters();send('effects');return;}
  if(entry||event.ctrlKey||event.altKey||event.metaKey)return;
  if(state.effects)send('touch');
  const key=event.key.toLowerCase();
  if(key==='e'){event.preventDefault();flushParameters();send('effects');return;}
  if(!state.effects&&['p','a','c','k'].includes(key)){event.preventDefault();send('screen',({p:2,a:3,c:4,k:6})[key]);return;}
  if(!state.effects&&state.screen==='home'&&event.key.startsWith('Arrow')){
   const cards=[...document.querySelectorAll('[data-category]')],index=Math.max(0,cards.indexOf(target.closest('[data-category]'))),x=index%4,y=Math.floor(index/4);let nx=x,ny=y;
   if(event.key==='ArrowLeft')nx=Math.max(0,x-1);if(event.key==='ArrowRight')nx=Math.min(3,x+1);if(event.key==='ArrowUp')ny=Math.max(0,y-1);if(event.key==='ArrowDown')ny=Math.min(3,y+1);event.preventDefault();cards[ny*4+nx]?.focus();return;
  }
  if(!state.effects&&state.screen==='instruments'&&['ArrowDown','ArrowUp','PageDown','PageUp',' '].includes(event.key)){
   event.preventDefault();const rows=state.category===15?state.favorites:state.catalog;if(!rows.length)return;
   let index=Number(target.closest('[data-row]')?.dataset.row??state.selected);if(index<0)index=0;
   if(event.key===' '){send(state.category===15?'removeFavorite':'favorite',index);return;}
   const delta=event.key==='ArrowDown'?1:event.key==='ArrowUp'?-1:event.key==='PageDown'?6:-6;index=Math.min(rows.length-1,Math.max(0,index+delta));document.querySelector(`[data-select="${index}"]`)?.focus();send(state.category===15?'activateFavorite':'select',index);return;
  }
  if(['ArrowDown','ArrowUp','PageDown','PageUp'].includes(event.key)&&!target.matches('input,select,textarea')&&state.screen!=='keyboard'){event.preventDefault();const delta=event.key==='ArrowDown'?60:event.key==='ArrowUp'?-60:event.key==='PageDown'?$('content').clientHeight-50:50-$('content').clientHeight;$('content').scrollBy({top:delta,behavior:'instant'});}
 });
 $('content').addEventListener('pointerdown',()=>{if(state?.effects)send('touch');});$('content').addEventListener('wheel',()=>{if(state?.effects)send('touch');},{passive:true});
 window.addEventListener('blur',()=>{parameterDrag?.finish();releaseNotes();});document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseNotes();});window.addEventListener('pagehide',releaseNotes);
 if(assets.fontCss){const style=node('style');style.textContent=assets.fontCss;document.head.append(style);}if(assets.portrait)$('portrait').src=assets.portrait;
 mountOpening();
 if(native?.addEventListener){native.addEventListener('message',receive);send('ready');}
 else $('connection-message').textContent='Abra o Live Performance Vision no Windows. A interface não cria áudio por conta própria.';
 window.LPVision=Object.freeze({receive,send,get state(){return state;}});
})();
