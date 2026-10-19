"""Checks for the hand-maintained dictionary merge; no third-party packages."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.build_dictionary import merge_words


class DictionaryMergeTest(unittest.TestCase):
    source = '大 100 0 da\n模 100 0 mo\n型 100 0 xing\n大模型 60 0 da mo xing\n'
    valid = set('大模型')

    def merge(self, additions):
        return merge_words(self.source, additions, self.valid)

    def test_replace_and_preserve_source(self):
        result, count = self.merge('# comment\n\n大模型 6000 0 da mo xing\n')
        self.assertEqual(count, 1)
        self.assertEqual(len(result.splitlines()), 4)
        self.assertIn('大 100 0 da\n', result)
        self.assertIn('大模型 6000 0 da mo xing\n', result)
        self.assertNotIn('大模型 60 0 da mo xing', result)

    def test_add_word(self):
        result, count = self.merge('模型 100 0 mo xing')
        self.assertEqual(count, 1)
        self.assertEqual(result, self.source + '模型 100 0 mo xing\n')

    def test_reject_invalid_entries(self):
        for row in ('大模型 100', '大模型 100 0 da mo', '大模型 100 0 da mo xign',
                    '大模型 100 1 da mo xing', '大模型 100 0 Da mo xing',
                    '大模型 nan 0 da mo xing', '大模型 inf 0 da mo xing',
                    '大模型 -1 0 da mo xing', '大模型 59 0 da mo xing',
                    '大模型 word 0 da mo xing', '新模型 100 0 da mo xing',
                    '大大大大大大大大大 100 0 da da da da da da da da da'):
            with self.subTest(row=row), self.assertRaises(ValueError):
                self.merge(row)

    def test_reject_duplicate_additions(self):
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.merge('模型 100 0 mo xing\n模型 200 0 mo xing')


if __name__ == '__main__':
    unittest.main()
