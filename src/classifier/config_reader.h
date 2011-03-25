#ifndef _INAGIST_CLASSIFIERS_CONFIG_READER_H_
#define _INAGIST_CLASSIFIERS_CONFIG_READER_H_

#include <string>
#include <set>

namespace inagist_classifiers {

typedef struct _class_struct { 
  std::string name;
  std::string training_data_file;
  std::string handles_file;
  std::string corpus_file;
  std::string tweets_file; 
  friend bool operator<(_class_struct const& a, _class_struct const& b) {
    return a.name.compare(b.name) < 0;
  }
} ClassStruct;

typedef struct _config_struct {
  std::string test_data_file;
  std::set<ClassStruct> classes;
  std::set<ClassStruct>::iterator iter;
} Config;

class ConfigReader {
 public:
  // functions
  ConfigReader();
  ~ConfigReader();
  static int Read(const char* config_file_name, Config& config);
  static int Clear(Config& config);
};

} // namespace inagist_classifiers

#endif // _INAGIST_CLASSIFIERS_CONFIG_READER_H_
