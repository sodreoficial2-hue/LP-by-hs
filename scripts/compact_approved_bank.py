#!/usr/bin/env python3
"""Keep only the SFZ instruments approved for the compact stage build.
Run against an already assembled soundbank. Never fabricate missing samples.
"""
import argparse, pathlib, shlex, shutil, sys
p=argparse.ArgumentParser()
p.add_argument("source",type=pathlib.Path)
p.add_argument("destination",type=pathlib.Path)
a=p.parse_args()
keep={"piano-c5","ep-cp80","ep-pianet","ep-rhodes","ep-wurlitzer",
"cellos","contrabass","strings-ensemble","strings-pizz","violas","violin","violins",
"new-age","sweep-pad","synth-pad-bowed","synth-pad-choir","sonor-kit","accordion",
"bass-finger","bassoon","clarinet","flute","guitar","guitar-steel","harp","oboe","piccolo","sitar","synth-bass"}
src=a.source.resolve();dst=a.destination.resolve()
if src==dst or src in dst.parents:sys.exit("Destination must be outside source")
lines=(src/"catalog.txt").read_text(encoding="utf-8-sig").splitlines()
chosen=[]
for line in lines[1:]:
 if not line.strip():continue
 fields=shlex.split(line)
 if fields[0] in keep:chosen.append((line,fields))
missing=keep-{fields[0] for _,fields in chosen}
if missing:sys.exit("Missing approved presets: "+", ".join(sorted(missing)))
if dst.exists():sys.exit("Destination already exists")
dst.mkdir(parents=True)
# Preserve original directory hierarchy so relative SFZ sample paths work.
# Source packages may share directories, so copy the source roots of selected patches.
roots=set()
for line,fields in chosen:
 rel=pathlib.Path(fields[3])
 if not (src/rel).is_file():sys.exit("Missing instrument: "+str(rel))
 if rel.parts[0]=="sources":roots.add(pathlib.Path(*rel.parts[:2]))
 else:roots.add(pathlib.Path("patches"))
for root in sorted(roots):
 if (src/root).is_dir():shutil.copytree(src/root,dst/root)
(dst/"catalog.txt").write_text("LPHS_BANK 1\n"+"\n".join(line for line,_ in chosen)+"\n",encoding="utf-8")
for name in ("CREDITS.txt","SOURCES.json","README.txt"):
 if (src/name).exists():shutil.copy2(src/name,dst/name)
print("Selected",len(chosen),"SFZ instruments; sources copied:",len(roots))
