// SPDX-License-Identifier: AGPL-3.0-or-later
// Generate test instruments with the PUBLIC OpenNord workshop writers and decode
// them with its unmodified algorithms. Requires Node 24+ and the pinned checkout.
// This is NOT a recording exported by Nord hardware. No factory sample audio.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';
const upstream=path.resolve(process.argv[2]||'../upstream-nord');
const output=path.resolve(process.argv[3]||'tests/fixtures/nord');
// ESM requires explicit extensions. Only import specifiers are adjusted, not algorithms.
const patchImports=dir=>{for(const e of fs.readdirSync(dir,{withFileTypes:true})){const p=path.join(dir,e.name);if(e.isDirectory())patchImports(p);else if(p.endsWith('.ts')){let s=fs.readFileSync(p,'utf8');s=s.replace(/(from ['"])(\.[^'"]+)(['"])/g,(_,a,b,c)=>a+b+(b.endsWith('.ts')?'':'.ts')+c);fs.writeFileSync(p,s);}}};
patchImports(path.join(upstream,'src/lib'));
fs.writeFileSync(path.join(upstream,'package.json'),'{"type":"module"}\n');
const mod=async p=>import(pathToFileURL(path.join(upstream,'src/lib',p)));
const {writeNsmpMulti}=await mod('ns4/nsmp-write.ts');
const {writeOgNsmp}=await mod('ns4/nsmp-og.ts');
const {writeCodec2Nsmp}=await mod('ns4/nsmp-codec2-write.ts');
const {readNsmp,parseNsmpSections,readNsmpZones,decodeNsmp}=await mod('ns4/nsmp.ts');
const {patchNs4Checksum}=await mod('clavia/checksum.ts');
const {sealTrailerChecksum}=await mod('clavia/crc16.ts');
const pcm=(n,amp,period)=>Int32Array.from({length:n},(_,i)=>Math.round(amp*Math.sin(2*Math.PI*i/period)+amp*.13*Math.sin(4*Math.PI*i/period)));
const channels=[pcm(4096,6000,79),pcm(4096,4000,113)];
const meta={loop:{loops:true,loopStart:1024,altStart:256},decayRaw:0x800000};
const variants=[
 ['workshop-og.nsmp',writeOgNsmp({zones:[{channels:[channels[0]],globalID:673,rootKey:60,keyHigh:71},{channels,globalID:900,rootKey:72,keyHigh:96}]})],
 ['workshop-v2.nsmp',writeCodec2Nsmp({name:'LP workshop codec2',zones:[{channels:[channels[0]],globalID:673,rootKey:60,keyHigh:71,stroke:meta},{channels,globalID:900,rootKey:72,keyHigh:96}]})],
];
const level=(gain,tune)=>{const r=new Uint8Array(6);let g=Math.round(gain*1048576);r[0]=g>>>16;r[1]=g>>>8;r[2]=g;let t=Math.trunc(tune*256/100)&0xffffff;r[3]=t>>>16;r[4]=t>>>8;r[5]=t;return r;};
const perNote=Array.from({length:128},()=>level(1,0));perNote[61]=level(.5,-25);
const v3=writeNsmpMulti({name:'LP workshop codec3',codec:3,globalLevelDetune:level(.5,100),perNote,zones:[{channels,rootKey:60,keyLow:40,keyHigh:72,velLow:0,velTop:60,stroke:meta},{channels:[channels[0]],rootKey:72,keyLow:73,keyHigh:90,velLow:61,velTop:127}]});variants.push(['workshop-v3.nsmp3',v3]);
const v4=writeNsmpMulti({name:'LP workshop codec4',codec:4,layeredZones:true,zones:[{channels,rootKey:60,keyLow:48,keyHigh:72,velLow:0,velTop:63,stroke:meta},{channels:[channels[0]],rootKey:60,keyLow:48,keyHigh:72,velLow:64,velTop:127,stroke:meta},{channels:[channels[1]],rootKey:84,keyLow:73,keyHigh:96,velLow:0,velTop:127,zoneMode:1,zoneIsOneShot:1}]});variants.push(['workshop-v4.nsmp4',v4]);
let f0=new Uint8Array(24+v3.length-44+2);f0.set(v3.subarray(0,24));f0[4]=0;f0.set(v3.subarray(44),24);sealTrailerChecksum(f0);variants.push(['workshop-v3-format0.nsmp3',f0]);
// Codec3 map v12: exact upstream documented 11-byte stroke-reference records.
const sections=parseNsmpSections(v3);const parts=[v3.subarray(0,44)];
for(const s of sections){if(!s.tag.endsWith('map')){parts.push(v3.subarray(s.payloadOffset-12,s.endOffset));continue;}
 let payload=new Uint8Array(6+768+2*11);payload.set(v3.subarray(s.payloadOffset,s.payloadOffset+6+768));
 for(let i=0;i<2;i++){let o=6+768+i*11;payload[o]=0;payload[o+1]=1;payload[o+3]=i?90:72;payload[o+6]=1;new DataView(payload.buffer).setUint32(o+7,i+1,false);}
 let block=new Uint8Array(12+payload.length);block.set(v3.subarray(s.payloadOffset-12,s.payloadOffset-8));let dv=new DataView(block.buffer);dv.setUint32(4,12,false);dv.setUint32(8,payload.length,false);block.set(payload,12);parts.push(block);
}
let v12=new Uint8Array(parts.reduce((n,p)=>n+p.length,0));let at=0;for(const p of parts){v12.set(p,at);at+=p.length;}v12=patchNs4Checksum(v12);variants.push(['workshop-v3-map12.nsmp3',v12]);
fs.mkdirSync(output,{recursive:true});const hash=b=>createHash('sha256').update(b).digest('hex');const manifest=[];let golden=[];
for(const [name,bytes] of variants){fs.writeFileSync(path.join(output,name),bytes);const file=readNsmp(bytes),sections=parseNsmpSections(bytes),zones=readNsmpZones(bytes),strokes=decodeNsmp(bytes);if(!strokes.length||!zones.length)throw Error('upstream produced no decoded data: '+name);
 let item={name,sha256:hash(bytes),codec:file.codec,legacy:file.legacy,sections,zones,strokes:[]};
 golden.push(`FILE ${name} ${file.codec||1} ${sections.length} ${zones.length} ${strokes.length}`);
 for(const s of sections)golden.push(`SECTION ${s.tag} ${s.version} ${s.payloadOffset} ${s.size}`);
 for(const z of zones)golden.push(`ZONE ${z.globalID} ${z.rootKey} ${z.keyLow} ${z.keyHigh} ${z.velLow} ${z.velTop}`);
 for(const [index,s] of strokes.entries()){let data=Buffer.alloc(s.channels.reduce((n,c)=>n+c.length,0)*4);let o=0;for(const c of s.channels)for(const x of c){data.writeInt32LE(x,o);o+=4;}let pcmName=`${name}.stroke${index}.pcm`;fs.writeFileSync(path.join(output,pcmName),data);item.strokes.push({id:s.globalID,rate:s.sampleRate,channels:s.channelCount,frames:s.channels[0].length,loop:s.loop,pcm:pcmName,sha256:hash(data)});
 golden.push(`STROKE ${s.globalID} ${s.sampleRate} ${s.channelCount} ${s.channels[0].length} ${s.loop?1:0} ${s.loop?.loops?1:0} ${s.loop?.loopStart||0} ${s.loop?.loopEnd||0} ${s.loop?.altStart||0} ${pcmName}`);
 }
 manifest.push(item);
}
fs.writeFileSync(path.join(output,'golden.txt'),golden.join('\n')+'\n');
fs.writeFileSync(path.join(output,'manifest.json'),JSON.stringify({upstream:'https://github.com/simonflore/opennord',commit:'67ba1b657ff65a803d568f0ba1fba08598fdc830',provenance:'Generated synthetic user audio through upstream workshop writers, not a hardware-origin recording',fixtures:manifest},null,2)+'\n');
console.log(manifest.map(f=>`${f.name}: ${f.zones.length} zones, ${f.strokes.length} decoded strokes`).join('\n'));
