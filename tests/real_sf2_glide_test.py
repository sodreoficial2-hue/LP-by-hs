"""Measure glide in audio rendered by production Sf2Engine + real FluidSynth.
The generated SF2 uses a deterministic looping sine. Its second instrument
replaces the SF2 default pitch-wheel modulator with a zero-amount modulator.
No user samples or SoundFonts are modified.
"""
import array
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

RATE = 48000

def fixture(path):
    def chunk(tag, data):
        return tag.encode() + struct.pack('<I', len(data)) + data + b'\0' * (len(data) % 2)
    def lst(tag, *parts):
        return chunk('LIST', tag.encode() + b''.join(parts))
    def name(s):
        return s.encode().ljust(20, b'\0')
    samples = struct.pack('<' + 'h' * (RATE + 46), *[int(10000 * math.sin(2 * math.pi * 440 * n / RATE)) for n in range(RATE)], *([0] * 46))
    phdr = b''.join(struct.pack('<20sHHHIII', name(n), p, 0, i, 0, 0, 0) for i, (n, p) in enumerate([('Sine', 0), ('No pitch bend', 1), ('EOP', 0)]))
    pbag = b''.join(struct.pack('<HH', i, 0) for i in range(3))
    pgen = struct.pack('<HHHHHH', 41, 0, 41, 1, 0, 0)
    inst = b''.join(struct.pack('<20sH', name(n), i) for i, n in enumerate(['Normal', 'No pitch bend', 'EOI']))
    ibag = struct.pack('<HHHHHH', 0, 0, 6, 0, 12, 1)
    generators = [(34, -12000), (36, -12000), (37, 0), (38, -12000), (54, 1), (53, 0)]
    igen = b''.join(struct.pack('<Hh', *g) for g in generators * 2 + [(0, 0)])
    # Pitch wheel (bipolar), fine tune, amount=0, wheel sensitivity (source2).
    imod = struct.pack('<HHhHH', 0x20e, 52, 0, 16, 0) + b'\0' * 10
    shdr = struct.pack('<20sIIIIIBbHH', name('Looped Sine'), 0, RATE, 0, RATE, RATE, 69, 0, 0, 1)
    shdr += struct.pack('<20sIIIIIBbHH', name('EOS'), RATE + 46, RATE + 46, RATE + 46, RATE + 46, RATE, 0, 0, 0, 1)
    data = b'sfbk' + lst('INFO', chunk('ifil', struct.pack('<HH', 2, 1)), chunk('isng', b'EMU8000\0'), chunk('INAM', b'LP glide regression\0'))
    data += lst('sdta', chunk('smpl', samples))
    data += lst('pdta', chunk('phdr', phdr), chunk('pbag', pbag), chunk('pmod', b'\0' * 10), chunk('pgen', pgen), chunk('inst', inst), chunk('ibag', ibag), chunk('imod', imod), chunk('igen', igen), chunk('shdr', shdr))
    path.write_bytes(chunk('RIFF', data))

def read(path):
    a = array.array('f'); a.frombytes(path.read_bytes()); return a

def frequency(data, t, window=.04):
    start = int((t - window / 2) * RATE); end = int((t + window / 2) * RATE)
    crossings = []
    for i in range(max(1, start), min(len(data), end)):
        if data[i - 1] <= 0 < data[i]:
            crossings.append(i - 1 - data[i - 1] / (data[i] - data[i - 1]))
    if len(crossings) < 3:
        raise AssertionError(f'No measurable tone at {t}s')
    return RATE * (len(crossings) - 1) / (crossings[-1] - crossings[0])

def near(actual, expected, tolerance=.03):
    assert abs(actual / expected - 1) < tolerance, (actual, expected)

def analyze(folder):
    report = {}
    cases = [('slow_up', .9, 69, 81), ('medium_down', .5, 81, 69), ('fast_up', .15, 69, 81), ('bend_disabled_font', .9, 69, 81), ('large_interval', .9, 45, 93)]
    for name, amount, source, target in cases:
        data = read(folder / (name + '.f32')); duration = round(5 + amount * amount * 1995) / 1000
        hz = lambda key: 440 * 2 ** ((key - 69) / 12)
        near(frequency(data, .18), hz(source))
        points = []
        for fraction in [.15, .4, .7, 1.2]:
            t = duration * fraction
            actual = frequency(data, .25 + t, min(.04, duration * .15))
            expected = hz(source + (target - source) * min(fraction, 1))
            near(actual, expected, .045); points.append({'seconds_after_note': t, 'hz': actual, 'expected_hz': expected})
        near(frequency(data, 2.65 + duration * .5), hz((source + target) / 2), .05)
        near(frequency(data, 4.75), hz(source))
        assert max(abs(v) for v in data[-2400:]) < 1e-5, 'Panic did not silence'
        report[name] = {'duration_ms': duration * 1000, 'pitch_points': points}
    data = read(folder / 'staccato_bend_panic.f32')
    near(frequency(data, .28 + .504 * .5), 440 * 2 ** .5, .045)
    near(frequency(data, 1.2), 880 * 2 ** (2 / 12), .02)
    near(frequency(data, 1.4), 880, .02)
    near(frequency(data, 1.6), 440, .02)
    data = read(folder / 'sustain_release.f32')
    assert sum(v * v for v in data[int(.25 * RATE):int(.35 * RATE)]) > .01
    assert max(abs(v) for v in data[int(.55 * RATE):]) < 1e-5
    data = read(folder / 'polyphonic_zero.f32')
    for first, last in [(.05, .25), (.55, .75)]:
        segment = data[int(first * RATE):int(last * RATE)]
        for hz in [440, 440 * 2 ** (7 / 12)]:
            amplitude = abs(sum(v * complex(math.cos(2 * math.pi * hz * n / RATE), math.sin(2 * math.pi * hz * n / RATE)) for n, v in enumerate(segment))) / len(segment)
            assert amplitude > .005, ('Polyphony missing', first, hz, amplitude)
    data = read(folder / 'rapid_legato.f32')
    before = frequency(data, .58); after = frequency(data, .64)
    assert 1 < after / before < 1.08, ('Abrupt pitch jump during rapid legato', before, after)
    near(frequency(data, 2.3), 440 * 2 ** (7 / 12), .02)
    data = read(folder / 'controller_reset.f32')
    for start in [.25, 1.3]:
        near(frequency(data, start + .252), 440 * 2 ** .5, .045)
        near(frequency(data, start + .7), 880, .02)
    return report

if __name__ == '__main__':
    executable, library, destination = sys.argv[1:]
    folder = Path(destination).resolve(); folder.mkdir(parents=True, exist_ok=True)
    sf2 = folder / 'sine-glide.sf2'; fixture(sf2)
    # One process per scenario avoids re-registering libinstpatch/GLib types on
    # older Linux FluidSynth versions when the dynamic library is unloaded.
    names = ['slow_up', 'medium_down', 'fast_up', 'bend_disabled_font', 'large_interval',
             'staccato_bend_panic', 'polyphonic_zero', 'sustain_release', 'rapid_legato', 'controller_reset']
    for name in names:
        result = subprocess.run([executable, library, str(sf2), str(folder), name], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f'{name}: {result.stdout} {result.stderr}')
    report = analyze(folder)
    (folder / 'glide-results.json').write_text(json.dumps({'result': 'PASS', 'scope': 'Real FluidSynth audio, offline', 'cases': report}, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'cases': list(report), 'additional_checks': ['staccato', 'pitch wheel independent', 'panic resets previous note', 'sustain release', 'polyphony at zero and after disabling glide', 'continuous rapid legato', 'knob owns native glide controls', 'CC121 restores portamento settings']}))
