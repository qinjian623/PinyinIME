#!/usr/bin/env python3
"""Merge the reviewed UTF-8 additions and build the bundled AOSP dictionary."""
import argparse
import hashlib
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def merge_words(source, additions, valid_chars):
    rows = [line.split() for line in source.splitlines() if line.strip()]
    entries = {(row[0], tuple(row[3:])): row for row in rows}
    spellings = {spelling for row in rows for spelling in row[3:]}
    seen = set()
    count = 0
    for number, line in enumerate(additions.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        row = line.split()
        if len(row) < 4:
            raise ValueError(f'Extra words line {number}: expected word weight 0 spellings')
        word, weight, flag = row[:3]
        frequency = float(weight)
        if (not 1 <= len(word) <= 8 or len(row[3:]) != len(word)
                or not set(word) <= valid_chars or flag != '0'
                or not math.isfinite(frequency) or frequency <= 0
                or (len(word) > 1 and frequency < 60)
                or any(not re.fullmatch('[a-z]+', p) or p not in spellings for p in row[3:])):
            raise ValueError(f'Extra words line {number}: invalid word, weight or spelling')
        key = (word, tuple(row[3:]))
        if key in seen:
            raise ValueError(f'Extra words line {number}: duplicate entry')
        seen.add(key)
        entries[key] = row
        count += 1
    return '\n'.join(' '.join(row) for row in entries.values()) + '\n', count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='compare against the committed binary')
    args = parser.parse_args()
    if sys.byteorder != 'little':
        raise SystemExit('The original binary format requires a little-endian build host')
    data = ROOT / 'jni/data'
    merged, count = merge_words(
        (data / 'rawdict_utf16_65105_freq.txt').read_text(encoding='utf-16'),
        (data / 'extra_words.txt').read_text(encoding='utf-8'),
        set((data / 'valid_utf16.txt').read_text(encoding='utf-16')))
    target = ROOT / 'res/raw/dict_pinyin.dat'
    with tempfile.TemporaryDirectory(prefix='pinyinime-dictionary-') as directory:
        build = Path(directory)
        source = build / 'merged.txt'
        source.write_bytes(b'\xff\xfe' + merged.encode('utf-16le'))
        executable = build / 'dictbuilder'
        command = [os.environ.get('CXX', 'g++'), '-std=c++11', '-O2', '-D___BUILD_MODEL___',
                   str(ROOT / 'jni/command/pinyinime_dictbuilder.cpp')]
        command += [str(path) for path in sorted((ROOT / 'jni/share').glob('*.cpp'))]
        subprocess.run(command + ['-o', str(executable)], check=True)
        result = build / 'dict_pinyin.dat'
        subprocess.run([str(executable), str(source), str(data / 'valid_utf16.txt'), str(result)],
                       cwd=build, check=True, stdout=subprocess.DEVNULL)
        content = result.read_bytes()
        if args.check:
            if content != target.read_bytes():
                raise SystemExit('Dictionary differs; run tools/build_dictionary.py to regenerate it')
        else:
            target.write_bytes(content)
        print(f'{"Verified" if args.check else "Built"} dictionary with {count} reviewed entries; '
              f'{len(content)} bytes; SHA256 {hashlib.sha256(content).hexdigest()}')


if __name__ == '__main__':
    main()
