"""Use a synthetic sine SF2 with about 10 dB of measured preset separation."""
from pathlib import Path
import importlib.util
import struct
import array
import subprocess
import sys

executable, fluid_library, destination, sfizz_test_double = sys.argv[1:]
folder = Path(destination).resolve()
folder.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location("sf2_fixture", Path(__file__).with_name("real_sf2_glide_test.py"))
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
bank = folder / "levels.sf2"
fixture.fixture(bank)
data = bytearray(bank.read_bytes())
# Scale the synthetic source into the supported correction range, then add
# attenuation to only the second preset. Assert the actual rendered difference.
sample_offset = data.index(b"smpl")
size = struct.unpack_from("<I", data, sample_offset + 4)[0]
samples = array.array("h")
samples.frombytes(data[sample_offset + 8:sample_offset + 8 + size])
for index in range(len(samples)):
    samples[index] = int(samples[index] * 3.2)
data[sample_offset + 8:sample_offset + 8 + size] = samples.tobytes()
offset = data.index(b"igen") + 8 + 7 * 4
struct.pack_into("<Hh", data, offset, 48, 240)
bank.write_bytes(data)
alias = folder / "libfluidsynth-3.dll"
if alias.is_symlink():
    alias.unlink()
alias.symlink_to(Path(fluid_library).resolve())
sfizz_alias = folder / "sfizz-test-double.so"
if sfizz_alias.is_symlink():
    sfizz_alias.unlink()
sfizz_alias.symlink_to(Path(sfizz_test_double).resolve())
cache = folder / "real-levels.cache"
if cache.exists():
    cache.unlink()
subprocess.run([executable, str(sfizz_alias), str(bank), str(folder)], check=True)
