const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../vision-ui/vision.js'),'utf8');
const start=source.indexOf(' function profiles(){'),end=source.indexOf('\n let partCategories',start);
assert(start>=0&&end>start);
const cards=[],sent=[];const context={title(){},node(tag,attrs,...children){return {tag,attrs,children};},button(children,click){const card={name:children[0].children[0],click};cards.push(card);return card;},send(...args){sent.push(args);}};
vm.createContext(context);vm.runInContext(source.slice(start,end),context);context.profiles();
const create=cards.find(c=>c.name==='Criar timbre de áudio');assert(create);create.click();assert.deepEqual(sent,[['createSample']]);
assert(cards.some(c=>c.name==='Importar SF2'));assert(cards.some(c=>c.name==='Importar SFZ'));
console.log('PASS: production sampler button sends createSample and preserves existing import entries');
