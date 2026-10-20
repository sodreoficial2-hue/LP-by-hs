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
