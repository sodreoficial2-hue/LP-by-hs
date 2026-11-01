const {JSDOM}=require('jsdom'),fs=require('fs'),path=require('path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),w=new JSDOM(fs.readFileSync(root+'/vision-ui/index.html','utf8'),{runScripts:'outside-only',pretendToBeVisual:true}).window;
w.matchMedia=()=>({matches:true});w.CSS={escape:x=>x};w.HTMLElement.prototype.scrollIntoView=function(){};
const fixture=fs.readFileSync(__dirname+'/vision_ui_test.cjs','utf8').split('await page.addInitScript(()=>{')[1].split('\n });\n await page.goto')[0];w.eval(fixture);w.eval(fs.readFileSync(root+'/vision-ui/vision.js','utf8'));w.__setLoading(false);
try{
 const doc=w.document,by=id=>doc.getElementById(id),state=JSON.parse(JSON.stringify(w.LPVision.state));
 const emit=(peak,midi,clip=false)=>{state.peak=peak;state.midiActive=midi;state.clipping=clip;w.LPVision.receive(state);};
 const before=w.__commands.length;
 emit(0,true);assert.equal(by('vu-fill').style.width,'0%');assert(by('midi-activity').classList.contains('active'));assert.match(by('peak-label').textContent,/silêncio/);
 emit(.1,false);assert(Math.abs(parseFloat(by('vu-fill').style.width)-66.6667)<.01);assert.match(by('peak-label').textContent,/-20 dBFS/);assert.equal(by('output-meter').value,.1);assert(!by('midi-activity').classList.contains('active'));
 emit(1.2,true,true);assert.equal(by('vu-fill').style.width,'100%');assert(doc.querySelector('.vu-track.clipping'));assert.match(by('peak-label').textContent,/Pico excessivo/);
 state.audio.connected=false;emit(0,false);assert.equal(by('vu-fill').style.width,'0%');assert.equal(by('audio-label').textContent,'ASIO desconectado');assert(!doc.querySelector('.vu-track.clipping'));
 assert.equal(w.__commands.length,before,'Meter telemetry must never send notes or control commands');
 assert.equal(doc.querySelectorAll('#output-meter').length,1);assert(by('output-meter').closest('footer'));assert(by('status').classList.contains('sr-only'));
 console.log('PASS: real amplitude display, logarithmic scale, silence, clipping, MIDI independent of audio, disconnection and command-free telemetry');
}finally{w.close();}
