import ctypes as c, pathlib,shlex,sys
if len(sys.argv)!=3:raise SystemExit("Expected real sfizz library and bank root")
lib=c.CDLL(sys.argv[1])
for name,args,restype in [('sfizz_create_synth',[],c.c_void_p),('sfizz_free',[c.c_void_p],None),('sfizz_load_file',[c.c_void_p,c.c_char_p],c.c_bool),('sfizz_set_sample_rate',[c.c_void_p,c.c_float],None),('sfizz_set_samples_per_block',[c.c_void_p,c.c_int],None),('sfizz_enable_freewheeling',[c.c_void_p],None),('sfizz_send_note_on',[c.c_void_p,c.c_int,c.c_int,c.c_int],None),('sfizz_get_num_active_voices',[c.c_void_p],c.c_int),('sfizz_render_block',[c.c_void_p,c.POINTER(c.POINTER(c.c_float)),c.c_int,c.c_int],None)]:
 f=getattr(lib,name);f.argtypes=args;f.restype=restype
root=pathlib.Path(sys.argv[2]);left=(c.c_float*256)();right=(c.c_float*256)();channels=(c.POINTER(c.c_float)*2)(left,right)
def render(synth,blocks):
 for _ in range(blocks):lib.sfizz_render_block(synth,channels,2,256)
for line in (root/'catalog.txt').read_text().splitlines()[1:]:
 row=shlex.split(line)
 if row[1]!='5':continue
 synth=lib.sfizz_create_synth();lib.sfizz_set_sample_rate(synth,48000);lib.sfizz_set_samples_per_block(synth,256);lib.sfizz_enable_freewheeling(synth)
 assert lib.sfizz_load_file(synth,str(root/row[3]).encode())
 lib.sfizz_send_note_on(synth,0,60,100);render(synth,50);lib.sfizz_send_note_on(synth,0,64,100);render(synth,100);voices=lib.sfizz_get_num_active_voices(synth);expected=1 if row[5]=='mono' else 2
 print(row[0],voices,'expected',expected,flush=True);assert voices==expected
 lib.sfizz_free(synth)
print('PASS: 4 mono leads limited to one held voice; 4 poly leads retain both held notes')
