#!/usr/bin/env python3
"""Download the five FreePats CC0 SF2 evaluation instruments; no paid/private packs."""
import pathlib, subprocess, tempfile, urllib.request, shutil, sys
ROOT=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else "soundbank/sf2").resolve()
BANKS=[
 ("DX7-Electric-Piano-1","https://github.com/freepats/fm-piano1/releases/download/2019-09-16/FM-Piano1-SF2-20190916.7z"),
 ("DX7-Electric-Piano-2","https://github.com/freepats/fm-piano2/releases/download/2016-11-12/FM-Piano2-SF2-20161112.7z"),
 ("DX7-Brass-7","https://github.com/freepats/synth-brass-2/releases/download/2024-06-10/SynthBrass2-SF2-20240610.7z"),
 ("TX81Z-Lately-Bass","https://github.com/freepats/lately-bass/releases/download/2024-04-09/LatelyBass-SF2-20240409.7z"),
 ("DX7-Synth-Bass","https://github.com/freepats/synth-bass-2/releases/download/2021-04-05/SynthBass2-SF2-20210405.7z"),
]
ROOT.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
 for label,url in BANKS:
  print("Downloading",label,flush=True)
  archive=pathlib.Path(tmp)/(label+".7z")
  request=urllib.request.Request(url,headers={"User-Agent":"LPbyHS-personal-evaluation/1.0"})
  with urllib.request.urlopen(request,timeout=90) as response, archive.open("wb") as output:
   shutil.copyfileobj(response,output)
  dest=pathlib.Path(tmp)/label
  subprocess.run(["7z","x","-y",str(archive),"-o"+str(dest)],check=True,stdout=subprocess.DEVNULL)
  sf2s=list(dest.rglob("*.sf2"))
  if len(sf2s)!=1: raise RuntimeError(f"{label}: expected one SF2, got {len(sf2s)}")
  shutil.copy2(sf2s[0],ROOT/(label+".sf2"))
  print(label, (ROOT/(label+".sf2")).stat().st_size,"bytes",flush=True)
(ROOT/"FONTES-E-LICENCAS.txt").write_text("FreePats CC0 1.0 soundfonts for personal evaluation.\n"+"\n".join(f"{name}: {url}" for name,url in BANKS)+"\n",encoding="utf-8")
