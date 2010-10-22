#include "keywords_extract.h"
#include <cstring>
#include "utf8.h"

//#define DEBUG 0
#define KEYPHRASE_ENABLED 1

namespace inagist_trends {
  // unless otherwise specified functions return 0 or NULL or false as default
  // return values less than 0 are likely error codes

using std::cout;
using std::endl;
using std::string;

KeywordsExtract::KeywordsExtract() {
}

KeywordsExtract::~KeywordsExtract() {
  if (DeInit() < 0)
    std::cerr << "ERROR: DeInit() failed\n";
}

// every input parameter is optional!
//
// stopwords, dictionary and stemmer dictionary file paths, if not given will
// just mean that those dictionaries will not be populated
// 
// if input_file or output_file are given, they will be initialized
// the above are two are typically used by a test program which will
// subsequently call GetKeywords() and PrintKeywords()
//
int KeywordsExtract::Init(const char *stopwords_file,
    const char *dictionary_file,
    const char *stemmer_dictionary_file,
    const char *input_file,
    const char *output_file) {

  // load dictionaries
  if (stopwords_file) {
    int ret = LoadDictionary(stopwords_file, m_stopwords_dictionary);
    if (ret < 0) {
      std::cerr << "ERROR: could not load stopwords file into dictionary\n";
      return -1;
    }
    //PrintDictionary(m_stopwords_dictionary);
  }

  if (dictionary_file) {
    int ret = LoadDictionary(dictionary_file, m_dictionary);
    if (ret < 0) {
      std::cerr << "ERROR: could not load dictionary file into dictionary\n";
      return -1;
    }
    //PrintDictionary(m_dictionary);
  }

  if (input_file) {
    m_tweet_stream.open(input_file, std::ifstream::in);

    m_tweet_stream.seekg(0, std::ios::end);
    unsigned int length = m_tweet_stream.tellg();
    if (length < 2) {
      std::cerr << "ERROR: empty file\n";
      m_tweet_stream.close();
    }
  }

  // open output file
  if (output_file) {
    std::ofstream m_out_stream(output_file, std::ofstream::out);
  }
  return 0;
}

int KeywordsExtract::DeInit() {
  //std::cout << "closing input stream\n";
  if (m_tweet_stream && m_tweet_stream.is_open())
    m_tweet_stream.close();

  //std::cout << "clearing dictionaries\n";
  m_dictionary.clear();
  m_stopwords_dictionary.clear();

  //std::cout << "closing output stream\n";
  if (m_out_stream && m_out_stream.is_open())
    m_out_stream.close();

  //std::cout << "deinit done\n";
  return 0;
}

// this function expects the dictionary words in the following format:
//
// one word or phrase per line
// a single newline character at the end of the line
// lower case expected in most cases
// upper case or mixed case will be inserted as is
// no unnecessary blankspace anywhere. word phrases separated by single spaces
// no empty lines

// the caller MUST ensure that the above conditions are met
// this function checks nothing of the above. just inserts whatever is given
//
int KeywordsExtract::LoadDictionary(const char* file, string_hash_set &dictionary) {
  if (NULL == file) {
    std::cerr << "ERROR: invalid dictionary file\n";
    return -1;
  }

  std::ifstream ifs(file);
  if (!ifs) {
    std::cerr << "ERROR: error opening dictionary file\n";
    return -1;
  }

  std::string str;
  while (getline(ifs, str)) {
    dictionary.insert(str.c_str());
  }

  ifs.close();

  return 0;
}

int KeywordsExtract::DictionaryLookup(char *word) {
  if (m_stopwords_dictionary.find(word) != m_stopwords_dictionary.end())
    cout << word << "stopword" << endl;
  if (m_dictionary.find(word) != m_dictionary.end())
    cout << word << "dictionary word" << endl;
  return 0;
}

int KeywordsExtract::PrintDictionary(string_hash_set dictionary) {
  string_hash_set::const_iterator iter;
  for (iter = dictionary.begin(); iter != dictionary.end(); iter++)
    std::cout << *iter << std::endl;
  return 0;
}

void KeywordsExtract::PrintKeywords(std::set<std::string> &keywords_set) {
  std::set<std::string>::iterator iter;

  for (iter = keywords_set.begin(); iter != keywords_set.end(); iter++)
//  std::cout << *iter << " ";
//std::cout << std::endl;
    std::cout << *iter << std::endl;
}

// this function isn't unicode safe
// TODO (balaji) for ascii, we can ofcourse use an array lookup to speed up
bool KeywordsExtract::IsPunct(char *ptr, char *prev, char *next) {
  if (!ptr || *ptr == ' ' || *ptr == '\0')
    return true;
  if (!ispunct(*ptr))
    return false;

  switch (*ptr) {
    case '\'':
      if (prev)
        if (!IsPunct(prev) &&
            (!strncmp(ptr, "'s", 2) || !strncmp(ptr, "'t", 2) || !strncmp(ptr, "'ve", 2) ||
             !strncmp(ptr, "'ll", 2) || !strncmp(ptr, "'re", 2) || !strncmp(ptr, "'m", 2) ||
             !strncmp(ptr, "'em", 3))
           )
          return false;
      break;
    case '@':
      if (prev && !IsPunct(prev))
        return true;
      return IsPunct(next);
      break;
    case '#':
      if (!next)
        return true;
      else
        if (*next == ' ' || IsPunct(next))
          return true;
      //if (prev)
      //  if (*prev != ' ' && *prev != '\0' && IsPunct(prev))
      //   return true;
      return false;
      break;
    case '-':
      if (prev && next)
        if (isalnum(*prev) && (isalnum(*next)))
          return false;
      break;
    case ':':
    case ',':
      if (prev && next)
        if (isdigit(*prev) && isdigit(*next))
          return false;
      break;
    case '.':
      if (prev && next)
        if (isdigit(*prev) && isdigit(*next))
          return false;
      if (next) {
        if (*next == ' ')
          return true;
        if (!strncmp(ptr, ".com", 4) || !strncmp(ptr, ".org", 4))
          return false; // not handling .come on or .organization etc
      }
      break;
    case '&':
      if (next)
        if (*next == '#' && isdigit(*(next+1)))
          return false;
    case '_':
      return false;
    default:
      break;
  }

  return true;
}

bool KeywordsExtract::IsIgnore(char *&ptr) {
  if (!ptr || '\0' == *ptr)
    return false;
  if ('@' == *ptr || !strncmp(ptr, "&#", 2) || !strncmp(ptr, "http://", 7) || !strncmp(ptr, "www.", 4)) {
#ifdef DEBUG
    printf("%s is ignore word\n", ptr);
#endif
    while (' ' != *(ptr+1) && '\0' != *(ptr+1)) {
      ptr++;
    }
    return true;
  }
  return false;
}

// this function compares the input the code point ranges and
// populates output parameter with 2 letter script identifier
int KeywordsExtract::DetectScript(int code_point, std::string &script) {

  // TODO (balaji) will optimize later.
  // if its only few languages, we can create fixed array based hash
  // else something on the lines of BST

  if (code_point >= 0 && code_point <= 879) {
    if (code_point >= 0 && code_point <= 127) {
      script = "en"; //Basic
    } else if (code_point >= 128 && code_point <= 255) {
      script = "en"; // Latin-1
    } else if (code_point >= 256 && code_point <= 383) {
      script = "en"; //Latin
    } else if (code_point >= 384 && code_point <= 591) {
      script = "en"; //Latin
    } else if (code_point >= 592 && code_point <= 687) {
      script = "en"; //IPA
    } else if (code_point >= 688 && code_point <= 767) {
      script = "en"; //Spacing
    } else if (code_point >= 768 && code_point <= 879) {
      script = "en"; //Combining
    }
    return 0;
  }
  else if (code_point >= 880 && code_point <= 1023) {
    script = "el"; //Greek
  } else if (code_point >= 1024 && code_point <= 1279) {
    script = "ru"; //Cyrillic
  } else if (code_point >= 1328 && code_point <= 1423) {
    script = "hy"; //Armenian
  } else if (code_point >= 1424 && code_point <= 1535) {
    script = "he"; //Hebrew
  } else if (code_point >= 1536 && code_point <= 1791) {
    script = "ar"; //Arabic
  } else if (code_point >= 1792 && code_point <= 1871) {
    script = "arc"; //Syriac
  } else if (code_point >= 1920 && code_point <= 1983) {
    script = "dv"; //Thaana
  } else if (code_point >= 2304 && code_point <= 2431) {
    script = "hi"; //Devanagari
  } else if (code_point >= 2432 && code_point <= 2559) {
    script = "bn"; //Bengali
  } else if (code_point >= 2560 && code_point <= 2687) {
    script = "pa"; //Gurmukhi
  } else if (code_point >= 2688 && code_point <= 2815) {
    script = "gu"; //Gujarati
  } else if (code_point >= 2816 && code_point <= 2943) {
    script = "or"; //Oriya
  } else if (code_point >= 2944 && code_point <= 3071) {
    script = "ta"; //Tamil
  } else if (code_point >= 3072 && code_point <= 3199) {
    script = "te"; //Telugu
  } else if (code_point >= 3200 && code_point <= 3327) {
    script = "kn"; //Kannada
  } else if (code_point >= 3328 && code_point <= 3455) {
    script = "ml"; //Malayalam
  } else if (code_point >= 3456 && code_point <= 3583) {
    script = "si"; //Sinhala
  } else if (code_point >= 3584 && code_point <= 3711) {
    script = "th"; //Thai
  } else if (code_point >= 3712 && code_point <= 3839) {
    script = "lo"; //Lao
  } else if (code_point >= 3840 && code_point <= 4095) {
    script = "bo"; //Tibetan
  } else if (code_point >= 4096 && code_point <= 4255) {
    script = "my"; //Myanmar
  } else if (code_point >= 4256 && code_point <= 4351) {
    script = "ka"; //Georgian
  } else if (code_point >= 4352 && code_point <= 4607) {
    script = "ko"; //Hangul
  } else if (code_point >= 4608 && code_point <= 4991) {
    script = "ti"; //Ethiopic
  } else if (code_point >= 5024 && code_point <= 5119) {
    script = "chr"; //Cherokee
  } else if (code_point >= 5120 && code_point <= 5759) {
    script = "cr"; //Unified
  } else if (code_point >= 5760 && code_point <= 5791) {
    script = "ogam"; //Ogham
  } else if (code_point >= 5792 && code_point <= 5887) {
    script = "runr"; //Runic
  } else if (code_point >= 6016 && code_point <= 6143) {
    script = "km"; //Khmer
  } else if (code_point >= 6144 && code_point <= 6319) {
    script = "mn"; //Mongolian
  } else if (code_point >= 7680 && code_point <= 7935) {
    script = "la"; //Latin
  } else if (code_point >= 7936 && code_point <= 8191) {
    script = "el"; //Greek
  } else if (code_point >= 8192 && code_point <= 8303) {
    script = "en"; //General
  } else if (code_point >= 8304 && code_point <= 8351) {
    script = "en"; //Superscripts
  } else if (code_point >= 8352 && code_point <= 8399) {
    script = "en"; //Currency
  } else if (code_point >= 8400 && code_point <= 8447) {
    script = "en"; //Combining
  } else if (code_point >= 8448 && code_point <= 8527) {
    script = "en"; //Letterlike
  } else if (code_point >= 8528 && code_point <= 8591) {
    script = "en"; //Number
  } else if (code_point >= 8592 && code_point <= 8703) {
    script = "en"; //Arrows
  } else if (code_point >= 8704 && code_point <= 8959) {
    script = "en"; //Mathematical
  } else if (code_point >= 8960 && code_point <= 9215) {
    script = "en"; //Miscellaneous
  } else if (code_point >= 9216 && code_point <= 9279) {
    script = "en"; //Control
  } else if (code_point >= 9280 && code_point <= 9311) {
    script = "en"; //Optical
  } else if (code_point >= 9312 && code_point <= 9471) {
    script = "en"; //Enclosed
  } else if (code_point >= 9472 && code_point <= 9599) {
    script = "en"; //Box
  } else if (code_point >= 9600 && code_point <= 9631) {
    script = "en"; //Block
  } else if (code_point >= 9632 && code_point <= 9727) {
    script = "en"; //Geometric
  } else if (code_point >= 9728 && code_point <= 9983) {
    script = "en"; //Miscellaneous
  } else if (code_point >= 9984 && code_point <= 10175) {
    script = "en"; //Dingbats
  } else if (code_point >= 10240 && code_point <= 10495) {
    script = "en"; //Braille
  } else if (code_point >= 11904 && code_point <= 12031) {
    script = "zh"; //CJK
  } else if (code_point >= 12032 && code_point <= 12255) {
    script = "zh"; //Kangxi
  } else if (code_point >= 12272 && code_point <= 12287) {
    script = "en"; //Ideographic
  } else if (code_point >= 12288 && code_point <= 12351) {
    script = "zh"; //CJK
  } else if (code_point >= 12352 && code_point <= 12447) {
    script = "jp"; //Hiragana
  } else if (code_point >= 12448 && code_point <= 12543) {
    script = "jp"; //Katakana
  } else if (code_point >= 12544 && code_point <= 12591) {
    script = "zh"; //Bopomofo
  } else if (code_point >= 12592 && code_point <= 12687) {
    script = "ko"; //Hangul
  } else if (code_point >= 12688 && code_point <= 12703) {
    script = "jp"; //Kanbun
  } else if (code_point >= 12704 && code_point <= 12735) {
    script = "zh"; //Bopomofo
  } else if (code_point >= 12800 && code_point <= 13055) {
    script = "zh"; //Enclosed
  } else if (code_point >= 13056 && code_point <= 13311) {
    script = "zh"; //CJK
  } else if (code_point >= 13312 && code_point <= 19893) {
    script = "zh"; //CJK
  } else if (code_point >= 19968 && code_point <= 40959) {
    script = "zh"; //CJK
  } else if (code_point >= 40960 && code_point <= 42127) {
    script = "zh"; //Yi
  } else if (code_point >= 42128 && code_point <= 42191) {
    script = "zh"; //Yi
  } else if (code_point >= 44032 && code_point <= 55203) {
    script = "ko"; //Hangul
  } else if (code_point >= 55296 && code_point <= 56191) {
    script = "zh"; //High
  } else if (code_point >= 56192 && code_point <= 56319) {
    script = "zh"; //High
  } else if (code_point >= 56320 && code_point <= 57343) {
    script = "zh"; //Low
  } else if (code_point >= 57344 && code_point <= 63743) {
    script = "zh"; //Private
  } else if (code_point >= 63744 && code_point <= 64255) {
    script = "zh"; //CJK
  } else if (code_point >= 64256 && code_point <= 64335) {
    script = "en"; //Alphabetic
  } else if (code_point >= 64336 && code_point <= 65023) {
    script = "ar"; //Arabic
  } else if (code_point >= 65056 && code_point <= 65071) {
    script = "zh"; //Combining
  } else if (code_point >= 65072 && code_point <= 65103) {
    script = "zh"; //CJK
  } else if (code_point >= 65104 && code_point <= 65135) {
    script = "en"; //Small
  } else if (code_point >= 65136 && code_point <= 65278) {
    script = "ar"; //Arabic
  } else if (code_point >= 65279 && code_point <= 65279) {
    script = "en"; //Specials
  } else if (code_point >= 65280 && code_point <= 65519) {
    script = "en"; //Halfwidth
  } else if (code_point >= 65520 && code_point <= 65533) {
    script = "en"; //Specials
  }

  return 1;
}

int KeywordsExtract::GetKeywords(char *str, std::set<std::string> &keywords_set) {
  std::set<std::string> keyphrases_set;
  std::string script;
  return GetKeywords(str, script, keywords_set, keyphrases_set);
}

int KeywordsExtract::GetKeywords(char *str,
                                 std::string &user,
                                 std::set<std::string> &keywords_set,
                                 std::map<std::string, std::string> &script_user_map,
                                 std::map<std::string, std::string> &keyword_user_map) {
  std::string script;
  if (GetKeywords(str, script, keywords_set) < 0) {
    cout << "Error: could not get keywords\n";
    return -1;
  }

  std::map<std::string, std::string>::iterator map_iter;
  if (script != "en") {
    if ((map_iter = script_user_map.find(script)) != script_user_map.end()) {
      script_user_map[script]+= ", " + user;
    } else {
      script_user_map[script] = user;
    }
  }
  std::set<std::string>::iterator set_iter;
  for (set_iter = keywords_set.begin(); set_iter != keywords_set.end(); set_iter++) {
    if ((map_iter = keyword_user_map.find(*set_iter)) != keyword_user_map.end()) {
      keyword_user_map[*set_iter]+= ", " + user;
    } else {
      keyword_user_map[*set_iter] = user;
    }
  }

  return 0;
}

#ifdef KEYPHRASE_ENABLED
int KeywordsExtract::GetKeywords(char *str, std::string &script, std::set<std::string> &keywords_set) {
  std::set<std::string> keyphrases_set;
  return GetKeywords(str, script, keywords_set, keyphrases_set);
}
#endif

#ifdef KEYPHRASE_ENABLED
int KeywordsExtract::GetKeywords(char *str, std::string &script, std::set<std::string> &keywords_set, std::set<std::string> &keyphrases_set) {
#else
int KeywordsExtract::GetKeywords(char *str, std::string &script, std::set<std::string> &keywords_set) {
#endif
  if (!str)
    return -1;

  char *ptr = NULL;
  char *probe = NULL;
  char current_word_delimiter;
  char prev_word_delimiter;
  char next_word_delimiter;

  //unsigned in_len = 0;
  //unsigned out_len = 0;
  unsigned int current_word_len = 0;
  unsigned int next_word_len = 0;
#ifdef DEBUG
  int score = 0;
#endif
  int num_mixed_words = 0;
  int num_caps_words = 0;
  int num_words = 0;
  int num_stop_words = 0;
  int num_dict_words = 0;
  int num_numeric_words = 0;
  int num_normal_words = 0; // not caps or stop or dict or numeric

  char *current_word_start = NULL;
  char *current_word_end = NULL;
  char *prev_word_start = NULL;
  char *prev_word_end = NULL;
  char *next_word_start = NULL;
  char *next_word_end = NULL;

  char *caps_entity_start = NULL;
  char *caps_entity_end = NULL;
  char *stopwords_entity_start = NULL;
  char *stopwords_entity_end = NULL;
#ifdef KEYPHRASE_ENABLED
  char *stopwords_keyphrase_start = NULL;
  char *stopwords_keyphrase_end = NULL;
#endif
  char *sentence_start = NULL;

  // TODO (balaji) use bit map and masks to reduce comparisons
  bool current_word_caps = false;
  bool current_word_all_caps = false;
  bool current_word_has_mixed_case = false;
  bool current_word_starts_num = false;
  bool current_word_precedes_punct = false;
  bool current_word_precedes_ignore_word = false;
  bool prev_word_caps = false;
  bool prev_word_all_caps = false;
  bool prev_word_starts_num = false;
  bool prev_word_has_mixed_case = false;
  bool prev_word_precedes_punct = false;
  bool prev_word_precedes_ignore_word = false;
  bool next_word_caps = false;
  bool next_word_all_caps = false;
  bool next_word_starts_num = false;
  bool next_word_has_mixed_case = false;
  bool next_word_precedes_punct = false;
  bool next_word_precedes_ignore_word = false;

  bool current_word_stop = false;
  bool current_word_dict = false;
  bool prev_word_stop = false;
  bool prev_word_dict = false;
  bool next_word_stop = false;
  bool next_word_dict = false;
  bool is_ignore_word = false;
  bool is_punct = false;

  //bool second_letter_has_caps = false;

  // misc
  char *pch = NULL;
  char ch;

  // script detection
  char *end = strchr(str, '\0');
  script = "en";
  int code_point = 0;
  string script_temp;
  //std::map<std::string, int> script_map;
  int script_count = 0;

  // the whole thing starts here
  ptr = str;

#ifdef DEBUG
  cout << endl << "original query: " << std::string(str) << endl << endl;
#endif

  // go to the first word, ignoring handles and punctuations
  char *prev = NULL;
  while (ptr && '\0' != *ptr && (' ' == *ptr || (ispunct(*ptr) && IsPunct(ptr, prev, ptr+1)) || IsIgnore(ptr))) {
    prev = ptr;
    ptr++;
  }

  if (!ptr || '\0' == *ptr) {
#ifdef DEBUG
    cout << "either the input is empty or has ignore words only" << endl;
    return 0;
#endif
  }

  current_word_start = ptr;
  sentence_start = ptr;
#ifdef DEBUG
  cout << "sentence start: " << sentence_start << endl;
#endif

  if (isupper(*ptr)) {
    current_word_caps = true;
    current_word_all_caps = true;
    current_word_starts_num = false;
    num_caps_words++;
  } else {
    if (isdigit(*ptr)) {
      current_word_starts_num = true; 
      num_numeric_words++;
    } else {
      current_word_starts_num = false;
    }
  }

  // now lets find the end of the current word - while loop works from the second letter
  ptr++;
  while (ptr && ' ' != *ptr && '\0' != *ptr && !(is_punct = IsPunct(ptr, ptr-1, ptr+1))) {
    if (!strcmp(ptr, "&#")) {
      while (' ' != *ptr && '\0' != *ptr)
        ptr++;
      if ('\0' == *ptr)
        break;
    }
    if (isupper(*ptr)) {
      if (!current_word_all_caps && !ispunct(*ptr)) {
          current_word_has_mixed_case = true;
      }
    } else {
      if (current_word_caps)
        current_word_has_mixed_case = false;
      current_word_all_caps = false;
    }
    //ptr++;
    try {
      code_point = utf8::next(ptr, end);
      if (code_point > 0x7F) {
        if (DetectScript(code_point, script_temp) > 0) {
          script = script_temp;
          script_count++;
        }
      }
    } catch (...) {
#ifdef DEBUG
      std::cout << "Exception: " << code_point << " " << ptr << std::endl;
#endif
      ptr++;
    }
  }

  if (!ptr || '\0' == *ptr) {
#ifdef DEBUG
    cout << "either the input has only one word or the other words are ignore words" << endl;
    return 0;
#endif
  }
  current_word_end = ptr;
  current_word_delimiter = *ptr;
  current_word_len = current_word_end - current_word_start;
  *ptr = '\0';
  current_word_precedes_punct = is_punct;
  num_words++;

  // stop words
  if ((m_stopwords_dictionary.find(current_word_start) != m_stopwords_dictionary.end())) {
    current_word_stop = true;
    num_stop_words++;
#ifdef DEBUG
    //cout << "current word: " << current_word_start << " :stopword" << endl;
#endif
  } else {
    current_word_stop = false;
  }

  // dictionary words
  if ((m_dictionary.find(current_word_start) != m_dictionary.end())) {
    current_word_dict = true;
    num_dict_words++;
#ifdef DEBUG
    //cout << "current word: " << current_word_start << " :dictionary word" << endl;
#endif
  } else {
    current_word_dict = false;
  }

  // go to the next word, ignoring punctuation and ignore words.
  // however passing over ignorewords must be recorded
  ptr++;
  is_ignore_word = false;
  is_punct = false;
  while ('\0' != *ptr &&
         (' ' == *ptr || (ispunct(*ptr) && (is_punct = IsPunct(ptr, ptr-1, ptr+1))) || (is_ignore_word = IsIgnore(ptr)))) {
    current_word_precedes_ignore_word |= is_ignore_word;
    current_word_precedes_punct |= is_punct;
    ptr++;
  }

  if (ptr && '\0' != *ptr) {
    next_word_start = ptr;
    num_words++;
    if (current_word_precedes_ignore_word || current_word_precedes_punct) {
      sentence_start = next_word_start;
#ifdef DEBUG
      cout << "sentence start: " << sentence_start << endl;
#endif
    }

    if (isupper(*next_word_start)) {
      next_word_caps = true;
      num_caps_words++;
      next_word_all_caps = true;
      next_word_starts_num = false;
    } else {
      next_word_caps = false;
      next_word_all_caps = false;
      if (isdigit(*next_word_start)) {
        next_word_starts_num = true;
        num_numeric_words++;
      } else {
        next_word_starts_num = false;
      }
    }
  } else {
    next_word_start = NULL;
  }
  probe = ptr + 1;

  while (ptr && probe && *ptr != '\n' && *ptr != '\0') {
    // this loop works between second letter to end punctuation for each word
    is_punct = false;
    if (' ' == *probe || '\0' == *probe || (ispunct(*probe) && (is_punct = IsPunct(probe, probe-1, probe+1)))) {

#ifdef DEBUG
      if (NULL != stopwords_entity_end)
        cout << "ERROR: stopswords entity end is not null. did you not write it before?" << endl;
      if (NULL != caps_entity_end)
        cout << "ERROR: caps entity end is not null. did you not write it before?" << endl;
#endif

      // word boundary
#ifdef DEBUG
      score = 0;
#endif

      if (next_word_start) {
        if (is_punct)
          next_word_precedes_punct = true;
        next_word_delimiter = *probe;
        next_word_end = probe;
        *probe = '\0';
        next_word_len = next_word_end - next_word_start;
      }

#ifdef DEBUG
      cout << endl;
      if (prev_word_start)
        cout << "prev word: " << prev_word_start << endl;
      else
        cout << "prev word: NULL" << endl;
      cout << "current word: " << current_word_start << endl;
      if (next_word_start)
        cout << "next word: " << next_word_start << endl;
      else
        cout << "next word: NULL" << endl;
      cout << endl;
#endif

#ifdef DEBUG
      if ((current_word_len < 2) && !isdigit(*current_word_start))
        score-=5;

      if ('#' == *current_word_start) {
        score++;
      }
#endif

#ifdef DEBUG
      if (prev_word_caps)
        cout << "prev word: " << prev_word_start << " :starts with caps" << endl;
      if (current_word_all_caps) {
        if (current_word_len > 1 && current_word_len < 6) {
          cout << "current word: " << current_word_start << " :all caps" << endl;
        } else {
          cout << "current word: " << current_word_start << " :all caps but bad length" << endl;
        }
      } else if (current_word_has_mixed_case) {
        cout << "current word: " << current_word_start << " :mixed case" << endl;
      } else if (current_word_caps) {
        cout << "current word: " << current_word_start << " :starts with caps" << endl;
      }
      if (next_word_caps)
        cout << "next word: " << next_word_start << " :starts with caps" << endl;
#endif

      // stop words
      if (next_word_start) {
        if ((m_stopwords_dictionary.find(next_word_start) != m_stopwords_dictionary.end())) {
          next_word_stop = true;
          num_stop_words++;
#ifdef DEBUG
          score--;
          cout << "next word: " << next_word_start << " :stopword" << endl;
#endif
        } else {
          next_word_stop = false;
        }

        // dictionary words
        if ((m_dictionary.find(next_word_start) != m_dictionary.end())) {
          next_word_dict = true;
          num_dict_words++;
#ifdef DEBUG
          score--;
          cout << "next word: " << next_word_start << " :dictionary word" << endl;
#endif
        } else {
          next_word_dict = false;
        }
      }

      if (prev_word_end)
        *prev_word_end = prev_word_delimiter;

      if (!current_word_stop && !current_word_dict && !current_word_caps &&
          !current_word_starts_num && !current_word_has_mixed_case &&
          (current_word_len > 1) && '#' != *current_word_start) {
#ifdef DEBUG
        cout << current_word_start << ": normal word" << endl;
#endif
        num_normal_words++;
      }
      if (current_word_has_mixed_case)
        num_mixed_words++;

      if (NULL == stopwords_entity_start) {
        if (current_word_stop) {
          // X of Y case
          if (strcmp(current_word_start, "of") == 0 && NULL != next_word_start && NULL != prev_word_start) {
            if ((prev_word_caps && next_word_caps) &&
                (!prev_word_stop && !next_word_stop) &&
                (!prev_word_dict && !next_word_dict) &&
                (!prev_word_dict && !next_word_dict) &&
                !current_word_precedes_ignore_word &&
                !prev_word_precedes_ignore_word) {
              if (caps_entity_start && caps_entity_start < prev_word_start)
                stopwords_entity_start = caps_entity_start;
              else
                stopwords_entity_start = prev_word_start;
            }
          }
          if (caps_entity_start && (strcmp(current_word_start, "and") == 0) &&
              next_word_start && next_word_caps && !next_word_dict && !next_word_stop &&
              !prev_word_precedes_ignore_word && !prev_word_precedes_punct &&
              !current_word_precedes_ignore_word && !current_word_precedes_punct) {
            stopwords_entity_start = caps_entity_start;
          }
        } else if (NULL != prev_word_start && current_word_starts_num &&
                   (' ' == current_word_delimiter || '\0' == current_word_delimiter)) {
          // handling numbers that occur with cap entities
          if (prev_word_caps && !prev_word_stop && !prev_word_dict &&
              !prev_word_precedes_ignore_word && !prev_word_precedes_punct && 
              (current_word_len > 1 || prev_word_all_caps)) {

            if (caps_entity_start && caps_entity_start < prev_word_start)
              stopwords_entity_start = caps_entity_start;
            else
              stopwords_entity_start = prev_word_start;

            if (!next_word_start || !next_word_caps || next_word_stop || next_word_dict ||
                current_word_precedes_ignore_word || current_word_precedes_punct) {
                stopwords_entity_end = current_word_end;
            }
          }
        } else if (prev_word_stop) {
          if ((NULL == next_word_start || next_word_start == sentence_start) &&
              prev_word_start && strncmp(prev_word_start, "at", 2) == 0 && current_word_caps &&
              !current_word_stop && !current_word_dict) {
            // TODO (balaji) dangerous! don't use strncmp. instead preserve prev_word_delimiter
            stopwords_entity_start = current_word_start;
            stopwords_entity_end = current_word_end;
          } else if (!caps_entity_start && prev_word_start && next_word_start) {
            // Experimental location detection - TODO (balaji) use regex if this experiment succeeds
            if (current_word_caps &&
                strcmp(prev_word_start, "in") == 0 && ',' == current_word_delimiter &&
                next_word_caps && !current_word_dict &&
                !next_word_stop && !next_word_dict && !current_word_stop
               ) {
              stopwords_entity_start = current_word_start;
              stopwords_entity_end = current_word_end;
            } else if (next_word_caps &&
                       ((strcmp(prev_word_start, "place") == 0 && strcmp(current_word_start, "called") == 0 &&
                         !next_word_stop && (',' == next_word_delimiter || '.' == next_word_delimiter || '\0' == next_word_delimiter)) ||
                        (strcmp(prev_word_start, "town") == 0 &&
                         (strcmp(current_word_start, "of") == 0 || strcmp(current_word_start, "called") == 0) &&
                         !next_word_stop && (',' == next_word_delimiter || '.' == next_word_delimiter || '\0' == next_word_delimiter)))
                      ) {
              stopwords_entity_start = next_word_start;
              stopwords_entity_end = next_word_end;
            }
          }
        } else if (caps_entity_start &&
                   next_word_start && next_word_caps && !next_word_stop && !next_word_dict) {
          // Experimental sports event detection - TODO (balaji) use regex if this experiment succeeds
          if ((strcmp(current_word_start, "vs") == 0) ||
              (strcmp(current_word_start, "v") == 0) ||
              (strcmp(current_word_start, "beat") == 0) ||
              (strcmp(current_word_start, "def") == 0) ||
              (strcmp(current_word_start, "defeat") == 0) ||
              (strcmp(current_word_start, "beats") == 0) ||
              (strcmp(current_word_start, "defeats") == 0)) {
            stopwords_entity_start = caps_entity_start;
          }
        }
      } else {
#ifdef DEBUG
        cout << "stopword entity candidate: " << stopwords_entity_start << endl;
#endif
        if (!current_word_caps || current_word_stop || current_word_dict || current_word_starts_num) {
          if (stopwords_entity_start != prev_word_start) {
            stopwords_entity_end = prev_word_end;
          }
          else {
            stopwords_entity_start = NULL;
            stopwords_entity_end = NULL;
          }
        } else if (NULL == next_word_start ||
                   current_word_precedes_ignore_word ||
                   ((current_word_end + 1) != next_word_start)) {
          if (stopwords_entity_start != current_word_start) {
            stopwords_entity_end = current_word_end;
          }
          else {
            stopwords_entity_start = NULL;
            stopwords_entity_end = NULL;
          }
        }
      }

#ifdef KEYPHRASE_ENABLED
      if (NULL == stopwords_keyphrase_start) {
        if ('\0' != current_word_delimiter &&
            !current_word_stop &&
            !current_word_dict &&
            '#' != *current_word_start &&
            NULL != next_word_start &&
            !current_word_precedes_ignore_word &&
            ((current_word_len > 1) || isdigit(*current_word_start))) {
          if (' ' == current_word_delimiter &&
              ((current_word_end + 1) == next_word_start)) {
            stopwords_keyphrase_start = current_word_start;
          }
        }
        stopwords_keyphrase_end = NULL;
      } else {
        if (current_word_stop || current_word_dict || '#' == *current_word_start || ((current_word_len < 2) && !isdigit(*current_word_start))) {
          if (stopwords_keyphrase_start != prev_word_start) {
            stopwords_keyphrase_end = prev_word_end;
          }
          else {
            stopwords_keyphrase_start = NULL;
            stopwords_keyphrase_end = NULL;
          }
        } else {
          if (' ' != current_word_delimiter ||
              NULL == next_word_start ||
              current_word_precedes_ignore_word ||
              ((current_word_end + 1) != next_word_start)) {
            if (stopwords_keyphrase_start != current_word_start) {
              stopwords_keyphrase_end = current_word_end;
            }
            else {
              stopwords_keyphrase_start = NULL;
              stopwords_keyphrase_end = NULL;
            }
          }
        }
      }
#endif

      if (NULL == caps_entity_start) {
        caps_entity_end = NULL;
        if ((current_word_len > 1 && (current_word_caps || ('#' == *current_word_start && isupper(*(current_word_start+1)))) &&
            !current_word_stop && !current_word_dict) && 
            (!prev_word_start || prev_word_precedes_ignore_word || prev_word_precedes_punct || !prev_word_caps || '#' == *current_word_start)) {

          if (' ' == current_word_delimiter &&
              NULL != next_word_start &&
              ((current_word_end + 1) == next_word_start)) {
            if (current_word_start == sentence_start) {
              if (next_word_caps && !next_word_stop && !next_word_dict)
                caps_entity_start = current_word_start;
            } else {
              caps_entity_start = current_word_start;
            }
            caps_entity_end = NULL;

          } /*else if (prev_word_stop &&
              ('\0' == current_word_delimiter ||
              NULL == next_word_start ||
              current_word_precedes_ignore_word ||
              current_word_precedes_punct)) { 

              caps_entity_start = current_word_start;
              caps_entity_end = current_word_end;
          }*/
        }
      } else {
#ifdef DEBUG
        cout << "caps entity candidate: " << caps_entity_start << endl;
#endif
        if (current_word_stop ||
            !current_word_caps ||
            current_word_dict ||
            ((current_word_len < 2) && !isdigit(*current_word_start))) {
          if (caps_entity_start != prev_word_start) {
            caps_entity_end = prev_word_end;
          }
          else {
            caps_entity_start = NULL;
            caps_entity_end = NULL;
          }
        } else {
          if (' ' != current_word_delimiter ||
              NULL == next_word_start ||
              current_word_precedes_ignore_word ||
              ((current_word_end + 1) != next_word_start)) {
            if (caps_entity_start != current_word_start) {
              caps_entity_end = current_word_end;
            }
            else {
              caps_entity_start = NULL;
              caps_entity_end = NULL;
            }
          }
        }
      }

#ifdef KEYPHRASE_ENABLED
      // write keyphrases
      if (NULL != stopwords_keyphrase_start && NULL != stopwords_keyphrase_end) {
        if (stopwords_keyphrase_start != caps_entity_start || stopwords_keyphrase_end != caps_entity_end) {
#ifdef DEBUG
          cout << endl << string(stopwords_keyphrase_start, (stopwords_keyphrase_end - stopwords_keyphrase_start)) << " :keyphrase";
#endif
          if (strncmp(stopwords_keyphrase_end-2, "\'s", 2) == 0) {
            ch = *(stopwords_keyphrase_end-2);
            *(stopwords_keyphrase_end-2) = '\0';
            keyphrases_set.insert(string(stopwords_keyphrase_start, ((stopwords_keyphrase_end-2) - stopwords_keyphrase_start)));
            *(stopwords_keyphrase_end-2) = ch;
          }
          else if ((pch = strstr(stopwords_keyphrase_start, "\'s")) && (pch < stopwords_keyphrase_end)) {
            ch = *pch;
            *pch = '\0';
            // but don't insert the X in X's if X is a single word!
            if (strstr(stopwords_keyphrase_start, " "))
              keyphrases_set.insert(string(stopwords_keyphrase_start, (pch - stopwords_keyphrase_start)));
            *pch = ch;
            keyphrases_set.insert(string(stopwords_keyphrase_start, (stopwords_keyphrase_end - stopwords_keyphrase_start)));
          }
          else { 
            keyphrases_set.insert(string(stopwords_keyphrase_start, (stopwords_keyphrase_end - stopwords_keyphrase_start)));
          }
        } else {
          if (stopwords_keyphrase_start > stopwords_keyphrase_end)
            cout << "ERROR: keyphrase markers are wrong\n";
        }
        stopwords_keyphrase_start = NULL;
        stopwords_keyphrase_end = NULL;
      }
#endif

      // write entities
      if (NULL != stopwords_entity_start && NULL != stopwords_entity_end) {
        if (stopwords_entity_start < stopwords_entity_end) {
          if ('#' == *stopwords_entity_start)
            stopwords_entity_start++;
#ifdef DEBUG
          cout << endl << string(stopwords_entity_start, (stopwords_entity_end - stopwords_entity_start)) << " :entity by stopword";
#endif
          if (strncmp(stopwords_entity_end-2, "\'s", 2) == 0) {
            ch = *(stopwords_entity_end-2);
            *(stopwords_entity_end-2) = '\0';
            keywords_set.insert(string(stopwords_entity_start, ((stopwords_entity_end-2) - stopwords_entity_start)));
            *(stopwords_entity_end-2) = ch;
          }
          else if ((pch = strstr(stopwords_entity_start, "\'s")) && (pch < stopwords_entity_end)) {
            ch = *pch;
            *pch = '\0';
            // but don't insert the X in X's if X is a single word!
            if (strstr(stopwords_entity_start, " "))
              keywords_set.insert(string(stopwords_entity_start, (pch - stopwords_entity_start)));
            *pch = ch;
            keywords_set.insert(string(stopwords_entity_start, (stopwords_entity_end - stopwords_entity_start)));
          } else {
           keywords_set.insert(string(stopwords_entity_start, (stopwords_entity_end - stopwords_entity_start)));
          }
        } else {
          cout << "ERROR: stopwords entity markers are wrong\n";
        }
        stopwords_entity_start = NULL;
        stopwords_entity_end = NULL;
      }

      //  note, keyphrase code above refers to start of caps_entity_start; any change here, will also affect that
      if (NULL != caps_entity_start && NULL != caps_entity_end) {
        if (caps_entity_start < caps_entity_end) {
          if ('#' == *caps_entity_start)
            caps_entity_start++;
#ifdef DEBUG
          cout << endl << string(caps_entity_start, (caps_entity_end - caps_entity_start)) << " :entity by caps";
#endif
          if (strncmp(caps_entity_end-2, "\'s", 2) == 0) {
            ch = *(caps_entity_end-2);
            *(caps_entity_end-2) = '\0';
            keywords_set.insert(string(caps_entity_start, ((caps_entity_end-2) - caps_entity_start)));
            *(caps_entity_end-2) = ch;
          }
          else if ((pch = strstr(caps_entity_start, "\'s")) && (pch < caps_entity_end)) {
            ch = *pch;
            *pch = '\0';
            // but don't insert the X in X's if X is a single word!
            if (strstr(caps_entity_start, " "))
              keywords_set.insert(string(caps_entity_start, (pch - caps_entity_start)));
            *pch = ch;
            keywords_set.insert(string(caps_entity_start, (caps_entity_end - caps_entity_start)));
          }
          else { 
            keywords_set.insert(string(caps_entity_start, (caps_entity_end - caps_entity_start)));
          }
        } else {
          cout << "ERROR: caps entity markers are wrong\n";
        }
        caps_entity_start = NULL;
        caps_entity_end = NULL;
      }

#ifdef DEBUG
      //cout << endl;
#endif

      // exit conditions
      if ('\0' == current_word_delimiter || !next_word_start || '\0' == *next_word_start) {
        *current_word_end = current_word_delimiter;
        break;
      }

      // ****************************** beginning of next cycle ******************************* //

      prev_word_end = current_word_end;
      prev_word_start = current_word_start;

      prev_word_caps = current_word_caps;
      prev_word_has_mixed_case = current_word_has_mixed_case;
      prev_word_all_caps = current_word_all_caps;
      prev_word_stop = current_word_stop;
      prev_word_dict = current_word_dict;
      prev_word_starts_num = current_word_starts_num;
      prev_word_delimiter = current_word_delimiter;
      prev_word_precedes_ignore_word = current_word_precedes_ignore_word;
      prev_word_precedes_punct = current_word_precedes_punct;

      current_word_end = next_word_end;
      current_word_start = next_word_start;

      current_word_caps = next_word_caps;
      current_word_has_mixed_case = next_word_has_mixed_case; 
      current_word_all_caps = next_word_all_caps;
      current_word_stop = next_word_stop;
      current_word_dict = next_word_dict;
      current_word_starts_num = next_word_starts_num;
      current_word_delimiter = next_word_delimiter;
      current_word_precedes_ignore_word = next_word_precedes_ignore_word;
      current_word_precedes_punct = next_word_precedes_punct;
      current_word_len = next_word_len;

      next_word_start = NULL;
      next_word_end = NULL;
      next_word_caps = false;
      next_word_has_mixed_case = false;
      next_word_all_caps = false;
      next_word_stop = false;
      next_word_dict = false;
      next_word_starts_num = false;
      next_word_delimiter = '\0';
      next_word_precedes_ignore_word = false;
      next_word_precedes_punct = false;

      // BE CAREFUL ABOUT WHAT IS NEXT WORD OR CURRENT WORD NOW

      // if current word is not the known last word, briefly probe to see if next word exists
      if ('\0' != current_word_delimiter) {
        ptr = probe + 1;
        if (!ptr) {
          std::cerr << "ERROR: Fatal Exception trying to access unallocated memory space\n";
          exit(-1);
        }

        // find the next position of ptr
        // IsIgnore will literally ignore the word by changing the cursor to next word end
        is_ignore_word = false;
        is_punct = false;
        while ('\0' != *ptr && (' ' == *ptr || (ispunct(*ptr) && (is_punct = IsPunct(ptr, ptr-1, ptr+1))) || (is_ignore_word = IsIgnore(ptr)))) {
          current_word_precedes_ignore_word |= is_ignore_word;
          current_word_precedes_punct |= is_punct; 
          ptr++;
        }

        if (ptr && '\0' != *ptr) {
          next_word_start = ptr;
          num_words++;
          if (current_word_precedes_ignore_word) {
            sentence_start = next_word_start;
#ifdef DEBUG
            cout << "sentence start: " << sentence_start << endl;
#endif
          }

          if (current_word_precedes_punct) {
            sentence_start = next_word_start;
#ifdef DEBUG
            cout << "sentence start: " << sentence_start << endl;
#endif
            if (':' == current_word_delimiter || '>' == current_word_delimiter || '-' == current_word_delimiter || '(' == current_word_delimiter) {
              if (num_normal_words == 0) {
                keywords_set.clear();
                caps_entity_start = NULL;
                caps_entity_end = NULL;
                stopwords_entity_start = NULL;
                stopwords_entity_end = NULL;
              }
            } else {
              for (pch = current_word_end + 1; (pch != next_word_start); pch++) {
                if (':' == *pch || '>' == *pch || '-' == *pch || '(' == *pch) {
                  if (num_normal_words == 0) {
                    keywords_set.clear();
                    caps_entity_start = NULL;
                    caps_entity_end = NULL;
                    stopwords_entity_start = NULL;
                    stopwords_entity_end = NULL;
                  }
                }
              }
            }
          }

          // after finding the start of next word, probe shud be at the same place as ptr
          probe = ptr;

          if (isupper(*next_word_start)) {
            next_word_caps = true;
            num_caps_words++;
            next_word_all_caps = true;
            next_word_starts_num = false;
          } else {
            next_word_caps = false;
            next_word_all_caps = false;
            if (isdigit(*next_word_start)) {
              next_word_starts_num = true;
              num_numeric_words++;
            } else {
              next_word_starts_num = false;
            }
          }
        } else {
          // placing the probe before '/0' so that loop will make it probe++
          // loop will terminate in the next cycle
          probe = ptr-1;
        }
      } // check for current word delimiter 

    } else {
      if (!strcmp(probe, "&#")) {
        while (' ' != *probe && '\0' != *probe)
          probe++;
        if ('\0' == *probe)
          break;
        current_word_precedes_ignore_word = true;
      }
      // TODO (balaji) - mixed case logic seems twisted
      if (isupper(*probe)) {
        if (!next_word_all_caps && !ispunct(*probe)) {
          //if ((probe-1) == ptr)
            //second_letter_has_caps = true;
          //else
          next_word_has_mixed_case = true;
        }
      } else {
        if (next_word_caps)
          next_word_has_mixed_case = false;
        next_word_all_caps = false;
      }
    }

    // a mere cog in a loop wheel, but a giant killer if commented
    if (script_count > 4) {
      probe++;
    } else {
      try {
        code_point = utf8::next(probe, end);
        if (code_point > 0x7F) {
          if (DetectScript(code_point, script_temp) < 0) {
            script = script_temp;
            script_count++;
          }
        }
      } catch (...) {
#ifdef DEBUG
        std::cout << "Exception: " << code_point << " " << probe << std::endl;
#endif
        probe++;
      }
    }
  }

  if (num_mixed_words > 2) {
#ifdef DEBUG
    cout << "non-english tweet. ignoring." << endl;
#endif
    keywords_set.clear();
  }

#ifdef DEBUG
  cout << endl << "\norginal query: " << std::string(str) << endl;
  cout << "num words: " << num_words << endl;
  cout << "num caps words: " << num_caps_words << endl;
  cout << "num stop words: " << num_stop_words << endl;
  cout << "num dict words: " << num_dict_words << endl;
  cout << "num numeric words: " << num_numeric_words << endl;
  cout << "num normal words: " << num_normal_words << endl;
#endif
  std::set<std::string>::iterator iter;
  if ((num_normal_words == 0) && (num_dict_words != 0 || num_words > 5))
    keywords_set.clear();

  return 0;
}

} // namespace inagist_trends
