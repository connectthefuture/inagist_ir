#include "ngrams_generator.h"
#include <iostream>
#include <set>
#include <map>
#include <cstdlib>
#include <cassert>
#include "twitter_api.h"
#include "twitter_searcher.h"
#include "string_utils.h"

inagist_classifiers::NgramsGenerator g_ng;

int GetNgrams(unsigned int test_type,
              std::string& text,
              std::set<std::string>& words_set,
              inagist_classifiers::Corpus& corpus) {

  int ngrams_count = 0;
  inagist_classifiers::CorpusIter map_iter;

  if (0 == test_type) {
    ngrams_count = g_ng.GetNgrams((unsigned char*) text.c_str(), text.length(), corpus);
  } else if (1 == test_type) {
    if (inagist_utils::Tokenize(text, words_set) < 0) {
      std::cout << "ERROR: could not tokenize text:\n" << text << std::endl;
    } else {
      ngrams_count = g_ng.GetNgramsFromWords(words_set, corpus);
    }
  } else if (2 == test_type) {
    ngrams_count = g_ng.GetAllNgrams(text, corpus);
  }

  if (ngrams_count > 0) {
    for (map_iter = corpus.begin(); map_iter != corpus.end(); map_iter++)
      std::cout << (*map_iter).first << " " << (*map_iter).second << std::endl;
    corpus.clear();
  } else if (ngrams_count < 0) {
    std::cout << "ERROR: could not find ngrams" << std::endl;
  } else {
    std::cout << "no ngrams found" << std::endl;
  }

  return ngrams_count;
}

int main(int argc, char* argv[]) {

  if (argc > 4 || argc < 3) {
    std::cout << "Usage: " << argv[0] << " \n\t<0/1/2/3, 0-interactive/1-file> \n\t<0/1/2, 0-normal/1-words/2-allgrams/3-tweet> \n\t[<input_file_name>/<handle>]\n";
    return -1;
  }

  inagist_classifiers::Corpus corpus;
  std::string text;

  int input_type = atoi(argv[1]);
  assert((input_type >= 0 && input_type <= 3));
 
  int test_type = atoi(argv[2]);
  assert(test_type >= 0 && test_type <= 1);

  std::set<std::string> words_set;
  if (0 == input_type) {
    while (getline(std::cin, text)) {
      GetNgrams(test_type, text, words_set, corpus);
    }
    return 0;
  }

  if (1 == input_type) {
    if (argc != 4) {
      std::cout << "ERROR: input file needed\n";
      return -1;
    } else {
      std::cout << "This feature is not implemented yet\n";
    }
  }

  if (2 == input_type || 3 == input_type) {
    std::set<std::string> tweets;
    inagist_api::TwitterAPI tapi;
    if (tapi.GetPublicTimeLine(tweets) < 0) {
      std::cout << "Error: could not get trending tweets from inagist\n";
      return -1;
    }
    std::set<std::string>::iterator set_iter;
    for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
      text = *set_iter;
      std::cout << text << std::endl;
      if (GetNgrams(test_type, text, words_set, corpus) > 0)
        std::cout << text << std::endl;
      if (2 == input_type)
        break;
    }
    tweets.clear();
  }

  return 0;

}
