#ifdef _CPLUSPLUS
#include <cstring>
#else
#include <string.h>
#endif

#include "sentiment_analyser_cppi.h"
#include "keytuples_extracter.h"

#define MAX_BUFFER_LEN 1024

inagist_trends::KeyTuplesExtracter g_kt;

#ifdef _CPLUSPLUS
extern "C"
#endif
int InitKeyTuplesExtracter(const char* keytuples_config) {

  if (g_kt.Init(keytuples_config) < 0) {
    std::cerr << "ERROR: could not initialize keytuples extracter\n";
    return -1;
  }

  return 0;
}

#ifdef _CPLUSPLUS
extern "C"
#endif
int GetSentiment(unsigned char* text_buffer, const unsigned int text_buffer_len,
                 const unsigned int text_len,
                 char* safe_status_buffer, const unsigned int safe_status_buffer_len,
                 char* script_buffer, const unsigned int script_buffer_len,
                 int* sentiment_valence_ptr) {

  int ret_value = 0;

  int sentiment_valence = 0;

  if ((ret_value = g_kt.GetKeyTuples(text_buffer, text_buffer_len, text_len,
                safe_status_buffer, safe_status_buffer_len,
                script_buffer, script_buffer_len,
                sentiment_valence)) < 0) {
    std::cerr << "ERROR: could not get keytuples\n";
    return -1;
  } else {
    *sentiment_valence_ptr = sentiment_valence;
  }

  return ret_value;
}

