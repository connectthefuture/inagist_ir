#include "classifier.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "twitter_searcher.h"
#include "twitter_api.h"

#ifdef DEBUG
#if DEBUG>0
#define CLASSIFIER_DEBUG DEBUG
#endif
#endif
//#define CLASSIFIER_DEBUG 3

#define MIN_TWEETS_REQUIRED 15

namespace inagist_classifiers {

Classifier::Classifier() {
}

Classifier::~Classifier() {

  if (ConfigReader::Clear(m_config) < 0) {
    std::cerr << "ERROR: could not clear config\n";
  }

  try {
    m_corpus_manager.Clear();
  } catch (...) {
    std::cerr << "ERROR: Corpus Manager throws exception" << std::endl;
  }

}

int Classifier::Init(std::string config_file_name, bool ignore_history) {

  // this config file name should have corpus files
  // and the strings with which the corpus contents can be uniquely identified

  if (ConfigReader::Read(config_file_name.c_str(), m_config) < 0) {
    std::cerr << "ERROR: could not read config file: " << config_file_name << std::endl;
    return -1;
  }

  if (m_config.classes.empty()) {
    std::cerr << "ERROR: class structs could not be read from config file: " \
              << config_file_name << std::endl;
    return -1;
  }

  std::map<std::string, std::string> class_name_file_map;
  for (m_config.iter = m_config.classes.begin();
       m_config.iter != m_config.classes.end();
       m_config.iter++) {
    class_name_file_map[m_config.iter->name] = m_config.iter->training_data_file;
  }
  if (class_name_file_map.empty()) {
    std::cerr << "ERROR: class_name_file_map cannot be empty\n";
    return -1;
  }

  if (!ignore_history) {
    if (m_corpus_manager.LoadCorpus(m_config.test_data_file,
                                    m_corpus_manager.m_classes_freq_map) < 0) {
      std::cerr << "ERROR: could not load the text classes freq file (test data)\n";
      std::cout << "WARNING: continuing without the text classes freq data\n";
    }
  } else {
    std::cout << "INFO: ignoring historical data. plain vanilla classification\n";
  }

  if (m_corpus_manager.LoadCorpusMap(class_name_file_map) < 0) {
    std::cerr << "ERROR: could not load Corpus Map\n";
    return -1;
  }

  return 0;
}

// this functioin uses round robin to get training data.
// TODO (balaji) - handle rate limiting and get data from all sources
// persist with round robin till an equal number of training text is obtained
int Classifier::GetTrainingData(const char* config_file_name) {

  if (!config_file_name) {
    std::cerr << "ERROR: invalid config file name\n";
    return -1;
  }

  Config config;
  if (ConfigReader::Read(config_file_name, config) < 0) {
    std::cerr << "ERROR: could not read config file: " << config_file_name << std::endl;
    return -1;
  }

  if (config.classes.empty()) {
    std::cerr << "ERROR: class structs could not be read from config file: " << config_file_name << std::endl;
    return -1;
  }

  // the following code, gets a random starting point.
  // though all the sources are pinged, this randomization is to save from biases
  int i = rand() % config.classes.size();
  int j = 0;
  for (config.iter = config.classes.begin(); config.iter != config.classes.end(); config.iter++) {
    if (j == i)
      break;
    j++;
  }

  int count = 0;
  int count_temp = 0;
  unsigned int num_docs = 0;
  unsigned int corpus_size = 0;
  for (; config.iter != config.classes.end(); config.iter++) {
    num_docs = 0;
    corpus_size = 0;
    if ((count_temp = GetTrainingData(config.iter->name,
                                      config.iter->handles_file,
                                      config.iter->tweets_file,
                                      num_docs,
                                      config.iter->corpus_file,
                                      corpus_size)) < 0) {
      std::cerr << "ERROR: could not get training data for handles in file: " \
                << config.iter->handles_file << std::endl; 
    } else {
      std::cout << "Corpus of size " << count_temp \
                << " generated for " << config.iter->name << " from " \
                << num_docs << " docs" << std::endl;
      count += count_temp;
    }
  }
  j = 0;
  for (config.iter = config.classes.begin(); config.iter != config.classes.end() && j<i; config.iter++) {
    j++;
    num_docs = 0;
    corpus_size = 0;
    if ((count_temp = GetTrainingData(config.iter->name,
                                      config.iter->handles_file,
                                      config.iter->tweets_file,
                                      num_docs,
                                      config.iter->corpus_file,
                                      corpus_size)) < 0) {
      std::cerr << "ERROR: could not get training data for handles in file: " \
                << config.iter->handles_file << std::endl; 
    } else {
      std::cout << "Corpus of size " << count_temp \
                << " generated for " << config.iter->name << " from " \
                << num_docs << " docs" << std::endl;
      count += count_temp;
    }
  }

  ConfigReader::Clear(config);

  return count;
}

int Classifier::GetTrainingData(const std::string& class_name,
                                const std::string& twitter_handles_file_name,
                                const std::string& output_tweets_file_name,
                                unsigned int& output_num_docs,
                                const std::string& output_corpus_file_name,
                                unsigned int& output_corpus_size) {

  std::ifstream ifs(twitter_handles_file_name.c_str());
  if (!ifs.is_open()) {
    std::cerr << "ERROR: could not open twitter handles file: " << twitter_handles_file_name << std::endl;
    return -1;
  }

#ifdef CLASSIFIER_DEBUG
  std::cout << "INFO: training data from handles file: " << twitter_handles_file_name << std::endl;
#endif

  std::string line;
  std::string handle;
  std::string::size_type loc;
  unsigned int flag = 0;
  unsigned int exe_count = 0;
  std::string exe_count_str;
  std::map<std::string, unsigned int> handles;
  std::map<std::string, unsigned int>::iterator handle_iter;

  while(getline(ifs, line)) {
    if (line.length() <= 1) {
      continue;
    }
    loc = line.find("=", 0);
    if (loc == std::string::npos) {
      handle.assign(line);
      exe_count = 0;
    } else {
      handle.assign(line, 0, loc);
      exe_count_str.assign(line, loc+1, line.length()-loc-1);
      exe_count = atoi((char *) exe_count_str.c_str());
    }
    handles[handle] = exe_count;
  }
  ifs.close();

  if (handles.size() < 1) {
    std::cerr << "ERROR: no handles found in file " << twitter_handles_file_name << std::endl;
    return 0;
  }

  std::set<std::string> tweets;
  inagist_api::TwitterSearcher twitter_searcher;

  unsigned int count = 0;
  unsigned int count_temp = 0;
  Corpus corpus;

  // the following code is to randomly pick one handle and get tweets from it
  int i = rand() % handles.size();
  int j = 0;
  for (handle_iter = handles.begin(); handle_iter != handles.end(); handle_iter++) {
    if (i==j)
      break;
    j++;
  }

  // this flag indicates a flip in state and hence needs to be committed to file
  flag = 0;
  bool no_fresh_handle = true;
  bool get_user_info = false;
  unsigned int local_num_docs = 0;
  unsigned int local_corpus_size = 0;
  for (; handle_iter != handles.end(); handle_iter++) {
    if (handle_iter->second == 1)
      continue;
    no_fresh_handle = false;
    handle = handle_iter->first;

    // need user info. 0 indicates its the first time this handles is being processed
    if (handle_iter->second == 0) {
      count_temp = GetTrainingData(handle, local_num_docs, corpus, local_corpus_size, get_user_info=true);
    } else {
      count_temp = GetTrainingData(handle, local_num_docs, corpus, local_corpus_size, get_user_info=false);
    }
    usleep(100000);

    if (count_temp < 0) {
      std::cerr << "ERROR: could not get training data for handle: " << handle \
                << " for class: " << class_name << std::endl;
    } else {
      handles[handle] += 1;
      handles[handle] %= 2;
      flag = 1;
    }
    if (0 == count_temp) {
      continue;
    } else {
      count += count_temp;
    }

    if (local_num_docs < MIN_TWEETS_REQUIRED) {
      continue;
    } else {
      output_num_docs += local_num_docs;
      output_corpus_size += local_corpus_size;
      break;
    }
  }

  // if no fresh handle has been found from the random location to end of the hash map,
  // continue from the first till that random location
  if (0 == flag) {
    j = 0;
    local_num_docs = 0;
    local_corpus_size = 0;
    for (handle_iter = handles.begin(); handle_iter != handles.end(); handle_iter++) {

      if (i==j) // i is the random location
        break;
      j++;

      if (handle_iter->second == 1)
        continue;
      no_fresh_handle = false;
      handle = handle_iter->first;

      // need user info
      if (handle_iter->second == 0) {
        count_temp = GetTrainingData(handle, local_num_docs, corpus, local_corpus_size, get_user_info=true);
      } else {
        count_temp = GetTrainingData(handle, local_num_docs, corpus, local_corpus_size, get_user_info=false);
      }
      usleep(100000);

      if (count_temp < 0) {
        std::cerr << "ERROR: could not get training data for handle: " << handle << std::endl;
      } else {
        handles[handle] += 1;
        handles[handle] %= 2;
        flag = 1;
      }

      if (0 == count_temp) {
        continue;
      } else {
        count += count_temp;
      }

      if (local_num_docs < MIN_TWEETS_REQUIRED) {
        continue;
      } else {
        output_num_docs += local_num_docs;
        output_corpus_size += local_corpus_size;
        break;
      }
    }
  }

  if (no_fresh_handle) {
    // all the entries in the handles file may have been examined and hence have "1".
    // lets flip them all to 2.
#ifdef CLASSIFIER_DEBUG
    if (CLASSIFIER_DEBUG > 1) {
      std::cout << "INFO: flipping all the handle entries to zero" << std::endl;
    }
#endif
    for (handle_iter = handles.begin(); handle_iter != handles.end(); handle_iter++) {
      if (handle_iter->second == 0) {
        std::cerr << "ERROR: a fresh handle found while flipping. logical error\n";
        handles.clear();
        return -1;
      } else if (handle_iter->second == 1) {
        handle = handle_iter->first;
        handles[handle] = 2;
        flag = 1;
      } else {
        std::cerr << "ERROR: ill-maintained traning data state or unexplained error\n";
        handles.clear();
        return -1;
      }
    }
  }

  if (1 == flag) {
    std::ofstream ofs(twitter_handles_file_name.c_str());
    if (!ofs.is_open()) {
      std::cerr << "ERROR: handles file: " << twitter_handles_file_name << " could not be opened for write.\n";
      return -1;
    } else {
#ifdef CLASSIFIER_DEBUG
    if (CLASSIFIER_DEBUG > 3) {
      std::cout << "INFO: updating the handles file after current round\n";
    }
#endif
      for (handle_iter = handles.begin(); handle_iter != handles.end(); handle_iter++) {
         ofs << handle_iter->first << "=" << handle_iter->second << std::endl;
      }
      ofs.close();
    }
  }

  handles.clear();

  if (no_fresh_handle) {
#ifdef CLASSIFIER_DEBUG
    if (CLASSIFIER_DEBUG > 1) {
      std::cout << "INFO: no fresh handle found. hence all handles were flipped." \
                << " now making recursive call\n";
    }
#endif
    // recursive call
    return GetTrainingData(class_name,
                           twitter_handles_file_name,
                           output_tweets_file_name,
                           output_num_docs,
                           output_corpus_file_name,
                           output_corpus_size);
  }

  if (count == 0) {
    std::cout << "WARNING: No tweets found for handles in file " << twitter_handles_file_name << std::endl;
    return 0;
  } else {
    if (CorpusManager::UpdateCorpusFile(corpus, output_corpus_file_name) < 0) {
      std::cerr << "ERROR: could not update features to output file " << output_corpus_file_name << std::endl;
    }
  }

  corpus.clear();

  return count;
}

int Classifier::GetTrainingData(const std::string& handle,
                                unsigned int& output_num_docs,
                                Corpus& corpus,
                                unsigned int& output_corpus_size,
                                bool get_user_info) {

  std::set<std::string> tweets;
  inagist_api::TwitterSearcher twitter_searcher;

  unsigned int count = 0;
  unsigned int user_info_count = 0;
  unsigned int count_temp = 0;

  if (get_user_info) {
    std::string user_info;
    if (inagist_api::TwitterAPI::GetUserInfo(handle, user_info) < 0) {
      std::cerr << "ERROR: could not get user info for handle: " << handle << std::endl;
    } else {
      if (user_info.length() > 0 && (user_info_count = GetCorpus(user_info, corpus)) < 0) {
        std::cerr << "ERROR: could not find ngrams for user info string: " << user_info << std::endl;
      } else {
        count += user_info_count;
      }
    }
  }
  output_corpus_size += user_info_count;

  if (twitter_searcher.GetTweetsFromUser(handle, tweets) < 0) {
    std::cout << "ERROR: could not get tweets from user: " << handle << std::endl;
    // this 'count' is from above user info
    if (0 == user_info_count) {
      return -1;
    } else {
      std::cout << "INFO: corpus of size " << count \
                << " generated from user info of handle: " << handle << std::endl;
      return user_info_count;
    }
  }

  if (!tweets.empty()) {
    std::set<std::string>::iterator set_iter;
    for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
      // this GetCorpus is a pure virtual function
      // ensure your derivation of this classifier provides this function
      if ((count_temp = GetCorpus(*set_iter, corpus)) < 0) {
        std::cerr << "ERROR: could not find ngrams from tweet: " << *set_iter << std::endl;
      } else {
        count += count_temp;
      }
    }
    output_num_docs += tweets.size();
    std::cout << "corpus of size " << count \
              << " generated from " << tweets.size() \
              << " tweets of handle: " << handle << std::endl;
    tweets.clear();
  }

  output_corpus_size += count;

  return count + user_info_count;
}

int Classifier::WriteTestData(Corpus& corpus, const char* classes_freq_file) {

  if (corpus.empty() || !classes_freq_file) {
    std::cerr << "ERROR: invalid input. could not write test data\n";
    return -1;
  }

  inagist_classifiers::CorpusIter corpus_iter;
  unsigned int sum = 0;
  for (corpus_iter = corpus.begin(); corpus_iter != corpus.end(); corpus_iter++) {
    std::cout << corpus_iter->first << ":" << corpus_iter->second << std::endl;
    if ((corpus_iter->first.compare("UU") == 0) ||
        (corpus_iter->first.compare("XX") == 0) ||
        (corpus_iter->first.compare("RR") == 0)) {
      std::cout << "deleting" << std::endl;
      corpus.erase(corpus_iter);
      continue;
    } else if (corpus_iter->first.compare("all_classes") != 0) {
      sum += corpus_iter->second;
    } else {
      std::cout << "all_classes. hence ignoring" << std::endl;
    }
  }

  // update the all_classes value
  if ((corpus_iter = corpus.find("all_classes")) != corpus.end()) {
    corpus_iter->second = sum;
  } else {
    corpus.insert(std::pair<std::string, unsigned int> ("all_classes", sum));
  }

  // write to classes_freq file
  if (m_corpus_manager.UpdateCorpusFile(corpus, classes_freq_file) < 0) {
    std::cerr << "ERROR: could not update corpus file\n";
    return -1;
  }

  return corpus.size();
}

// this function assumes that the classifier is already initialized.
// and also assumes that the GetCorpus implementation by the derived class
// will work fine with its dependencies already initalized
//
// input_type:
//          0 - twitter timeline
//          1 - random selection of training sources
//          2 - all training sources (all handles)
//          3 - only given training source will be tested 
//          4 - test input is taken from the given file
// output_type:
//          0 - stdout
//          1 - class frequency file
//          2 - html version
//
// Note: the output file is to write texts that contributed to the class frequenceies.
//
int Classifier::GetTestData(const unsigned int& input_type,
                            const char* input_file,
                            const char* input_handle,
                            const std::string& expected_class_name,
                            const unsigned int& output_type,
                            const char* output_file) {

  if (NULL == input_file && 4 == input_type) {
    std::cerr << "ERROR: invalid input file. cannot get test data.\n";
    return -1;
  }

  if (NULL == input_handle && 5 == input_type) {
    std::cerr << "ERROR: invalid input handle. cannot get test data.\n";
    return -1;
  }

  if (expected_class_name.empty() && 3 == input_type) {
    std::cerr << "ERROR: invalid expected class name. cannot get test data.\n";
    return -1;
  }

  if (output_type > 0 && NULL == output_file) {
    std::cerr << "ERROR: invalid output file. cannot get test data.\n";
    return -1;
  }

  std::ofstream ofs;
  std::ostream* output_stream; // note ostream not ofstream

  if (0 == output_type) {
    output_stream = &std::cout;
  } else {
    ofs.open(output_file);
    if (!ofs.is_open()) {
      std::cerr << "ERROR: could not open output file: " << output_file << std::endl;
      return -1;
    }
    output_stream = &ofs;
  }

  Corpus class_freq_map;
  bool random_selection = false;
  const char* training_class = NULL;
  std::string handle;
  TestResult test_result;
  test_result.clear();
  const char* training_texts_file = NULL;

  switch (input_type) {
    case 0:
      // send empty handle and expected class_name to test public timeline
      if (TestTwitterTimeline(handle,
                              expected_class_name,
                              class_freq_map,
                              test_result,
                              *output_stream) < 0) {
        std::cerr << "ERROR: could not test twitter timeline\n";
        ofs.close();
        return -1;
      }
      break;
    case 1:
      if (TestTrainingSources(training_class=NULL,
                              class_freq_map,
                              test_result,
                              *output_stream,
                              random_selection = true) < 0) {
        std::cerr << "ERROR: could not test training sources at random\n";
        ofs.close();
        return -1;
      }
      break;
    case 2:
      if (TestTrainingSources(training_class=NULL,
                              class_freq_map,
                              test_result,
                              *output_stream,
                              random_selection = false) < 0) {
        std::cerr << "ERROR: could not test training sources at random\n";
        ofs.close();
        return -1;
      }
      break;
    case 3:
      if (TestTrainingSources(training_class=expected_class_name.c_str(),
                              class_freq_map,
                              test_result,
                              *output_stream,
                              random_selection = true) < 0) {
        std::cerr << "ERROR: could not test training sources at random\n";
        ofs.close();
        return -1;
      }
      break;
    case 4:
        if (TestTrainingTexts(training_texts_file=input_file,
                              expected_class_name,
                              class_freq_map,
                              test_result,
                              *output_stream) < 0) {
          std::cerr << "ERROR: could not test training sources from file: " \
                    << training_texts_file << std::endl;
          return -1;
        }
      break;
    case 5:
      if (input_handle)
        handle.assign(input_handle);
      if (TestTwitterTimeline(handle,
                              expected_class_name,
                              class_freq_map,
                              test_result,
                              *output_stream) < 0) {
        std::cerr << "ERROR: could not test tweets from hanlde: " << handle << std::endl;
        ofs.close();
        return -1;
      }
      break;
    default:
      break;
  }

  if (class_freq_map.empty()) {
    std::cout << "WARNING: no test data found\n";
    ofs.close();
    return 0;
  }

  int ret_val = 0;
  switch (output_type) {
    case 0:
      if (CorpusManager::PrintCorpus(class_freq_map) < 0) {
        std::cerr << "ERROR: could not print corpus\n";
        ret_val = -1;
      }
      break;
    case 1:
      if (WriteTestData(class_freq_map, m_config.freqs_file.c_str()) < 0) {
        std::cerr << "ERROR: could not write test data to file: " \
                  << m_config.freqs_file << std::endl;
        ret_val = -1;
      }
      break;
    case 2:
      break;
    default:
      break;
  }

  ofs.close();
  class_freq_map.clear();

  std::cout << "Total text: " << test_result.total << std::endl;
  if (!expected_class_name.empty()) {
    std::cout << "Expected Class: " << expected_class_name << std::endl;
  }
  std::cout << "Correct: " << test_result.correct << std::endl;
  std::cout << "Wrong: " << test_result.wrong << std::endl;
  std::cout << "Undefined: " << test_result.undefined << std::endl;

  return ret_val;

}

int Classifier::TestTrainingTexts(const char* training_texts_file,
                                  const std::string& expected_class_name,
                                  Corpus& class_freq_map,
                                  TestResult& test_result,
                                  std::ostream &output_stream) {

  if (!training_texts_file) {
    std::cerr << "ERROR: invalid input training texts file\n";
    return -1;
  }

  std::ifstream ifs(training_texts_file);
  if (!ifs.is_open()) {
    std::cerr << "ERROR: could not open file with traning texts: " \
              << training_texts_file << std::endl;
    return -1;
  }

  std::string tweet;
  std::set<std::string> tweets;
  while (getline(ifs, tweet)) {
    tweets.insert(tweet);
  }
  ifs.close();

  if (tweets.empty()) {
    return 0;
  }

  std::set<std::string>::iterator set_iter;
  std::string output_class;
  int ret_val = 0;
#ifdef CLASSIFIER_DEBUG
  if (CLASSIFIER_DEBUG > 2) {
    std::cout << "check corpus map" << std::endl;
    CorpusManager::PrintCorpusMap(m_corpus_manager.m_corpus_map);
  }
#endif
  for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
    test_result.total++;
    tweet = *set_iter;
    if ((ret_val = Classify(tweet, tweet.length(), output_class)) < 0) {
      std::cerr << "ERROR: could not find class\n";
      test_result.undefined++;
    } else if (ret_val == 0) {
      if (output_stream) {
        output_stream << expected_class_name << "|" << output_class << "|" << tweet << std::endl;
      }
      test_result.undefined++;
    } else {
      if (output_stream) {
        output_stream << expected_class_name << "|" << output_class << "|" << tweet << std::endl;
      }
      if (!expected_class_name.empty()) {
        if (expected_class_name.compare(output_class) == 0) {
          test_result.correct++;
        } else if ((output_class.compare(0,2,"UU") == 0) ||
                   (output_class.compare(0,2,"XX") == 0) ||
                   (output_class.compare(0,2,"RR") == 0)) {
           test_result.undefined++;
        } else {
          test_result.wrong++;
        }
      }
      if (class_freq_map.find(output_class) != class_freq_map.end()) {
        class_freq_map[output_class] += 1;
      } else {
        class_freq_map[output_class] = 1;
      }
    }
  }

  tweets.clear();

  return class_freq_map.size();
}

int Classifier::TestTwitterTimeline(const std::string& handle,
                                    const std::string& expected_class_name,
                                    Corpus& class_freq_map,
                                    TestResult& test_result,
                                    std::ostream &output_stream) {

  std::set<std::string> tweets;

  if (handle.empty()) {
    if (inagist_api::TwitterAPI::GetPublicTimeLine(tweets) < 0) {
      std::cout << "ERROR: could not get twitter's public timeline\n";
      return -1;
    }
  } else {
    if (inagist_api::TwitterSearcher::GetTweetsFromUser(handle, tweets) < 0) {
      std::cout << "ERROR: could not search twitter for user handle: " << handle << std::endl;
      return -1;
    }
  }

  if (tweets.empty()) {
    return 0;
  }

  std::set<std::string>::iterator set_iter;
  std::string tweet;
  std::string output_class;
  int ret_val = 0;
#ifdef CLASSIFIER_DEBUG
  if (CLASSIFIER_DEBUG > 2) {
    std::cout << "check corpus map" << std::endl;
    CorpusManager::PrintCorpusMap(m_corpus_manager.m_corpus_map);
  }
#endif
  for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
    test_result.total++;
    tweet = *set_iter;
    if ((ret_val = Classify(tweet, tweet.length(), output_class)) < 0) {
      std::cerr << "ERROR: could not find class\n";
      test_result.undefined++;
    } else if (ret_val == 0) {
      if (output_stream) {
        output_stream << expected_class_name << "|" << output_class << "|" << tweet << std::endl;
      }
      test_result.undefined++;
    } else {
      if (output_stream) {
        output_stream << expected_class_name << "|" << output_class << "|" << tweet << std::endl;
      }
      if (!expected_class_name.empty()) {
        if (expected_class_name.compare(output_class) == 0) {
          test_result.correct++;
        } else if ((output_class.compare(0,2,"UU") == 0) ||
                   (output_class.compare(0,2,"XX") == 0) ||
                   (output_class.compare(0,2,"RR") == 0)) {
           test_result.undefined++;
        } else {
          test_result.wrong++;
        }
      }
      if (class_freq_map.find(output_class) != class_freq_map.end()) {
        class_freq_map[output_class] += 1;
      } else {
        class_freq_map[output_class] = 1;
      }
    }
  }

  tweets.clear();

  return class_freq_map.size();
}

// training_class - this can be null. when not null, only the handles in the given class will be tested
// class_freq_map - output parameter for returning classes and their frequencies
// test_result - test result is a typedef to return the results of this testing exercise
// output_stream - if an output file is given, its fstream will be pointed by output_stream or else stdout
// random_selection - while choosing input from training sources, whether to randomly select classes and handles
int Classifier::TestTrainingSources(const char* training_class,
                                    Corpus& class_freq_map,
                                    TestResult& test_result,
                                    std::ostream &output_stream,
                                    bool random_selection) {

  std::set<std::string> handles_set;
  std::set<std::string>::iterator set_iter;
  for (m_config.iter = m_config.classes.begin();
       m_config.iter != m_config.classes.end();
       m_config.iter++) {

    std::string class_name = m_config.iter->name;

    if (NULL != training_class) {
      if (class_name.compare(0, strlen(training_class), training_class) != 0) {
        continue;
      }
    }

    std::ifstream hfs(m_config.iter->handles_file.c_str());

    if (!hfs.is_open()) {
      std::cout << "ERROR: could not open handles file: " << m_config.iter->handles_file \
                << " for class: " << class_name << std::endl;
      continue;
    }

    std::string line;
    std::string::size_type loc;
    std::string handle;
    while (getline(hfs, line)) {
      loc = line.find("=", 0);
      if (loc != std::string::npos) {
        handle.assign(line, 0, loc);
        handles_set.insert(handle);
      } else {
        std::cerr << "ERROR: ill-written handles file\n";
      }
    }
    hfs.close();

    if (handles_set.size() <= 1) {
      continue;
    }

    if (random_selection) {
      unsigned int index = rand();
      index = index % handles_set.size();
      if (index > 0 && index >= handles_set.size()) {
        continue;
      }

      unsigned int temp_index = 0;
      for (set_iter = handles_set.begin(); set_iter != handles_set.end(); set_iter++) {
        if (temp_index == index) {
          handle = *set_iter;
          break;
        }
        temp_index++;
      }
      handles_set.clear();

      if (TestTwitterTimeline(handle, class_name, class_freq_map, test_result, output_stream) < 0) {
        std::cout << "ERROR: TestTwitterTimeline failed for class: " \
                  << class_name << " on handle: " << handle << std::endl;
      }
    } else {
      for (set_iter = handles_set.begin(); set_iter != handles_set.end(); set_iter++) {
        handle = *set_iter;
        if (TestTwitterTimeline(handle, class_name, class_freq_map, test_result, output_stream) < 0) {
          std::cout << "ERROR: TestTwitterTimeline failed for class: " \
                    << class_name << " on handle: " << handle << std::endl;
        }
      }
    }

/*
    if (CorpusManager::PrintCorpus(class_freq_map) < 0) {
      std::cerr << "ERROR: could not print corpus\n";
      break;
    }
*/
  } // end of for loop

  return 0;
}

int Classifier::ValidateTestDataInput(int argc, char* argv[],
                                      const char* &config_file,
                                      const char* &keytuples_config_file,
                                      unsigned int &input_type,
                                      unsigned int &output_type,
                                      const char* &input_file,
                                      const char* &output_file,
                                      const char* &input_handle,
                                      std::string &class_name) {

  config_file = argv[1];
  keytuples_config_file = argv[2];
  input_type = atoi(argv[3]);
  output_type = atoi(argv[4]);

  if (8 == argc) {
    if (4 == input_type)
      input_file = argv[7];
    else if (5 == input_type)
      input_handle = argv[7];
    output_file = argv[6];

    if ((3 == input_type || 4 == input_type) &&
        (argv[5] && strlen(argv[5]) > 0))
      class_name = std::string(argv[5]);
  }

  if (7 == argc) {
    if (input_type == 4)
      input_file = argv[6];
    else if (input_type == 5)
      input_handle = argv[6];
    else if (output_type > 0)
      output_file = argv[6];

    if ((3 == input_type || 4 == input_type) &&
        (argv[5] && strlen(argv[5]) > 0))
      class_name = std::string(argv[5]);
  }

  if (6 == argc) {
    if (argv[5] && strlen(argv[5]) > 0) {
      if (3 == input_type || 4 == input_type) {
        class_name = argv[5];
      } else {
        output_file = argv[5];
      }
    }
  }

// validate inputs and assign inputs
  if (3 == input_type) {
    if (class_name.empty()) {
      std::cout << "class_name is required for input_type: " << input_type << std::endl;
      return -1;
    }
  } else if (4 == input_type) {
    if (!input_file) {
      std::cout << "input file required for input_type: " << input_type << std::endl;
      return -1;
    }
  } else if (5 == input_type) {
    if (!input_handle) {
      std::cout << "input handle required for input_type: " << input_type << std::endl;
      return -1;
    }
  }

  if (output_type < 0 || output_type > 2) {
    std::cout << "invalid output type: " << output_type << std::endl;
    return -1;
  }

  if (1 == output_type) {
    if (!output_file) {
      std::cout << "output file required for output_type: " << output_type << std::endl;
      return -1;
    }
    if (strlen(output_file) < 4) {
      std::cout << "invalide output file name: " << output_file << std::endl;
      return -1;
    }
  }

  std::cout << "config_file: ";
  if (config_file)
    std::cout << config_file;
  std::cout << std::endl;

  std::cout << "keytuples_config: ";
  if (keytuples_config_file)
    std::cout << keytuples_config_file;
  std::cout << std::endl;

  std::cout << "input_type: " << input_type << std::endl;
  std::cout << "output_type: " << output_type << std::endl;
  std::cout << "class_name: " << class_name << std::endl;

  std::cout << "input_file: ";
  if (input_file)
    std::cout << input_file;
  std::cout << std::endl;

  std::cout << "output_file: ";
  if (output_file)
    std::cout << output_file;
  std::cout << std::endl;

  std::cout << "input_handle: ";
  if (input_handle)
    std::cout << input_handle;
  std::cout << std::endl;

  return 0;
}

} // namespace inagist_classifiers
