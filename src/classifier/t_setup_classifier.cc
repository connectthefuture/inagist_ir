#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include "classifier_config.h"

int CreateDir(std::string file_name) {

  std::string::size_type loc;
  std::string::size_type last_loc = 0;
  while ((loc = file_name.find("/", last_loc+1)) != std::string::npos) {
    last_loc = loc;
  }
  std::string test_data_dir;
  test_data_dir.assign(file_name, 0, last_loc);
  std::cout << test_data_dir << std::endl;

  std::string cmd;
  cmd = "mkdir -p " + test_data_dir;
  std::cout << cmd << std::endl;
  int ret_val = 0;
  if ((ret_val = system(cmd.c_str())) != 0) {
    std::cerr << "ERROR: could not execute system command: " << cmd << std::endl;
    return -1;
  }

  return 0;
}

int main(int argc, char* argv[]) {

  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <config_file_name> [0/1 - class_name_is_handle]\n";
    return -1;
  }

  std::string config_file_name = std::string(argv[1]);
  unsigned int class_name_is_handle = 0;
  if (3 == argc) {
    class_name_is_handle = atoi(argv[2]);
  }

  inagist_classifiers::Config config;
  if (inagist_classifiers::ClassifierConfig::Read(config_file_name.c_str(), config) < 0) {
    std::cerr << "ERROR: could not read config file: " << config_file_name << std::endl;
    return -1;
  }

  std::cout << config.class_freqs_file << std::endl;
  if (CreateDir(config.class_freqs_file) < 0) {
    inagist_classifiers::ClassifierConfig::Clear(config);
    return -1;
  }
  std::string cmd = "touch " + config.class_freqs_file;
  unsigned int sys_out = 0;
  std::cout << cmd << std::endl;
  sys_out = system(cmd.c_str());

  std::cout << config.test_freqs_file << std::endl;
  if (CreateDir(config.test_freqs_file) < 0) {
    inagist_classifiers::ClassifierConfig::Clear(config);
    return -1;
  }
  cmd = "touch " + config.test_freqs_file;
  std::cout << cmd << std::endl;
  sys_out = system(cmd.c_str());

  if (config.classes.empty()) {
    std::cerr << "ERROR: class structs could not be read from config file: " << config_file_name << std::endl;
  } else {
    std::cout << config.classes.size() << " classes" << std::endl;
    std::string cmd; 
    bool flag = true;
    for (config.iter = config.classes.begin(); config.iter != config.classes.end(); config.iter++) {
      std::cout << config.iter->name << std::endl;
      if (flag) {
        // training data folder
        if (CreateDir(config.iter->training_data_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // training handles file folder
        if (CreateDir(config.iter->training_handles_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // training file folder
        if (CreateDir(config.iter->training_timestamps_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // training corpus file folder
        if (CreateDir(config.iter->training_corpus_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // training tweets file folder
        if (CreateDir(config.iter->training_tweets_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // testing data folder
        if (CreateDir(config.iter->testing_data_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // testing timestamps file folder
        if (CreateDir(config.iter->testing_timestamps_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // testing corpus file folder
        if (CreateDir(config.iter->testing_corpus_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
        // testing tweets file folder
        if (CreateDir(config.iter->testing_tweets_file) < 0) {
          inagist_classifiers::ClassifierConfig::Clear(config);
          return -1;
        }
      }
      cmd = "touch " + config.iter->training_timestamps_file;
      std::cout << cmd << std::endl;
      sys_out = system(cmd.c_str());
      if (class_name_is_handle) {
        std::ofstream ofs(config.iter->training_timestamps_file.c_str(), std::ios::app);
        if (!ofs.is_open()) {
          std::cout << "ERROR: could not open timestamps file: " << config.iter->training_timestamps_file << std::endl;
          break;
        } else {
          ofs << config.iter->name << std::endl;
        }
        ofs.close();
      }
      cmd = "touch " + config.iter->training_corpus_file;
      std::cout << cmd << std::endl;
      sys_out = system(cmd.c_str());
      cmd = "touch " + config.iter->training_tweets_file;
      std::cout << cmd << std::endl;
      sys_out = system(cmd.c_str());

      // testing
      cmd = "touch " + config.iter->testing_timestamps_file;
      std::cout << cmd << std::endl;
      sys_out = system(cmd.c_str());
      cmd = "touch " + config.iter->testing_corpus_file;
      std::cout << cmd << std::endl;
      sys_out = system(cmd.c_str());
      cmd = "touch " + config.iter->testing_tweets_file;
      std::cout << cmd << std::endl;
      sys_out = system(cmd.c_str());
    }
  }
  inagist_classifiers::ClassifierConfig::Clear(config);

  return 0;
}
