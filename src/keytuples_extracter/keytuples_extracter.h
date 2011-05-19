#ifndef _INAGIST_TRENDS_KEYTUPLES_EXTRACT_H_
#define _INAGIST_TRENDS_KEYTUPLES_EXTRACT_H_

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

#include <string>
#include <set>
#include <map>
#include <fstream>
#include <iostream>
#include "dictionary_set.h"
#include "dictionary_map.h"

namespace inagist_trends {

class KeyTuplesExtracter {
 public:
  // variables
  inagist_utils::DictionarySet m_dictionary;

  // functions
  KeyTuplesExtracter();
  ~KeyTuplesExtracter();

  int Init(std::string config_file);
  int Init(const char* stopwords_file,
           const char* dictionary_file,
           const char* unsafe_dictionary_file,
           const char* stemmer_dictionary_file=NULL,
           const char* input_file=NULL,
           const char* output_file=NULL);
  int DeInit();

  int GetKeywords(char* str,
                  std::string& safe_status,
                  std::string& script,
                  std::set<std::string>& keywords_set);

  int GetKeywords(char *str,
                  std::string& user,
                  std::set<std::string>& keywords_set,
                  std::map<std::string, std::string>& script_user_map,
                  std::map<std::string, std::string>& keyword_user_map);

  int GetKeyTuples(char* str,
                   std::string& safe_status,
                   std::string& script,
                   std::set<std::string>& keywords_set
#ifdef HASHTAGS_ENABLED
                   , std::set<std::string>& hashtags_set
#endif
#ifdef KEYPHRASE_ENABLED
                   , std::set<std::string>& keyphrases_set
#endif
#ifdef LANG_WORDS_ENABLED
                   , std::set<std::string>& lang_words_set
#endif
                  );

  // directly writing to an output buffer instead of a set
  int GetKeyTuples(unsigned char* buffer, const unsigned int& buffer_len,
                   char* safe_status_buffer, const unsigned int& safe_status_buffer_len,
                   char* script_buffer, const unsigned int& script_buffer_len,
                   unsigned char* keywords_buffer, const unsigned int& keywords_buffer_len,
                   unsigned int& keywords_len, unsigned int& keywords_count
#ifdef HASHTAGS_ENABLED
                   , unsigned char* hashtags_buffer, const unsigned int& hashtags_buffer_len,
                   unsigned int& hashtags_len, unsigned int& hashtags_count
#endif
#ifdef KEYPHRASE_ENABLED
                   , unsigned char* keyphrases_buffer, const unsigned int& keyphrases_buffer_len,
                   unsigned int& keyphrases_len, unsigned int& keyphrases_count
#endif
#ifdef LANG_WORDS_ENABLED
                   , unsigned char* lang_words_buffer, const unsigned int& lang_words_buffer_len,
                   unsigned int& lang_words_len, unsigned int& lang_words_count
#endif
                  );

  void PrintKeywords(std::set<std::string> &keywords_set);
  int DetectScript(int code_point, std::string &script);

 private:
  std::ifstream m_tweet_stream;
  std::ofstream m_out_stream;
  inagist_utils::DictionarySet m_stopwords_dictionary;
  inagist_utils::DictionarySet m_unsafe_dictionary;

  DISALLOW_COPY_AND_ASSIGN(KeyTuplesExtracter);
  bool IsPunct(char *ptr, char *prev=NULL, char *next=NULL);
  bool IsIgnore(char *&ptr);
  inline void Insert(unsigned char* buffer, unsigned int& current_len,
                     unsigned char* str_to_add, const unsigned int& str_len,
                     unsigned int& buffer_content_count);
  inline void Insert(unsigned char* buffer, unsigned int& current_len,
                     std::string& str, unsigned int& buffer_content_count);
};

} // namespace inagist_trends

#endif // _INAGIST_TRENDS_KEYTUPLES_EXTRACT_H_
