#!/usr/bin/env python3
"""Compare full deterministic NPNO output trees; never rewrite expected values."""
import hashlib, json, os, pathlib, subprocess, sys, time
baseline, candidate, output = map(lambda p: pathlib.Path(p).resolve(), sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
renderer = os.environ["LP_REAL_SFIZZ_LIBRARY"]
sndfile = "/usr/lib/x86_64-linux-gnu/libsndfile.so.1"
results = []
for fixture in ("tine-demo.npno", "layers-v5.npno", "layers-v6.npno"):
    modes = ("fallback", "sustain", "isotonic") if fixture != "tine-demo.npno" else ("fallback", "sustain")
    for mode in modes:
        manifests, timings, phases = [], [], []
        for label, root in (("baseline", baseline), ("candidate", candidate)):
            source = root / "build/import-fixtures/tests/fixtures/npno" / fixture
            destination = output / fixture / mode / label
            destination.parent.mkdir(parents=True, exist_ok=True)
            command = [str(root / "build/lp_convert_timbre"), str(source), str(destination), sndfile]
            environment = dict(os.environ)
            environment["LP_REAL_SFIZZ_LIBRARY"] = "" if mode == "fallback" else renderer
            if mode == "isotonic":
                command.append("--calibrate-sfizz=" + renderer)
            start = time.perf_counter()
            markers = {}
            with (destination.parent / (label + ".log")).open("w") as log:
                completed = subprocess.Popen(command, env=environment, stdout=log, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
                for line in completed.stderr:
                    log.write(line)
                    for prefix, marker in (("lendo módulo", "reading"), ("convertendo amostra ", "samples"), ("convertendo zona ", "zones"), ("calibrando níveis e sustain", "calibration"), ("concluído", "done")):
                        if line.startswith(prefix):
                            markers.setdefault(marker, time.perf_counter())
                completed.wait()
            phases.append({name: markers[right] - markers[left]
                           for name, left, right in (("read_hash_setup", "reading", "samples"),
                                                      ("decode_wav_hash", "samples", "zones"),
                                                      ("zone_mapping", "zones", "calibration"),
                                                      ("calibration_sustain_sfz_meta_original_copy", "calibration", "done"))
                           if left in markers and right in markers})
            if completed.returncode:
                print((destination.parent / (label + ".log")).read_text()[-4000:], flush=True)
                raise RuntimeError(f"{label} conversion failed: {fixture}/{mode}")
            timings.append(time.perf_counter() - start)
            manifests.append({str(p.relative_to(destination)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(destination.rglob("*")) if p.is_file() and ("--pcm-only" not in sys.argv or p.suffix == ".wav")})
        differing = sorted(p for p in manifests[0].keys() | manifests[1].keys()
                           if manifests[0].get(p) != manifests[1].get(p))
        results.append(dict(fixture=fixture, mode=mode, baseline_seconds=timings[0],
                            candidate_seconds=timings[1], phase_seconds=dict(baseline=phases[0], candidate=phases[1]), identical=not differing,
                            differences=differing, manifests=manifests))
        (output / "comparison.json").write_text(json.dumps(results, indent=2))
        print(json.dumps({k:v for k,v in results[-1].items() if k != "manifests"}), flush=True)
if any(not result["identical"] for result in results):
    raise SystemExit("FAIL: output bytes differ; do not merge the optimization")
