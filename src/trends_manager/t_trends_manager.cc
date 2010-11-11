#include <iostream>
#include <cstring>
#include "trends_manager.h"
#include "twitter_api.h"
#include "twitter_searcher.h"

#define T_MAX_BUFFER_LEN 1024 

extern int Init(const char*, const char*);
extern int SubmitTweet(const char* tweet, const unsigned int tweet_len,
                        char* tweet_script, const unsigned int script_buffer_len,
                        char* keywords, const unsigned int keywords_buffer_len,
                        char* keyphrases, const unsigned int keyphrases_buffer_len);
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

  Init(stopwords_file.c_str(), dictionary_file.c_str());

  std::set<std::string> tweets;
  int num_docs = 0;
  if (argc == 1) { 
    inagist_api::TwitterAPI twitter_api;
    num_docs = twitter_api.GetPublicTimeLine(tweets);
  } else {
    inagist_api::TwitterSearcher twitter_searcher;
    num_docs = twitter_searcher.GetTweetsFromUser(std::string(argv[1]), tweets);
  }

  if (num_docs <= 0)
    return -1;

  char script[4];
  memset(script, 0, 4);
  char keywords[T_MAX_BUFFER_LEN];
  memset(keywords, 0, T_MAX_BUFFER_LEN);
  char keyphrases[T_MAX_BUFFER_LEN];
  memset(keyphrases, 0, T_MAX_BUFFER_LEN);
  
  std::set<std::string>::iterator set_iter;
  std::string tweet;
  for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
    tweet = *set_iter;
    std::cout << tweet << std::endl;
    SubmitTweet(tweet.c_str(), tweet.length(),
                script, 4,
                keywords, T_MAX_BUFFER_LEN,
                keyphrases, T_MAX_BUFFER_LEN);
    std::cout << "script: " << script << std::endl << "keywords: " << keywords << std::endl << "keyphrases: " << keyphrases << std::endl;
    memset(script, 0, 4);
    keywords[0] = '\0';
    keyphrases[0] = '\0';
  }
  tweets.clear();
  memset(script, 0, 4);
  memset(keywords, 0, T_MAX_BUFFER_LEN);
  memset(keyphrases, 0, T_MAX_BUFFER_LEN);

  return 0;
}
