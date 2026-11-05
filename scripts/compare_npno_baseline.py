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
        manifests, timings = [], []
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
            with (destination.parent / (label + ".log")).open("w") as log:
                subprocess.run(command, env=environment, stdout=log, stderr=log, check=True)
            timings.append(time.perf_counter() - start)
            manifests.append({str(p.relative_to(destination)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(destination.rglob("*")) if p.is_file()})
        differing = sorted(p for p in manifests[0].keys() | manifests[1].keys()
                           if manifests[0].get(p) != manifests[1].get(p))
        results.append(dict(fixture=fixture, mode=mode, baseline_seconds=timings[0],
                            candidate_seconds=timings[1], identical=not differing,
                            differences=differing, manifests=manifests))
        (output / "comparison.json").write_text(json.dumps(results, indent=2))
        print(json.dumps({k:v for k,v in results[-1].items() if k != "manifests"}), flush=True)
if any(not result["identical"] for result in results):
    raise SystemExit("FAIL: output bytes differ; do not merge the optimization")
