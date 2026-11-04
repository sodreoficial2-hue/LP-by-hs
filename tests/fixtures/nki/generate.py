"""Deterministic spec fixtures; not claimed as files exported by Kontakt.
No manufacturer samples. Python stdlib only; re-run with an output directory.
"""
import pathlib,sys,math,struct,zlib,gzip,json
from xml.etree import ElementTree as ET
root=pathlib.Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
def wav(rate=44100,channels=1,frames=None):
    frames=frames or rate*2
    pcm=b''.join(struct.pack('<h',round(0.2*32767*math.sin(2*math.pi*440*i/rate))) * channels for i in range(frames))
    fmt=struct.pack('<HHIIHH',1,channels,rate,rate*channels*2,channels*2,16)
    return b'RIFF'+struct.pack('<I',36+len(pcm))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm
def zone(parent,**fields):
    z=ET.SubElement(parent,'Zone')
    for k,v in fields.items():ET.SubElement(z,'V',name=k,value=str(v))
    return z
def document(programs):
    tree=ET.Element('K2_Container',version='0.50');ps=ET.SubElement(tree,'Programs')
    for name,fields in programs:
        p=ET.SubElement(ps,'K2_Program',name=name);groups=ET.SubElement(p,'Groups');g=ET.SubElement(groups,'K2_Group');zs=ET.SubElement(g,'Zones')
        for f in fields:zone(zs,**f)
    return ET.tostring(tree,encoding='utf-8',xml_declaration=True)
def save(name,xml,offset=73,gz=False,tail=b''):
    (root/name).write_bytes((b'' if gz else bytes(offset))+(gzip.compress(xml,mtime=0) if gz else zlib.compress(xml))+tail)
(root/'Samples').mkdir(exist_ok=True)
(root/'Samples'/'São Paulo A4.wav').write_bytes(wav())
(root/'Samples'/'Stereo 48.wav').write_bytes(wav(48000,2))
pcm=b''.join(struct.pack('>hh',round(4000*math.sin(2*math.pi*440*i/44100)),round(3000*math.sin(2*math.pi*440*i/44100))) for i in range(44100))
def chunk(tag,b):return tag+struct.pack('>I',len(b))+b+(b'\0' if len(b)%2 else b'')
comm=struct.pack('>HIH',2,44100,16)+bytes.fromhex('400eac44000000000000')
aiff=b'AIFF'+chunk(b'COMM',comm)+chunk(b'SSND',bytes(8)+pcm)
(root/'Samples'/'áudio estéreo.aiff').write_bytes(b'FORM'+struct.pack('>I',len(aiff))+aiff)
save('aiff.nki',document([('AIFF',[dict(sampleFile='Samples/áudio estéreo.aiff',rootKey=69)])]))
base=dict(sampleFile='Samples/São Paulo A4.wav',rootKey=69,lowKey=0,highKey=127,zoneTune=1)
# All modes, velocity coverage, neighbor overlap, defaults, ignored fields, Unicode.
a=dict(base,lowKey=0,highKey=70,loopMode=1,loopStart=200,loopEnd=22049,zoneVolume=0.5,zonePan=-20,fadeHighKey=4)
b=dict(base,rootKey=72,lowKey=68,highKey=127,loopMode=2,loopStart=100,loopEnd=44099,zoneVolume=1,zoneTune=2**(0.5/1200))
xml=document([('Piano São Paulo',[a,b,dict(sampleFile=''),dict(base,rootKey=999),dict(base,zoneVolume=0),dict(base,zoneVolume='nan')]),('Estéreo',[dict(sampleFile='Samples/Stereo 48.wav',opaqueField='keep me',zonePan=0)])])
save('multi.nki',xml,170);save('gzip.nki',xml,gz=True)
for offset in [0,17,255]:save(f'offset-{offset}.nki',xml,offset)
# A false candidate in the binary header must not hide the real stream.
raw=bytearray((root/'multi.nki').read_bytes());raw[3:5]=b'\x78\x01';(root/'false-candidate.nki').write_bytes(raw)
(root/'fallback.nki').write_bytes(b'UNRECOGNIZED_LEGACY_NKI')
# Flat fallback gets only this WAV via explicit sampleRoot, avoiding unrelated samples.
(root/'fallback-samples').mkdir(exist_ok=True);(root/'fallback-samples'/'A4_v64.wav').write_bytes(wav())
save('missing.nki',document([('Missing',[dict(base,sampleFile='Samples/arquivo ausente.wav')])]))
save('monolith.nki',document([('Embedded',[dict(base,sampleFile='embedded.wav')])]),tail=wav())
save('malformed-xml.nki',b'<K2_Container><Programs>')
save('compressed-corrupt.nki',b'<K2_Container/>')
bad=bytearray((root/'compressed-corrupt.nki').read_bytes());bad[-3]^=0xff;(root/'compressed-corrupt.nki').write_bytes(bad)
save('expansion-limit.nki',b' '*(16*1024*1024+1))
save('dtd.nki',b'<!DOCTYPE K2_Container [<!ENTITY x SYSTEM "file:///etc/passwd">]><K2_Container/>')
(root/'truncated.nki').write_bytes((root/'multi.nki').read_bytes()[:-8])
(root/'project.nsmpproj').write_text('sample project, not CBIN')
# Two RR alternatives for each of two exhaustive, disjoint velocity layers.
rr=[]
for lo,hi in [(0,63),(64,127)]:
    for pos in [1,2]:rr.append(dict(base,lowVelo=lo,highVelo=hi,seqLength=2,seqPosition=pos,loopMode=1,loopStart=100,loopEnd=44099))
save('rr.nki',document([('RR',rr)]))
save('gap.nki',document([('Gap',[dict(base,lowVelo=1,highVelo=50),dict(base,lowVelo=64,highVelo=127)])]))
save('overlap-velocity.nki',document([('Overlap',[dict(base,lowVelo=1,highVelo=80),dict(base,lowVelo=64,highVelo=127)])]))
manifest={p.name:{'bytes':p.stat().st_size} for p in root.glob('*.nki')}
(root/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('Generated',len(manifest),'spec NKI fixtures')
