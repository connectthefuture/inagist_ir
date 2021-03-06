#include "keytuples_extracter.h"
#include <cstring>
#include <cmath>
#include "keytuples_config.h"
#include "script_detector_utils.h"
#include "string_utils.h"
#include "url_words_extracter.h"

#ifdef I18N_ENABLED
#include "utf8.h"
#endif // I18N_ENABLED

#define MAX_DEBUG_BUFFER_LEN 1024
//#define I18N_ENABLED 0

#ifdef SCRIPT_DETECTION_ENABLED
extern int DetectScript(int code_point, std::string &script);
extern int ExtendedAsciiText(int code_point);
#endif // SCRIPT_DETECTION_ENABLED

namespace inagist_trends {
  // unless otherwise specified functions return 0 or NULL or false as default
  // return values less than 0 are likely error codes

KeyTuplesExtracter::KeyTuplesExtracter() {
  m_debug_level = 0;
#ifdef GM_DEBUG
  if (GM_DEBUG > 0) {
    m_debug_level = GM_DEBUG;
  }
#endif // GM_DEBUG
}

KeyTuplesExtracter::~KeyTuplesExtracter() {
  if (Clear() < 0)
    std::cerr << "ERROR: Clear() failed\n";
}

int KeyTuplesExtracter::SetDebugLevel(unsigned int& debug_level) {
  m_debug_level = debug_level;
  return 0;
}

int KeyTuplesExtracter::Init(std::string config_file) {

  inagist_trends::Config config;
  if (inagist_trends::KeyTuplesConfig::Read(config_file.c_str(), config) < 0) {
    std::cerr << "ERROR: could not read config file: " << config_file << std::endl;
    return -1;
  }

  if (Init(config.stopwords_file.c_str(),
           config.dictionary_file.c_str(),
           config.unsafe_dictionary_file.c_str(),
#ifdef INTENT_ENABLED
           config.intent_words_file.c_str(),
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
           config.sentiment_words_file.c_str(),
#endif // SENTIMENT_ENABLED
           config.stemmer_dictionary_file.c_str()) < 0) {
    std::cerr << "ERROR: could not initialize KeyTuplesExtracter\n";
    return -1;
  }

  inagist_trends::KeyTuplesConfig::Clear(config);

  return 0;
}

// every input parameter is optional!
//
// stopwords, dictionary, intent-words and stemmer dictionary file paths, if not given will
// just mean that those dictionaries will not be populated
// 
//
int KeyTuplesExtracter::Init(const char *stopwords_file,
    const char *dictionary_file,
    const char *unsafe_dictionary_file,
#ifdef INTENT_ENABLED
    const char *intent_words_file,
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
    const char *sentiment_words_file,
#endif // SENTIMENT_ENABLED
    const char *stemmer_dictionary_file) {

  // load dictionaries
  if (stopwords_file) {
#ifdef GM_DEBUG
    if (m_debug_level > 3) {
      std::cout << "Info: loading stopwords file - " << stopwords_file << std::endl;
    }
#endif // GM_DEBUG
    int ret = m_stopwords_dictionary.Load(stopwords_file);
    if (ret < 0) {
      std::cerr << "ERROR: could not load stopwords file " \
                << stopwords_file << " into dictionary set\n";
      return -1;
    }
  }

  if (dictionary_file) {
#ifdef GM_DEBUG
    if (m_debug_level > 3) {
      std::cout << "Info: loading dictionary file - " << dictionary_file << std::endl;
    }
#endif // GM_DEBUG
    int ret = m_dictionary.Load(dictionary_file);
    if (ret < 0) {
      std::cerr << "ERROR: could not load dictionary file " \
                << dictionary_file << " into dictionary set\n";
      return -1;
    }
  }

  if (unsafe_dictionary_file) {
#ifdef GM_DEBUG
    if (m_debug_level > 3) {
      std::cout << "Info: loading unsafe dictionary file - " << unsafe_dictionary_file << std::endl;
    }
#endif // GM_DEBUG
    int ret = m_unsafe_dictionary.Load(unsafe_dictionary_file);
    if (ret < 0) {
      std::cerr << "ERROR: could not load unsafe file " \
                << unsafe_dictionary_file << " into dictionary set\n";
      return -1;
    }
  }

#ifdef INTENT_ENABLED
  if (intent_words_file) {
#ifdef GM_DEBUG
    if (m_debug_level > 3) {
      std::cout << "Info: loading intent words file - " << intent_words_file << std::endl;
    }
#endif // GM_DEBUG
    int ret = m_intent_words_dictionary.Load(intent_words_file);
    if (ret < 0) {
      std::cerr << "ERROR: could not load intent words file " \
                << intent_words_file << " into dictionary map\n";
      return -1;
    }
  }
#endif // INTENT_ENABLED

#ifdef SENTIMENT_ENABLED
  if (sentiment_words_file) {
#ifdef GM_DEBUG
    if (m_debug_level > 3) {
      std::cout << "Info: loading sentiment words file - " << sentiment_words_file << std::endl;
    }
#endif // GM_DEBUG
    int ret = m_sentiment_words_dictionary.Load(sentiment_words_file);
    if (ret < 0) {
      std::cerr << "ERROR: could not load sentiment words file: " \
                << sentiment_words_file << " into dictionary map\n";
      return -1;
    }
  }
#endif // SENTIMENT_ENABLED

  return 0;
}

int KeyTuplesExtracter::Clear() {

  m_stopwords_dictionary.Clear();
  m_dictionary.Clear();
  m_unsafe_dictionary.Clear();
#ifdef INTENT_ENABLED
  m_intent_words_dictionary.Clear();
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
  m_sentiment_words_dictionary.Clear();
#endif // SENTIMENT_ENABLED

  return 0;
}

int KeyTuplesExtracter::LoadClassifierDictionary(const char* classifier_dictionary_file) {
  int ret = m_dictionary.Load(classifier_dictionary_file);
  if (ret < 0) {
    std::cerr << "ERROR: could not load classifier dictionary file: " \
              << classifier_dictionary_file;
    return -1;
  }
  return 0;
}

bool KeyTuplesExtracter::IsIgnore(char *&ptr, int& ignore_intent) {
  if (!ptr || '\0' == *ptr)
    return false;
  // anyword which starts with punct (except #) is ignore word
  if ((ispunct(*ptr) && '#' != *ptr && ' ' != *(ptr+1)) ||
      !strncmp(ptr, "#fb", 3) ||
      !strncmp(ptr, "#FB", 3) ||
      !strncmp(ptr, "FB ", 3) ||
      !strncmp(ptr, "fb ", 3)
     ) {
    while (' ' != *(ptr+1) && '\0' != *(ptr+1)) {
      ptr++;
    }
    return true;
  } else if (!strncmp(ptr, "http://", 7) ||
             !strncmp(ptr, "www.", 4) ||
             !strncmp(ptr, "t.co/", 5) ||
             !strncmp(ptr, "lx.im/", 6) ||
             !strncmp(ptr, "RT ", 3)
            ) {
    while (' ' != *(ptr+1) && '\0' != *(ptr+1)) {
      ptr++;
    }
    ignore_intent = -1;
    return true;
  }
  return false;
}

int KeyTuplesExtracter::GetKeyTuples(char* str
#ifdef PROFANITY_CHECK_ENABLED
                                     , std::string& profanity_status
#endif // PROFANITY_CHECK_ENABLED
#ifdef SCRIPT_DETECTION_ENABLED
                                     , std::string& script
#endif // SCRIPT_DETECTION_ENABLED
#ifdef NAMED_ENTITIES_ENABLED
                                     , std::set<std::string>& named_entities_set
#endif // NAMED_ENTITIES_ENABLED
#ifdef KEYWORDS_ENABLED
                                     , std::set<std::string>& keywords_set
#endif // KEYWORDS_ENABLED
#ifdef KEYPHRASE_ENABLED
                                     , std::set<std::string>& keyphrases_set
#endif // KEYPHRASE_ENABLED
#ifdef LANG_ENABLED
                                     , std::set<std::string>& lang_words_set
#endif // LANG_ENABLED
#ifdef TEXT_CLASSIFICATION_ENABLED
                                     , std::set<std::string>& text_class_words_set
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef INTENT_ENABLED
                                     , int& intent_valence
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
                                     , int& sentiment_valence
#endif // SENTIMENT_ENABLED
                                    ) {

  unsigned int text_len = strlen(str);
  if (text_len > MAX_DEBUG_BUFFER_LEN) {
    std::cout << "ERROR: invalid input size\n";
    return -1;
  }

  unsigned char text_buffer[MAX_DEBUG_BUFFER_LEN];
  unsigned int text_buffer_len = MAX_DEBUG_BUFFER_LEN;
  memset(text_buffer, '\0', MAX_DEBUG_BUFFER_LEN);
  strcpy((char *) text_buffer, str);

#ifdef PROFANITY_CHECK_ENABLED
  char profanity_status_buffer[10];
  memset(profanity_status_buffer, '\0', 10);
  unsigned int profanity_status_buffer_len = 10;
#endif // PROFANITY_CHECK_ENABLED

#ifdef SCRIPT_DETECTION_ENABLED
  char script_buffer[4];
  unsigned int script_buffer_len = 4;
  memset(script_buffer, '\0', 4);
#endif // SCRIPT_DETECTION_ENABLED

#ifdef NAMED_ENTITIES_ENABLED
  unsigned char named_entities_buffer[MAX_DEBUG_BUFFER_LEN];
  memset(named_entities_buffer, '\0', MAX_DEBUG_BUFFER_LEN);
  unsigned int named_entities_buffer_len = MAX_DEBUG_BUFFER_LEN;
  unsigned int named_entities_len = 0;
  unsigned int named_entities_count = 0;
#endif

#ifdef KEYWORDS_ENABLED
  unsigned char keywords_buffer[MAX_DEBUG_BUFFER_LEN];
  memset(keywords_buffer, '\0', MAX_DEBUG_BUFFER_LEN);
  unsigned int keywords_buffer_len = MAX_DEBUG_BUFFER_LEN;
  unsigned int keywords_len = 0;
  unsigned int keywords_count = 0;
#endif

#ifdef KEYPHRASE_ENABLED
  unsigned char keyphrases_buffer[MAX_DEBUG_BUFFER_LEN];
  memset(keyphrases_buffer, '\0', MAX_DEBUG_BUFFER_LEN);
  unsigned int keyphrases_buffer_len = MAX_DEBUG_BUFFER_LEN;
  unsigned int keyphrases_len = 0;
  unsigned int keyphrases_count = 0;
#endif

#ifdef LANG_ENABLED
  unsigned char lang_words_buffer[MAX_DEBUG_BUFFER_LEN];
  unsigned int lang_words_buffer_len = MAX_DEBUG_BUFFER_LEN;
  memset(lang_words_buffer, '\0', MAX_DEBUG_BUFFER_LEN);
  unsigned int lang_words_len = 0;
  unsigned int lang_words_count = 0;
#endif

#ifdef TEXT_CLASSIFICATION_ENABLED
  unsigned char text_class_words_buffer[MAX_DEBUG_BUFFER_LEN];
  unsigned int text_class_words_buffer_len = MAX_DEBUG_BUFFER_LEN;
  memset(text_class_words_buffer, '\0', MAX_DEBUG_BUFFER_LEN);
  unsigned int text_class_words_len = 0;
  unsigned int text_class_words_count = 0;
#endif

  int count = 0;

  if ((count = GetKeyTuples((unsigned char*) text_buffer, text_buffer_len, text_len
#ifdef PROFANITY_CHECK_ENABLED
                  , (char *) profanity_status_buffer, profanity_status_buffer_len
#endif // PROFANITY_CHECK_ENABLED
#ifdef SCRIPT_DETECTION_ENABLED
                  , (char *) script_buffer, script_buffer_len
#endif // SCRIPT_DETECTION_ENABLED
#ifdef NAMED_ENTITIES_ENABLED
                  , (unsigned char*) named_entities_buffer, named_entities_buffer_len,
                  named_entities_len, named_entities_count
#endif // NAMED_ENTITIES_ENABLED
#ifdef KEYWORDS_ENABLED
                  , (unsigned char*) keywords_buffer, keywords_buffer_len,
                  keywords_len, keywords_count
#endif // KEYWORDS_ENABLED
#ifdef KEYPHRASE_ENABLED
                  , (unsigned char*) keyphrases_buffer, keyphrases_buffer_len,
                  keyphrases_len, keyphrases_count
#endif // KEYPHRASE_ENABLED
#ifdef LANG_ENABLED
                  , (unsigned char*) lang_words_buffer, lang_words_buffer_len,
                  lang_words_len, lang_words_count
#endif // LANG_ENABLED
#ifdef TEXT_CLASSIFICATION_ENABLED
                  , (unsigned char*) text_class_words_buffer, text_class_words_buffer_len,
                  text_class_words_len, text_class_words_count
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef INTENT_ENABLED
                  , intent_valence
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
                  , sentiment_valence
#endif // SENTIMENT_ENABLED
                )) < 0) {
    std::cout << "ERROR: could not get keytuples\n";
    text_buffer[0] = '\0';
#ifdef NAMED_ENTITIES_ENABLED
    named_entities_buffer[0] = '\0';
#endif
#ifdef KEYWORDS_ENABLED
    keywords_buffer[0] = '\0';
#endif
#ifdef KEYPHRASE_ENABLED
    keyphrases_buffer[0] = '\0';
#endif
#ifdef LANG_ENABLED
    lang_words_buffer[0] = '\0';
#endif // LANG_ENABLED
#ifdef TEXT_CLASSIFICATION_ENABLED
    text_class_words_buffer[0] = '\0';
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef INTENT_ENABLED
    intent_valence = 0;
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
    sentiment_valence = 0;
#endif // SENTIMENT_ENABLED
    return -1;
  }

  text_buffer[0] = '\0';

#ifdef SCRIPT_DETECTION_ENABLED
  if (strlen(script_buffer) > 0) {
    script.assign(script_buffer);
  }
#endif // SCRIPT_DETECTION_ENABLED

#ifdef PROFANITY_CHECK_ENABLED
  if (strlen(profanity_status_buffer) > 0) {
    profanity_status.assign(profanity_status_buffer);
  }
#endif // PROFANITY_CHECK_ENABLED

#if defined NAMED_ENTITIES_ENABLED || defined KEYWORDS_ENABLED || defined KEYPHRASE_ENABLED || defined TEXT_CLASSIFICATION_ENABLED || defined LANG_ENABLED
  unsigned char* pch1 = NULL;
  unsigned char* pch2 = NULL;
  unsigned char uch;
#endif

#ifdef NAMED_ENTITIES_ENABLED
  if (named_entities_len > 0) {
    pch1 = named_entities_buffer;
    pch2 = pch1;
    pch1 = (unsigned char*) strchr((char *) pch2, '|');
    while (pch1 && pch1 != '\0') {
      uch = *pch1;
      *pch1 = '\0';
      named_entities_set.insert((char *) pch2);
      *pch1 = uch;
      pch2 = pch1 + 1;
      pch1 = (unsigned char*) strchr((char *) pch2, '|');
    }
  }
  named_entities_buffer[0] = '\0';
#endif // NAMED_ENTITIES_ENABLED

#ifdef KEYWORDS_ENABLED
  if (keywords_len > 0) {
    pch1 = keywords_buffer;
    pch2 = pch1;
    pch1 = (unsigned char*) strchr((char *) pch2, '|');
    while (pch1 && pch1 != '\0') {
      uch = *pch1;
      *pch1 = '\0';
      keywords_set.insert((char *) pch2);
      *pch1 = uch;
      pch2 = pch1 + 1;
      pch1 = (unsigned char*) strchr((char *) pch2, '|');
    }
  }
  keywords_buffer[0] = '\0';
#endif // KEYWORDS_ENABLED

#ifdef KEYPHRASE_ENABLED
  if (keyphrases_len > 0) {
    pch1 = keyphrases_buffer;
    pch2 = pch1;
    pch1 = (unsigned char*) strchr((char *) pch2, '|');
    while (pch1 && pch1 != '\0') {
      uch = *pch1;
      *pch1 = '\0';
      keyphrases_set.insert((char *) pch2);
      *pch1 = uch;
      pch2 = pch1 + 1;
      pch1 = (unsigned char*) strchr((char *) pch2, '|');
    }
  }
  keyphrases_buffer[0] = '\0';
#endif // KEYPHRASE_ENABLED

#ifdef LANG_ENABLED
  if (lang_words_len > 0) {
    pch1 = lang_words_buffer;
    pch2 = pch1;
    pch1 = (unsigned char*) strchr((char *) pch2, '|');
    while (pch1 && pch1 != '\0') {
      uch = *pch1;
      *pch1 = '\0';
      lang_words_set.insert((char *) pch2);
      *pch1 = uch;
      pch2 = pch1 + 1;
      pch1 = (unsigned char*) strchr((char *) pch2, '|');
    }
  }
  lang_words_buffer[0] = '\0';
#endif // LANG_ENABLED

  unsigned char* pch3 = NULL;
  unsigned char* pch4 = NULL;
  unsigned char ch;

#ifdef TEXT_CLASSIFICATION_ENABLED
  if (text_class_words_len > 0) {
    pch3 = text_class_words_buffer;
    pch4 = pch3;
    pch3 = (unsigned char*) strchr((char *) pch4, '|');
    while (pch3 && pch3 != '\0') {
      ch = *pch3;
      *pch3 = '\0';
      text_class_words_set.insert((char *) pch4);
      *pch3 = ch;
      pch4 = pch3 + 1;
      pch3 = (unsigned char*) strchr((char *) pch4, '|');
    }
  }
  text_class_words_buffer[0] = '\0';
#endif // TEXT_CLASSIFICATION_ENABLED

  return count;
}

// this is an helper function used to make a|b|c| kind of strings given a, b, c in consecutive calls
inline void KeyTuplesExtracter::Insert(unsigned char* buffer, unsigned int& current_len,
                   unsigned char* str_to_add, const unsigned int& str_len,
                   unsigned int& buffer_content_count) { 
  if (strstr((char *) buffer, (char *) str_to_add) == NULL) {
    strncpy((char *) buffer + current_len, (char *) str_to_add, str_len);
    current_len += str_len;
    strcpy((char *) buffer + current_len, "|");
    current_len += 1;
    buffer_content_count++;
  }
}

inline void KeyTuplesExtracter::Insert(unsigned char* buffer, unsigned int& current_len,
                                    std::string& str, unsigned int& buffer_content_count) { 
  unsigned int len = str.length();
  if (len > 0) {
    if (strstr((char *) buffer, str.c_str()) == NULL) {
      strncpy((char *) buffer + current_len, str.c_str(), len);
      current_len += len;
      strcpy((char *) buffer + current_len, "|");
      current_len += 1;
      buffer_content_count++;
    }
  }
}

inline void KeyTuplesExtracter::CopyMapInto(std::map<std::string, int>& sentence_intent_words,
                                   std::map<std::string, int>& intent_words) {
   if (sentence_intent_words.empty())
     return;

   std::map<std::string, int>::iterator map_iter;
   for (map_iter = sentence_intent_words.begin(); map_iter != sentence_intent_words.end(); map_iter++) {
     intent_words.insert(std::pair<std::string, int> (map_iter->first, map_iter->second));
   }
}

int KeyTuplesExtracter::GetURLwords(unsigned char* text_buffer, const unsigned int& text_buffer_len,
                         const unsigned int& text_len,
                         unsigned char* url_words_buffer,
                         const unsigned int& url_words_buffer_len,
                         unsigned int& url_words_len,
                         unsigned int& url_words_count
                        ) {
  return URLwordsExtracter::GetURLwords(text_buffer, text_buffer_len, text_len,
                     m_dictionary,
                     m_stopwords_dictionary,
                     m_unsafe_dictionary,
                     url_words_buffer, url_words_buffer_len,
                     url_words_len, url_words_count,
                     m_debug_level
                    );
}

int KeyTuplesExtracter::GetKeyTuples(unsigned char* text_buffer, const unsigned int& text_buffer_len,
                        const unsigned int& text_len
#ifdef PROFANITY_CHECK_ENABLED
                        , char* profanity_status_buffer, const unsigned int& profanity_status_buffer_len
#endif // PROFANITY_CHECK_ENABLED
#ifdef SCRIPT_DETECTION_ENABLED
                        , char* script_buffer, const unsigned int& script_buffer_len
#endif // SCRIPT_DETECTION_ENABLED
#ifdef NAMED_ENTITIES_ENABLED
                        , unsigned char* named_entities_buffer,
                        const unsigned int& named_entities_buffer_len,
                        unsigned int& named_entities_len,
                        unsigned int& named_entities_count
#endif // NAMED_ENTITIES_ENABLED
#ifdef KEYWORDS_ENABLED
                        , unsigned char* keywords_buffer,
                        const unsigned int& keywords_buffer_len,
                        unsigned int& keywords_len,
                        unsigned int& keywords_count
#endif // KEYWORDS_ENABLED
#ifdef KEYPHRASE_ENABLED
                        , unsigned char* keyphrases_buffer,
                        const unsigned int& keyphrases_buffer_len,
                        unsigned int& keyphrases_len,
                        unsigned int& keyphrases_count
#endif // KEYPHRASE_ENABLED
#ifdef LANG_ENABLED
                        , unsigned char* lang_words_buffer,
                        const unsigned int& lang_words_buffer_len,
                        unsigned int& lang_words_len,
                        unsigned int& lang_words_count
#endif // LANG_ENABLED
#ifdef TEXT_CLASSIFICATION_ENABLED
                        , unsigned char* text_class_words_buffer,
                        const unsigned int& text_class_words_buffer_len,
                        unsigned int& text_class_words_len,
                        unsigned int& text_class_words_count
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef INTENT_ENABLED
                        , int &intent_valence
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
                        , int &sentiment_valence
#endif // SENTIMENT_ENABLED
                       ) {

  if (!text_buffer || text_buffer_len < 1 || text_len < 1) {
    std::cout << "ERROR: invalid buffer(s) at input\n";
    return -1;
  }

  // these pointers are used throughout the function to move over the string
  unsigned char *ptr = NULL;
  unsigned char *end = NULL;

  int ret_val = 0;

  // do we need to work with non-ascii data at all?
#ifdef I18N_ENABLED
  unsigned int code_point = 0;
  bool current_word_ascii = false;
  bool next_word_ascii = false;
#endif // I18N_ENABLED

#if defined SCRIPT_DETECTION_ENABLED && I18N_ENABLED
  // script detection
  if (!script_buffer) {
    std::cout << "ERROR: invalid script buffer\n";
    return -1;
  }
  // initialize output parameter for script
  *script_buffer = '\0';
  std::string script = "UU";
  strcpy(script_buffer, "UU");
  std::string script_temp;
  unsigned int script_count = 0;
  unsigned int english_count = 0;

  // whole thing starts here
  ptr = text_buffer;
  end = ptr + text_len;

  while (ptr && *ptr != '\0') {
#ifdef I18N_ENABLED
    try {
      code_point = utf8::next(ptr, end);
#ifdef SCRIPT_DETECTION_ENABLED
      if (code_point > 0x7F) {
        if (inagist_classifiers::DetectScript(code_point, script_temp) > 0) {
          if (script_temp != "en") {
            if (script_temp != script) {
              script_count = 0;
              script = script_temp;
            }
            else {
              script_count++;
            }
          }
        }
      } else {
        if (code_point > 0x40 && code_point < 0x7B)
          english_count++;
      }
#endif // SCRIPT_DETECTION_ENABLED
    } catch (...) {
#ifdef GM_DEBUG
      if (m_debug_level > 0) {
        std::cout << "EXCEPTION 1: utf8 returned exception" << std::endl;
        std::cout << std::endl << "original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
      }
#endif // GM_DEBUG
      return -1;
    }
#else // I18N_ENABLED
    ptr++;
#endif // I18N_ENABLED
  }

  bool script_flag = false;
  if (script_count == 0 && english_count > 10) {
    script = "en";
    script_flag = true;
  } else if (script_count > 0 && (script_count < 11 || script_count < english_count)) {
    script = "UU";
  }
  strcpy(script_buffer, script.c_str());

  if ((script.compare("UU") != 0) &&
      (script.compare("uu") != 0) &&
      (script.compare("XX") != 0) &&
      (script.compare("xx") != 0)) {
    ret_val++;
  }

  if (!script_flag) {
    return ret_val;
  }
#endif // SCRIPT_DETECTION_ENABLED && I18N_ENABLED

  // initialize parameters for other outputs
#ifdef PROFANITY_CHECK_ENABLED
  if (!profanity_status_buffer) {
#ifdef GM_DEBUG
    std::cerr << "ERROR: invalid profanity_status_buffer\n";
#endif // GM_DEBUG
    return -1;
  }
  *profanity_status_buffer = '\0';
#endif // PROFANITY_CHECK_ENABLED

#ifdef NAMED_ENTITIES_ENABLED
  if (!named_entities_buffer) {
#ifdef GM_DEBUG
    std::cerr << "ERROR: invalid named_entities buffer\n";
#endif // GM_DEBUG
    return -1;
  }
  *named_entities_buffer = '\0';
  named_entities_len = 0;
  named_entities_count = 0;
#endif // NAMED_ENTITIES_ENABLED

#ifdef KEYWORDS_ENABLED
  if (!keywords_buffer) {
#ifdef GM_DEBUG
    std::cerr << "ERROR: invalid keywords buffer\n";
#endif // GM_DEBUG
    return -1;
  }
  *keywords_buffer = '\0';
  keywords_len = 0;
  keywords_count = 0;
#endif // KEYWORDS_ENABLED

#ifdef KEYPHRASE_ENABLED
  if (!keyphrases_buffer) {
#ifdef GM_DEBUG
    std::cerr << "ERROR: invalid keyphrases buffer\n";
#endif // GM_DEBUG
    return -1;
  }
  *keyphrases_buffer = '\0';
  keyphrases_len = 0;
  keyphrases_count = 0;
#endif // KEYPHRASE_ENABLED

#ifdef LANG_ENABLED
  if (!lang_words_buffer) {
#ifdef GM_DEBUG
    std::cerr << "ERROR: invalid lang_words buffer\n";
#endif // GM_DEBUG
    return -1;
  }
  *lang_words_buffer = '\0';
  lang_words_len = 0;
  lang_words_count = 0;
  std::set<std::string> lang_words_set;
#endif // LANG_ENABLED

#ifdef TEXT_CLASSIFICATION_ENABLED
  if (!text_class_words_buffer) {
#ifdef GM_DEBUG
    std::cerr << "ERROR: invalid text_class_words buffer\n";
#endif // GM_DEBUG
    return -1;
  }
  *text_class_words_buffer = '\0';
  text_class_words_len = 0;
  text_class_words_count = 0;
  std::set<std::string> text_class_words_set;
  std::string text_class_word;
#endif // TEXT_CLASSIFICATION_ENABLED

#ifdef SENTIMENT_ENABLED
  sentiment_valence = 0;
#endif // SENTIMENT_ENABLED

#ifdef INTENT_ENABLED
  intent_valence = 0;
#endif // INTENT_ENABLED

  unsigned char *probe = NULL;
  unsigned char current_word_delimiter;
  unsigned char prev_word_delimiter;
  unsigned char next_word_delimiter;

  unsigned int current_word_len = 0;
  unsigned int next_word_len = 0;
  int num_mixed_words = 0;
  int num_caps_words = 0;
  int num_words = 0;
  int num_stop_words = 0;
  int num_dict_words = 0;
  int num_numeric_words = 0;
  int num_normal_words = 0; // not caps or stop or dict or numeric

  unsigned char *current_word_start = NULL;
  unsigned char *current_word_end = NULL;
  std::string current_word;
  unsigned char *prev_word_start = NULL;
  unsigned char *prev_word_end = NULL;
  std::string prev_word;
  unsigned char *next_word_start = NULL;
  unsigned char *next_word_end = NULL;
  std::string next_word;

#if defined NAMED_ENTITIES_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
  unsigned char *caps_entity_start = NULL;
  unsigned char *caps_entity_end = NULL;
  unsigned char *stopwords_entity_start = NULL;
  unsigned char *stopwords_entity_end = NULL;
#endif // NAMED_ENTITIES_ENABLED || TEXT_CLASSIFICATION_ENABLED

#ifdef KEYPHRASE_ENABLED
  unsigned char *stopwords_keyphrase_start = NULL;
  unsigned char *stopwords_keyphrase_end = NULL;
#endif // KEYPHRASE_ENABLED

  unsigned char *sentence_start = NULL;
  bool sentence_is_question = false;

#ifdef INTENT_ENABLED
  unsigned char *intent_start = NULL;
  unsigned char *intent_end = NULL;
  std::string intent_phrase;
  std::map<std::string, int> sentence_intent_words;
  std::map<std::string, int> all_intent_words;
  bool prev_word_negation = false;
  bool current_word_negation = false;
  bool next_word_negation = false;
  int intent_value = 0;
  int sentence_intent_value = 0;
#endif // INTENT_ENABLED

  // TODO (balaji) use bit map and masks to reduce comparisons
  bool current_word_caps = false;
  bool current_word_all_caps = false;
  bool current_word_has_mixed_case = false;
  bool current_word_starts_num = false;
  bool current_word_precedes_punct = false;
  bool current_word_precedes_ignore_word = false;
  unsigned char* current_word_has_apostropheS = NULL;
  bool current_word_hashtag = false;
  bool prev_word_caps = false;
  bool prev_word_all_caps = false;
  bool prev_word_starts_num = false;
  bool prev_word_has_mixed_case = false;
  bool prev_word_precedes_punct = false;
  bool prev_word_precedes_ignore_word = false;
  unsigned char* prev_word_has_apostropheS = NULL;
  bool prev_word_hashtag = false;
  bool next_word_caps = false;
  bool next_word_all_caps = false;
  bool next_word_starts_num = false;
  bool next_word_has_mixed_case = false;
  bool next_word_precedes_punct = false;
  bool next_word_precedes_ignore_word = false;
  unsigned char* next_word_has_apostropheS = NULL;
  bool next_word_hashtag = false;

  bool current_word_stop = false;
  bool current_word_dict = false;
  bool prev_word_stop = false;
  bool prev_word_dict = false;
  bool next_word_stop = false;
  bool next_word_dict = false;
  bool is_ignore_word = false;
  bool is_punct = false;

  // misc
#if defined NAMED_ENTITIES_ENABLED || defined KEYWORDS_ENABLED || defined KEYPHRASE_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
  unsigned char *pch = NULL;
  unsigned char ch;
#endif

#if defined SENTIMENT_ENABLED || defined INTENT_ENABLED
  int dict_value = 0;
#endif // SENTIMENT_ENABLED || INTENT_ENABLED

  int punct_senti = 0;

#ifdef INTENT_ENABLED
  int first_person_valence = 0;
  int class_word_valence = 0;
#endif // INTENT_ENABLED
  int punct_intent = 0;
  int ignore_intent = 0;
  int sentence_punct_intent = 0;

#ifdef PROFANITY_CHECK_ENABLED
  bool text_has_unsafe_words = false;
#endif // PROFANITY_CHECK_ENABLED

  unsigned char* hashword_start = NULL;
  std::string hashtag_phrase;
  std::string hashword;

  // the whole thing starts here again!
  ptr = text_buffer;
  end = ptr + text_len;

#ifdef GM_DEBUG
  if (m_debug_level > 5)
    std::cout << std::endl << "INFO: original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
#endif // GM_DEBUG

  // go to the first word, ignoring handles and punctuations
  unsigned char *prev = NULL;
  while (ptr && '\0' != *ptr && (' ' == *ptr || (ispunct(*ptr) && inagist_utils::IsPunct((char *&) ptr, (char *) prev, (char *) ptr+1, &sentence_punct_intent, &punct_senti)) || IsIgnore((char *&) ptr, ignore_intent))) {
    prev = ptr;
    ptr++;
  }

  if (!ptr || '\0' == *ptr) {
#ifdef GM_DEBUG
    if (m_debug_level > 2)
      std::cout << "INFO: either the input is empty or has ignore words only" << std::endl;
#endif // GM_DEBUG
    return 0;
  }

  if ('#' == *ptr) {
    current_word_hashtag = true;
    ptr++;
  }

  current_word_start = ptr;
  sentence_start = ptr;
  sentence_punct_intent = 0;
#ifdef INTENT_ENABLED
  sentence_is_question = false;
#endif // INTENT_ENABLED

  if (isupper(*ptr)) {
    current_word_caps = true;
    num_caps_words++;
    if (isupper(*(ptr+1))) {
      current_word_all_caps = true;
    }
    *ptr += 32;
  } else if (isdigit(*ptr)) {
    current_word_starts_num = true; 
    num_numeric_words++;
  }

  // now lets find the end of the current word - while loop works from the second letter
#ifdef I18N_ENABLED
  try {
    code_point = utf8::next(ptr, end);
#ifdef SCRIPT_DETECTION_ENABLED
    if (code_point > 0x7F) {
      if (inagist_classifiers::DetectScript(code_point, script_temp) > 0) {
        if (script_temp != "en") {
          if (script_temp != script) {
            script_count = 0;
            script = script_temp;
          }
          else {
            script_count++;
          }
        }
      }
    } else {
      if (code_point > 0x40 && code_point < 0x7B)
        english_count++;
    }
#endif // SCRIPT_DETECTION_ENABLED
  } catch (...) {
#ifdef GM_DEBUG
    if (m_debug_level > 1) {
      std::cout << "EXCEPTION 1: utf8 returned exception" << std::endl;
      std::cout << std::endl << "original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
    }
#endif // GM_DEBUG
    return -1;
  }
#else // I18N_ENABLED
  ptr++;
#endif // I18N_ENABLED

  while (ptr && ' ' != *ptr && '\0' != *ptr &&
         !(is_punct = inagist_utils::IsPunct((char *&) ptr, (char *) ptr-1, (char *) ptr+1, &punct_intent, &punct_senti))) {

    if (!ptr || '\0' == *ptr) {
#ifdef GM_DEBUG
      if (m_debug_level > 2) {
        std::cout << "INFO: either the input is empty or has ignore words only" << std::endl;
      }
#endif // GM_DEBUG
      return 0;
    }


    if (isupper(*ptr)) {
      if (!current_word_all_caps) {
        current_word_has_mixed_case = true;
        if (current_word_hashtag) {
          if (!hashword_start) {
            hashword_start = current_word_start;
          }
          hashword = std::string((const char*) hashword_start, 0, (ptr-hashword_start));
          if (!hashtag_phrase.empty()) {
            hashtag_phrase += " ";
          }
          hashtag_phrase += hashword; 
#ifdef TEXT_CLASSIFICATION_ENABLED
          text_class_words_set.insert(hashword);
#endif // TEXT_CLASSIFICATION_ENABLED
          hashword_start = ptr;
        }
      }
      *ptr += 32;
    } else if (islower(*ptr)) {
      if (current_word_all_caps) {
        current_word_has_mixed_case = true;
        current_word_all_caps = false;
      }
    }

#ifdef I18N_ENABLED
    try {
      code_point = utf8::next(ptr, end);
#ifdef SCRIPT_DETECTION_ENABLED
      if (code_point > 0xFF) {
        if (inagist_classifiers::DetectScript(code_point, script_temp) > 0) {
          if (script_temp != "en") {
            if (script_temp != script) {
              script_count = 0;
              script = script_temp;
            }
            else {
              script_count++;
            }
          }
        }
      } else {
        if (code_point > 0x40 && code_point < 0x7B)
          english_count++;
      }
#endif // SCRIPT_DETECTION_ENABLED
    } catch (...) {
#ifdef GM_DEBUG
      if (m_debug_level > 1) {
        std::cout << "EXCEPTION 2: utf8 returned exception" << std::endl;
        std::cout << std::endl << "original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
      }
#endif // GM_DEBUG
      return -1;
    }
#else // I18N_ENABLED
    ptr++;
#endif // I18N_ENABLED
  } // while loop

  if (!ptr) {
#ifdef GM_DEBUG
    if (m_debug_level > 2) {
      std::cout << "INFO: either the input is corrupt or the only word is ignore word" << std::endl;
    }
#endif // GM_DEBUG
    return 0;
  }

  current_word_end = ptr;
  current_word_delimiter = *ptr;
  current_word_len = current_word_end - current_word_start;
  current_word.assign((char *) current_word_start, current_word_len);
  // *ptr = '\0'; balaji
  if (current_word_len > 2 && memcmp((char *) current_word_end - 2, "'s", 2) == 0) {
    current_word_has_apostropheS = current_word_end-2;
  }
  current_word_precedes_punct = is_punct;
  if (current_word_hashtag) {
    if (hashword_start) {
      hashword = std::string((const char*) hashword_start, 0, (current_word_end - hashword_start));
      if (!hashtag_phrase.empty()) {
        hashtag_phrase += " ";
      }
      hashtag_phrase += hashword; 
#ifdef TEXT_CLASSIFICATION_ENABLED
      text_class_words_set.insert(hashword);
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef NAMED_ENTITIES_ENABLED
      Insert(named_entities_buffer, named_entities_len,
             (unsigned char*) hashtag_phrase.c_str(), hashtag_phrase.length(),
             named_entities_count);
#endif // NAMED_ENTITIES_ENABLED
      hashword_start = NULL;
    }
  }
  num_words++;

#ifdef I18N_ENABLED
  code_point = *current_word_start;
  if (code_point > 0x40 && code_point < 0x7B) {
    current_word_ascii = true;
  } else if (code_point == 0x23) {
    code_point = *(current_word_start+1);
    if (code_point > 0x40 && code_point < 0x7B)
      current_word_ascii = true;
  } else if (inagist_classifiers::ExtendedAsciiText(code_point)) {
    current_word_ascii = true;
  }

  if (current_word_ascii) {
#endif // I18N_ENABLED
    // stop words
    if (m_stopwords_dictionary.Find(current_word) == 1) {
      current_word_stop = true;
      num_stop_words++;
#ifdef GM_DEBUG
      if (m_debug_level > 5) {
        std::cout << "current word: " << current_word << " :stopword" << std::endl;
      }
#endif // GM_DEBUG
    } else {
      current_word_stop = false;
    }

    // dictionary words
    if (m_dictionary.Find(current_word) == 1) {
      current_word_dict = true;
      num_dict_words++;
#ifdef GM_DEBUG
      if (m_debug_level > 5) {
        std::cout << "current word: " << current_word << " :dictionary word" << std::endl;
      }
#endif // GM_DEBUG
    } else {
      current_word_dict = false;
    }
  
#ifdef PROFANITY_CHECK_ENABLED
    if (m_unsafe_dictionary.Find(current_word) == 1) {
      text_has_unsafe_words = true;
    }
#endif // PROFANITY_CHECK_ENABLED

#ifdef SENTIMENT_ENABLED
    if (m_sentiment_words_dictionary.Find(current_word, dict_value) == 1) {
      sentiment_valence += dict_value;
#ifdef GM_DEBUG
      if (m_debug_level > 3) {
        std::cout << "word:" << current_word \
                  << " sentiment_dict_value: " << dict_value << std::endl;
      }
#endif // GM_DEBUG
    }
#endif // SENTIMENT_ENABLED

#ifdef INTENT_ENABLED
    if (m_intent_words_dictionary.Find(current_word, dict_value) == 1) {
      if (dict_value == 10) {
        first_person_valence = 1;
      } else if (dict_value == 20) {
        current_word_negation = true;
      } else if (dict_value == 30) {
        class_word_valence = 1;
      } else {
        if (dict_value == 40) {
          dict_value = 0;
          sentence_is_question = true;
        }
        intent_start = current_word_start;
        sentence_intent_value = dict_value;
      }
    }
#endif

#ifdef I18N_ENABLED
  }
#endif // I18N_ENABLED

#ifdef LANG_ENABLED
  if (current_word_start &&
      !current_word_hashtag) {
    lang_words_set.insert(current_word);
  }
#endif // LANG_ENABLED

#ifdef TEXT_CLASSIFICATION_ENABLED
  //bool is_apostrophe;
  //is_apostrophe = false;
  if (current_word_start &&
      !current_word_stop &&
      !current_word_dict &&
      !current_word_starts_num &&
      !current_word_hashtag &&
      current_word_len > 1) {
    if (current_word_has_apostropheS) {
      text_class_word.assign((char *) current_word_start, (current_word_has_apostropheS-current_word_start));
    } else {
      text_class_word.assign(current_word);
    }
    text_class_words_set.insert(text_class_word);
  }
#endif // TEXT_CLASSIFICATION_ENABLED

  if ('\0' != current_word_delimiter) {
    // this is one gigantic if ending at the end of this function

  // go to the next word, ignoring punctuation and ignore words.
  // however passing over ignorewords must be recorded
#ifdef I18N_ENABLED
  try {
    code_point = utf8::next(ptr, end);
#ifdef SCRIPT_DETECTION_ENABLED
    if (code_point > 0xFF) {
      if (inagist_classifiers::DetectScript(code_point, script_temp) > 0) {
        if (script_temp != "en") {
          if (script_temp != script) {
            script_count = 0;
            script = script_temp;
          }
          else {
            script_count++;
          }
        }
      }
    } else {
      if (code_point > 0x40 && code_point < 0x7B)
        english_count++;
    }
#endif // SCRIPT_DETECTION_ENABLED
  } catch (...) {
#ifdef GM_DEBUG
    if (m_debug_level > 1) {
      std::cout << "EXCEPTION 3: utf8 returned exception" << std::endl;
      std::cout << std::endl << "original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
    }
#endif // GM_DEBUG
    return -1;
  }
#else // I18N_ENABLED
  ptr++;
#endif // I18N_ENABLED

  is_ignore_word = false;
  is_punct = false;
  while ('\0' != *ptr &&
         (' ' == *ptr ||
          (ispunct(*ptr) &&
           (is_punct = inagist_utils::IsPunct((char *&) ptr, (char *) ptr-1, (char *) ptr+1, &sentence_punct_intent, &punct_senti))) ||
          (is_ignore_word = IsIgnore((char *&) ptr, ignore_intent))
         )
        ) {
    current_word_precedes_ignore_word |= is_ignore_word;
    current_word_precedes_punct |= is_punct;
    ptr++;
  }

  if (!ptr || '\0' == *ptr) {
    next_word_start = NULL;
#ifdef GM_DEBUG
    if (m_debug_level > 2) {
      std::cout << "INFO: only one word found\n";
    }
#endif // GM_DEBUG
    return 0;
  } else {
    if ('#' == *ptr) {
      next_word_hashtag = true;
      ptr++;
    }
    next_word_start = ptr;
    num_words++;
    if (current_word_precedes_ignore_word || current_word_precedes_punct) {
#ifdef GM_DEBUG
      if (m_debug_level > 3) {
        std::cout << "sentence: " \
                  << std::string((char *) sentence_start, current_word_end-sentence_start) \
                  << std::endl; 
#ifdef INTENT_ENABLED
        if (sentence_is_question) {
           std::cout << "sentence is question" << std::endl;
        }
#endif // INTENT_ENABLED
      }
#endif // GM_DEBUG

#ifdef INTENT_ENABLED
      if (sentence_punct_intent > 0) {
        if (sentence_is_question) {
          punct_intent = 1;
        } else {
          sentence_intent_words.clear();
        }
      }
      CopyMapInto(sentence_intent_words, all_intent_words);
      sentence_intent_words.clear();
#endif // INTENT_ENABLED

      sentence_start = next_word_start;
      sentence_is_question = false;
      sentence_punct_intent = 0;
    }

    // remember - this is start of word
    if (isupper(*next_word_start)) {
      next_word_caps = true;
      num_caps_words++;
      if (isupper(*(next_word_start + 1))) {
        next_word_all_caps = true;
      }
      *next_word_start += 32;
    } else if (isdigit(*next_word_start)) {
      next_word_starts_num = true;
      num_numeric_words++;
    }

  }

  // now we need to achieve the following
  // probe = ptr + 1;
#ifdef I18N_ENABLED
  probe = ptr;
  try {
    if (probe < end) {
      code_point = utf8::next(probe, end);
    } else {
#ifdef GM_DEBUG
      if (m_debug_level > 0) {
        std::cout << "ERROR: invalid pointers?" << std::endl;
      }
#endif // GM_DEBUG
      return 0;
    }
  } catch (...) {
#ifdef GM_DEBUG
    if (m_debug_level > 1) {
      std::cout << "EXCEPTION 4: utf8 returned exception" << std::endl;
      std::cout << std::endl << "original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
    }
#endif // GM_DEBUG
    return -1;
  }
#else // I18N_ENABLED
  probe = ptr + 1;
#endif // I18N_ENABLED

  while (ptr && probe/* && *ptr != '\n' && *ptr != '\0'*/) {
    // this loop works between second letter to end punctuation for each word
    is_punct = false;
    if (' ' == *probe || '\0' == *probe ||
        (ispunct(*probe) &&
         (is_punct = inagist_utils::IsPunct((char *&) probe, (char *) probe-1, (char *) probe+1, &sentence_punct_intent, &punct_senti))
        )
       ) {

      // word boundary
      if (next_word_start) {
        if (is_punct)
          next_word_precedes_punct = true;
        next_word_delimiter = *probe;
        next_word_end = probe;
        // *probe = '\0'; // balaji
        next_word_len = next_word_end - next_word_start;
        next_word.assign((char *) next_word_start, next_word_len);
        if (next_word_len > 2 && memcmp((char *) next_word_end - 2, "'s", 2) == 0) {
          next_word_has_apostropheS = next_word_end-2;
        }
        if (next_word_hashtag) {
          if (hashword_start) {
            hashword = std::string((const char*) hashword_start, 0, (next_word_end - hashword_start));
            if (!hashtag_phrase.empty()) {
              hashtag_phrase += " ";
            }
            hashtag_phrase += hashword; 
#ifdef TEXT_CLASSIFICATION_ENABLED
            text_class_words_set.insert(hashword);
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef NAMED_ENTITIES_ENABLED
            Insert(named_entities_buffer, named_entities_len,
                   (unsigned char*) hashtag_phrase.c_str(), hashtag_phrase.length(),
                   named_entities_count);
#endif // NAMED_ENTITIES_ENABLED
            hashword_start = NULL;
          }
        }
      }

#ifdef GM_DEBUG
      if (m_debug_level > 5) {
        std::cout << std::endl;
        std::cout << "prev word: " << prev_word << std::endl;
        std::cout << "current word: " << current_word << std::endl;
        std::cout << "next word: " << next_word << std::endl;
        std::cout << std::endl;
      }
#endif // GM_DEBUG

#ifdef GM_DEBUG
      if (m_debug_level > 5) {
        std::cout << "prev word: " << prev_word << " :starts with caps" << std::endl;
        if (current_word_all_caps) {
          if (current_word_len > 1 && current_word_len < 6) {
            std::cout << "current word: " << current_word << " :all caps" << std::endl;
          } else {
            std::cout << "current word: " << current_word << " :all caps but bad length" << std::endl;
          }
        } else if (current_word_has_mixed_case) {
          std::cout << "current word: " << current_word << " :mixed case" << std::endl;
        } else if (current_word_caps) {
          std::cout << "current word: " << current_word << " :starts with caps" << std::endl;
        }
        if (next_word_caps)
          std::cout << "next word: " << next_word << " :starts with caps" << std::endl;
      }
#endif // GM_DEBUG

      if (next_word_start) {
#ifdef I18N_ENABLED
        code_point = *next_word_start;
        if (code_point > 0x40 && code_point < 0x7B) {
          next_word_ascii = true;
        } else if (code_point == 0x23) {
          code_point = *(next_word_start+1);
          if (code_point > 0x40 && code_point < 0x7B)
            next_word_ascii = true;
        } else if (inagist_classifiers::ExtendedAsciiText(code_point)) {
          next_word_ascii = true;
        }

        if (next_word_ascii) {
#endif // I18N_ENABLED
        // stop words
        if (m_stopwords_dictionary.Find(next_word) == 1) {
          next_word_stop = true;
          num_stop_words++;
#ifdef GM_DEBUG
          if (m_debug_level > 5) {
            std::cout << "next word: " << next_word << " :stopword" << std::endl;
          }
#endif // GM_DEBUG
        } else {
          next_word_stop = false;
        }

        // dictionary words
        if (m_dictionary.Find(next_word) == 1) {
          next_word_dict = true;
          num_dict_words++;
#ifdef GM_DEBUG
          if (m_debug_level > 5) {
            std::cout << "next word: " << next_word << " :dictionary word" << std::endl;
          }
#endif // GM_DEBUG
        } else {
          next_word_dict = false;
        }

#ifdef PROFANITY_CHECK_ENABLED
        if (m_unsafe_dictionary.Find(next_word) == 1) {
          text_has_unsafe_words = true;
        }
#endif // PROFANITY_CHECK_ENABLED

#ifdef SENTIMENT_ENABLED
        if (m_sentiment_words_dictionary.Find(next_word, dict_value) == 1) {
          sentiment_valence += dict_value;
#ifdef GM_DEBUG
          if (m_debug_level > 3) {
            std::cout << "word:" << next_word << " dict_value: " << dict_value << std::endl;
          }
#endif // GM_DEBUG
        }
#endif // SENTIMENT_ENABLED

#ifdef INTENT_ENABLED
        bool flag = false;
        if (intent_start) {
          intent_end = next_word_end;
          intent_phrase.assign((const char*) intent_start, 0, intent_end-intent_start);
          if (m_intent_words_dictionary.Find(intent_phrase, dict_value) == 1) {
            if (dict_value == 10) {
              first_person_valence = 1;
            } else if (dict_value == 20) {
              next_word_negation = true;
            } else if (dict_value == 30) {
              class_word_valence = 1;
            } else {
              if (dict_value == 40) {
                dict_value = 0;
                sentence_is_question = true;
              }
              flag = true;
              sentence_intent_value = dict_value;
            }
          }
          if (!flag) {
            intent_end = current_word_end;
            intent_phrase.assign((const char*) intent_start, 0, intent_end-intent_start);
            sentence_intent_words[intent_phrase] = sentence_intent_value;
          }
        }
        if (!flag) {
          if (m_intent_words_dictionary.Find(next_word, dict_value) == 1) {
            if (dict_value == 10) {
              first_person_valence = 1;
            } else if (dict_value == 20) {
              next_word_negation = true;
            } else if (dict_value == 30) {
              class_word_valence = 1;
            } else {
              if (dict_value == 40) {
                dict_value = 0;
                sentence_is_question = true;
              }
              if (!current_word_negation) {
                flag = true;
                intent_start = next_word_start;
                sentence_intent_value = dict_value;
              }
            }
          }
        }
        if (!flag) {
          intent_start = NULL;
        }
#endif // INTENT_ENABLED

#ifdef LANG_ENABLED
        if (!next_word_hashtag) {
          lang_words_set.insert(next_word);
        }
#endif // LANG_ENABLED

#ifdef TEXT_CLASSIFICATION_ENABLED
        if (!next_word_stop &&
            !next_word_dict &&
            !next_word_starts_num &&
            !next_word_hashtag && // balaji
            next_word_len > 1) {
          if (next_word_has_apostropheS) {
            text_class_word.assign((char *) next_word_start, (next_word_has_apostropheS - next_word_start));
          } else {
            text_class_word.assign(next_word);
          }
          text_class_words_set.insert(text_class_word);      
        }
#endif // TEXT_CLASSIFICATION_ENABLED

#ifdef I18N_ENABLED
        }
#endif // I18N_ENABLED
      } // next_word_start

#ifdef I18N_ENABLED
      if (current_word_ascii) {
#endif // I18N_ENABLED

      if (!current_word_stop && /* !current_word_dict && */ !current_word_caps &&
          !current_word_starts_num && !current_word_has_mixed_case &&
          (current_word_len > 1) && !current_word_hashtag) {
#ifdef GM_DEBUG
        if (m_debug_level > 5) {
          std::cout << current_word_start << ": normal word" << std::endl;
        }
#endif // GM_DEBUG
        num_normal_words++;
      }

      if (current_word_has_mixed_case)
        num_mixed_words++;

#if defined NAMED_ENTITIES_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
      if (NULL == stopwords_entity_start) {
        if (current_word_stop) {
          // X of Y case
          if (strncmp((char *) current_word_start, "of", 2) == 0 &&
              NULL != next_word_start &&
              NULL != prev_word_start) {
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
          if (caps_entity_start && (strncmp((char *) current_word_start, "and", 2) == 0) &&
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
              prev_word_start && prev_word.compare("at") == 0 && current_word_caps &&
              !current_word_stop && !current_word_dict) {
            stopwords_entity_start = current_word_start;
            stopwords_entity_end = current_word_end;
          } else if (!caps_entity_start && prev_word_start && next_word_start) {
            // Experimental location detection - TODO (balaji) use regex if this experiment succeeds
            if (current_word_caps &&
                prev_word.compare("in") == 0 && ',' == current_word_delimiter &&
                next_word_caps && !current_word_dict &&
                !next_word_stop && !next_word_dict && !current_word_stop
               ) {
              stopwords_entity_start = current_word_start;
              stopwords_entity_end = current_word_end;
            } else if (next_word_caps &&
                       ((prev_word.compare("place") == 0 &&
                         current_word.compare("called") == 0 &&
                         !next_word_stop &&
                         (',' == next_word_delimiter || '.' == next_word_delimiter || '\0' == next_word_delimiter)
                        ) ||
                        (prev_word.compare("town") == 0 &&
                         (current_word.compare("of") == 0 || current_word.compare("called") == 0) &&
                         !next_word_stop &&
                         (',' == next_word_delimiter || '.' == next_word_delimiter || '\0' == next_word_delimiter)
                        )
                       )
                      ) {
              stopwords_entity_start = next_word_start;
              stopwords_entity_end = next_word_end;
            }
          }
        } else if (caps_entity_start &&
                   next_word_start && next_word_caps && !next_word_stop && !next_word_dict) {
          // Experimental sports event detection - TODO (balaji) use regex if this experiment succeeds
          if ((current_word.compare("vs") == 0) ||
              (current_word.compare("v") == 0) ||
              (current_word.compare("beat") == 0) ||
              (current_word.compare("def") == 0) ||
              (current_word.compare("defeat") == 0) ||
              (current_word.compare("beats") == 0) ||
              (current_word.compare("defeats") == 0)) {
            stopwords_entity_start = caps_entity_start;
          }
        }
      } else {
#ifdef GM_DEBUG
        if (m_debug_level > 5) {
          std::cout << "stopword entity candidate: " << stopwords_entity_start << std::endl;
        }
#endif // GM_DEBUG

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
#endif // NAMED_ENTITIES_ENABLED || TEXT_CLASSIFICATION_ENABLED

#ifdef KEYPHRASE_ENABLED
      if (NULL == stopwords_keyphrase_start) {
        if ('\0' != current_word_delimiter &&
#ifdef I18N_ENABLED
            current_word_ascii &&
#endif // I18N_ENABLED
            !current_word_stop &&
            !current_word_dict &&
            //!current_word_hashtag && // NOTE: hashtags can be start of keyphrases
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
        if (current_word_stop ||
            current_word_dict ||
            current_word_hashtag || // NOTE: using hashtag to end keyphrase
            ((current_word_len < 2) && !isdigit(*current_word_start))
           ) {
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
#endif // KEYPHRASE_ENABLED

#if defined NAMED_ENTITIES_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
      if (NULL == caps_entity_start) {
        caps_entity_end = NULL;
        if ((current_word_len > 1 && (current_word_caps) &&
            !current_word_stop && !current_word_dict) && 
            (!prev_word_start ||
             prev_word_precedes_ignore_word ||
             prev_word_precedes_punct ||
             !prev_word_caps ||
             current_word_hashtag
            )
           ) {

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

          } else if (prev_word_stop &&
              ('\0' == current_word_delimiter ||
              NULL == next_word_start ||
              current_word_precedes_ignore_word ||
              current_word_precedes_punct)) { 

              // this is a single word entity. so not inserting into named_entities
              // caps_entity_start = current_word_start;
              // caps_entity_end = current_word_end;

              // instead insert into keywords set
#ifdef KEYWORDS_ENABLED
              if ((keywords_len + current_word_len + 1) < keywords_buffer_len) {
                Insert(keywords_buffer, keywords_len, current_word_start, current_word_len, keywords_count);
              }
#endif // KEYWORDS_ENABLED
          }
        }
      } else {
#ifdef GM_DEBUG
        if (m_debug_level > 5) {
          std::cout << "caps entity candidate: " << caps_entity_start << std::endl;
        }
#endif // GM_DEBUG

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
#endif // NAMED_ENTITIES_ENABLED || TEXT_CLASSIFICATION_ENABLED

      unsigned int temp_len;
      temp_len = 0;

#ifdef KEYPHRASE_ENABLED
      // write keyphrases
      if (NULL != stopwords_keyphrase_start && NULL != stopwords_keyphrase_end) {

#ifdef NAMED_ENTITIES_ENABLED
        if (stopwords_keyphrase_start != caps_entity_start || stopwords_keyphrase_end != caps_entity_end) {
#endif // NAMED_ENTITIES_ENABLED

#ifdef GM_DEBUG
          if (m_debug_level > 1) {
            temp_len = stopwords_keyphrase_end - stopwords_keyphrase_start;
            std::cout << std::endl << std::string((char *) stopwords_keyphrase_start, temp_len) << " :keyphrase";
          }
#endif // GM_DEBUG

          if (strncmp((char *) stopwords_keyphrase_end-2, "\'s", 2) == 0) {
            temp_len = ((stopwords_keyphrase_end-2) - stopwords_keyphrase_start);
            if ((keyphrases_len + temp_len + 1) < keyphrases_buffer_len) {
              Insert(keyphrases_buffer, keyphrases_len,
                     stopwords_keyphrase_start, temp_len,
                     keyphrases_count);
            }
          }
          else if ((pch = (unsigned char*) strstr((char *) stopwords_keyphrase_start, "\'s")) &&
                   (pch < stopwords_keyphrase_end)) {
            // but don't insert the X in X's if X is a single word!
            if (strstr((char *) stopwords_keyphrase_start, " ")) {
              temp_len = pch - stopwords_keyphrase_start;
              if ((keyphrases_len + temp_len + 1) < keyphrases_buffer_len) {
                Insert(keyphrases_buffer, keyphrases_len,
                       stopwords_keyphrase_start, temp_len,
                       keyphrases_count);
              }
            }
            temp_len = stopwords_keyphrase_end - stopwords_keyphrase_start;
            if ((keyphrases_len + temp_len + 1) < keyphrases_buffer_len) {
              Insert(keyphrases_buffer, keyphrases_len,
                     stopwords_keyphrase_start, temp_len,
                     keyphrases_count);
            }
          }
          else { 
            temp_len = stopwords_keyphrase_end - stopwords_keyphrase_start;
            if ((keyphrases_len + temp_len + 1) < keyphrases_buffer_len) {
              Insert(keyphrases_buffer, keyphrases_len,
                     stopwords_keyphrase_start, temp_len,
                     keyphrases_count);
            }
          }
#ifdef NAMED_ENTITIES_ENABLED
        } else {
          if (stopwords_keyphrase_start > stopwords_keyphrase_end)
            std::cout << "ERROR: keyphrase markers are wrong\n";
        }
#endif // NAMED_ENTITIES_ENABLED
        stopwords_keyphrase_start = NULL;
        stopwords_keyphrase_end = NULL;
      }
#endif // KEYPHRASE_ENABLED

#if defined NAMED_ENTITIES_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
      // write entities
      if (NULL != stopwords_entity_start && NULL != stopwords_entity_end) {
        if (stopwords_entity_start < stopwords_entity_end) {
#ifdef GM_DEBUG
          if (m_debug_level > 1) {
            std::cout << std::endl
                 << std::string((char *) stopwords_entity_start, (stopwords_entity_end - stopwords_entity_start)) \
                 << " :entity by stopword";
          }
#endif // GM_DEBUG
          if (strncmp((char *) stopwords_entity_end-2, "\'s", 2) == 0) {
            temp_len = (stopwords_entity_end-2) - stopwords_entity_start;
#ifdef NAMED_ENTITIES_ENABLED
            if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
              Insert(named_entities_buffer, named_entities_len,
                     stopwords_entity_start, temp_len,
                     named_entities_count);
            }
#elif defined TEXT_CLASSIFICATION_ENABLED
            if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
              Insert(text_class_words_buffer, text_class_words_len,
                     stopwords_entity_start, temp_len,
                     text_class_words_count);
            }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
         }
          else if ((pch = (unsigned char*) strstr((char *) stopwords_entity_start, "\'s")) && (pch < stopwords_entity_end)) {
            // but don't insert the X in X's if X is a single word!
            temp_len = pch - stopwords_entity_start;
            if ((pch = (unsigned char*) strstr((char *) stopwords_entity_start, " ")) &&
                (pch < stopwords_entity_end)) {
#ifdef NAMED_ENTITIES_ENABLED
              if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
                Insert(named_entities_buffer, named_entities_len,
                       stopwords_entity_start, temp_len,
                       named_entities_count);
              }
#elif defined TEXT_CLASSIFICATION_ENABLED
              if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
                Insert(text_class_words_buffer, text_class_words_len,
                       stopwords_entity_start, temp_len,
                       text_class_words_count);
              }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
            } else {
#ifdef KEYWORDS_ENABLED
              if ((keywords_len + temp_len + 1) < keywords_buffer_len) {
                Insert(keywords_buffer, keywords_len,
                       stopwords_entity_start, temp_len,
                       keywords_count);
              }
#elif defined TEXT_CLASSIFICATION_ENABLED
              if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
                Insert(text_class_words_buffer, text_class_words_len,
                       stopwords_entity_start, temp_len,
                       text_class_words_count);
              }
#endif // KEYWORDS_ENABLED || TEXT_CLASSIFICATION_ENABLED
            }

            temp_len = stopwords_entity_end - stopwords_entity_start;
#ifdef NAMED_ENTITIES_ENABLED
            if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
              Insert(named_entities_buffer, named_entities_len,
                     stopwords_entity_start, temp_len,
                     named_entities_count);
            }
#elif defined TEXT_CLASSIFICATION_ENABLED
            if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
              Insert(text_class_words_buffer, text_class_words_len,
                     stopwords_entity_start, temp_len,
                     text_class_words_count);
            }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
          } else {
            temp_len = stopwords_entity_end - stopwords_entity_start;
#ifdef NAMED_ENTITIES_ENABLED
            if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
              Insert(named_entities_buffer, named_entities_len,
                     stopwords_entity_start, temp_len,
                     named_entities_count);
            }
#elif defined TEXT_CLASSIFICATION_ENABLED
            if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
              Insert(text_class_words_buffer, text_class_words_len,
                     stopwords_entity_start, temp_len,
                     text_class_words_count);
            }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
          }
        } else {
          std::cout << "ERROR: stopwords entity markers are wrong\n";
        }
        stopwords_entity_start = NULL;
        stopwords_entity_end = NULL;
      }

      //  note, keyphrase code above refers to start of caps_entity_start; any change here, will also affect that
      if (NULL != caps_entity_start && NULL != caps_entity_end) {
        if (caps_entity_start < caps_entity_end) {
#ifdef GM_DEBUG
          if (m_debug_level > 4) {
            std::cout << std::endl \
                 << std::string((char *) caps_entity_start, (caps_entity_end - caps_entity_start)) \
                 << " :entity by caps\n";
          }
#endif // GM_DEBUG
          if (strncmp((char *) caps_entity_end-2, "\'s", 2) == 0) {
            temp_len = (caps_entity_end-2) - caps_entity_start;
#ifdef NAMED_ENTITIES_ENABLED
            if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
              Insert(named_entities_buffer, named_entities_len,
                     caps_entity_start, temp_len,
                     named_entities_count);
            }
#elif defined TEXT_CLASSIFICATION_ENABLED
            if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
              Insert(text_class_words_buffer, text_class_words_len,
                     caps_entity_start, temp_len,
                     text_class_words_count);
            }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
          }
          else if ((pch = (unsigned char*) strstr((char *) caps_entity_start, "\'s"))
                    && (pch < caps_entity_end)) {
            // don't insert the X in X's if X is a single word!
            temp_len = pch - caps_entity_start;
            if ((pch = (unsigned char*) strstr((char *) caps_entity_start, " ")) &&
                (pch < caps_entity_end)) {
#ifdef NAMED_ENTITIES_ENABLED
              if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
                Insert(named_entities_buffer, named_entities_len,
                       caps_entity_start, temp_len,
                       named_entities_count);
              }
#elif defined TEXT_CLASSIFICATION_ENABLED
              if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
                Insert(text_class_words_buffer, text_class_words_len,
                       caps_entity_start, temp_len,
                       text_class_words_count);
              }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
            } else {
              // but such single word X's can be keywords
#ifdef KEYWORDS_ENABLED
              if ((keywords_len + temp_len + 1) < keywords_buffer_len) {
                Insert(keywords_buffer, keywords_len,
                       caps_entity_start, temp_len,
                       keywords_count);
              }
#endif // KEYWORDS_ENABLED
            }

            temp_len = caps_entity_end - caps_entity_start;
#ifdef NAMED_ENTITIES_ENABLED
            if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
              Insert(named_entities_buffer, named_entities_len,
                     caps_entity_start, temp_len,
                     named_entities_count);
            }
#elif defined TEXT_CLASSIFICATION_ENABLED
            if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
              Insert(text_class_words_buffer, text_class_words_len,
                     caps_entity_start, temp_len,
                     text_class_words_count);
            }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
          } else { 
            temp_len = caps_entity_end - caps_entity_start;
#ifdef NAMED_ENTITIES_ENABLED
            if ((named_entities_len + temp_len + 1) < named_entities_buffer_len) {
              Insert(named_entities_buffer, named_entities_len,
                     caps_entity_start, temp_len,
                     named_entities_count);
            }
#elif defined TEXT_CLASSIFICATION_ENABLED
            if ((text_class_words_len + temp_len + 1) < text_class_words_buffer_len) {
              Insert(text_class_words_buffer, text_class_words_len,
                     caps_entity_start, temp_len,
                     text_class_words_count);
            }
#endif // NAMED_ENTITIES_ENABLED elif TEXT_CLASSIFICATION_ENABLED
          }
        } else {
          std::cout << "ERROR: caps entity markers are wrong\n";
        }
        caps_entity_start = NULL;
        caps_entity_end = NULL;
      }
#endif // NAMED_ENTITIES_ENABLED || TEXT_CLASSIFICATION_ENABLED

#ifdef KEYWORDS_ENABLED 
      // hash tags
      if (current_word_hashtag) {
        if ((keywords_len + current_word_len + 1) < keywords_buffer_len) {
          Insert(keywords_buffer, keywords_len,
                 current_word_start, current_word_len,
                 keywords_count); 
        }
      }
      if (!current_word_stop && !current_word_dict && current_word_has_apostropheS) {
        // we are checking if there is enough space for the word without "'s" in keywords_buffer
        if ((keywords_len + current_word_len - 1) < keywords_buffer_len) {
          Insert(keywords_buffer, keywords_len,
                 current_word_start, current_word_len-2,
                 keywords_count); 
        }
      }
#elif TEXT_CLASSIFICATION_ENABLED
      if (current_word_hashtag) {
        if ((text_class_words_len + current_word_len + 1) < text_class_words_buffer_len) {
          Insert(text_class_words_buffer, text_class_words_len,
                 current_word_start, current_word_len,
                 text_class_words_count); 
        }
      }
      if (!current_word_stop && !current_word_dict && current_word_has_apostropheS) {
        // we are checking if there is enough space for the word without "'s" in text_class_words_buffer
        if ((text_class_words_len + current_word_len - 1) < text_class_words_buffer_len) {
          Insert(text_class_words_buffer, text_class_words_len,
                 current_word_start, current_word_len-2,
                 text_class_words_count); 
        }
      }
#endif // KEYWORDS_ENABLED elif TEXT_CLASSIFICATION_ENABLED

      // intent
      if ('\0' == current_word_delimiter || !next_word_start || '\0' == *next_word_start) {
#ifdef GM_DEBUG
        if (m_debug_level > 3) {
          std::cout << "sentence: " \
                    << std::string((char *) sentence_start, end-sentence_start) \
                    << std::endl; 
#ifdef INTENT_ENABLED
          if (sentence_is_question) {
             std::cout << "sentence is question" << std::endl;
          }
#endif // INTENT_ENABLED
        }
#endif // GM_DEBUG
#ifdef INTENT_ENABLED
        if (sentence_punct_intent > 0) {
          if (sentence_is_question) {
            punct_intent = 1;
          } else {
            sentence_intent_words.clear();
          }
        }
        CopyMapInto(sentence_intent_words, all_intent_words);
        sentence_intent_words.clear();
#endif // INTENT_ENABLED
      }

#ifdef I18N_ENABLED
      }
#endif // I18N_ENABLED

#ifdef GM_DEBUG
      if (m_debug_level > 5) {
        std::cout << std::endl;
      }
#endif // GM_DEBUG

      // exit conditions
      if ('\0' == current_word_delimiter || !next_word_start || '\0' == *next_word_start) {
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
      prev_word_has_apostropheS = current_word_has_apostropheS;
      prev_word_hashtag = current_word_hashtag;
      prev_word = current_word;
#ifdef INTENT_ENABLED
      prev_word_negation = current_word_negation;
#endif // INTENT_ENABLED

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
#ifdef I18N_ENABLED
      current_word_ascii = next_word_ascii;
#endif // I18N_ENABLED
      current_word_has_apostropheS = next_word_has_apostropheS;
      current_word_hashtag = next_word_hashtag;
      current_word = next_word;
#ifdef INTENT_ENABLED
      current_word_negation = next_word_negation;
#endif // INTENT_ENABLED

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
#ifdef I18N_ENABLED
      next_word_ascii = false;
#endif // I18N_ENABLED
      next_word_has_apostropheS = NULL;
      next_word_hashtag = false;
      next_word.clear();
#ifdef INTENT_ENABLED
      next_word_negation = false;
#endif // INTENT_ENABLED

      // BE CAREFUL ABOUT WHAT IS NEXT WORD OR CURRENT WORD NOW

      // if current word is not the known last word, briefly probe to see if next word exists
      if ('\0' != current_word_delimiter) {
        ptr = probe + 1;
        if (!ptr) {
          std::cerr << "ERROR: Fatal Exception trying to access unallocated memory space\n";
          return -1;
        }

        // find the next position of ptr
        // IsIgnore will literally ignore the word by changing the cursor to next word end
        is_ignore_word = false;
        is_punct = false;
        while (('\0' != *ptr)
               && (' ' == *ptr
                   || (ispunct(*ptr) &&
                      (is_punct = inagist_utils::IsPunct((char *&) ptr, (char *) ptr-1, (char *) ptr+1, &sentence_punct_intent, &punct_senti))
                      )
                   || (is_ignore_word = IsIgnore((char *&) ptr, ignore_intent))
                  )
              ) {
          current_word_precedes_ignore_word |= is_ignore_word;
          current_word_precedes_punct |= is_punct; 
          ptr++;
        }

        if (ptr && '\0' != *ptr) {
          if ('#' == *ptr) {
            next_word_hashtag = true;
            ptr++;
          }
          next_word_start = ptr;
          num_words++;
          if (current_word_precedes_ignore_word) {
#ifdef GM_DEBUG
            if (m_debug_level > 3) {
              std::cout << "sentence: " \
                        << std::string((char *) sentence_start, next_word_start-sentence_start) \
                        << std::endl; 
              if (sentence_is_question) {
                 std::cout << "sentence is question" << std::endl;
              }
            }
#endif // GM_DEBUG
#ifdef INTENT_ENABLED
            if (sentence_punct_intent > 0) {
              if (sentence_is_question) {
                punct_intent = 1;
              } else {
                sentence_intent_words.clear();
              }
            }
            CopyMapInto(sentence_intent_words, all_intent_words);
            sentence_intent_words.clear();
            sentence_punct_intent = 0;
#endif // INTENT_ENABLED

            sentence_start = next_word_start;
            sentence_is_question = false;
          }

          // this code is to ignore publisher name in "CNN Breaking News : blah blah" and take only the blah blah part
          // this also sets the sentence_start - pay close attention. bad piece of coding here!
          if (current_word_precedes_punct) {
            if (',' != current_word_delimiter &&
                '*' != current_word_delimiter &&
                '/' != current_word_delimiter &&
                '\'' != current_word_delimiter &&
                '\"' != current_word_delimiter &&
                '$' != current_word_delimiter
               ) {
#ifdef GM_DEBUG
                if (m_debug_level > 3) {
                  std::cout << "sentence: " \
                            << std::string((char *) sentence_start, next_word_start-sentence_start) \
                            << std::endl; 
                  if (sentence_is_question) {
                     std::cout << "sentence is question" << std::endl;
                  }
                }
#endif // GM_DEBUG
#ifdef INTENT_ENABLED
              if (sentence_punct_intent > 0) {
                if (sentence_is_question) {
                  punct_intent = 1;
                } else {
                  sentence_intent_words.clear();
                }
              }
              CopyMapInto(sentence_intent_words, all_intent_words);
              sentence_intent_words.clear();
              sentence_punct_intent = 0;
#endif // INTENT_ENABLED

              sentence_start = next_word_start;
              sentence_is_question = false;
            }
            if (':' == current_word_delimiter ||
                '>' == current_word_delimiter ||
                '-' == current_word_delimiter ||
                '(' == current_word_delimiter) {
              if (num_normal_words == 0) {
#ifdef NAMED_ENTITIES_ENABLED
                *named_entities_buffer = '\0';
                named_entities_len = 0;
#endif // NAMED_ENTITIES_ENABLED
#if defined NAMED_ENTITIES_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
                caps_entity_start = NULL;
                caps_entity_end = NULL;
                stopwords_entity_start = NULL;
                stopwords_entity_end = NULL;
#endif // NAMED_ENTITIES_ENABLED || TEXT_CLASSIFICATION_ENABLED
#ifdef KEYPHRASE_ENABLED
                stopwords_keyphrase_start = NULL;
                stopwords_keyphrase_end = NULL;
#endif // KEYPHRASE_ENABLED
              }
            } else {
#if defined NAMED_ENTITIES_ENABLED || defined TEXT_CLASSIFICATION_ENABLED
              for (pch = current_word_end + 1; (pch != next_word_start); pch++) {
                if (':' == *pch || '>' == *pch || '-' == *pch || '(' == *pch) {
                  if (num_normal_words == 0) {
#ifdef NAMED_ENTITIES_ENABLED
                    *named_entities_buffer = '\0';
                    named_entities_len = 0;
#endif // NAMED_ENTITIES_ENABLED
                    caps_entity_start = NULL;
                    caps_entity_end = NULL;
                    stopwords_entity_start = NULL;
                    stopwords_entity_end = NULL;
#ifdef KEYPHRASE_ENABLED
                    stopwords_keyphrase_start = NULL;
                    stopwords_keyphrase_end = NULL;
#endif // KEYPHRASE_ENABLED
#ifdef INTENT_ENABLED
                    intent_start = NULL;
#endif // INTENT_ENABLED
                  }
                }
              }
#endif // NAMED_ENTITIES_ENABLED || TEXT_CLASSIFICATION_ENABLED
            }
          }

          // after finding the start of next word, probe shud be at the same place as ptr
          probe = ptr;

          if (isupper(*next_word_start)) {
            next_word_caps = true;
            num_caps_words++;
            if (isupper(*(next_word_start+1))) {
              next_word_all_caps = true;
            }
            *next_word_start += 32;
          } else if (isdigit(*next_word_start)) {
            next_word_starts_num = true;
            num_numeric_words++;
          }
        } else {
          // placing the probe before '/0' so that loop will make it probe++
          // loop will terminate in the next cycle
          probe = ptr-1;
        }
      } // check for current word delimiter 

    } else {

      if (isupper(*probe)) {
        if (!next_word_all_caps) {
          next_word_has_mixed_case = true;
          if (next_word_hashtag) {
            if (!hashword_start) {
              hashword_start = next_word_start;
            }
            hashword = std::string((const char*) hashword_start, 0, (probe - hashword_start));
            if (!hashtag_phrase.empty()) {
              hashtag_phrase += " ";
            }
            hashtag_phrase += hashword; 
#ifdef TEXT_CLASSIFICATION_ENABLED
            text_class_words_set.insert(hashword);
#endif // TEXT_CLASSIFICATION_ENABLED
            hashword_start = probe;
          }
        }
        *probe += 32;
      } else if (islower(*probe)) {
        if (next_word_all_caps) {
          next_word_has_mixed_case = true;
          next_word_all_caps = false;
        }
      }

    }

    // a mere cog in a loop wheel, but a giant killer if commented
    if (probe && *probe != '\0') {
#ifdef I18N_ENABLED
        try {
          code_point = utf8::next(probe, end);
#ifdef SCRIPT_DETECTION_ENABLED
          if (code_point > 0xFF) {
            if (inagist_classifiers::DetectScript(code_point, script_temp) > 0) {
              if (script_temp != "en") {
                if (script_temp != script) {
                  script_count = 0;
                  script = script_temp;
                } else {
                  script_count++;
                }
              }
            }
          } else {
            if (code_point > 0x40 && code_point < 0x7B)
              english_count++;
          }
#endif // SCRIPT_DETECTION_ENABLED
        } catch (...) {
#ifdef GM_DEBUG
          if (m_debug_level > 0) {
            std::cout << "Exception 5: " << code_point << " " << probe << std::endl;
            std::cout << std::endl << "original query: " << std::string((char *) text_buffer) << std::endl << std::endl;
          }
#endif // GM_DEBUG
          return -1;
        }
#else // I18N_ENABLED
        probe++;
#endif // I18N_ENABLED
    }
  }
  } // end of gigantic if
  probe = NULL;
  ptr = NULL;
  end = NULL;

#ifdef GM_DEBUG
  if (m_debug_level > 5) {
    std::cout << std::endl << "\norginal query: " << std::string((char *) text_buffer) << std::endl;
    std::cout << "num words: " << num_words << std::endl;
    std::cout << "num caps words: " << num_caps_words << std::endl;
    std::cout << "num stop words: " << num_stop_words << std::endl;
    std::cout << "num dict words: " << num_dict_words << std::endl;
    std::cout << "num numeric words: " << num_numeric_words << std::endl;
    std::cout << "num mixed words: " << num_mixed_words << std::endl;
    std::cout << "num normal words: " << num_normal_words << std::endl;
  }
#endif // GM_DEBUG

#ifdef PROFANITY_CHECK_ENABLED
  if (text_has_unsafe_words)
    strcpy(profanity_status_buffer, "unsafe");
  else
    strcpy(profanity_status_buffer, "safe");
  ret_val += 1;
#endif // PROFANITY_CHECK_ENABLED

#ifdef NAMED_ENTITIES_ENABLED
  if ((num_normal_words == 0) && (num_dict_words != 0 || num_words > 5)) {
#ifdef GM_DEBUG
    if (m_debug_level > 1) {
      std::cout << "INFO: no normal words. ignoring named_entities." << std::endl;
    }
    if (m_debug_level > 3) {
      std::cout << "num normal words: " << num_normal_words << std::endl;
      std::cout << "num words: " << num_words << std::endl;
      std::cout << "num dict words: " << num_dict_words << std::endl;
    }
#endif // GM_DEBUG
    *named_entities_buffer = '\0';
    named_entities_len = 0;
    named_entities_count = 0;
  }

  ret_val += named_entities_count;
#endif // NAMED_ENTITIES_ENABLED

#ifdef KEYWORDS_ENABLED
  ret_val += keywords_count;
#endif // KEYWORDS_ENABLED

#ifdef KEYPHRASE_ENABLED
  ret_val += keyphrases_count;
#endif // KEYPHRASE_ENABLED

  std::set<std::string>::iterator words_iter;
  std::string word;

#ifdef LANG_ENABLED
  for (words_iter = lang_words_set.begin(); words_iter != lang_words_set.end(); words_iter++) {
    word.assign(*words_iter);
    Insert(lang_words_buffer, lang_words_len, word, lang_words_count);
    word.clear();
  }
  ret_val += lang_words_count;
#endif // LANG_ENABLED

#ifdef TEXT_CLASSIFICATION_ENABLED
  for (words_iter = text_class_words_set.begin();
       words_iter != text_class_words_set.end();
       words_iter++) {
    word.assign(*words_iter);
    Insert(text_class_words_buffer, text_class_words_len, word, text_class_words_count);
    word.clear();
  }
#ifdef NAMED_ENTITIES_ENABLED
  if (named_entities_count > 0) {
    strncpy((char *) text_class_words_buffer + text_class_words_len,
            (const char*) named_entities_buffer,
            named_entities_len);
    text_class_words_len += named_entities_len;
    text_class_words_count += named_entities_count;
  }
#endif // NAMED_ENTITIES_ENABLED
#ifdef KEYWORDS_ENABLED
  if (keywords_count > 0) {
    strncpy((char *) text_class_words_buffer + text_class_words_len,
            (const char*) keywords_buffer,
            keywords_len);
    text_class_words_len += keywords_len;
    text_class_words_count += keywords_count;
  }
#endif // KEYWORDS_ENABLED
  ret_val += text_class_words_count;
#endif // TEXT_CLASSIFICATION_ENABLED

#ifdef SENTIMENT_ENABLED
  sentiment_valence += punct_senti;
  /*
  if (sentiment_valence == 0) {
    strcpy(sentiment_buffer, "neutral");
  } else if (sentiment_valence < 0) {
    strcpy(sentiment_buffer, "negative");
  } else {
    strcpy(sentiment_buffer, "positive");
  }
  */
  //sprintf(sentiment_buffer, "%d", sentiment_valence);
  ret_val += 1;
#endif // SENTIMENT_ENABLED

#ifdef INTENT_ENABLED
  std::map<std::string, int>::iterator intent_iter;
  for (intent_iter = all_intent_words.begin(); intent_iter != all_intent_words.end(); intent_iter++) {
#ifdef GM_DEBUG
    if (m_debug_level > 2) {
      std::cout << "intent= " << intent_iter->first << ":" << intent_iter->second << std::endl;
    }
#endif // GM_DEBUG
    intent_valence += intent_iter->second;
  }
  all_intent_words.clear();

  // if no intent word is present, don't consider additional indicators
  if (intent_valence > 0) {
    intent_valence += first_person_valence;
    intent_valence += punct_intent;
    intent_valence += ignore_intent;
    intent_valence += class_word_valence;
  }

#ifdef GM_DEBUG
  if (m_debug_level > 2) {
    std::cout << "first_person_valence:" << first_person_valence << std::endl;
    std::cout << "punct_intent:" << punct_intent << std::endl;
    std::cout << "ignore_intent:" << ignore_intent << std::endl;
    std::cout << "class_word_valence:" << class_word_valence << std::endl;
    std::cout << "intent_valence: " << intent_valence << std::endl;
  }
#endif // GM_DEBUG

  ret_val += 1;
#endif // INTENT_ENABELD

#ifdef GM_DEBUG
  if (m_debug_level > 2) {
    std::cout << "keytuples_extracter summary:" << std::endl;
    std::cout << "input: " << text_buffer << std::endl;
#ifdef SCRIPT_DETECTION_ENABLED
    std::cout << "script: " << script_buffer << std::endl;
#endif // SCRIPT_DETECTION_ENABLED
#ifdef PROFANITY_CHECK_ENABLED
    std::cout << "profanity_status: " << profanity_status_buffer << std::endl;
#endif // PROFANITY_CHECK_ENABLED
#ifdef NAMED_ENTITIES_ENABLED
    std::cout << "named_entities (" << named_entities_count << "): " << named_entities_buffer << std::endl;
#endif // NAMED_ENTITIES_ENABLED
#ifdef KEYWORDS_ENABLED
    std::cout << "keywords (" << keywords_count << "): " << keywords_buffer << std::endl;
#endif // KEYWORDS_ENABLED
#ifdef KEYPHRASE_ENABLED
    std::cout << "keyphrases (" << keyphrases_count << "): " << keyphrases_buffer << std::endl;
#endif // KEYPHRASE_ENABLED
#ifdef LANG_ENABLED
    std::cout << "langwords (" << lang_words_count << "): " << lang_words_buffer << std::endl;
#endif // LANG_ENABLED
#ifdef TEXT_CLASSIFICATION_ENABLED
    std::cout << "text_class_words (" << text_class_words_count << "): " << text_class_words_buffer << std::endl;
#endif // TEXT_CLASSIFICATION_ENABLED
#ifdef INTENT_ENABLED
    std::cout << "intent: " << intent_valence << std::endl;
#endif // INTENT_ENABLED
#ifdef SENTIMENT_ENABLED
    std::cout << "sentiment: " << sentiment_valence << std::endl;
#endif // SENTIMENT_ENABLED
    std::cout << "ret_val: " << ret_val << std::endl;
  }
#endif // GM_DEBUG

  return ret_val;

}

} // namespace inagist_trends

