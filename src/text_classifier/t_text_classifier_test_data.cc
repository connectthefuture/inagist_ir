#include "text_classifier.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

using namespace inagist_classifiers;

int main(int argc, char* argv[]) {

  if (argc != 5 && argc != 6) {
    std::cout << "Usage: " << argv[0] << " <classifier_config> <keytuples_config> <input_type> <output_type> [class_name] [output_file]\n";
    std::cout << "input_type:\n\t0 - twitter timeline\n\t1 - random selection of training sources\n\t2 - all training sources (all handles)\n\t3 - given class(random handles)\noutput_type:\n\t0 - stdout\n\t1 - class frequency file\n\t2 - html version\n";
    return -1;
  }

  const char* config_file = argv[1];
  const char* keytuples_config_file = argv[2];
  unsigned int input_type = atoi(argv[3]);
  unsigned int output_type = atoi(argv[4]);
  char* output_file = NULL;
  const char* class_name = NULL;

  if (6 == argc) {
    output_file = argv[5];
    if (3 == input_type)
      class_name = argv[4];
  }
  if (5 == argc) {
    if (3 == input_type) {
      class_name = argv[4];
    } else {
      output_file = argv[4];
    }
  }
  const char* input_value = class_name;

  TextClassifier *tc = (TextClassifier *) new TextClassifier();

  bool ignore_history = true;
  if (tc->Init(config_file, ignore_history) < 0) {
    std::cerr << "ERROR: could not initialize text classifier\n";
    return -1;
  }

  int my_argc = 1;
  char* my_argv[1];
  char* temp_location = (char *) malloc(255);
  my_argv[0] = temp_location; 
  memset(temp_location, '\0', 255);
  strcpy(temp_location, keytuples_config_file);
  if (tc->InitDependencies(my_argc, (char **) my_argv) < 0) {
    std::cerr << "ERROR: could not initialize dependencies for text classifier\n";
    return -1;
  }
  free(my_argv[0]);

  int ret_val = 0;
  if (tc->GetTestData(input_type, input_value, output_type, output_file) < 0) {
    std::cerr << "ERROR: could not get test data\n";
    ret_val = -1;
  }

  try {
    if (tc->ClearDependencies() < 0) {
      std::cerr << "ERROR: could not clear dependencies\n";
      ret_val = -1;
    }

    if (tc->Clear() < 0) {
      std::cerr << "ERROR: could not clear text classifier\n";
      ret_val = -1;
    }

    delete tc;
  } catch (...) {
    std::cerr << "Exception raised\n";
    return -1;
  }

  return ret_val;
}
