#include "twitter_api.h"
#include <set>
#include <map>
#include <iostream>
#include <cstdlib>
#include <cassert>
#include <fstream>

int main(int argc, char* argv[]) {

  if (argc < 2 || argc > 4) {
    std::cerr << "Usage: " << argv[0] \
              << "\n\t<0-7, 0-tweets, 1-lists, 2-list_statuses, 3-list_members, 4-usersummary, 5-userdetails, 6-followers, 7-followertweets, 8-urls>" \
              << "\n\t[<user_name>]" \
              << "\n\t[<output_file>]\n";
    return -1;
  }

  int request_type = atoi(argv[1]);
  assert((request_type >= 0) && (request_type <= 8) && (request_type == 0 || request_type == 8 || argc > 2));

  inagist_api::TwitterAPI tapi;
  std::string user_name;
  std::string output_file_name;
  if (argc > 3) {
    user_name = std::string(argv[2]);
  } else if (3 == argc) {
    if (0 == request_type) {
      output_file_name = std::string(argv[2]);
    } else {
      user_name = std::string(argv[2]);
    }
  }
  if (4 == argc) {
    output_file_name = std::string(argv[3]); 
  }

  switch (request_type) {
    case 0:
      { // needed for scope of variables

        // TODO (balaji) badly written code.
        // keep a single file stream for both std::cout and output file
        std::ofstream ofs;
        if (!output_file_name.empty()) {
          ofs.open(output_file_name.c_str());
          if (!ofs.is_open()) {
            std::cerr << "ERROR: could not open output file: " << output_file_name << std::endl;
            return -1;
          }
        }

        std::set<std::string> tweets;
        if (tapi.GetPublicTimeLine(tweets) < 0) {
          std::cout << "Error: could not get public timeline\n" << std::endl;
        } else {
          std::set<std::string>::iterator set_iter;
          for (set_iter = tweets.begin(); set_iter != tweets.end(); set_iter++) {
            if (!ofs.is_open()) {
              std::cout << *set_iter << std::endl;
            } else {
              ofs << *set_iter << std::endl;
            }
          }
          tweets.clear();
        }

        if (ofs.is_open()) {
          ofs.close();
        }

      }
      break;
    case 1:
    case 2:
    case 3:
      { // scope
        if (argc < 3) {
          std::cerr << "ERROR: user name required\n";
          return -1;
        }
        std::map<std::string, std::string> list_id_name_map;
        if (tapi.GetLists(user_name, list_id_name_map) < 0) {
          std::cout << "Error: could not get lists for user: " << user_name << std::endl;
          return -1;
        }

        std::ofstream ofs;
        if (4 == argc) {
          ofs.open(output_file_name.c_str());
          if (!ofs.is_open()) {
            std::cerr << "ERROR: could not open output file: " << output_file_name << std::endl;
            list_id_name_map.clear();
            return -1;
          }
        }

        std::map<std::string, std::string>::iterator list_id_name_map_iter;
        std::set<std::string> tweets;
        std::set<std::string> members;
        for (list_id_name_map_iter = list_id_name_map.begin();
             list_id_name_map_iter != list_id_name_map.end();
             list_id_name_map_iter++) {
          std::cout << list_id_name_map_iter->second \
                    << " (id: " << list_id_name_map_iter->first << ")" << std::endl;
          if (2 == request_type) {
            std::set<std::string>::iterator tweets_iter;
            if (tapi.GetListStatuses(user_name, list_id_name_map_iter->second, tweets) < 0) {
              std::cerr << "ERROR: could not get tweets for user " << user_name \
                        << " on list: " << list_id_name_map_iter->second << std::endl;
            }
            for (tweets_iter = tweets.begin(); tweets_iter != tweets.end(); tweets_iter++) {
              std::cout << *tweets_iter << std::endl;
            }
            tweets.clear();
          } else if (3 == request_type) {
            std::set<std::string>::iterator members_iter;
            if (tapi.GetListMembers(user_name, list_id_name_map_iter->first, members) < 0) {
              std::cerr << "ERROR: could not get members for user " << user_name \
                        << " on list: " << list_id_name_map_iter->second << std::endl;
            }
            for (members_iter = members.begin(); members_iter != members.end(); members_iter++) {
              if (4 == argc) {
                ofs << *members_iter << std::endl;
              } else {
                std::cout << *members_iter << std::endl;
              }
            }
            members.clear();
          }
        }
        if (4 == argc) {
          ofs.close();
        }
        members.clear();
        list_id_name_map.clear();
      }
      break;
    case 4:
      { // scope
        std::string info;
        if (tapi.GetUserInfo(user_name, info) < 0) {
          std::cerr << "ERROR: could not get user info for handle: " << user_name << std::endl;
          return -1;
        } else {
          std::cout << info << std::endl;
        }
      }
      break;
    case 5:
      { // scope
        std::string name;
        std::string description;
        std::string url;
        std::string age;
        std::string gender;
        std::string language;
        std::string location;
        std::string time_zone;
        std::string city;
        std::string state;
        std::string country;
        std::string user_info;
        if (tapi.GetUserInfo(user_name, name, description, url,
                             age, gender, language,
                             location, time_zone,
                             city, state, country,
                             user_info) < 0) {
          std::cerr << "ERROR: could not get user info for handle: " << user_name << std::endl;
          return -1;
        } else {
          std::cout << "name: " << name << std::endl;
          std::cout << "description: " << description << std::endl;
          std::cout << "url: " << url << std::endl;
          std::cout << "age: " << age << std::endl;
          std::cout << "location: " << location << std::endl;
          std::cout << "time_zone: " << time_zone << std::endl;
          std::cout << "city: " << city << std::endl;
          std::cout << "state: " << state << std::endl;
          std::cout << "country: " << country << std::endl;
          std::cout << "language: " << language << std::endl;
          std::cout << "user_info: " << user_info << std::endl;
        }
      }
      break;
    case 6:
      {
        std::set<std::string> followers;
        if (tapi.GetFollowers(user_name, followers) < 0) {
          std::cerr << "ERROR: could not get followers for user: " << user_name << std::endl;
        }
        followers.clear();
      }
      break;
    case 7:
      {
        std::set<std::string> tweets;
        if (tapi.GetFollowerTweets(user_name, tweets) < 0) {
          std::cerr << "ERROR: could not get followerTweets for user: " << user_name << std::endl;
        }
        tweets.clear();
      }
      break;
    case 8:
      {
        std::set<std::string> urls;
        bool expand_urls = true;
        if (tapi.GetTweets(user_name, urls, expand_urls=true) < 0) { // user_name can be empty
          std::cerr << "ERROR: GetUrls failed\n";
        }
        std::set<std::string>::iterator set_iter;
        for (set_iter = urls.begin(); set_iter != urls.end(); set_iter++) {
          std::cout << *set_iter << std::endl;
        }
        urls.clear();
      }
    default:
      break;
  }

  return 0;
}
