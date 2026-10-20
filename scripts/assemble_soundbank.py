#!/usr/bin/env python3
"""Assemble a curated SFZ bank from separately downloaded, licensed sources.
Does not download, resample, replace recovered files, or build a release.
Usage: assemble_soundbank.py SOURCES OUTPUT [--recovered PATH]
Preserves original SFZs, samples and notices; copies only selected dependencies.
"""
import argparse,collections,hashlib,json,pathlib,re,shutil,struct
P=pathlib.Path
ap=argparse.ArgumentParser();ap.add_argument('sources',type=P);ap.add_argument('output',type=P);ap.add_argument('--recovered',type=P,required=True);a=ap.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rows=[];sources={};copied=set()
def root(name):
 p=a.sources/name
 children=[x for x in p.iterdir() if x.is_dir()]
 return children[0] if len(children)==1 and not list(p.glob('*.sfz')) else p

def source(key,folder,url,license,author):
 src=folder.resolve();dst=out/'sources'/key;sources[key]={'root':src,'destination':dst,'url':url,'license':license,'author':author}
 # Keep complete licenses, readmes and other original attribution notices.
 for p in src.rglob('*'):
  if p.is_file() and any(x in p.name.lower() for x in ['license','readme','copyright','origin','credit']):copy(key,p)

def copy(key,p):
 src=sources[key]['root'];p=p.resolve();rel=p.relative_to(src);dst=sources[key]['destination']/rel
 if str(dst) in copied:return
 if not p.is_file():raise FileNotFoundError(p)
 dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst);copied.add(str(dst))

def expand(key,p,macros,stack=(),base=None):
 if base is None:base=p.parent
 if p in stack:raise ValueError('Recursive SFZ include')
 copy(key,p);text=p.read_text(encoding='utf-8-sig',errors='strict')
 def inc(m):return expand(key,(base/m[1].replace('\\','/')).resolve(),macros,stack+(p,),base)
 return re.sub(r'#include\s+"([^"]+)"',inc,text)

def prepare(key,relative):
 src=sources[key]['root'];p=src/relative;macros={};text=expand(key,p,macros)
 if '//dummy' in text or 'sample=*sine' in text:raise ValueError('Placeholder instrument rejected: '+str(p))
 text=re.sub(r'//[^\n]*','',text)
 # SFZ macros are sequential: each velocity layer redefines $VEL and
 # each tuning master redefines $TUNE. Expanding only their final values
 # would omit the quieter layers from the dependency inventory.
 expanded=[]
 for line in text.splitlines():
  for match in list(re.finditer(r'#define\s+(\$\w+)\s+(\S+)',line)):
   macros[match[1]]=match[2];line=line.replace(match[0],'')
  for _ in range(4):
   for name,value in sorted(macros.items(),key=lambda x:-len(x[0])):line=line.replace(name,value)
  expanded.append(line)
 text='\n'.join(expanded)
 default=''
 for m in re.finditer(r'\b(default_path|sample)\s*=\s*(.*?)(?=\s+[a-zA-Z_]\w*\s*=|<|\r?\n|$)',text):
  value=m[2].strip().strip('"').replace('\\','/')
  if m[1]=='default_path':default=value
  elif not value.startswith('*'):
   if '$' in value:raise ValueError('Unresolved sample macro: '+value)
   copy(key,p.parent/default/value)
 return (sources[key]['destination']/relative).relative_to(out).as_posix(),text

def patch(key,rel,id,category,name,note=60,mode='poly'):
 path,_=prepare(key,rel);rows.append([id,category,name,path,note,mode,sources[key]['license']]);return path

def generated(id,category,name,text,note=60,mode='poly',license='CC0-1.0'):
 p=out/'patches'/(id+'.sfz');p.parent.mkdir(exist_ok=True);p.write_text(text,encoding='utf-8');rows.append([id,category,name,p.relative_to(out).as_posix(),note,mode,license])

source('salamander',root('Salamander'),'https://github.com/sfzinstruments/SalamanderGrandPiano','CC-BY-3.0','Alexander Holm; tuning Markus Fiedler; SFZ kinwie')
patch('salamander','Salamander Grand Piano V3.sfz','piano-c5',0,'Yamaha C5 — Salamander, 16 camadas')
source('vcsl-keys',root('VCSL-Keys'),'https://versilian-studios.com/vcsl-keys/','CC0-1.0','Sam Gossner / Versilian Studios')
for rel,id,name in [('Grand Piano, K.sfz','piano-kawai','Kawai — piano de cauda'),('Upright Piano, Knight.sfz','piano-knight','Knight — piano vertical'),('Upright Piano, Y.sfz','piano-y-upright','Yamaha — piano vertical')]:patch('vcsl-keys',rel,id,0,name)
source('greg-ep',a.recovered,'https://github.com/sfzinstruments/GregSullivan.E-Pianos','CC-BY-3.0','Greg Sullivan; SFZ kinwie')
for rel,id,name in [('CP80/CP80.sfz','ep-cp80','Yamaha CP80'),('Pianet T/Pianet T.sfz','ep-pianet','Hohner Pianet T'),('Wurlitzer EP200/Wurlitzer EP200.sfz','ep-wurlitzer','Wurlitzer EP200')]:patch('greg-ep',rel,id,1,name)
source('discord',root('Discord-SFZ')/'Discord GM','https://github.com/sfzinstruments/Discord-SFZ-GM-Bank','per-instrument','See individual SFZ headers; curated CC0 subset only')
# Each selected main SFZ explicitly carries its own CC0 license.
for rel,id,cat,name,note in [('005-Electric Piano 1.sfz','ep-rhodes',1,'Rhodes — jRhodes GM',60),('034-Electric Bass (finger).sfz','bass-finger',10,'Baixo elétrico — finger',40),('026-Acoustic Guitar (steel).sfz','guitar-steel',10,'Violão de aço — Martin',60),('105-Sitar.sfz','sitar',10,'Sitar',60)]:
 path=patch('discord','Melodic/'+rel,id,cat,name,note);rows[-1][-1]='CC0-1.0'
# Some CC0 GM patches place opcodes before their first header. Add an
# explicit global header so sfizz applies their intended native parameters.
for id in ['ep-rhodes','guitar-steel','sitar']:
 target=out/next(row[3] for row in rows if row[0]==id);text=target.read_text()
 lines=text.splitlines(keepends=True)
 for index,line in enumerate(lines):
  if not line.strip() or line.lstrip().startswith('//'):continue
  if not line.lstrip().startswith('<'):lines.insert(index,'// LP adaptation: explicit global header for sfizz.\n<global>\n')
  break
 target.write_text(''.join(lines))
for key,cat,name,url in [('Drawbar',2,'Órgão drawbar — emulação','Organ/electric-organ.html'),('PercOrgan',2,'Órgão percussivo — emulação','Organ/electric-organ.html'),('RockOrgan',2,'Órgão rock — emulação','Organ/electric-organ.html'),('Tenor',4,'Sax tenor','Reed/saxophone.html'),('Accordion',9,'Acordeom de botões — Hohner','Organ/accordion.html')]:
 source(key,root(key),'https://freepats.zenvoid.org/'+url,'CC0-1.0','FreePats contributors; original readme retained')
 sfz=next(sources[key]['root'].glob('*.sfz'));patch(key,sfz.name,key.lower(),cat,name)
source('vsco',root('VSCO2-CE'),'https://github.com/sgossner/VSCO-2-CE/releases/tag/1.1.0','CC0-1.0','Sam Gossner / Versilian Studios; Simon Dalzell / Ivy Audio')
selections=[(3,'SViolinVib.sfz','violin','Violino solo',69),(3,'ViolinEnsSusVib.sfz','violins','Violinos — ensemble',69),(3,'ViolaEnsSusVib.sfz','violas','Violas — ensemble',60),(3,'CelloEnsSusVib.sfz','cellos','Violoncelos — ensemble',48),(3,'ContrabassSusNV.sfz','contrabass','Contrabaixo acústico',40),(3,'ViolinEnsPizz.sfz','strings-pizz','Cordas — pizzicato',69),(4,'TrumpetSusVib.sfz','trumpet','Trompete solo',64),(4,'TrumpetHarmonMuteSus.sfz','trumpet-mute','Trompete com surdina',64),(4,'TromboneSus.sfz','trombone','Trombone solo',53),(4,'FHornSus.sfz','horn','Trompa',60),(4,'TubaSus.sfz','tuba','Tuba',40),(7,'Marimba.sfz','marimba','Marimba',60),(7,'Glockenspiel.sfz','glockenspiel','Glockenspiel',84),(7,'Xylophone.sfz','xylophone','Xilofone',72),(7,'Timpani.sfz','timpani','Tímpanos',48),(7,'TubularBells.sfz','bells','Sinos tubulares',72),(10,'Harp.sfz','harp','Harpa',60),(10,'FluteSusVib.sfz','flute','Flauta transversal',72),(10,'PiccoloSus.sfz','piccolo','Flautim',84),(10,'ClarinetSus.sfz','clarinet','Clarinete',60),(10,'OboeSusNV.sfz','oboe','Oboé',64),(10,'BassoonSus.sfz','bassoon','Fagote',48),(2,'OrganLoud.sfz','pipe-organ','Órgão de tubos',60)]
for cat,rel,id,name,note in selections:patch('vsco',rel,id,cat,name,note)
# Layers made from real ensemble/brass recordings. Paths are rewritten relative
# to this new SFZ; upstream files and samples remain unchanged.
def layered(id,cat,name,rels):
 texts=[]
 for rel in rels:
  _,text=prepare('vsco',rel);default=re.search(r'default_path\s*=([^\n\r]*)',text)[1].strip().replace('\\','/')
  text=re.sub(r'<control>.*?(?=<global>)','',text,flags=re.S)
  text=re.sub(r'\bsample\s*=\s*([^\r\n]+)',lambda m:'sample=../sources/vsco/'+default+m[1].strip().replace('\\','/'),text)
  text=text.replace('volume=0','volume=-9');texts.append(text)
 generated(id,cat,name,'\n'.join(texts))
layered('strings-ensemble',3,'Cordas — ensemble completo',['ViolinEnsSusVib.sfz','ViolaEnsSusVib.sfz','CelloEnsSusVib.sfz'])
layered('brass-section',4,'Brass — naipe de metais',['TrumpetSus.sfz','TromboneSus.sfz','FHornSus.sfz'])
for key,name,rel in [('BearSax','Sax barítono — Bear Sax','Programs/2-solo-poly.sfz'),('Guitar','Guitarra elétrica limpa — Emily','emily_clean.sfz')]:
 source(key,root(key),'https://github.com/sfzinstruments/karoryfer.'+('bear-sax' if key=='BearSax' else 'emilyguitar'),'CC0-1.0','Karoryfer Samples / bigcat instruments')
 patch(key,rel,key.lower(),4 if key=='BearSax' else 10,name,48 if key=='BearSax' else 60)
source('mtg-sax',root('MTGSax'),'https://github.com/sfzinstruments/MTG.SoloSax','CC-BY-4.0','Music Technology Group / Universitat Pompeu Fabra (MTG), samples; SFZ kinwie')
for rel,id,name,note in [('MTG Soprano Sax.sfz','sax-soprano','Sax soprano — MTG',72),('MTG Alto Sax.sfz','sax-alto','Sax alto — MTG',60)]:patch('mtg-sax','MTG Solo Saxophones/'+rel,id,4,name,note)
for key,label in [('synth-square','Square'),('synth-fifths','Fifths'),('synth-bass-lead','Bass & Lead'),('synth-calliope','Calliope')]:
 source(key,root(key),'https://github.com/freepats/'+key,'CC0-1.0','FreePats contributors; original README retained')
 rel=next(sources[key]['root'].glob('*.sfz')).name
 path,text=prepare(key,rel)
 # Mono variants apply the documented SFZ group voice limit to every group.
 # Keep these variants next to originals so relative sample paths stay valid.
 original=(out/path).read_text();mono=original.replace('<group>','<group> polyphony=1 group=900 off_by=900 off_mode=fast')
 if '<group>' not in original:mono=mono.replace('<region>','<group> polyphony=1 group=900 off_by=900 off_mode=fast\n<region>',1)
 dest=(out/path).with_name('LP Mono.sfz');dest.write_text(mono)
 rows.extend([[key+'-poly',5,label+' — polifônico',path,60,'poly','CC0-1.0'],[key+'-mono',5,label+' — monofônico',dest.relative_to(out).as_posix(),60,'mono','CC0-1.0']])
# A dedicated bass patch from the sampled Surge synthesizer, kept with
# its samples; filtering differs from the lead and its category is Outros.
basssource=next(sources['synth-bass-lead']['destination'].glob('Synth*.sfz'))
bass=basssource.with_name('LP Synth Bass.sfz')
bass.write_text(basssource.read_text().replace('<global>','<global> fil_type=lpf_2p cutoff=1800 volume=-6'))
rows.append(['synth-bass',10,'Baixo sintetizado — Surge',bass.relative_to(out).as_posix(),40,'poly','CC0-1.0'])
for key,name in [('new-age','Pad New Age'),('sweep-pad','Pad Sweep'),('synth-pad-choir','Pad de coro sintetizado'),('synth-pad-bowed','Pad Bowed')]:
 source(key,root(key),'https://github.com/freepats/'+key,'CC0-1.0','FreePats contributors; original README retained');rel=next(sources[key]['root'].glob('*.sfz')).name;patch(key,rel,key,6,name)
source('sonor',root('SamsSonor'),'https://github.com/sfzinstruments/SamsSonor','CC-BY-SA-4.0','Sam Greene; SFZ kinwie; unmodified kit and samples')
patch('sonor','SamsSonor.sfz','sonor-kit',8,'Bateria acústica — Sam’s Sonor',36,'kit')
# Multisampled FM keyboard and percussion from the pinned original VCSL tree.
source('vcsl',a.sources/'VCSL-subset','https://github.com/sgossner/VCSL/tree/c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e','CC0-1.0','Sam Gossner / Versilian Studios and VCSL contributors')
for folder,id,label in [('FM Piano','fm-piano','FM Piano — Yamaha TX81Z'),('Piano 1','fm-digital','Digital Piano — Yamaha TX81Z'),('Clavisynth','fm-clavi','FM Clavisynth — Yamaha TX81Z')]:
 files=list((sources['vcsl']['root']/'Electrophones/TX81Z'/folder).glob('*.wav'));notes=[]
 for f in files:
  m=re.search(r'_([A-G]#?)(-?\d)_vl(\d)',f.name)
  if not m:continue
  # VCSL labels middle C as C3. Use the recording's RIFF smpl unity
  # note (60), avoiding the octave ambiguity in filename conventions.
  data=f.read_bytes();at=12;note=None
  while at+8<=len(data):
   chunk=data[at:at+4];size=struct.unpack_from('<I',data,at+4)[0]
   if chunk==b'smpl' and size>=16:note=struct.unpack_from('<I',data,at+20)[0];break
   at+=8+size+(size%2)
  if note is None:raise ValueError('Missing MIDI root note: '+str(f))
  notes.append((note,int(m[3]),f))
 if not notes:raise ValueError('No FM samples')
 centers=sorted(set(n for n,v,f in notes));text='<global> ampeg_release=0.5 volume=-9\n<group>\n'
 for note,vel,f in notes:
  i=centers.index(note);lo=0 if i==0 else (centers[i-1]+note)//2+1;hi=127 if i==len(centers)-1 else (note+centers[i+1])//2;copy('vcsl',f)
  text+=f'<region> sample=../sources/vcsl/{f.relative_to(sources["vcsl"]["root"]).as_posix()} pitch_keycenter={note} lokey={lo} hikey={hi} lovel={1 if vel==1 else 51 if vel==2 else 91} hivel={50 if vel==1 else 90 if vel==2 else 127}\n'
 generated(id,1,label,text)
percs=[('Conga','Membranophones/Struck Membranophones/Conga','Conga_HitN_v',64),('Bongôs','Membranophones/Struck Membranophones/Bongos','BongoH_Hit1_v',60),('Tamborim com platinelas (tambourine)','Idiophones/Struck Idiophones/Tambourine 1','Tamb1_Hit_v',54)]
kit='<group> loop_mode=one_shot amp_veltrack=70\n'
for i,(label,folder,prefix,key) in enumerate(percs):
 files=sorted(f for f in (sources['vcsl']['root']/folder).glob('*.wav') if f.name.startswith(prefix));text='<group> loop_mode=one_shot pitch_keytrack=0 amp_veltrack=70\n';maxvel=max(int(re.search(r'_v(\d)',f.name)[1]) for f in files)
 for f in files:
  v=int(re.search(r'_v(\d)',f.name)[1]);rr=int(re.search(r'_rr(\d)',f.name)[1]);copy('vcsl',f);sample='../sources/vcsl/'+f.relative_to(sources['vcsl']['root']).as_posix();rrcount=max(int(re.search(r'_rr(\d)',other.name)[1]) for other in files if int(re.search(r'_v(\d)',other.name)[1])==v);region=f'sample={sample} pitch_keycenter=60 lovel={(v-1)*127//maxvel+1} hivel={v*127//maxvel} seq_length={rrcount} seq_position={rr}'
  text+='<region> '+region+'\n';kit+=f'<region> {region} key={key} pitch_keytrack=0\n'
 generated('percussion-'+str(i),7,label,text)
generated('percussion-kit',7,'Kit de percussão — bongôs, conga e tambourine',kit,60,'kit')
rows.sort(key=lambda x:(x[1],x[0]))
with (out/'catalog.txt').open('w',encoding='utf-8',newline='\n') as f:
 f.write('LPHS_BANK 1\n')
 for row in rows:f.write(' '.join(json.dumps(x,ensure_ascii=False) if isinstance(x,str) else str(x) for x in row)+'\n')
credits={key:{k:v for k,v in value.items() if k not in ['root','destination']} for key,value in sources.items()}
(out/'SOURCES.json').write_text(json.dumps(credits,ensure_ascii=False,indent=2)+'\n')
(out/'CREDITS.txt').write_text('Live Performance by Hugo Sodré — banco curado inicial\n\n'+ '\n\n'.join(f'{k}: {v["author"]}\n{v["url"]}\nLicença: {v["license"]}; avisos originais na pasta sources/'+k for k,v in credits.items())+'\n\nLicenças: https://creativecommons.org/publicdomain/zero/1.0/ ; https://creativecommons.org/licenses/by/3.0/ ; https://creativecommons.org/licenses/by/4.0/ ; https://creativecommons.org/licenses/by-sa/4.0/ .\n\nAlterações: seleção de patches; mapeamento SFZ do subconjunto VCSL; layers de cordas e brass; cabeçalhos globais de Rhodes, violão Martin e sitar para sfizz; variantes mono dos leads e filtro do baixo sintetizado. Áudio sem reamostragem. Banco Sonor preservado sob CC-BY-SA-4.0, incluindo SFZ e amostras. Instrumentos e licenças independentes do host.\n')
(out/'README.txt').write_text('Live Performance by Hugo Sodré — banco inicial de 66 presets\n\nBanco de dados sonoros, sem executável. Para restaurar, extrair TODAS as partes ZIP no mesmo diretório do aplicativo portátil. A pasta soundbank deve ficar ao lado de LivePerformance.exe; manter suas subpastas. Cada parte contém arquivos complementares. Não é necessário reinstalar o aplicativo.\n\nAs licenças e créditos constam de CREDITS.txt, SOURCES.json e dos avisos originais em sources. SHA256SUMS verifica a integridade dos arquivos. Catálogo em catalog.txt. A qualidade musical será avaliada ao tocar; não confundir variantes de sintetizador com gravações dos instrumentos eletromecânicos.\n',encoding='utf-8')
# Package inventory allows future restore/integrity checks without Git sample blobs.
with (out/'SHA256SUMS').open('w') as f:
 for p in sorted(out.rglob('*')):
  if p.is_file() and p.name!='SHA256SUMS':f.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(out).as_posix()+'\n')
print(json.dumps({'instruments':len(rows),'categories':dict(collections.Counter(x[1] for x in rows)),'files':len(list(out.rglob('*')))},indent=2))
