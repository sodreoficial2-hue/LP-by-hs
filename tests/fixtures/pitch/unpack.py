"""Restore independent binary pitch vectors without downloads or reference tools."""
import pathlib,json,base64,zlib,hashlib,sys
source=pathlib.Path(__file__).with_name('reference.json')
r=json.loads(source.read_text());dest=pathlib.Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
for name,item in r['files'].items():
    if pathlib.Path(name).name!=name:raise ValueError('invalid fixture path')
    data=zlib.decompress(base64.b64decode(item['zlib_base64']))
    if len(data)!=item['bytes'] or hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('fixture hash mismatch')
    (dest/name).write_bytes(data)
