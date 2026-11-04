// Independent producer: OpenNord c86b22a8014afcc9d6dc96409992f10f3c004f57, AGPL-3.0-or-later.
// Run only on a disposable reference checkout: import extensions are adjusted.
import fs from 'node:fs';import path from 'node:path';import {pathToFileURL} from 'node:url';import {createHash} from 'node:crypto';import {deflateSync} from 'node:zlib';
const upstream=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
function patch(dir){for(const f of fs.readdirSync(dir,{withFileTypes:true})){const p=path.join(dir,f.name);if(f.isDirectory())patch(p);else if(p.endsWith('.ts'))fs.writeFileSync(p,fs.readFileSync(p,'utf8').replace(/(from ['"])(\.[^'"]+)(['"])/g,(_,a,b,c)=>a+b+(b.endsWith('.ts')?'':'.ts')+c));}}
patch(path.join(upstream,'src/lib'));fs.writeFileSync(path.join(upstream,'package.json'),'{"type":"module"}');
const mod=p=>import(pathToFileURL(path.join(upstream,'src/lib',p)));
const {writeNsmpMulti}=await mod('ns4/nsmp-write.ts');const {writeOgNsmp}=await mod('ns4/nsmp-og.ts');const {writeCodec2Nsmp}=await mod('ns4/nsmp-codec2-write.ts');const {makeNpno}=await mod('ns4/nsp-synth.testing.ts');const {sealFileChecksum}=await mod('clavia/checksum.ts');
const rate=35002,pcm=Array.from({length:rate},(_,i)=>Math.round(6000*Math.sin(2*Math.PI*440*i/rate))),channels=[Int32Array.from(pcm)];
const files={
 'pitch-og.nsmp':writeOgNsmp({zones:[{channels,globalID:1,rootKey:69,keyHigh:127}]}),
 'pitch-v2.nsmp':writeCodec2Nsmp({name:'Reference 440',zones:[{channels,globalID:1,rootKey:69,keyHigh:127,stroke:{pitchBase:rate}}]}),
 'pitch-v3.nsmp3':writeNsmpMulti({name:'Reference 440',codec:3,zones:[{channels,rootKey:69,keyLow:0,keyHigh:127,velLow:0,velTop:127,stroke:{pitchBase:rate}}]}),
 'pitch-v4.nsmp4':writeNsmpMulti({name:'Reference 440',codec:4,zones:[{channels,rootKey:69,keyLow:0,keyHigh:127,velLow:0,velTop:127,stroke:{pitchBase:rate}}]})
};
for(const [name,base] of [['pitch-v5.npno',24],['pitch-v6.npno',44]]){
 const bytes=makeNpno([{pcm:[pcm],before:[[0,0,0,0]],velocityFrom:0}],1,base),v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
 v.setUint16(base+4,base===24?0x450:0x464);v.setUint16(base+0x622+69*2,1);for(let key=0;key<128;++key)bytes[base+0x8c+key]=69;
 files[name]=sealFileChecksum(bytes);
}
const result={producer_commit:'c86b22a8014afcc9d6dc96409992f10f3c004f57',source_rate:rate,frequency_hz:440,amplitude_pcm16:6000,files:Object.fromEntries(Object.entries(files).map(([name,bytes])=>[name,{sha256:createHash('sha256').update(bytes).digest('hex'),bytes:bytes.length,zlib_base64:deflateSync(bytes).toString('base64')}]))};
fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
