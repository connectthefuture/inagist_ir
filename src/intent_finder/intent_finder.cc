#include "intent_finder.h"
#include <iostream>
#include <cstring>

namespace inagist_classifiers {

IntentFinder::IntentFinder() {
}

IntentFinder::~IntentFinder() {
}

int IntentFinder::Init(const char* keytuples_extracter_config_file) {

  // initialize keytuples extracter
  bool load_classifier_dictionary=false;
  if (m_keytuples_extracter.Init(keytuples_extracter_config_file) < 0) {
    std::cerr << "ERROR: could not initialize KeyTuplesExtracter\n";
    return -1;
  }

  return 0;
}

int IntentFinder::FindIntent(unsigned char* text_buffer, const unsigned int& text_buffer_len,
                             const unsigned int& text_len,
                             int& intent_valence) {

  if (!text_buffer) {
    std::cerr << "ERROR: invalid text buffer\n";
    return -1;
  }

  char safe_status_buffer[10];
  unsigned int safe_status_buffer_len = 10;
  char script_buffer[4];
  unsigned int script_buffer_len = 4;

  if (m_keytuples_extracter.GetKeyTuples(text_buffer, text_buffer_len, text_len,
                                         safe_status_buffer, safe_status_buffer_len,
                                         script_buffer, script_buffer_len,
                                         intent_valence) < 0) {
    std::cerr << "ERROR: could not get intent\n";
    return -1;
  }

  if (strcmp(script_buffer, "en") != 0) {
    return 0;
  }

  return 1;
}

int IntentFinder::Clear() {
  return 0;
}

}
