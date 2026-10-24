#!/usr/bin/env python3
"""Bundle the original opening and a title-only edit for SVG composition.

The UI overlays only the two title rectangles from the edited image. All other
areas, including the portrait, signature and waveform, use the original image.
"""
import argparse,base64,hashlib,json,struct,zipfile
from pathlib import Path

def image_info(data):
    if data.startswith(b'\x89PNG\r\n\x1a\n'):
        return 'image/png',struct.unpack('>II',data[16:24])
    if data.startswith(b'\xff\xd8'):
        offset=2
        while offset<len(data):
            while data[offset]==255:offset+=1
            marker=data[offset];offset+=1
            length=struct.unpack('>H',data[offset:offset+2])[0]
            if marker in (0xc0,0xc1,0xc2,0xc3,0xc5,0xc6,0xc7,0xc9,0xca,0xcb,0xcd,0xce,0xcf):
                height,width=struct.unpack('>HH',data[offset+3:offset+7])
                return 'image/jpeg',(width,height)
            offset+=length
    raise ValueError('Expected PNG or JPEG image data')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('vision_package',type=Path)
    parser.add_argument('title_edit',type=Path)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'vision-ui')
    args=parser.parse_args()
    with zipfile.ZipFile(args.vision_package) as archive:
        name=next(n for n in archive.namelist() if n.endswith('resources/app/package.json'))
        prefix=name[:-len('package.json')]
        if json.loads(archive.read(name)).get('version')!='2.1.0':
            raise ValueError('Use the original Vision Laiberry 2.1.0 package')
        original=archive.read(prefix+'out/splash-vision-2026.png')
    edited=args.title_edit.read_bytes()
    sizes={'original':image_info(original)[1],'titleEdit':image_info(edited)[1]}
    content={key:'data:'+image_info(value)[0]+';base64,'+base64.b64encode(value).decode('ascii') for key,value in [('original',original),('titleEdit',edited)]}
    content['width']=sizes['original'][0];content['height']=sizes['original'][1]
    content['title']='Live Performance'
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'splash-assets.js').write_text('/* Title-only opening composition. Original photo and background are retained. */\nwindow.LPVisionSplash=Object.freeze('+json.dumps(content,separators=(',',':'))+');\n')
    manifest={'title':'Live Performance','original_version':'Vision Laiberry 2.1.0','original_sha256':hashlib.sha256(original).hexdigest(),'title_edit_sha256':hashlib.sha256(edited).hexdigest(),'sizes':sizes,'title_clip_rectangles':[[550,140,770,120],[565,250,875,95]],'preserved_waveform_rectangle':[1320,140,120,120],'composition':'Original image everywhere; edited image only inside title rectangles, with the original waveform restored on top. No image pixels were recomposed on disk.','image_edit_prompt':'Replace only vision laiberry / INSTRUMENTS with Live / Performance. Preserve the portrait, studio, keyboard, light waves, loading ring, by Hugo Sodré and Acessibilidade para baixa visão.','image_edit_method':'Built-in image generation editor; title-only SVG clipping in the application.'}
    (args.output/'splash-provenance.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    print('Opening resources bundled:',args.output/'splash-assets.js')
if __name__=='__main__':main()
