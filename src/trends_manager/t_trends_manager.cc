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
                    unsigned char* keyphrases, const unsigned int keyphrases_buffer_len,
                    unsigned int* keyphrases_len_ptr, unsigned int* keyphrases_count_ptr);
extern int GetTrends();

using std::string;

int init(const char* binary_name) {

  string arguments(binary_name);
  string::size_type loc = arguments.find("bin", 0);
  string root_dir;
  if (loc != string::npos) {
    loc-=1;
    root_dir.assign(binary_name, loc);
  }

  string stopwords_file = root_dir + "/data/static_data/stopwords.txt";
  string dictionary_file = root_dir + "/data/static_data/dictionary.txt";
  string unsafe_dictionary_file = root_dir + "/data/static_data/unsafe_dictionary.txt";

  if (Init(stopwords_file.c_str(), dictionary_file.c_str(), unsafe_dictionary_file.c_str()) < 0)
    return -1;

  return 0;
}

int test_trends_manager(std::set<std::string> tweets) {

  unsigned char buffer[T_MAX_BUFFER_LEN];
  memset(buffer, 0, T_MAX_BUFFER_LEN);
  char safe_status[10];
  memset(safe_status, 0, 10);
  char script[4];
  memset(script, 0, 4);
  unsigned char keywords[T_MAX_BUFFER_LEN];
  memset(keywords, 0, T_MAX_BUFFER_LEN);
  unsigned int keywords_len = 0;
  unsigned int keywords_count = 0;
  unsigned char keyphrases[T_MAX_BUFFER_LEN];
  memset(keyphrases, 0, T_MAX_BUFFER_LEN);
  unsigned int keyphrases_len = 0;
  unsigned int keyphrases_count = 0;

  std::set<std::string>::iterator set_iter;
  std::string tweet;
  for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
    tweet = *set_iter;
    std::cout << tweet << std::endl;
    strcpy((char *) buffer, tweet.c_str());
    //SubmitTweet((const unsigned char*) tweet.c_str(), tweet.length(),
    SubmitTweet((const unsigned char*) buffer, strlen((char *) buffer),
                safe_status, 10,
                script, 4,
                (unsigned char*) keywords, T_MAX_BUFFER_LEN,
                &keywords_len, &keywords_count,
                (unsigned char*) keyphrases, T_MAX_BUFFER_LEN,
                &keyphrases_len, &keyphrases_count);
    std::cout << "safe status: " << safe_status << std::endl \
              << "script: " << script << std::endl \
              << "keywords: " << keywords << std::endl \
              << "keyphrases: " << keyphrases << std::endl;
    memset(buffer, 0, T_MAX_BUFFER_LEN);
    memset(script, 0, 4);
    memset(keywords, 0, T_MAX_BUFFER_LEN);
    memset(keyphrases, 0, T_MAX_BUFFER_LEN);
    //keywords[0] = '\0';
    //keyphrases[0] = '\0';
  }
  tweets.clear();
  memset(script, 0, 4);
  memset(keywords, 0, T_MAX_BUFFER_LEN);
  memset(keyphrases, 0, T_MAX_BUFFER_LEN);

  return 0;
}

int main(int argc, char *argv[]) {

  if (argc > 2) {
    std::cout << "Usage: " << argv[0] << " < -i for interactive | user_name >" << std::endl;
    return -1;
  }

  std::set<std::string> tweets;
  std::string line;
  if (argc == 2) { 
    if (strcmp(argv[1], "-i") == 0) {
      if (init(argv[0]) < 0) {
        std::cout << "ERROR: could not initialize\n";
        return -1;
      }
      while (getline(std::cin, line)) {
        if (line.compare("quit") == 0)
          break;
        tweets.insert(line);
        if (test_trends_manager(tweets) < 0) {
          tweets.clear();
          return -1;
        }
        tweets.clear();
      }
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "-f") == 0) {
      std::string file_name = std::string(argv[2]);
      std::ifstream ifs(file_name.c_str());
      if (ifs.is_open()) {
        while (getline(ifs, line)) {
          if (line.length() > 1)
            tweets.insert(line);
        }
      } else {
        std::cout << "could not open file " << file_name << std::endl;
        return -1;
      }
      ifs.close();
    }
  }

  int num_docs = 0;
  if (argc == 1) { 
    inagist_api::TwitterAPI twitter_api;
    num_docs = twitter_api.GetPublicTimeLine(tweets);
  } else {
    inagist_api::TwitterSearcher twitter_searcher;
    num_docs = twitter_searcher.GetTweetsFromUser(std::string(argv[1]), tweets);
  }

  if (num_docs <= 0) {
    tweets.clear();
    return -1;
  }

  if (init(argv[0]) < 0) {
    std::cout << "ERROR: could not initialize\n";
    tweets.clear();
    return -1;
  }

  if (test_trends_manager(tweets) < 0) {
    tweets.clear();
    return -1;
  }

  return 0;
}
