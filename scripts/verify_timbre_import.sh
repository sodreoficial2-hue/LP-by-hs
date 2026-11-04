#!/usr/bin/env bash
set -euo pipefail
lp_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
lp_test_dir="$(mktemp -d)"
trap 'rm -rf -- "$lp_test_dir"' EXIT
lp_sndfile="${1:-/lib/x86_64-linux-gnu/libsndfile.so.1}"
tar -xzf "$lp_root/tests/fixtures/import-fixtures.tar.gz" -C "$lp_test_dir"
cd "$lp_root"
lp_sources=(src/timbre_format.cpp src/sfz_conversion.cpp src/nsmp_to_sfz.cpp
 src/nki_to_sfz.cpp src/npno_reader.cpp src/npno_to_sfz.cpp src/nsmp/*.cpp)
lp_objects=()
for lp_source in "${lp_sources[@]}"; do
 lp_object="$lp_test_dir/$(basename "$lp_source").o"
 g++ -std=c++17 -g -Isrc -c "$lp_source" -o "$lp_object"
 lp_objects+=("$lp_object")
done
for lp_test in timbre_import_test npno_import_test instrument_catalog_test catalog_edits_test sample_builder_test; do
 g++ -std=c++17 -g -Isrc "tests/$lp_test.cpp" "${lp_objects[@]}" -ldl -o "$lp_test_dir/$lp_test"
done
"$lp_test_dir/timbre_import_test" "$lp_sndfile" "$lp_test_dir/nsmp-output" "$lp_test_dir/tests/fixtures/nord"
"$lp_test_dir/npno_import_test" "$lp_sndfile" "$lp_test_dir/npno-output" "$lp_test_dir/tests/fixtures/npno"
"$lp_test_dir/instrument_catalog_test"
"$lp_test_dir/catalog_edits_test"
"$lp_test_dir/sample_builder_test" "$lp_sndfile" "$lp_test_dir/builder-output"
echo 'PASS: 5 import/catalog/sample-builder regression groups'
