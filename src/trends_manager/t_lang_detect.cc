#include "language_detector.h"
#include "trends_manager.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>

#define MAX_BUFFER_LEN 10240

inagist_trends::KeywordsExtract g_ke;

int Init(std::string& root_dir) {

  std::string stopwords_file = root_dir + "/data/static_data/stopwords.txt";
  std::string dictionary_file = root_dir + "/data/static_data/dictionary.txt";
  std::string unsafe_dictionary_file = root_dir + "/data/static_data/unsafe_dictionary.txt";
  std::string lang_detect_config_file = root_dir + "/configs/language_detection.config";
  std::string channels_dictionary_file = root_dir + "/data/static_data/channels_dictionary.txt";

  if (Init(stopwords_file.c_str(),
           dictionary_file.c_str(),
           unsafe_dictionary_file.c_str(),
           lang_detect_config_file.c_str(),
           channels_dictionary_file.c_str()) < 0) {
    std::cout << "ERROR: could not initialize keywords extract\n";
    return -1;
  }

  return 0;
}

int TestLangForHandle(std::string& handle, const char* expected_lang,
                      unsigned int& tweets_num, unsigned int& detected_num,
                      unsigned int& undefined_num,
                      const unsigned int output_type, std::ostream& ostream_ptr) {

  if (output_type == 1) {
    ostream_ptr << "<tr><td><br/>" << std::endl;
  }
  ostream_ptr << "handle: " << handle << std::endl;
  char tweets_buffer[MAX_BUFFER_LEN];
  unsigned int tweets_len = 0;
  if (GetTestTweets(handle.c_str(), MAX_BUFFER_LEN, tweets_buffer, &tweets_len) < 0) {
    std::cout << "ERROR: could not get tweets for handle: " << handle << std::endl;
  }

  char safe_status[10];
  char script[4];
  unsigned char keywords[MAX_BUFFER_LEN];
  unsigned int keywords_len = 0;
  unsigned int keywords_count = 0;
  unsigned char hashtags[MAX_BUFFER_LEN];
  unsigned int hashtags_len = 0;
  unsigned int hashtags_count = 0;
  unsigned char keyphrases[MAX_BUFFER_LEN];
  unsigned int keyphrases_len = 0;
  unsigned int keyphrases_count = 0;
  char buffer1[MAX_BUFFER_LEN];
  char buffer2[MAX_BUFFER_LEN];
  char buffer3[MAX_BUFFER_LEN];
  char buffer4[MAX_BUFFER_LEN];

  char *tweet_start = tweets_buffer;
  char *tweet_end = strstr(tweet_start, "|");
  unsigned int tweet_len = 0;
  bool flag = false;

  while (tweet_start && tweet_end && *tweet_end != '\0') {

    tweet_end = strstr(tweet_start, "|");
    if (!tweet_end)
      break;
    *tweet_end = '\0';
    tweet_len = tweet_end - tweet_start;

    if (tweet_len <= 0 || tweet_len >= MAX_BUFFER_LEN) {
      std::cout << "ERROR: invalid tweet\n";
    }

    memset(safe_status, 0, 10);
    memset(script, 0, 4);
    keywords[0] = '\0';
    hashtags[0] = '\0';
    keyphrases[0] = '\0';
    buffer1[0] = '\0';
    buffer2[0] = '\0';
    buffer3[0] = '\0';
    buffer4[0] = '\0';
    int ret_value = 0;
    tweets_num++;
    if ((ret_value = SubmitTweet((const unsigned char *) tweet_start, tweet_len,
                    (char *) safe_status, 10,
                    (char *) script, 4,
                    keywords, MAX_BUFFER_LEN,
                    &keywords_len, &keywords_count,
                    hashtags, MAX_BUFFER_LEN,
                    &hashtags_len, &hashtags_count,
                    keyphrases, MAX_BUFFER_LEN,
                    &keyphrases_len, &keyphrases_count,
                    (char *) buffer1, MAX_BUFFER_LEN,
                    (char *) buffer2, MAX_BUFFER_LEN,
                    (char *) buffer3, MAX_BUFFER_LEN,
                    (char *) buffer4, MAX_BUFFER_LEN)) < 0) {
      if (output_type == 1)
        ostream_ptr << "</tr></td><br/>" << std::endl;
      return -1;
    }

    flag = false;
    if ((strcmp(expected_lang, buffer1) == 0) ||
        ((strcmp(expected_lang, script) == 0) &&
         (strcmp("en", script) != 0))) {
      detected_num++;
      flag = true;
    } else if ((strlen(buffer1) < 2) ||
               (strcmp("RR", buffer1) == 0) ||
               (strcmp("xx", buffer1) == 0) ||
               (strcmp("uu", buffer1) == 0)) {
      undefined_num++;
    }

    if (output_type == 1) {
      if (flag) {
        ostream_ptr << "<tr><td bgcolor=#07B133><br/>" << tweet_start << "<br/>" << expected_lang << "<br/>" << script << "<br/>" << buffer1 << "<br/>" << buffer2 << std::endl;
      } else {
        ostream_ptr << "<tr><td bgcolor=#FFFFFF><br/>" << tweet_start << "<br/>" << expected_lang << "<br/>" << script << "<br/>" << buffer1 << "<br/>" << buffer2 << std::endl;
      }

      ostream_ptr << "</td></tr><br/>" << std::endl;
    } else {
      ostream_ptr << "Tweet: " << tweet_start << std::endl;
      ostream_ptr << "expected lang: " << expected_lang << std::endl;
      ostream_ptr << "script: " << script << std::endl;
      ostream_ptr << "lang guess 1: " << buffer1 << std::endl;
      ostream_ptr << "lang guess 2: " << buffer2 << std::endl;
    }

    *tweet_end = '|';
    tweet_start = tweet_end + 1;
  }
  tweet_start = NULL;
  tweet_end = NULL;

  if (output_type == 1)
    ostream_ptr << "<hr/>" << std::endl;

  ostream_ptr << std::endl << "tweets: " << tweets_num \
      << " detected: " << detected_num \
      << " undefined: " << undefined_num << std::endl;

  if (output_type == 1)
    ostream_ptr << "</td></tr><br/>" << std::endl;

  return 0;
}

int main(int argc, char* argv[]) {

  if (argc != 2 && argc != 3) {
    std::cout << "Usage: " << argv[0] << " <config_file_name> [output_html_file]\n";
    return -1;
  }

  std::string config_file_name = argv[1];

  std::ifstream ifs(config_file_name.c_str());
  if (!ifs.is_open()) {
    std::cout << "ERROR: could not open config file " << config_file_name << std::endl;
    return -1;
  }

  std::ofstream ofs;
  std::ostream* ostream_ptr;
  std::string output_html_file_name;
  unsigned int output_type = 0;
  if (argc == 3) {
    output_html_file_name = std::string(argv[2]);
    ofs.open(output_html_file_name.c_str());
    output_type = 1;
    ostream_ptr = &ofs;
    *ostream_ptr << "<html>\n<head>\n" \
                 << "<title>Inagist Language Detection</title>\n" \
                 << "<h3>Inagist Language Detection</h3>\n" \
                 << "</head>\n<body>\n<table>" << std::endl;
    ofs.close();
    ofs.open(output_html_file_name.c_str(), std::ios_base::app);
    output_type = 1;
    ostream_ptr = &ofs;
  } else {
    ostream_ptr = &std::cout;
  }

  std::string arguments(argv[0]);
  std::string::size_type loc = arguments.find("bin", 0);
  std::string root_dir;
  if (loc != std::string::npos) {
    loc-=1;
    root_dir.assign(argv[0], loc);
  }

  if (Init(root_dir) < 0) {
    std::cout << "ERROR: could not initialize keywords extract\n";
    return -1;
  }

  std::string line;
  std::string key;
  std::string value;
  int line_count = 0;
  std::string lang;
  std::string handles_file_name;
  std::string output_tweets_file_name;
  std::string output_corpus_file_name;
  unsigned int tweets_num = 0;
  unsigned int detected_num = 0;
  unsigned int undefined_num = 0;
  unsigned int total_tweets_num = 0;
  unsigned int total_detected_num = 0;
  unsigned int total_undefined_num = 0;
  char debug_str[255];
  memset(debug_str, '\0', 255);
  std::set<std::string> debug_str_set;
  while (getline(ifs, line)) {
    line_count++;
    loc = line.find("=", 0);
    if (loc == std::string::npos) {
      std::cout << "ERROR: invalid config file entry\n";
      break;
    }
    key.assign(line.c_str(), loc);
    value.assign(line.c_str(), loc+1, (line.length()-loc-1));
    if (key.compare(0, 4, "lang") == 0) {
      lang = value;
    } else if (key.compare(0, 7, "handles") == 0) {
      handles_file_name = value;
    } else if (key.compare(0, 6, "corpus") == 0) {
      output_corpus_file_name = value;
    } else if (key.compare(0, 6, "tweets") == 0) {
      output_tweets_file_name = value;
    }
    if (line_count == 4) {
      std::ifstream hfs(handles_file_name.c_str());
      if (!hfs.is_open()) {
        std::cout << "ERROR: could not open handles file: " << handles_file_name \
                  << " for lang: " << lang << std::endl;
      } else {
        std::string handle;
        getline(hfs, handle);
        hfs.close();
        tweets_num = 0;
        detected_num = 0;
        undefined_num = 0;
/*
        if (TestLangForHandle(handle, lang.c_str(),
                              tweets_num, detected_num, undefined_num,
                              output_type, *ostream_ptr) < 0) {
          std::cout << "ERROR: TestLangForHandle failed for lang: " \
                    << lang << "on handle: " << handle << std::endl;
        }
*/
        total_tweets_num += tweets_num;
        total_detected_num += detected_num;
        total_undefined_num += undefined_num;
        memset(debug_str, '\0', 255);
        if (output_type == 1)
          sprintf(debug_str, "<tr><td>%s</td><td>%u</td><td>%u</td><td>%u</td></tr>", lang.c_str(), tweets_num, detected_num, undefined_num);
        else
          sprintf(debug_str, "%s %u %u %u", lang.c_str(), tweets_num, detected_num, undefined_num);
        debug_str_set.insert(std::string(debug_str));
      }
      line_count = 0;
    }
  }
  ifs.close();

  if (output_type == 1)
    *ostream_ptr << "</table>\n<br/>" << std::endl;
  *ostream_ptr << "Summary:" << std::endl;
  if (output_type == 1)
    *ostream_ptr << "<br/>" << std::endl;
  std::set<std::string>::iterator set_iter;
  if (output_type == 1)
    *ostream_ptr << "<table>" << std::endl;
  for (set_iter = debug_str_set.begin(); set_iter != debug_str_set.end(); set_iter++) {
    *ostream_ptr << *set_iter << std::endl;
  }
  if (output_type == 1)
    *ostream_ptr << "</table>" << std::endl;
  debug_str_set.clear();

  if (output_type == 1)
    *ostream_ptr << "<br/>" << std::endl;

  *ostream_ptr << std::endl << "total tweets: " << total_tweets_num << std::endl;
  if (output_type == 1)
    *ostream_ptr << "<br/>" << std::endl;
  *ostream_ptr << "total detected: " << total_detected_num << std::endl;
  if (output_type == 1)
    *ostream_ptr << "<br/>" << std::endl;
  *ostream_ptr << "total undefined: " << total_undefined_num << std::endl;

  if (output_type == 1) {
    *ostream_ptr << "</body>\n</html>" << std::endl;
  }

  if (argc == 3) {
    flush(*ostream_ptr);
    ofs.close();
  }

  return 0;
}

