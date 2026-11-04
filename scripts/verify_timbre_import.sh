#!/usr/bin/env bash
set -euo pipefail
lp_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
lp_test_dir="$(mktemp -d)"
if [[ -n "${LP_IMPORT_TEST_OUTPUT:-}" ]]; then
 lp_test_dir="$LP_IMPORT_TEST_OUTPUT"
 mkdir -p "$lp_test_dir"
else
 trap 'rm -rf -- "$lp_test_dir"' EXIT
fi
lp_sndfile="${1:-/lib/x86_64-linux-gnu/libsndfile.so.1}"
# Some mounted workspaces reset generated file modes. Restore only our own test binary.
lp_run() { chmod u+x "$1"; "$@"; }
tar -xzf "$lp_root/tests/fixtures/import-fixtures.tar.gz" -C "$lp_test_dir"
cd "$lp_root"
lp_sources=(src/nki_reader.cpp src/sfz_policy.cpp src/timbre_format.cpp src/sfz_conversion.cpp src/nsmp_to_sfz.cpp
 src/nki_to_sfz.cpp src/npno_reader.cpp src/npno_to_sfz.cpp src/nsmp/*.cpp)
lp_objects=()
for lp_source in "${lp_sources[@]}"; do
 lp_object="$lp_test_dir/$(basename "$lp_source").o"
 g++ -std=c++17 -g -Isrc -c "$lp_source" -o "$lp_object"
 lp_objects+=("$lp_object")
done
for lp_test in timbre_import_test npno_import_test instrument_catalog_test catalog_edits_test sample_builder_test nki_binary_test format_policy_test; do
 g++ -std=c++17 -g -Isrc "tests/$lp_test.cpp" "${lp_objects[@]}" -ldl -lz -lexpat -o "$lp_test_dir/$lp_test"
done
lp_run "$lp_test_dir/timbre_import_test" "$lp_sndfile" "$lp_test_dir/nsmp-output" "$lp_test_dir/tests/fixtures/nord"
lp_run "$lp_test_dir/npno_import_test" "$lp_sndfile" "$lp_test_dir/npno-output" "$lp_test_dir/tests/fixtures/npno"
lp_run "$lp_test_dir/instrument_catalog_test"
lp_run "$lp_test_dir/catalog_edits_test"
lp_run "$lp_test_dir/sample_builder_test" "$lp_sndfile" "$lp_test_dir/builder-output"
python3 tests/fixtures/nki/generate.py "$lp_test_dir/nki-fixtures"
python3 tests/fixtures/pitch/unpack.py "$lp_test_dir/pitch-fixtures"
lp_run "$lp_test_dir/nki_binary_test" "$lp_sndfile" "$lp_test_dir/nki-fixtures" "$lp_test_dir/nki-output"
lp_run "$lp_test_dir/format_policy_test" "$lp_test_dir/tests/fixtures/npno" "$lp_test_dir/policy-output"
if [[ -n "${LP_REAL_SFIZZ_LIBRARY:-}" ]]; then
 g++ -std=c++17 -O2 -g -Isrc tests/format_render_test.cpp "${lp_objects[@]}" -ldl -lz -lexpat -o "$lp_test_dir/format_render_test"
 lp_run "$lp_test_dir/format_render_test" "$LP_REAL_SFIZZ_LIBRARY" "$lp_sndfile" "$lp_test_dir" "$lp_test_dir/render-output"
else
 echo 'N/A: real sfizz rendering; set LP_REAL_SFIZZ_LIBRARY to run measured pitch/voices/pedal gates'
fi
echo 'PASS: 5 original + 2 new import/policy regression groups'
