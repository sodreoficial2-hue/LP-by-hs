"""Reject a classic-only executable or incomplete Vision portable before delivery."""
import argparse
import json
from pathlib import Path
import struct


def verify(folder):
    folder = Path(folder)
    required = ['LivePerformance.exe', 'WebView2Loader.dll', 'sfizz.dll',
                'sndfile.dll', 'SDL3.dll', 'libfluidsynth-3.dll',
                'vision-ui/index.html', 'vision-ui/vision.js', 'vision-ui/vision.css',
                'vision-ui/assets.js', 'vision-ui/splash-assets.js']
    missing = [name for name in required if not (folder / name).is_file()]
    if missing:
        raise ValueError('Missing Vision portable files: ' + ', '.join(missing))
    for name in ['LivePerformance.exe', 'WebView2Loader.dll']:
        data = (folder / name).read_bytes()
        if data[:2] != b'MZ' or len(data) < 64:
            raise ValueError('Invalid executable: ' + name)
        offset = struct.unpack_from('<I', data, 0x3c)[0]
        if data[offset:offset + 4] != b'PE\0\0' or struct.unpack_from('<H', data, offset + 4)[0] != 0x8664:
            raise ValueError('Not Windows x64: ' + name)
    marker = 'https://lp-vision.local/index.html'.encode('utf-16le')
    if marker not in (folder / 'LivePerformance.exe').read_bytes():
        raise ValueError('Main executable does not contain the compiled Vision frontend')
    index = (folder / 'vision-ui/index.html').read_text(encoding='utf-8')
    for name in ['vision.js', 'vision.css', 'assets.js', 'splash-assets.js']:
        if name not in index:
            raise ValueError('Interface resource is not referenced: ' + name)
    return {'result': 'PASS', 'main_executable': 'LivePerformance.exe',
            'vision_compiled_marker': True, 'required_files': required,
            'scope': 'Static package verification, not Windows/WebView2/ASIO execution'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder')
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.folder), indent=2))
    except (ValueError, OSError, struct.error) as error:
        parser.exit(1, str(error) + '\n')
