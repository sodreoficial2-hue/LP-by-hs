#!/usr/bin/env python3
"""Apply performance controls to the curated bank, including older data ZIPs.
Only modifies SFZ/text mappings; never changes sample audio. Idempotent.
"""
import argparse,hashlib,pathlib,shlex

def configure(root):
    root=pathlib.Path(root).resolve()
    rows=[shlex.split(line) for line in (root/'catalog.txt').read_text().splitlines()[1:] if line and not line.startswith('#')]
    changed=[]
    for row in rows:
        id,category,name,relative,*_=row
        path=(root/relative).resolve()
        if not path.is_relative_to(root):raise ValueError('Path outside bank')
        text=path.read_text();updated=text
        if category=='5' and id.startswith(('synth-square-','synth-fifths-','synth-calliope-','synth-bass-lead-')):
            if 'pitchlfo_depth_oncc1=' not in text:
                updated=text.replace('<global>','<global>\n// LP: CC1 vibrato for the sampled synth lead.\npitchlfo_freq=5 pitchlfo_depth_oncc1=35',1)
        if id in ['sax-soprano','sax-alto']:
            updated=updated.replace('#define $LEGATO 64','#define $LEGATO 68 // LP: reserve pedal CC64 for sustain')
            mapping=path.parent/'Data/volpan.txt';old=mapping.read_text();new=old.replace('sustain_sw=off','sustain_sw=on').replace('sostenuto_sw=off','sostenuto_sw=on')
            if new!=old:mapping.write_text(new);changed.append(str(mapping.relative_to(root)))
        if updated!=text:path.write_text(updated);changed.append(relative)
    (root/"PERFORMANCE.txt").write_text("Live Performance — adaptações de performance MIDI, etapa 8.\nLeads curados FreePats: CC1 vibrato, 5 Hz e profundidade máxima 35 cents.\nSax soprano e alto MTG: sustain/sostenuto habilitados; legato transferido de CC64 para CC68 para liberar o pedal. Créditos e licença CC-BY-4.0 originais mantidos. Áudio original inalterado.\n",encoding="utf-8")
    return changed

def inventory(root):
    root=pathlib.Path(root)
    with (root/'SHA256SUMS').open('w') as out:
        for p in sorted(root.rglob('*')):
            if p.is_file() and p.name!='SHA256SUMS':out.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(root).as_posix()+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('bank',type=pathlib.Path);args=parser.parse_args()
    changed=configure(args.bank);inventory(args.bank);print('Configured performance mappings:',len(changed))
