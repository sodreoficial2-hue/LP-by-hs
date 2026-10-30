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
 let parameterTimer = null;
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
  const restoring=Boolean(state.restoring),favorites=state.category===15&&!restoring, category=restoring?'Restaurar timbres':state.categories[state.category];title('Escolha o timbre',favorites?'FAVORITOS · PROGRAM CHANGE 1–20':category.toUpperCase());
  const hero=node('section',{class:'instrument-hero panel','aria-label':category});
  if(favorites){const picture=node('div',{class:'hero-art'});picture.append(crop('favorites',190,300,540,450));hero.append(picture);}
  else if(state.category===0){const picture=node('div',{class:'hero-art'});picture.append(crop('piano',125,300,680,450));hero.append(picture);}
  else hero.append(art(state.category));
  hero.append(node('h2',{},category),node('p',{},favorites?'Clique direito: aprender Program Change.':'↑ ↓ Selecionar e carregar · Espaço Favoritar · Clique direito: opções · Ctrl+D Ocultar'));
  const list=node('ul',{class:'sound-list',id:'sound-list','aria-label':favorites?'Vinte posições de favoritos':'Timbres da categoria'}),rows=favorites?state.favorites:state.catalog;
  if(!rows.length)list.append(node('li',{class:'empty'},node('h2',{},'Nenhum timbre nesta categoria'),node('p',{},'Importe seus SFZ ou SF2 em Perfis. Os instrumentos serão adicionados a Meus Sons.')));
  rows.forEach(row=>{
   const item=node('li',{class:'sound-row','data-row':row.id});
   const choose=button('',()=>send(restoring?'restoreSound':favorites?'activateFavorite':'select',row.id),{class:'sound-select','data-select':row.id,'data-focus':`row-${row.id}`,'aria-label':favorites?`Favorito ${row.id+1}: ${row.name}`:row.name,disabled:favorites&&!row.occupied});
   if(favorites)choose.append(node('span',{class:'slot-number','aria-hidden':'true'},String(row.id+1).padStart(2,'0')));
   choose.append(node('span',{},row.name));
   const star=button(favorites?'★':(row.favorite?'★':'☆'),()=>send(favorites?'removeFavorite':'favorite',row.id),{class:'favorite-toggle','data-star':row.id,'data-focus':`star-${row.id}`,'aria-label':favorites?`Remover favorito ${row.id+1}`:`Favoritar ${row.name}`,disabled:favorites&&!row.occupied});
   item.append(choose);if(restoring)choose.append(node('span',{class:'restore-label'},'Restaurar'));else item.append(star);
   if(favorites){const learn=()=>send('learnFavorite',row.id);item.addEventListener('contextmenu',e=>{e.preventDefault();learn();});choose.addEventListener('keydown',e=>{if(e.key==='ContextMenu'||(e.shiftKey&&e.key==='F10')){e.preventDefault();learn();}});}
   if(!favorites&&!restoring){const menu=e=>{e.preventDefault();send('soundMenu',row.id);};item.addEventListener('contextmenu',menu);choose.addEventListener('keydown',e=>{if(e.key==='ContextMenu'||(e.shiftKey&&e.key==='F10')){e.stopPropagation();menu(e);}});}
   list.append(item);
  });
  const search=node('input',{id:'sound-search',type:'search',maxlength:256,value:state.search||'','data-focus':'sound-search',placeholder:'Digite parte do nome — Ctrl+F','aria-label':'Buscar timbre pelo nome'});
  search.addEventListener('input',()=>send('search',0,0,search.value));
  search.addEventListener('keydown',e=>{if(e.key==='ArrowDown'||e.key==='Enter'){e.preventDefault();const row=document.querySelector('[data-select]');row?.focus();if(!restoring&&row)send('select',Number(row.dataset.select));}});
  const right=node('section',{class:'catalog-results'},...(!favorites?[node('label',{class:'field',for:'sound-search'},'Buscar timbre',search)]:[]),list);
  if(restoring){hero.append(button('Restaurar todos',()=>send('restoreAll')));hero.querySelector('p').textContent='Selecione Restaurar para devolver o timbre à categoria. Os arquivos originais permanecem preservados.';}
  return node('div',{class:'instrument-layout'},hero,right);
 }
 function audio(){
  title('Áudio e MIDI','SUA CONEXÃO DE PALCO');const a=state.audio;
  const io=node('section',{class:'panel wide'},node('h2',{},'Driver ASIO e controlador MIDI'),field('Driver ASIO','driver',a.drivers,a.driver,i=>send('driver',i)),field('Entrada MIDI','midi',a.midiInputs,a.midi,i=>send('midi',i)),node('div',{class:'actions'},button('Conectar ASIO',()=>send('connect'),{class:'primary'}),button('Desconectar',()=>send('disconnect')),button('ASIO Control',()=>send('controlPanel'))));
  const info=node('section',{class:'panel wide'},node('h2',{},'Estado da conexão'),node('p',{class:'metric',id:'audio-detail'}),node('p',{class:'note'},'Ajuste o buffer no ASIO Control. As configurações de áudio são salvas automaticamente.'));
  return node('div',{class:'form-grid'},io,info);
 }
 function profiles(){
  title('Perfis de show e instrumentos','SEU REPERTÓRIO, PRONTO PARA VOLTAR');
  const card=(name,description,action)=>button([node('strong',{},name),node('span',{},description)],()=>send(action),{class:'panel profile-card'});
  return node('div',{class:'form-grid'},card('Abrir perfil','Carregue os favoritos, parâmetros e mapeamentos de um show.','open'),card('Salvar perfil','Escolha o local do arquivo pelo diálogo do Windows.','save'),card('Restaurar timbres ocultos','Recupere um timbre ocultado pelo menu ou Ctrl+D.','restoreSounds'),card('Localizar timbres ausentes','Atualize os arquivos usados pelos favoritos e Performances.','locateSounds'),card('Importar SFZ','Adicione um instrumento à categoria Meus Sons.','importSfz'),card('Importar SF2','Adicione os presets de um SoundFont à sua biblioteca.','importSf2'),node('section',{class:'panel wide'},node('h2',{},'Um perfil para cada apresentação'),node('p',{class:'details'},'As Performances são salvas permanentemente pelo editor. Use Salvar perfil para guardar favoritos, ajustes e MIDI Learn. Cada arquivo .lphs pode conter o repertório de um artista ou show.')));
 }
 let partCategories=[],partSearch=[],partScroll=[],partTargets=[];
 const searchFold=value=>String(value).normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLocaleLowerCase('pt-BR');
 function performance(){
  title('Monte sua Performance',state.performance.preparing?'PREPARANDO TIMBRE · INTERFACE DISPONÍVEL':'ATÉ QUATRO PARTES · CAMADAS E DIVISÕES');const p=state.performance;
  const zones=field('Distribuição do teclado','performance-mode',p.modes,p.mode,i=>send('performanceMode',i));
  const parts=node('section',{class:'panel wide'},node('h2',{},'Instrumentos das quatro partes'));
  p.parts.forEach((selected,i)=>{
   const categoryIds=state.categories.map((_,n)=>n).filter(n=>n!==13&&n!==15);
   let category=p.browseCategories?.[i]??partCategories[i]??p.instrumentCategories[selected];if(!categoryIds.includes(category))category=categoryIds[0];partCategories[i]=category;
   const categoryField=field(`Categoria da parte ${i+1}`,`part-category-${i}`,categoryIds.map(n=>state.categories[n]),categoryIds.indexOf(category),index=>{partCategories[i]=categoryIds[index];if(p.browseCategories)p.browseCategories[i]=categoryIds[index];send('partCategory',i,categoryIds[index]);structure='';render();});
   let lastAudition=selected;const audition=index=>{if(index!==lastAudition){lastAudition=index;flushParameters();send('performancePart',i,index);}};
   const soundField=field(`Timbre da parte ${i+1}`,`part-${i}`,[],selected,audition);
   const select=soundField.querySelector('select');select.size=6;select.setAttribute('aria-label',`Timbres da camada ${i+1}. Use as setas para ouvir.`);
   select.addEventListener('input',()=>audition(Number(select.value)));
   select.addEventListener('keydown',e=>{if(!['ArrowDown','ArrowUp','Home','End','PageDown','PageUp'].includes(e.key))return;e.preventDefault();const options=[...select.options];let index=select.selectedIndex;index=e.key==='Home'?0:e.key==='End'?options.length-1:Math.max(0,Math.min(options.length-1,index+(e.key==='ArrowDown'?1:e.key==='ArrowUp'?-1:e.key==='PageDown'?5:-5)));select.selectedIndex=index;options[index]?.scrollIntoView({block:'nearest'});audition(Number(select.value));});
   if(p.searches)partSearch[i]=p.searches[i];
   p.instruments.forEach((name,n)=>{if(n===0||n===selected||(p.instrumentCategories[n]===category&&searchFold(name).includes(searchFold(partSearch[i]||''))))select.append(node('option',{value:n},n===selected&&n>0&&p.instrumentCategories[n]!==category?`Atual: ${name}`:name));});select.value=String(selected);select.scrollTop=partScroll[i]||0;select.addEventListener('scroll',()=>{partScroll[i]=select.scrollTop;});select.addEventListener('focus',()=>select.selectedOptions?.[0]?.scrollIntoView({block:'nearest'}));
   if(p.searches)partSearch[i]=p.searches[i];
   const search=node('input',{id:`part-search-${i}`,type:'search',value:partSearch[i]||'','data-focus':`part-search-${i}`,'aria-label':`Buscar timbre da parte ${i+1}`,placeholder:'Buscar timbre nesta categoria',maxlength:256});
   search.addEventListener('input',()=>{partSearch[i]=search.value;if(p.searches)p.searches[i]=search.value;send('partSearch',i,0,search.value);structure='';render();});
   const volume=node('input',{type:'range',id:`part-volume-${i}`,min:0,max:1000,step:1,value:p.volumes?.[i]??1000,'data-focus':`part-volume-${i}`,'aria-label':`Volume da camada ${i+1}`,disabled:!selected});
   const output=node('output',{for:`part-volume-${i}`,id:`part-volume-value-${i}`},`${Math.round((p.volumes?.[i]??1000)/10)}%`);
   volume.addEventListener('input',()=>{output.textContent=`${Math.round(volume.value/10)}%`;send('partVolume',i,Number(volume.value));});
   const target=field(`Destino da camada ${i+1}`,`part-target-${i}`,['Camada 1','Camada 2','Camada 3','Camada 4'],partTargets[i]??(i+1)%4,n=>{partTargets[i]=n;});
   const destination=()=>Number(target.querySelector('select').value);
   const tools=node('div',{class:'part-tools'},
    node('label',{class:'field',for:`part-volume-${i}`},'Volume',volume,output),
    button(p.solo===i?'Ouvir todas':'Ouvir somente esta camada',()=>send('partSolo',i),{'aria-pressed':String(p.solo===i),disabled:!selected,'data-focus':`part-solo-${i}`}),
    button(p.muted?.[i]?'Reativar camada':'Silenciar camada',()=>send('partMute',i),{'aria-pressed':String(Boolean(p.muted?.[i])),disabled:!selected,'data-focus':`part-mute-${i}`}),
    target,button('Copiar para o destino',()=>send('copyPart',i,destination()),{disabled:!selected}),button('Trocar com o destino',()=>send('swapParts',i,destination())),
    node('p',{class:'part-zone'},`Zona: ${p.zones?.[i]||'Todo o teclado'}`),
    button(p.splitLearning===i*2?'Aguardando tecla de início…':'Aprender início pelo MIDI',()=>send('learnLow',i),{disabled:!selected}),
    button(p.splitLearning===i*2+1?'Aguardando tecla de fim…':'Aprender fim pelo MIDI',()=>send('learnHigh',i),{disabled:!selected}));
   const row=node('section',{class:'part-row'+(p.editingPart===i?' editing':''),'data-part':i},node('span',{class:'part-number'},String(i+1)),node('div',{class:'part-fields'},categoryField,search,soundField,tools),node('div',{class:'part-actions'},button('Parâmetros · E',()=>{flushParameters();send('editPart',i);send('effects');},{'aria-label':`Editar parâmetros da parte ${i+1}`}),button('Limpar',()=>send('clearPart',i))));
   row.addEventListener('focusin',()=>{if(state.performance.editingPart!==i)send('editPart',i);});parts.append(row);
  });
  const name=node('input',{id:'performance-name',type:'text',maxlength:255,value:p.name,'data-focus':'performance-name'});name.addEventListener('change',()=>send('performanceName',0,0,name.value));
  return node('div',{class:'form-grid'},node('section',{class:'panel wide'},zones),parts,node('section',{class:'panel wide'},node('label',{class:'field',for:'performance-name'},'Nome da Performance',name),node('div',{class:'actions'},button(p.editingSaved?'Salvar alterações':'Salvar Performance',()=>{send('performanceName',0,0,name.value);send('performanceSave');}),...(p.editingSaved?[button('Salvar como nova',()=>{send('performanceName',0,0,name.value);send('performanceSaveNew');})]:[])),node('p',{class:'note'},'As camadas tocam automaticamente. Use as setas para experimentar os timbres. Salvar Performance grava permanentemente e limpa o editor. A montagem em andamento é recuperada pelo rascunho automático.')));
 }
 function parameters(){
  title(state.partMix?'Mix da Performance':state.screen==='performance'?`Parâmetros da parte ${state.performance.editingPart+1}`:'Parâmetros do instrumento',state.partMix?'VOLUMES DAS QUATRO PARTES':'EXPRESSÃO · TEXTURA · AMBIENTE');
  const grid=node('div',{class:`parameter-grid${state.partMix?' mix':''}`});
  state.controls.slice(0,state.partMix?4:16).forEach(c=>{
   const range=node('input',{type:'range',min:0,max:1000,step:1,value:c.value,id:`parameter-${c.id}`,'aria-label':c.name,'data-focus':`parameter-${c.id}`,disabled:!c.enabled});
   const knob=node('div',{class:'knob','aria-hidden':'true'}),out=node('output',{for:`parameter-${c.id}`,id:`value-${c.id}`},`${Math.round(c.value/10)}%`);
   range.addEventListener('input',()=>{out.textContent=`${Math.round(range.value/10)}%`;knob.style.setProperty('--angle',`${Number(range.value)*.27}deg`);setParameter(c.id,range.value);});range.addEventListener('change',()=>setParameter(c.id,range.value,true));
   // Drag the visible knob vertically; the range remains keyboard accessible.
   if(c.enabled){
    knob.style.touchAction='none';
    knob.style.pointerEvents='auto';
    let drag=null;
    knob.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();drag={y:e.clientY,value:Number(range.value)};knob.setPointerCapture(e.pointerId);});
    knob.addEventListener('pointermove',e=>{if(!drag)return;const next=Math.max(0,Math.min(1000,Math.round(drag.value+(drag.y-e.clientY)*5)));if(Number(range.value)===next)return;range.value=String(next);range.dispatchEvent(new Event('input',{bubbles:true}));});
    const finish=e=>{if(!drag)return;drag=null;range.dispatchEvent(new Event('change',{bubbles:true}));if(knob.hasPointerCapture(e.pointerId))knob.releasePointerCapture(e.pointerId);};
    knob.addEventListener('pointerup',finish);knob.addEventListener('pointercancel',finish);
   }
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
  const active=document.activeElement,focus=active?.getAttribute('data-focus'),scroll=$('content').scrollTop;
  // Filtering rebuilds the results and their search input. Keep both the local
  // text and caret/selection, including edits in the middle of a search term.
  const textEdit=active?.matches('input[type="search"],input[type="text"]')?
   {value:active.value,start:active.selectionStart,end:active.selectionEnd,direction:active.selectionDirection}:null;
  const key=JSON.stringify([state.screen,state.effects,state.category,state.loading,state.partMix,state.restoring,state.catalog.map(r=>r.name),state.category===15?state.favorites.map(r=>[r.name,r.occupied]):null,state.audio.drivers,state.audio.midiInputs,state.performance.instruments,state.performance.instrumentCategories,state.screen==='performance'?[state.performance.parts,state.performance.name,state.performance.muted,state.performance.solo,state.performance.zones,state.performance.splitLearning,state.performance.editingSaved,state.performance.browseCategories,state.performance.searches]:null,state.screen==='performance'&&state.effects?state.performance.editingPart:null]);
  if(key!==structure){
   structure=key;let body;
   if(state.loading){title('Preparando seus instrumentos');body=node('section',{class:'waiting panel'},node('h2',{},'Carregando a biblioteca'),node('progress',{id:'progress',max:100,value:state.progress}),node('p',{class:'details'},state.status));}
   else if(state.effects)body=parameters();
   else body=({home,instruments,performance,audio,profiles,keyboard,about:()=>{title('Música, visão e propósito','HUGO SODRÉ · ARTISTA E CRIADOR');return node('article',{class:'panel about-copy'},state.about);}}[state.screen]||home)();
   $('content').replaceChildren(body);
   const next=focus?document.querySelector(`[data-focus="${CSS.escape(focus)}"]`):null;
   if(next){
    if(textEdit&&next.matches('input[type="search"],input[type="text"]')){
     next.value=textEdit.value;
     if(textEdit.start!==null)next.setSelectionRange(textEdit.start,textEdit.end,textEdit.direction||'none');
    }
    next.focus({preventScroll:true});$('content').scrollTop=scroll;
   }
   else if(state.screen==='instruments'&&!state.effects){const selected=document.querySelector(`[data-select="${state.selected}"]`);selected?.focus({preventScroll:true});}
   else {$('content').scrollTop=0;if(state.screen==='home'&&!state.effects)document.querySelector(`[data-category="${state.category}"]`)?.focus({preventScroll:true});}
  }
  if(state.screen==='performance'&&!state.effects)$('eyebrow').textContent=state.performance.preparing?'PREPARANDO TIMBRE · INTERFACE DISPONÍVEL':'ATÉ QUATRO PARTES · CAMADAS E DIVISÕES';
  $('profile-state').textContent=state.dirty?'Alterações não salvas':'Perfil sem alterações pendentes';$('profile-state').classList.toggle('dirty',Boolean(state.dirty));
  const peak=Number(state.peak)||0,db=peak>0?20*Math.log10(peak):-90;
  $('output-meter').value=Math.min(1,peak);$('peak-label').textContent=state.clipping?'Pico excessivo — reduza os volumes':`Saída: ${db<=-90?'silêncio':Math.round(db)+' dBFS'}`;$('peak-label').classList.toggle('clipping',Boolean(state.clipping));
  if($('sound-search')&&document.activeElement!==$('sound-search'))$('sound-search').value=state.search||'';
  $('loaded-name').textContent=state.loadedName||'Nenhum timbre carregado';$('status').textContent=state.status;$('audio-dot').classList.toggle('connected',state.audio.connected);
  $('back').hidden=state.screen==='home'&&!state.effects;$('keyboard-nav').hidden=state.screen==='keyboard'||state.effects;
  $('effects-nav').setAttribute('aria-pressed',String(state.effects));
  document.querySelectorAll('nav [data-screen]').forEach(b=>b.setAttribute('aria-current',String(!state.effects&&Number(b.dataset.screen)===['home','instruments','performance','audio','profiles','about','keyboard'].indexOf(state.screen))));
  const rows=state.category===15&&!state.restoring?state.favorites:state.catalog;
  for(const row of document.querySelectorAll('.sound-row')){const r=rows[Number(row.dataset.row)];if(!r)continue;row.classList.toggle('selected',Number(row.dataset.row)===state.selected);row.classList.toggle('loaded',Boolean(r.loaded));row.querySelector('.sound-select')?.setAttribute('aria-pressed',String(Boolean(r.loaded)));const star=row.querySelector('.favorite-toggle');if(star&&state.category!==15){star.textContent=r.favorite?'★':'☆';star.setAttribute('aria-pressed',String(r.favorite));star.setAttribute('aria-label',`${r.favorite?'Remover dos favoritos:':'Favoritar'} ${r.name}`);}}
  for(const c of state.controls){const range=$(`parameter-${c.id}`),cell=document.querySelector(`[data-parameter="${c.id}"]`);if(!range||!cell)continue;if(document.activeElement!==range){range.value=String(c.value);$(`value-${c.id}`).textContent=`${Math.round(c.value/10)}%`;cell.querySelector('.knob').style.setProperty('--angle',`${c.value*.27}deg`);}range.disabled=!c.enabled;cell.classList.toggle('disabled',!c.enabled);cell.querySelector('.learn').disabled=!c.enabled;$(`mapping-${c.id}`).textContent=c.label.includes(' · ')?c.label.split(' · ').slice(1).join(' · '):'Clique direito para mapear';}
  if($('audio-detail'))$('audio-detail').textContent=state.audio.connected?`ASIO conectado · ${state.audio.sampleRate} Hz · ${state.audio.latencyMs.toFixed(1)} ms de saída`:'ASIO desconectado';
  for(const [id,value] of [['driver',state.audio.driver],['midi',state.audio.midi],['performance-mode',state.performance.mode],['performance-name',state.performance.name],...state.performance.parts.map((p,i)=>[`part-${i}`,p])]){const target=$(id);if(target&&document.activeElement!==target)target.value=String(value);}
  for(let i=0;i<4;i++){const input=$(`part-volume-${i}`);if(input&&document.activeElement!==input){input.value=String(state.performance.volumes?.[i]??1000);$(`part-volume-value-${i}`).textContent=`${Math.round(input.value/10)}%`;}}
  document.querySelectorAll('[data-part]').forEach(row=>row.classList.toggle('editing',Number(row.dataset.part)===state.performance.editingPart));
  if($('progress'))$('progress').value=state.progress;
 }
 function receive(event){const next=event?.data??event;if(!next||next.type!=='state'||next.version!==1)return;if(state?.screen==='keyboard'&&(next.screen!=='keyboard'||next.effects))releaseNotes();if(next.screen==='performance'&&(state?.screen!=='performance'||(state?.performance.parts.some(Boolean)&&!next.performance.parts.some(Boolean)))){partCategories=[];partSearch=[];partScroll=[];}state=next;if(next.error)notice(next.error);render();finishOpening();}
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
  const target=event.target;
  if(event.ctrlKey&&!event.altKey&&!event.metaKey&&event.key.toLowerCase()==='f'){
   const search=state.screen==='performance'?document.querySelector(`#part-search-${state.performance.editingPart}`):$('sound-search');
   if(search){event.preventDefault();search.focus();search.select();}return;
  }
  const entry=target.matches('select,textarea,input:not([type=range])');
  if(state.screen==='performance'&&target.matches('select')&&event.key.toLowerCase()==='e'&&!event.ctrlKey&&!event.altKey&&!event.metaKey){event.preventDefault();flushParameters();send('effects');return;}
  if(event.ctrlKey&&!event.altKey&&!event.metaKey&&!event.shiftKey&&event.key.toLowerCase()==='d'&&!entry&&!target.isContentEditable){event.preventDefault();if(!event.repeat&&state.screen==='instruments'&&!state.effects){flushParameters();send('deleteSelected');}return;}
  if(entry||event.ctrlKey||event.altKey||event.metaKey)return;
  // Keyboard shortcuts are additive; MIDI Program Change/Learn routing is unchanged.
  if(state.screen!=='performance'&&!event.repeat && /^[0-9]$/.test(event.key) && !event.shiftKey && !target.matches('input,select,textarea,[contenteditable="true"]')){
   const slot=event.key==='0'?9:Number(event.key)-1;
   if(state.favorites?.[slot]?.occupied){event.preventDefault();flushParameters();send('activateFavorite',slot);return;}
  }
  const key=event.key.toLowerCase();
  if(key==='e'){event.preventDefault();flushParameters();send('effects');return;}
  if(!state.effects&&['p','a','c','k'].includes(key)){event.preventDefault();send('screen',({p:2,a:3,c:4,k:6})[key]);return;}
  if(!state.effects&&state.screen==='home'&&event.key.startsWith('Arrow')){
   const cards=[...document.querySelectorAll('[data-category]')],index=Math.max(0,cards.indexOf(target.closest('[data-category]'))),x=index%4,y=Math.floor(index/4);let nx=x,ny=y;
   if(event.key==='ArrowLeft')nx=Math.max(0,x-1);if(event.key==='ArrowRight')nx=Math.min(3,x+1);if(event.key==='ArrowUp')ny=Math.max(0,y-1);if(event.key==='ArrowDown')ny=Math.min(3,y+1);event.preventDefault();cards[ny*4+nx]?.focus();return;
  }
  if(!state.effects&&state.screen==='instruments'&&['ArrowDown','ArrowUp','PageDown','PageUp',' '].includes(event.key)){
   event.preventDefault();const rows=state.category===15&&!state.restoring?state.favorites:state.catalog;if(!rows.length)return;
   let index=Number(target.closest('[data-row]')?.dataset.row??state.selected);if(index<0)index=0;
   if(event.key===' '){if(state.restoring){send('restoreSound',index);return;}send(state.category===15?'removeFavorite':'favorite',index);return;}
   const delta=event.key==='ArrowDown'?1:event.key==='ArrowUp'?-1:event.key==='PageDown'?6:-6;index=Math.min(rows.length-1,Math.max(0,index+delta));document.querySelector(`[data-select="${index}"]`)?.focus();if(!state.restoring)send(state.category===15?'activateFavorite':'select',index);return;
  }
  if(['ArrowDown','ArrowUp','PageDown','PageUp'].includes(event.key)&&!target.matches('input,select,textarea')&&state.screen!=='keyboard'){event.preventDefault();const delta=event.key==='ArrowDown'?60:event.key==='ArrowUp'?-60:event.key==='PageDown'?$('content').clientHeight-50:50-$('content').clientHeight;$('content').scrollBy({top:delta,behavior:'instant'});}
 });
 window.addEventListener('blur',releaseNotes);document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseNotes();});window.addEventListener('pagehide',releaseNotes);
 if(assets.fontCss){const style=node('style');style.textContent=assets.fontCss;document.head.append(style);}if(assets.portrait)$('portrait').src=assets.portrait;
 mountOpening();
 if(native?.addEventListener){native.addEventListener('message',receive);send('ready');}
 else $('connection-message').textContent='Abra o Live Performance Vision no Windows. A interface não cria áudio por conta própria.';
 window.LPVision=Object.freeze({receive,send,get state(){return state;}});
})();
