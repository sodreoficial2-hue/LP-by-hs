#!/usr/bin/env python3
"""Stage additional free SF2 soundfonts from official FreePats links, separately from approved bank."""
import pathlib,urllib.request,urllib.parse,html.parser,sys
PAGE="https://freepats.zenvoid.org/SoundSets/general-midi.html"
ROOT=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else "soundbank/additional-sf2")
class Links(html.parser.HTMLParser):
 def __init__(self): super().__init__();self.links=[]
 def handle_starttag(self,tag,attrs):
  if tag=="a":
   url=dict(attrs).get("href","")
   if url:self.links.append(urllib.parse.urljoin(PAGE,url))
req=urllib.request.Request(PAGE,headers={"User-Agent":"Mozilla/5.0"})
with urllib.request.urlopen(req,timeout=45) as f: content=f.read().decode("utf-8","replace")
p=Links();p.feed(content)
candidates=[x for x in p.links if ".sf2" in x.lower() or (x.lower().endswith(".zip") and ("gm" in x.lower() or "perc" in x.lower()))]
print("Available direct candidate links:",*candidates,sep="\n")
ROOT.mkdir(parents=True,exist_ok=True)
for label,term in [("FreePats-General-MIDI","general"),("FreePats-Percussion","percussion")]:
 choices=[x for x in candidates if (term in x.lower() or (term=="general" and "gm" in x.lower())) and ".sf2" in x.lower()]
 if not choices:
  print("No unambiguous SF2 link for",label,"- manual review required");continue
 url=choices[0];suffix=".sf2" if ".sf2" in url.lower() else ".zip";target=ROOT/(label+suffix)
 print("Downloading",label,url,flush=True)
 try:
  with urllib.request.urlopen(urllib.request.Request(url,headers={"User-Agent":"Mozilla/5.0"}),timeout=90) as response,target.open("wb") as out:
   while True:
    chunk=response.read(1024*1024)
    if not chunk:break
    out.write(chunk)
  if suffix==".sf2" and target.open("rb").read(4)!=b"RIFF":raise ValueError("Not RIFF SF2")
  print("Saved",target,target.stat().st_size)
 except Exception as exc:
  target.unlink(missing_ok=True);print("Skipped",label,str(exc))
