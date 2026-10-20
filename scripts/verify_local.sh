#!/usr/bin/env bash
set -euo pipefail
lp_repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
lp_build_dir="$(mktemp -d)"
trap 'rm -rf -- "$lp_build_dir"' EXIT
cd "$lp_repo_root"
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/session_test.cpp -o "$lp_build_dir/session_test"
"$lp_build_dir/session_test" recovery/extracted/live/samples
g++ -std=c++17 -Wall -Wextra -Werror -fPIC -shared tests/fake_sfizz.cpp -o "$lp_build_dir/fake_sfizz.so"
g++ -std=c++17 -Wall -Wextra -Werror -pthread -Isrc src/sfizz_engine.cpp src/dsp.cpp tests/engine_midi_test.cpp -ldl -o "$lp_build_dir/engine_midi_test"
"$lp_build_dir/engine_midi_test" "$lp_build_dir/fake_sfizz.so"

g++ -std=c++17 -Wall -Wextra -Werror -Isrc src/dsp.cpp tests/dsp_test.cpp -o "$lp_build_dir/dsp_test"
"$lp_build_dir/dsp_test"

g++ -std=c++17 -Wall -Wextra -Werror -pthread -Isrc -Ithird_party/portaudio src/audio_host.cpp src/sfizz_engine.cpp src/dsp.cpp tests/fake_portaudio.cpp tests/audio_host_test.cpp -ldl -o "$lp_build_dir/audio_test"
"$lp_build_dir/audio_test" "$lp_build_dir/fake_sfizz.so"

if [[ -n "${LP_REAL_SFIZZ_LIBRARY:-}" ]]; then
 g++ -std=c++17 -Wall -Wextra -Werror -Isrc src/sfizz_engine.cpp src/dsp.cpp tests/real_sfizz_envelope_test.cpp -ldl -o "$lp_build_dir/real_envelope_test"
 "$lp_build_dir/real_envelope_test" "$LP_REAL_SFIZZ_LIBRARY" recovery/extracted/live/samples
 g++ -std=c++17 -Wall -Wextra -Werror -Isrc src/sfizz_engine.cpp src/dsp.cpp tests/real_performance_test.cpp -ldl -o "$lp_build_dir/real_performance_test"
 if [[ -n "${LP_BANK_TEST_ROOT:-}" ]]; then
  "$lp_build_dir/real_performance_test" "$LP_REAL_SFIZZ_LIBRARY" "$LP_BANK_TEST_ROOT"
 else
  "$lp_build_dir/real_performance_test" "$LP_REAL_SFIZZ_LIBRARY"
 fi
fi

g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/instrument_catalog_test.cpp -o "$lp_build_dir/catalog_test"
"$lp_build_dir/catalog_test"
if [[ -n "${LP_REAL_SFIZZ_LIBRARY:-}" && -n "${LP_BANK_TEST_ROOT:-}" ]]; then
 g++ -std=c++17 -Wall -Wextra -Werror -Isrc src/sfizz_engine.cpp src/dsp.cpp tests/bank_render_test.cpp -ldl -o "$lp_build_dir/bank_test"
 "$lp_build_dir/bank_test" "$LP_REAL_SFIZZ_LIBRARY" "$LP_BANK_TEST_ROOT"
 python3 tests/bank_mono_test.py "$LP_REAL_SFIZZ_LIBRARY" "$LP_BANK_TEST_ROOT"
fi
