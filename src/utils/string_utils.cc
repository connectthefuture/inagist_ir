#include "string_utils.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iostream>
#include "utf8.h"
#include "script_detector_utils.h"

//#define DEBUG 0
#define MAX_BUFFER_LEN 1024

extern int DetectScript(int code_point, std::string &script);

namespace inagist_utils {
  // unless otherwise specified functions return 0 or NULL or false as default
  // return values less than 0 are likely error codes

StringUtils::StringUtils() {
}

StringUtils::~StringUtils() {
}

// this function isn't unicode safe
// TODO (balaji) for ascii, we can ofcourse use an array lookup to speed up
bool StringUtils::IsPunct(char *ptr, char *prev, char *next) {
  if (!ptr || *ptr == ' ' || *ptr == '\0')
    return true;
  if (!ispunct(*ptr))
    return false;

  switch (*ptr) {
    case '\'':
      if (prev)
        if (!IsPunct(prev) &&
            (!strncmp(ptr, "'s", 2) || !strncmp(ptr, "'t", 2) || !strncmp(ptr, "'ve", 2) ||
             !strncmp(ptr, "'ll", 2) || !strncmp(ptr, "'re", 2) || !strncmp(ptr, "'m", 2) ||
             !strncmp(ptr, "'em", 3))
           )
          return false;
      break;
    case '@':
      if (prev && !IsPunct(prev))
        return true;
      return IsPunct(next);
      break;
    case '#':
      if (!next)
        return true;
      else
        if (*next == ' ' || IsPunct(next))
          return true;
      //if (prev)
      //  if (*prev != ' ' && *prev != '\0' && IsPunct(prev))
      //   return true;
      return false;
      break;
    case '-':
      if (prev && next)
        if (isalnum(*prev) && (isalnum(*next)))
          return false;
      break;
    case ':':
    case ',':
      if (prev && next)
        if (isdigit(*prev) && isdigit(*next))
          return false;
      break;
    case '.':
      if (prev && next)
        if (isdigit(*prev) && isdigit(*next))
          return false;
      if (next) {
        if (*next == ' ')
          return true;
        if (!strncmp(ptr, ".com", 4) || !strncmp(ptr, ".org", 4))
          return false; // not handling .come on or .organization etc
      }
      break;
    case '&':
      if (next)
        if (*next == '#' && isdigit(*(next+1)))
          return false;
    case '_':
      return false;
    default:
      break;
  }

  return true;
}

bool StringUtils::IsIgnore(char *&ptr) {
#ifdef DEBUG
  char* word = ptr;
#endif
  if (!ptr || '\0' == *ptr)
    return false;
  if ('@' == *ptr || !strncmp(ptr, "&#", 2) || !strncmp(ptr, "http://", 7) || !strncmp(ptr, "www.", 4)) {
    while (' ' != *(ptr+1) && '\0' != *(ptr+1)) {
      ptr++;
    }
#ifdef DEBUG
    char temp = *(ptr+1);
    *(ptr+1) = '\0'; 
    std::cout << "Ignore word: " << word << std::endl;
    *(ptr+1) = temp;
#endif
    return true;
  }
  return false;
}

// TODO (balaji) this code is long winded becos its copied from keyword_extract.cc
// need to write a simple version of this code
int StringUtils::TestUtils(const std::string& text, unsigned int text_len) {

  if (text.length() < 1)
    return -1;

  char str[MAX_BUFFER_LEN];
  strcpy(str, text.c_str());

  char *ptr = NULL;
  char *probe = NULL;
  char current_word_delimiter;

  unsigned int current_word_len = 0;

  char *current_word_start = NULL;
  char *current_word_end = NULL;
  char *next_word_start = NULL;
  bool is_ignore_word = false;
  bool is_punct = false;
  bool current_word_starts_num = false;
  int num_words = 0;

  // script detection
  char *end = strchr(str, '\0');
  std::string script = "uu";
  int code_point = 0;
  std::string script_temp;
  int script_count = 0;
  int english_count = 0;

  // the whole thing starts here
  ptr = str;

#ifdef DEBUG
  std::cout << std::endl << "original query: " << std::string(str) << std::endl << std::endl;
#endif

  // go to the first word, ignoring handles and punctuations
  char *prev = NULL;
  while (ptr && '\0' != *ptr && (' ' == *ptr || (ispunct(*ptr) && IsPunct(ptr, prev, ptr+1)) || IsIgnore(ptr))) {
    prev = ptr;
    ptr++;
  }

  if (!ptr || '\0' == *ptr) {
#ifdef DEBUG
    std::cout << "either the input is empty or has ignore words only" << std::endl;
    return 0;
#endif
  }

  current_word_start = ptr;
  num_words++;

  if (isdigit(*ptr)) {
    current_word_starts_num = true; 
  } else {
    current_word_starts_num = false;
  }

  probe = ptr + 1;

  while (ptr && probe && *ptr != '\n' && *ptr != '\0') {
    // this loop works between second letter to end punctuation for each word
    is_punct = false;
    if (' ' == *probe || '\0' == *probe || (ispunct(*probe) && (is_punct = IsPunct(probe, probe-1, probe+1)))) {

      current_word_delimiter = *probe;
      current_word_end = probe;
      *probe = '\0';
      current_word_len = current_word_end - current_word_start;
#ifdef DEBUG
      std::cout << "current word: " << current_word_start << std::endl;
#endif

      // exit conditions
      if ('\0' == current_word_delimiter) {
        *current_word_end = current_word_delimiter;
        break;
      }

      // if current word is not the known last word, briefly probe to see if next word exists
      if ('\0' != current_word_delimiter) {
        ptr = probe + 1;
        if (!ptr) {
          std::cerr << "ERROR: Fatal Exception trying to access unallocated memory space\n";
          exit(-1);
        }

        // find the next position of ptr
        // IsIgnore will literally ignore the word by changing the cursor to next word end
        is_ignore_word = false;
        is_punct = false;
        while ('\0' != *ptr && (' ' == *ptr || (ispunct(*ptr) && (is_punct = IsPunct(ptr, ptr-1, ptr+1))) || (is_ignore_word = IsIgnore(ptr)))) {
          ptr++;
        }

        if (ptr && '\0' != *ptr) {
          next_word_start = ptr;
          num_words++;
          // after finding the start of next word, probe shud be at the same place as ptr
          probe = ptr;
        } else {
          // placing the probe before '/0' so that loop will make it probe++
          // loop will terminate in the next cycle
          probe = ptr-1;
        }
      } // check for current word delimiter 

      *current_word_end = current_word_delimiter;
      current_word_start = next_word_start;
    } else {
      if (!strcmp(probe, "&#")) {
        while (' ' != *probe && '\0' != *probe)
          probe++;
        if ('\0' == *probe)
          break;
      }
    }

    // a mere cog in a loop wheel, but a giant killer if commented
    if (script_count > 9 || english_count > 20) {
      probe++;
    } else {
      try {
        code_point = utf8::next(probe, end);
        if (code_point > 0x7F) {
          if (inagist_classifiers::DetectScript(code_point, script_temp) > 0) {
            if (script_temp != "en") {
              if (script_temp != script) {
                script_count = 0;
                script = script_temp;
              } else {
                script_count++;
              }
            }
          }
        } else {
          if (code_point > 0x40 && code_point < 0x7B)
            english_count++;
        }
      } catch (...) {
#ifdef DEBUG
        std::cout << "Exception: " << code_point << " " << probe << std::endl;
#endif
        probe++;
      }
    }
  }

#ifdef DEBUG
  std::cout << "num words: " << num_words << std::endl;
#endif

  return 0;
}

} // namespace inagist_utils
