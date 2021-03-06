#include <iostream>
#include <fstream>
#include <map>
#include <cstdlib>
#include <cmath>
#include "ngrams_generator.h"

int main(int argc, char* argv[]) {

  if (argc != 4) {
    std::cout << "Usage: " << argv[0] << " <lang1_text_file> <long2_text_file> <test_file>\n";
    return -1;
  }

  std::string lang1_file = std::string(argv[1]);
  std::string lang2_file = std::string(argv[2]);
  std::string test_file = std::string(argv[3]);

  inagist_classifiers::NgramsGenerator ng;

  std::map<std::string, int> lang1_features_map;
  if (ng.GetNgramsFromFile(lang1_file, lang1_features_map) < 0) {
    std::cout << "ERROR: could not get features for lang1\n";
    return -1;
  }

  std::map<std::string, int> lang2_features_map;
  if (ng.GetNgramsFromFile(lang2_file, lang2_features_map) < 0) {
    std::cout << "ERROR: could not get features for lang2\n";
    return -1;
  }

  std::map<std::string, int> testfile_features_map;
  if (ng.GetNgramsFromFile(test_file, testfile_features_map) < 0) {
    std::cout << "ERROR: could not get features for testfile\n";
    return -1;
  }

  double lang1_freq = 0;
  double lang2_freq = 0;
  double freq = 0;
  std::map<std::string, int>::iterator map_iter;
  std::map<std::string, int>::iterator freq_iter;
  for (map_iter = testfile_features_map.begin(); map_iter != testfile_features_map.end(); map_iter++) {
    //std::cout << (*map_iter).first << " = " << (*map_iter).second << std::endl;
    freq_iter = lang1_features_map.find((*map_iter).first); 
    if (freq_iter != lang1_features_map.end()) {
       freq = (double) (*freq_iter).second;
       //std::cout << freq << std::endl;
       lang1_freq += log(freq); 
    }
    freq_iter = lang2_features_map.find((*map_iter).first); 
    if (freq_iter != lang2_features_map.end()) {
       freq = (double) (*freq_iter).second;
       //std::cout << freq << std::endl;
       lang2_freq += log(freq); 
    }
  }

  std::cout << "Lang 1 freq: " << lang1_freq << std::endl;
  std::cout << "Lang 2 freq: " << lang2_freq << std::endl;
  double score1 = exp(lang1_freq);
  double score2 = exp(lang2_freq);
  std::cout << "Lang 1 score: " << score1 << std::endl;
  std::cout << "Lang 2 score: " << score2 << std::endl;
  if (score1 > score2)
    std::cout << "Guess = Lang 1\n";
  else
    std::cout << "Guess = Lang 2\n";

  return 0;
}
