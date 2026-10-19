#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <chrono>
#include <codecvt>
#include <fstream>
#include <locale>
#include <sstream>

#include "dicttrie.h"
#include "matrixsearch.h"
#include "pinyinime.h"
#include "splparser.h"
#include "spellingtrie.h"

using namespace ime_pinyin;

// Keep test checks active when the decoder is compiled with NDEBUG.
#define CHECK(condition) do { \
  if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); \
  } \
} while (0)

static std::u16string candidate(size_t index) {
  char16 text[64] = {};
  CHECK(im_get_candidate(index, text, 64) != NULL);
  std::u16string result;
  for (size_t i = 0; i < 64 && text[i]; ++i)
    result.push_back(static_cast<char16_t>(text[i]));
  CHECK(result.size() < 64);
  return result;
}

static void candidates() {
  im_reset_search();
  CHECK(im_search("z", 1) > 0);
  CHECK(im_search("zz", 2) > 0);
  CHECK(!candidate(0).empty());
}

static void unmatched() {
  // A dictionary miss must leave the decoder usable for the next composition.
  const char* inputs[] = {"v", "ve", "vv", "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"};
  for (const char* input : inputs) {
    im_reset_search();
    for (size_t n = 1; n <= std::strlen(input); ++n)
      im_search(input, n);
  }
  im_reset_search();
  CHECK(im_search("nihao", 5) > 0);
  CHECK(candidate(0) == u"你好");
}

static DictExtPara extension(const char* spelling, uint16 depth) {
  SpellingParser parser;
  bool prefix = false;
  uint16 id = parser.get_splid_by_str(spelling, std::strlen(spelling), &prefix);
  CHECK(id != 0);
  DictExtPara dep = {};
  for (uint16 i = 0; i <= depth; ++i) dep.splids[i] = id;
  dep.splids_extended = depth;
  dep.ext_len = std::strlen(spelling);
  dep.step_no = (depth + 1) * dep.ext_len - 1;
  const SpellingTrie& trie = SpellingTrie::get_instance();
  if (trie.is_half_id(id)) dep.id_num = trie.half_to_full(id, &dep.id_start);
  else { dep.id_start = id; dep.id_num = 1; }
  CHECK(dep.id_num > 0);
  return dep;
}

static MileStoneHandle extend(DictTrie& dict, MileStoneHandle from,
                             const DictExtPara& dep) {
  LmaPsbItem items[2000];
  size_t count = 0;
  MileStoneHandle handle = dict.extend_dict(from, &dep, items, 2000, &count);
  CHECK(count <= 2000);
  return handle;
}

static void capacity(const char* dictionary) {
  DictTrie dict;
  CHECK(dict.load_dict(dictionary, 1, kSysDictIdEnd));
  DictExtPara z = extension("z", 0);
  DictExtPara zz = extension("z", 1);
  DictExtPara zzz = extension("z", 2);
  MileStoneHandle root = extend(dict, 0, z);
  CHECK(root != 0);
  bool exhausted = false;
  // Reuse valid prefixes until the parsing-mark pool fills. Extend each
  // returned handle so ASan catches counts that refer to unwritten marks.
  for (size_t i = 0; i < 1024; ++i) {
    MileStoneHandle next = extend(dict, root, zz);
    if (!next) { exhausted = true; break; }
    extend(dict, next, zzz);
  }
  CHECK(exhausted);

  dict.reset_milestones(0, 0);
  root = extend(dict, 0, z);
  MileStoneHandle second = extend(dict, root, zz);
  CHECK(root != 0 && second != 0);
  DictExtPara ni = extension("ni", 0);
  exhausted = false;
  // Single full spellings fill the milestone pool before the mark pool.
  for (size_t i = 0; i < 1024; ++i) {
    if (!extend(dict, 0, ni)) { exhausted = true; break; }
  }
  CHECK(exhausted);
  CHECK(extend(dict, root, zz) == 0);
  CHECK(extend(dict, second, zzz) == 0);
  dict.reset_milestones(0, 0);
  CHECK(extend(dict, 0, ni) != 0);
}

static void editing() {
  const char* inputs[] = {"nihao", "zhongguo", "shurufa", "wojintianhenkaixin", "lvse", "xi'an"};
  const char16_t* expected[] = {u"你好", u"中国", u"输入法", u"我今天很开心", u"绿色", u"西安"};
  for (size_t i = 0; i < 6; ++i) {
    im_reset_search();
    CHECK(im_search(inputs[i], std::strlen(inputs[i])) > 0);
    CHECK(candidate(0) == expected[i]);
    im_choose(0);
  }
  im_reset_search();
  size_t count = im_search("nihao", 5);
  bool selected = false;
  for (size_t i = 0; i < count; ++i) {
    if (candidate(i) != u"你") continue;
    im_choose(i);
    CHECK(im_get_fixed_len() == 1);
    im_delsearch(4, false, false);
    size_t decoded = 0;
    CHECK(std::string(im_get_sps_str(&decoded)) == "niha");
    CHECK(im_get_fixed_len() == 1);
    selected = true;
    break;
  }
  CHECK(selected);

  std::mt19937 random(42);
  for (size_t trial = 0; trial < 2000; ++trial) {
    im_reset_search();
    std::string raw;
    for (size_t i = 0; i < 32; ++i) {
      raw += trial == 0 ? 'z' : "abcdefghijklmnopqrstuvwxyz'"[random() % 27];
      size_t count = im_search(raw.c_str(), raw.size());
      size_t decoded = 0;
      std::string accepted = im_get_sps_str(&decoded);
      if (accepted.size() < raw.size()) {
        raw = raw.substr(accepted.size());
        im_reset_search();
        count = im_search(raw.c_str(), raw.size());
      }
      if (count) {
        CHECK(!candidate(0).empty());
        if (random() % 8 == 0) {
          im_choose(random() % count);
          const uint16* starts = NULL;
          if (im_get_fixed_len() == im_get_spl_start_pos(starts)) {
            im_reset_search();
            raw.clear();
          }
        }
      }
    }
    size_t decoded = 0;
    size_t length = std::strlen(im_get_sps_str(&decoded));
    while (length) im_delsearch(--length, false, false);
  }
}

static void vocabulary(const char* additions) {
  const char* inputs[] = {"damoxing", "dayuyanmoxing", "duomotai", "duandaoduan",
                         "shengchengshi", "weixin", "erweima", "yuyinshurufa",
                         "shenduxuexi", "zidongjiashi", "xuexilv", "xiancun"};
  const char16_t* expected[] = {u"大模型", u"大语言模型", u"多模态", u"端到端",
                               u"生成式", u"微信", u"二维码", u"语音输入法",
                               u"深度学习", u"自动驾驶", u"学习率", u"显存"};
  for (size_t i = 0; i < 12; ++i) {
    im_reset_search();
    CHECK(im_search(inputs[i], std::strlen(inputs[i])) > 0);
    if (candidate(0) != expected[i]) std::fprintf(stderr, "Vocabulary: %s\n", inputs[i]);
    CHECK(candidate(0) == expected[i]);
  }
  std::ifstream source(additions);
  CHECK(source.good());
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utf8;
  std::string line;
  size_t checked = 0;
  while (std::getline(source, line)) {
    std::istringstream fields(line);
    std::string word, weight, flag, syllable, spelling;
    if (!(fields >> word) || word[0] == '#') continue;
    CHECK(bool(fields >> weight >> flag));
    while (fields >> syllable) spelling += syllable;
    im_reset_search();
    size_t count = im_search(spelling.c_str(), spelling.size());
    const std::u16string text = utf8.from_bytes(word);
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      if (candidate(i) != text) continue;
      CHECK(im_choose(i) == 1);
      CHECK(candidate(0) == text);
      CHECK(im_get_fixed_len() == text.size());
      found = true;
      break;
    }
    if (!found) std::fprintf(stderr, "Missing dictionary entry: %s\n", word.c_str());
    CHECK(found);
    checked++;
  }
  CHECK(checked > 0);
  std::printf("Dictionary entries selected: %zu\n", checked);
}

static void corrections_test(const char* dictionary) {
  MatrixSearch literal;
  CHECK(literal.init(dictionary, ""));
  const char* inputs[] = {"zhongugo", "shurua", "shurufaa", "duomtoai", "damoxign", "duandaodaun"};
  const char16_t* expected[] = {u"中国", u"输入法", u"输入法", u"多模态", u"大模型", u"端到端"};
  for (size_t t = 0; t < 6; ++t) {
    im_reset_search();
    literal.reset_search();
    literal.search(inputs[t], std::strlen(inputs[t]));
    char16 first[64] = {};
    CHECK(literal.get_candidate(0, first, 64));
    size_t count = im_search(inputs[t], std::strlen(inputs[t]));
    size_t added = count - literal.get_candidate_num();
    CHECK(added <= 3);
    std::u16string original;
    for (size_t i = 0; first[i]; ++i) original.push_back(first[i]);
    CHECK(candidate(0) == original);
    // Inserting suggestions must preserve every original candidate index.
    for (size_t i = 1; i < literal.get_candidate_num(); ++i) {
      char16 text[64] = {};
      CHECK(literal.get_candidate(i, text, 64));
      std::u16string expected_text;
      for (size_t j = 0; text[j]; ++j) expected_text.push_back(text[j]);
      CHECK(candidate(i + added) == expected_text);
    }
    size_t choice = 0;
    for (size_t i = 1; i < count && i <= 3; ++i)
      if (candidate(i) == expected[t]) { choice = i; break; }
    if (!choice) std::fprintf(stderr, "Missing correction: %s\n", inputs[t]);
    CHECK(choice != 0);
    size_t decoded = 0;
    CHECK(std::string(im_get_sps_str(&decoded)) == inputs[t]);
    CHECK(im_get_fixed_len() == 0);
    char16 short_buffer[1] = {123};
    CHECK(im_get_candidate(choice, short_buffer, 1) == NULL);
    CHECK(short_buffer[0] == 123);
    CHECK(im_choose(choice) == 1);
    CHECK(candidate(0) == expected[t]);
    const uint16* splits = NULL;
    CHECK(im_get_spl_start_pos(splits) == candidate(0).size());
    CHECK(im_get_fixed_len() == candidate(0).size());
    const char* spelling = im_get_sps_str(&decoded);
    CHECK(splits[candidate(0).size()] == std::strlen(spelling));
    CHECK(decoded == std::strlen(spelling));
    CHECK(im_cancel_last_choice() > 0);
    CHECK(std::string(im_get_sps_str(&decoded)) == inputs[t]);
    CHECK(im_get_fixed_len() == 0);
    im_delsearch(std::strlen(inputs[t]) - 1, false, false);
    CHECK(std::string(im_get_sps_str(&decoded)) == std::string(inputs[t]).substr(0, std::strlen(inputs[t]) - 1));
  }

  const char* unchanged[] = {"nihao", "zhongguo", "xuexi", "shiyong", "damoxing", "xi'an", "zg", "zz"};
  for (const char* input : unchanged) {
    literal.reset_search();
    literal.search(input, std::strlen(input));
    im_reset_search();
    CHECK(im_search(input, std::strlen(input)) == literal.get_candidate_num());
  }
  im_reset_search();
  size_t count = im_search("zhongugo", 8);
  literal.reset_search();
  literal.search("zhongugo", 8);
  size_t added = count - literal.get_candidate_num();
  CHECK(added > 0);
  CHECK(im_choose(added + 1) == literal.choose(1));
  CHECK(im_get_fixed_len() == literal.get_fixedlen());
  CHECK(im_get_fixed_len() > 0);
  CHECK(im_cancel_last_choice() > 0);
  CHECK(im_get_fixed_len() == 0);
  count = im_search("zhongugo", 8);
  char16 text[64];
  CHECK(im_get_candidate(count, text, 64) == NULL);
  CHECK(im_choose(count) == count);
  im_reset_search();
  CHECK(im_get_candidate(1, text, 64) == NULL);

  auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < 200; ++i) {
    im_reset_search();
    const char* input = inputs[i % 6];
    im_search(input, std::strlen(input));
  }
  double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::printf("Correction search: %.3f ms average (200 host queries)\n", elapsed / 200);
}

int main(int argc, char** argv) {
  CHECK(argc == 4);
  const std::string test = argv[2];
  CHECK(test == "all" || test == "candidates" || test == "unmatched" ||
        test == "capacity" || test == "editing" || test == "vocabulary" || test == "corrections");
  CHECK(im_open_decoder(argv[1], ""));
  if (test == "all" || test == "candidates") candidates();
  if (test == "all" || test == "unmatched") unmatched();
  if (test == "all" || test == "vocabulary") vocabulary(argv[3]);
  if (test == "all" || test == "corrections") corrections_test(argv[1]);
  if (test == "all" || test == "editing") editing();
  im_close_decoder();
  if (test == "all" || test == "capacity") capacity(argv[1]);
  for (size_t i = 0; i < 10; ++i) {
    CHECK(im_open_decoder(argv[1], ""));
    CHECK(im_search("nihao", 5) > 0);
    CHECK(candidate(0) == u"你好");
    im_close_decoder();
  }
  std::printf("PASS: %s\n", test.c_str());
}
