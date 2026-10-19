#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "dicttrie.h"
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

int main(int argc, char** argv) {
  CHECK(argc == 2 || argc == 3);
  const std::string test = argc == 3 ? argv[2] : "all";
  CHECK(test == "all" || test == "candidates" || test == "unmatched" ||
        test == "capacity" || test == "editing");
  CHECK(im_open_decoder(argv[1], ""));
  if (test == "all" || test == "candidates") candidates();
  if (test == "all" || test == "unmatched") unmatched();
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
