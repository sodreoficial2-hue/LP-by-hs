// SPDX-License-Identifier: AGPL-3.0-or-later
// Independent reference encoder/decoder; attribution in third_party/npno/NOTICE.md.
// Node >=24. Argument: upstream checkout at the revision pinned in NOTICE.md.
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
const upstream=process.argv[2];if(!upstream)throw Error('usage: node generate.mjs REFERENCE_CHECKOUT');
const temporary=await fs.mkdtemp(path.join(os.tmpdir(),'lp-npno-oracle-'));
const target=path.dirname(fileURLToPath(import.meta.url));
try {
 const paths=['src/lib/ns4/nsp-audio.ts','src/lib/ns4/nsp-synth.testing.ts','src/lib/clavia/checksum.ts','src/lib/clavia/crc16.ts','src/lib/clavia/cbin.ts'];
 for(const name of paths){let source=await fs.readFile(path.join(upstream,name),'utf8');source=source.replace(/(from\s+['"])(\.[^'"]+)(['"])/g,(_,a,b,c)=>a+b+(b.endsWith('.ts')?'':'.ts')+c);const file=path.join(temporary,name);await fs.mkdir(path.dirname(file),{recursive:true});await fs.writeFile(file,source);}
 await fs.writeFile(path.join(temporary,'package.json'),'{"type":"module"}');
 const {makeNpno,tone}=await import(pathToFileURL(path.join(temporary,paths[1])));
 const {readNspDirectory,decodeNspStroke}=await import(pathToFileURL(path.join(temporary,paths[0])));
 const {sealFileChecksum}=await import(pathToFileURL(path.join(temporary,paths[2])));
 const manifest=[];
 const oracle=async(name,bytes)=>{
  const dir=readNspDirectory(bytes);if(!dir)throw Error('reference directory rejected');
  await fs.writeFile(path.join(target,name),bytes);
  const golden=[];for(const stroke of dir.strokes){const lanes=decodeNspStroke(bytes,dir,stroke);const raw=Buffer.alloc(lanes.length*stroke.frames*2);let at=0;for(const lane of lanes)for(const value of lane){if(value<-32768||value>32767)throw Error('oracle exceeds PCM16');raw.writeInt16LE(value,at);at+=2;}const file=name+'.'+stroke.index+'.pcm';await fs.writeFile(path.join(target,file),raw);golden.push({index:stroke.index,frames:stroke.frames,channels:dir.channels,file,sha256:createHash('sha256').update(raw).digest('hex')});}
  manifest.push({file:name,sha256:createHash('sha256').update(bytes).digest('hex'),origin:name==='tine-demo.npno'?'public synthesized demo':'independent upstream synthetic encoder',golden});
 };
 await oracle('tine-demo.npno',await fs.readFile(path.join(target,'tine-demo.npno')));
 for(const [name,channels,base] of [['layers-v5.npno',1,24],['layers-v6.npno',2,44]]){
  const strokes=[];const roots=[];const banks=[];
  for(const root of [48,72])for(const bank of [0,1,2])for(let layer=0;layer<3;++layer){
   strokes.push({pcm:Array.from({length:channels},(_,c)=>tone(1800,(c?1.001:1)*220*Math.pow(2,(root-57)/12)/35002,180+layer*60,root*99+bank*7+layer+c)),before:Array.from({length:channels},()=>[0,0,0,0]),velocityFrom:layer*(base===24?12:60),...(bank===2?{}:{loop:{start:100,length:800}})});roots.push(root);banks.push(bank);
  }
  let bytes=makeNpno(strokes,channels,base);const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),version=base===24?0x450:0x464;
  view.setUint16(base+4,version);view.setUint16(base+0x622+48*2,9);view.setUint16(base+0x622+72*2,9);
  bytes[base+0x40c]=10;bytes[base+0x40d]=127;bytes[base+0x18c+61]=3;
  for(let i=0;i<strokes.length;++i){const r=base+0x732+i*0x76;bytes[r+4]=banks[i];view.setInt16(r+0x34,base===24?2:-24);bytes[r+0x74]=[0,30,90][i%3];view.setUint32(r+0x2e,0x800000);for(let k=0;k<14;++k)view.setUint32(r+0x36+k*4,0x800000);}
  bytes=sealFileChecksum(bytes);await oracle(name,bytes);
 }
 await fs.writeFile(path.join(target,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');
 const lines=[];for(const m of manifest){lines.push(`FILE ${m.file} ${m.golden.length}`);for(const g of m.golden)lines.push(`PCM ${g.index} ${g.frames} ${g.channels} ${g.file}`);}await fs.writeFile(path.join(target,'golden.txt'),lines.join('\n')+'\n');
}finally{await fs.rm(temporary,{recursive:true,force:true});}
