"""Independent structural dump of the pinned test vectors (standard library only)."""
import collections
import binascii
import zlib
import hashlib
import json
import pathlib
import struct


def inspect(path):
    data = path.read_bytes()
    le = lambda at, code: struct.unpack_from('<' + code, data, at)[0]
    be = lambda at, code: struct.unpack_from('>' + code, data, at)[0]
    base = 44 if le(4, 'I') == 1 else 24
    count, channels = be(base + 0x620, 'H'), be(base + 0x61e, 'H')
    strokes, widths, orders = [], collections.Counter(), collections.Counter()
    for i in range(count):
        record = base + 0x732 + i * 118
        start = base + be(record, 'I')
        blocks = be(record + 10, 'H')
        for block in range(blocks):
            header = be(start + block * 1022 * channels, 'H')
            widths[header & 31] += 1
            orders[(header >> 5) & 7] += 1
        strokes.append(dict(index=i, record_offset=record, audio_offset=start,
                            bank=data[record + 4], layer=data[record + 5],
                            frames=be(record + 6, 'I'), blocks=blocks,
                            loop_start=be(record + 0x1c, 'I'),
                            loop_length=be(record + 0x20, 'I'),
                            trim=be(record + 0x34, 'h'),
                            threshold_byte=data[record + 0x74],
                            record_hex=data[record:record + 118].hex()))
    runs = []
    for key, root in enumerate(data[base + 0x8c:base + 0x10c]):
        if runs and runs[-1]['root'] == root:
            runs[-1]['high'] = key
        else:
            runs.append(dict(low=key, high=key, root=root))
    frames = sum(s['frames'] for s in strokes)
    return dict(file=path.name, size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                envelope=le(4, 'I'), content_version=le(20, 'I'), body_offset=base,
                stream_version=hex(be(base + 4, 'H')), channels=channels,
                name_raw=data[base + 0x1c:base + 0x3c].hex(),
                global_gain_tenths_db=struct.unpack_from('b', data, base + 0x40c)[0],
                damper_top=data[base + 0x40d],
                fine_tune_nonzero=[dict(key=n, raw=struct.unpack_from('b', data, base + 0x18c + n)[0]) for n in range(128) if data[base + 0x18c + n]],
                checksum_valid=(zlib.crc32(data[44:]) == le(24, 'I') if base == 44 else binascii.crc_hqx(data[:-2], 65535) == le(len(data)-2, 'H')),
                key_map=runs,
                roots=[dict(root=n, count=be(base + 0x622 + n * 2, 'H'))
                       for n in range(128) if be(base + 0x622 + n * 2, 'H')],
                sample_rate_reference_constant=35002, pcm_bits_reference_codec=16,
                total_frames=frames, total_seconds=frames / 35002,
                pcm_bytes=frames * channels * 2,
                compressed_bytes=sum(s['blocks'] * 1022 * channels for s in strokes),
                block_width_histogram=dict(widths), block_order_histogram=dict(orders),
                strokes=strokes)


if __name__ == '__main__':
    root = pathlib.Path(__file__).parent
    result = [inspect(p) for p in sorted(root.glob('*.npno'))]
    docs = root.parents[2] / 'docs' / 'npno'
    (docs / 'FIXTURES-ESTRUTURA.json').write_text(json.dumps(result, indent=2) + '\n')
    dumps = docs / 'hexdumps'
    dumps.mkdir(exist_ok=True)
    for p in sorted(root.glob('*.npno')):
        data = p.read_bytes()
        lines = [f'{i:08x}  {data[i:i+16].hex(" "):<47}  |' +
                 ''.join(chr(c) if 32 <= c < 127 else '.' for c in data[i:i+16]) + '|'
                 for i in range(0, len(data), 16)]
        (dumps / (p.name + '.hex')).write_text('\n'.join(lines) + '\n')
