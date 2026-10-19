# PinyinIME

This fork maintains the dictionary, typo suggestions and native decoder fixes
on top of AOSP PinyinIME commit
[`49aebad1c1cfbbcaa9288ffed5161e79e57c3679`](https://android.googlesource.com/platform/packages/inputmethods/PinyinIME/+/49aebad1c1cfbbcaa9288ffed5161e79e57c3679).
It was forked from the matching
[GitHub mirror](https://github.com/aosp-mirror-neo/platform_packages_inputmethods_PinyinIME).
The original Android UI is unchanged.

## Dictionary and typo suggestions

[extra_words.txt](jni/data/extra_words.txt) adds or updates 65 daily and technical
terms, including 微信、二维码、大模型、大语言模型、多模态、端到端、语音输入法
and 自动驾驶. The weights are hand-chosen ranking values, not corpus statistics.
The binary is rebuilt from the original AOSP source plus these entries; no
external word list or runtime dependency is added.

The native `im_search`/`im_delsearch` API now adds up to three whole-word typo
suggestions immediately after candidate 0. Examples include `zhongugo` → 中国,
`shurua` → 输入法 and `damoxign` → 大模型. It handles one adjacent transposition,
one missing letter or one extra letter. Candidate 0 remains the original
decoder result; a suggestion is applied only when selected. Selection updates
the spelling and segmentation, and cancelling restores the original input.

Suggestions are limited to system-dictionary words of 2–8 characters, lowercase
inputs of 4–38 letters, and compositions with no fixed prefix or apostrophes.
Exact whole-word spellings skip correction. Sentence correction, letter
substitution and user-dictionary correction are not implemented.

To edit the dictionary, change `extra_words.txt` and run on a little-endian Linux
host with GCC and Python 3:

```sh
python3 tools/build_dictionary.py
python3 tools/build_dictionary.py --check
python3 -B tests/dictionary_test.py
```

Each line is `word weight 0 syllables`, with one lowercase syllable per character.
Words may contain up to eight characters already present in AOSP's valid-character
list. Multi-character weights must be at least 60, the original builder's cutoff.
The merge rejects invalid or duplicate additions and replaces a matching source
entry instead of duplicating it. `--check` rebuilds and compares the exact bytes.
Distribute the regenerated `res/raw/dict_pinyin.dat` together with the decoder.

## Fixes

- Searching broad initial-letter combinations such as `zz` no longer aborts
  when candidates exceed the deduplication buffer. The existing capacity
  limit is applied before copying candidates.
- Dictionary extensions count only parsing marks that were actually stored.
  Exhausting either the parsing-mark pool or the milestone pool no longer
  creates handles referring to unwritten or out-of-bounds entries.
- An exhausted dictionary search can return no match without triggering a
  root-match assertion. Cached candidates are not used unless a match node
  was created, and failed extensions clear the previous candidate count.
  This also prevents segmentation-array overflows during long initial-letter
  searches.
- The Android-only performance logging header is included only when that
  instrumentation is enabled, allowing the decoder to be tested on a host.
- The dictionary builder now accepts words up to its declared eight-character
  limit, writes the existing 32-bit list fields correctly on 64-bit hosts, and
  includes the last word in its ID range. Missing word-length groups also use
  the correct list offsets.

## Native tests

On Linux with GCC and Bash:

```sh
./tests/run_native_tests.sh
CXXFLAGS=-DNDEBUG ./tests/run_native_tests.sh
```

Both commands enable AddressSanitizer and UndefinedBehaviorSanitizer. The
second disables decoder assertions while keeping the test checks active.
Leak detection is disabled because the original decoder retains process-wide
singleton caches. Temporary build files are removed when the script exits.

The tests cover `zz`, exhausted search pools, long initial-letter input,
ordinary sentences, partial candidate selection, deletion, 2,000 deterministic
random input sequences, all 65 added/updated entries, correction selection and
cancellation, and closing/reopening the decoder. Individual cases can be selected
with `candidates`, `unmatched`, `capacity`, `editing`, `vocabulary` or `corrections`:

```sh
./tests/run_native_tests.sh capacity
```

GitHub Actions runs both assertion configurations, the dictionary merge tests,
and a byte-for-byte dictionary rebuild. These are host decoder
tests with user dictionary learning disabled; they do not build the Android
application or replace physical-device testing.

## License

Apache License 2.0. See [NOTICE](NOTICE) and the copyright headers in the
original source files.
