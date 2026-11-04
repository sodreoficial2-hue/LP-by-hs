#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise the public CLI contract against independent bundled formats."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
cli, sndfile, fixtures, nki, output, *renderer = sys.argv[1:]
fixtures, nki, output = map(Path, (fixtures, nki, output))
shutil.rmtree(output, ignore_errors=True)
output.mkdir(parents=True)
for name, source in [('npno', fixtures/'npno/layers-v6.npno'),
                     ('nsmp', fixtures/'nord/workshop-v4.nsmp4'),
                     ('nki', nki/'rr.nki')]:
    destination = output/name
    args = [cli, str(source), str(destination), sndfile,
            '--velocity-crossfade', '--crossfade-width=2']
    result = subprocess.run(args, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    meta = json.loads((destination/(source.name+'.meta.json')).read_text())
    assert 'velocity_crossfade=on; width=2' in meta['preserved_features']
    assert any(z['xfin_lovel'] >= 0 for z in meta['zones'])
    if name == 'npno':
        default = output/'default'
        result = subprocess.run([cli, str(source), str(default), sndfile], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        default_meta = json.loads((default/(source.name+'.meta.json')).read_text())
        assert 'velocity_crossfade=on; width=8' in default_meta['preserved_features']
        assert all(z['channels'] == 2 and not z['pan_present'] for z in default_meta['zones'])
        legacy = output/'legacy'
        result = subprocess.run([cli, str(source), str(legacy), sndfile,
                                 '--velocity-crossfade=4', '--no-velocity-crossfade',
                                 '--stereo-mono-fallback'], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        legacy_meta = json.loads((legacy/(source.name+'.meta.json')).read_text())
        assert all(z['channels'] == 1 and z['xfin_lovel'] == -1 for z in legacy_meta['zones'])
    elif renderer and renderer[0]:
        # NPNO measured rendering is covered more extensively by format_render_test.
        calibrated = output/(name+'-calibrated')
        calibration_source = nki.parent/'pitch-fixtures/pitch-v4.nsmp4' if name == 'nsmp' else source
        result = subprocess.run([cli, str(calibration_source), str(calibrated), sndfile,
                                 '--calibrate-sfizz='+renderer[0]], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        data = json.loads((calibrated/(calibration_source.name+'.meta.json')).read_text())
        assert all(z['calibrated'] and z['amp_veltrack'] == 0
                   for z in data['zones'] if not z['muted'])
for option in ['--crossfade-width=0', '--crossfade-width=17',
               '--crossfade-width=invalid', '--opcao-inexistente']:
    result = subprocess.run([cli, str(nki/'rr.nki'), str(output/'bad'), sndfile, option],
                            capture_output=True, text=True)
    assert result.returncode == 1 and not (output/'bad').exists()
    assert 'Crossfade velocity deve ter' in result.stderr or 'Opção desconhecida' in result.stderr
print('PASSOU: CLI dos três formatos, padrões NPNO, width, aliases, fallback estéreo, calibração e erros em português')
