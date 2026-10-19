#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/pinyinime-tests.XXXXXX")"
trap 'rm -rf -- "$build_dir"' EXIT
ulimit -c 0

extra_flags=()
if [[ -n "${CXXFLAGS:-}" ]]; then read -r -a extra_flags <<< "$CXXFLAGS"; fi
"${CXX:-g++}" -std=c++11 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer "${extra_flags[@]}" \
  -I "$repo_dir/jni/include" "$repo_dir/tests/native_decoder_test.cpp" \
  "$repo_dir"/jni/share/*.cpp -o "$build_dir/native_decoder_test"

# The original decoder retains process-wide singleton caches.
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  "$build_dir/native_decoder_test" "$repo_dir/res/raw/dict_pinyin.dat" \
  "${1:-all}" "$repo_dir/jni/data/extra_words.txt"
