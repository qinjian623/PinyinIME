# PinyinIME

This fork maintains native decoder fixes on top of AOSP PinyinIME commit
[`49aebad1c1cfbbcaa9288ffed5161e79e57c3679`](https://android.googlesource.com/platform/packages/inputmethods/PinyinIME/+/49aebad1c1cfbbcaa9288ffed5161e79e57c3679).
It was forked from the matching
[GitHub mirror](https://github.com/aosp-mirror-neo/platform_packages_inputmethods_PinyinIME).
The original dictionary and Android UI are unchanged.

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
random input sequences, and closing/reopening the decoder. Individual cases
can be selected with `candidates`, `unmatched`, `capacity`, or `editing`:

```sh
./tests/run_native_tests.sh capacity
```

The regression cases fail when their corresponding original code is restored.
GitHub Actions runs both assertion configurations. These are host decoder
tests with user dictionary learning disabled; they do not build the Android
application or replace physical-device testing.

## License

Apache License 2.0. See [NOTICE](NOTICE) and the copyright headers in the
original source files.
