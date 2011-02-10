#include <iostream>
#include <cstring>
#include "trends_manager.h"
#include "twitter_api.h"
#include "twitter_searcher.h"

#define T_MAX_BUFFER_LEN 1024 

extern int Init(const char*, const char*);
extern int SubmitTweet(const unsigned char* tweet, const unsigned int tweet_len,
                    char* safe_status_buffer, const unsigned int safe_status_buffer_len,
                    char* script_buffer, const unsigned int script_buffer_len,
                    unsigned char* keywords, const unsigned int keywords_buffer_len,
                    unsigned int* keywords_len_ptr, unsigned int* keywords_count_ptr,
                    unsigned char* hashtags_buffer, const unsigned int hashtags_buffer_len,
                    unsigned int* hashtags_len_ptr, unsigned int* hashtags_count_ptr,
                    unsigned char* keyphrases, const unsigned int keyphrases_buffer_len,
                    unsigned int* keyphrases_len_ptr, unsigned int* keyphrases_count_ptr,
                    char* buffer1, const unsigned int buffer1_len,
                    char* buffer2, const unsigned int buffer2_len,
                    char* buffer3, const unsigned int buffer3_len,
                    char* buffer4, const unsigned int buffer4_len);
extern int GetTrends();

using std::string;

int main(int argc, char *argv[]) {
  if (argc > 2) {
    std::cout << "Usage: " << argv[0] << " < -i for interactive | user_name >" << std::endl;
    return -1;
  }

  if (argc == 2) { 
    if (strcmp(argv[1],"-i") == 0){
      std::cout << "interactive mode yet to be coded" << std::endl;
      return -1;
    }
  }

  string arguments(argv[0]);
  string::size_type loc = arguments.find("bin", 0);
  string root_dir;
  if (loc != string::npos) {
    loc-=1;
    root_dir.assign(argv[0], loc);
  }

  string stopwords_file = root_dir + "/data/static_data/stopwords.txt";
  string dictionary_file = root_dir + "/data/static_data/dictionary.txt";
  string unsafe_dictionary_file = root_dir + "/data/static_data/unsafe_dictionary.txt";

  Init(stopwords_file.c_str(), dictionary_file.c_str(), unsafe_dictionary_file.c_str());

  std::set<std::string> tweets;
  int num_docs = 0;
  if (argc == 1) { 
    inagist_api::TwitterAPI twitter_api;
    num_docs = twitter_api.GetPublicTimeLine(tweets);
  } else {
    inagist_api::TwitterSearcher twitter_searcher;
    num_docs = twitter_searcher.GetTweetsFromUser(std::string(argv[1]), tweets);
  }

  if (num_docs <= 0) {
    std::cout << "No input docs\n";
    return -1;
  }

  char safe_status[10];
  memset(safe_status, 0, 10);
  char script[4];
  memset(script, 0, 4);
  unsigned char keywords[T_MAX_BUFFER_LEN];
  memset(keywords, 0, T_MAX_BUFFER_LEN);
  unsigned int keywords_len = 0;
  unsigned int keywords_count = 0;
  unsigned char hashtags[T_MAX_BUFFER_LEN];
  memset(hashtags, 0, T_MAX_BUFFER_LEN);
  unsigned int hashtags_len = 0;
  unsigned int hashtags_count = 0;
  unsigned char keyphrases[T_MAX_BUFFER_LEN];
  memset(keyphrases, 0, T_MAX_BUFFER_LEN);
  unsigned int keyphrases_len = 0;
  unsigned int keyphrases_count = 0;
  char buffer1[4];
  memset(buffer1, 0, 4);
  char buffer2[4];
  memset(buffer2, 0, 4);
  char buffer3[4];
  memset(buffer3, 0, 4);
  char buffer4[4];
  memset(buffer4, 0, 4);

  std::set<std::string>::iterator set_iter;
  std::string tweet;
  for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
    tweet = *set_iter;
    //std::cout << "input: " << tweet << std::endl;
    SubmitTweet((const unsigned char*) tweet.c_str(), tweet.length(),
                safe_status, 10,
                script, 4,
                (unsigned char*) keywords, T_MAX_BUFFER_LEN,
                &keywords_len, &keywords_count,
                (unsigned char*) hashtags, T_MAX_BUFFER_LEN,
                &hashtags_len, &hashtags_count,
                (unsigned char*) keyphrases, T_MAX_BUFFER_LEN,
                &keyphrases_len, &keyphrases_count,
                buffer1, 4,
                buffer2, 4,
                buffer3, 4,
                buffer4, 4);

    if (strcmp(safe_status, "unsafe") == 0) {
      std::cout << tweet << std::endl;
      std::cout << "safe status: " << safe_status << std::endl \
                << "script: " << script << std::endl \
                << "keywords: " << keywords << std::endl \
                << "hashtags: " << hashtags << std::endl \
                << "keyphrases: " << keyphrases << std::endl \
                << "lang: " << buffer1 << std::endl \
                << "lang ignoring case: " << buffer2 << std::endl \
                << "class: " << buffer3 << std::endl \
                << "sub class: " << buffer4 << std::endl;
    }
    memset(script, 0, 4);
    keywords[0] = '\0';
    hashtags[0] = '\0';
    keyphrases[0] = '\0';
    memset(buffer1, 0, 4);
    memset(buffer2, 0, 4);
    memset(buffer3, 0, 4);
    memset(buffer4, 0, 4);
  }
  tweets.clear();
  memset(script, 0, 4);
  memset(keywords, 0, T_MAX_BUFFER_LEN);
  memset(hashtags, 0, T_MAX_BUFFER_LEN);
  memset(keyphrases, 0, T_MAX_BUFFER_LEN);
  memset(buffer1, 0, 4);
  memset(buffer2, 0, 4);
  memset(buffer3, 0, 4);
  memset(buffer4, 0, 4);

  return 0;
}
