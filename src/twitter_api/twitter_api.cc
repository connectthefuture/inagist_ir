#include "twitter_api.h"
#include "curl_request_maker.h"
#include "JSONValue.h"
#include <iostream>

namespace inagist_api {

TwitterAPI::TwitterAPI() {
}

TwitterAPI::~TwitterAPI() {
}

int TwitterAPI::GetPublicTimeLine(std::set<std::string>& tweets) {
  inagist_api::CurlRequestMaker curl_request_maker;

  int num_docs = 0;

  std::string url = "http://twitter.com/statuses/public_timeline.json";
  std::string reply_message;
  // get top tweets from inagist api
  if (curl_request_maker.GetTweets(url.c_str()) < 0) {
    std::cout << "ERROR: couldn't get top tweets" << std::endl;
  } else {
    curl_request_maker.GetLastWebResponse(reply_message);
    // the response is in json format
    JSONValue *json_value = JSON::Parse(reply_message.c_str());
    if (!json_value) {
      std::cout << "ERROR: JSON::Parse failed\n";
    } else {
      // to be specific, the response is a json array
      JSONArray tweet_array = json_value->AsArray();
      JSONValue *tweet_value;

      for (unsigned int i=0; i < tweet_array.size(); i++) {
        // don't know if array element shud again be treated as json value
        // but, what the heck. lets put it as value and then get the object
        tweet_value = tweet_array[i];
        if (false == tweet_value->IsObject())
          std::cout << "ERROR: tweet_value is not an object" << std::endl;
        JSONObject tweet_object = tweet_value->AsObject();

        // now lets work on the json object thus obtained
        if (tweet_object.find("text") != tweet_object.end() && tweet_object["text"]->IsString()) {
          tweets.insert(tweet_object["text"]->AsString());
          num_docs++;
        }
        //delete tweet_value;
      }
    }
    delete json_value;
  }

  return num_docs;
}

int TwitterAPI::GetLists(const std::string& user_name,
                         std::map<std::string, std::string>& list_id_name_map) {

  inagist_api::CurlRequestMaker curl_request_maker;

  std::string temp_str;
  std::string reply_message;
  std::string cursor = "-1";

  bool ret_value = true;
  std::string list_name;
  while (ret_value) {
    std::string url = "http://api.twitter.com/1/" + user_name + "/lists.json?cursor=" + cursor;
    ret_value = curl_request_maker.GetTweets(url.c_str());

    if (ret_value) {
      curl_request_maker.GetLastWebResponse(reply_message);
      if (reply_message.size() > 0) {
        // the response is in json format
        JSONValue *json_value = JSON::Parse(reply_message.c_str());
        if (!json_value || false == json_value->IsObject()) {
          std::cout << "Error: curl reply not a json object\n";
          break;
        } else {
          // to be specific, the response is a json array
          JSONObject t_o = json_value->AsObject(); 
          if (t_o.find("lists") != t_o.end() && t_o["lists"]->IsArray()) {
            JSONArray list_array = t_o["lists"]->AsArray();
            JSONObject list_object;
            for (unsigned int i=0; i < list_array.size(); i++) {
              JSONValue *list_value = list_array[i];
              if (false == list_value->IsObject()) {
                std::cout << "ERROR: list_value is not an object" << std::endl;
              } else {
                list_object = list_value->AsObject();
                // now lets work on the json object thus obtained
                // first we'll get list name then id. this code will go for a toss, if that is not the case
                if (list_object.find("name") != list_object.end() && list_object["name"]->IsString()) {
                  list_name = list_object["name"]->AsString();
                }
                if (list_object.find("id_str") != list_object.end() && list_object["id_str"]->IsString()) {
                  if (list_name.length() < 1) {
                    std::cerr << "ERROR: invalid list name. json read out of order\n";
                  } else {
                    list_id_name_map.insert(std::pair<std::string, std::string>(list_object["id_str"]->AsString(), list_name));
                    list_name.clear();
                  }
                }
              }
            }
          }
          if (t_o.find("next_cursor_str") != t_o.end() && t_o["next_cursor_str"]->IsString()) {
            cursor = t_o["next_cursor_str"]->AsString();
            if (cursor.compare("0") == 0) {
              break;
            }
          } else {
            std::cout << "could not find next_cursor_str" << std::endl;
            break;
          }
        }
        delete json_value;
      }
    }
  }

  return list_id_name_map.size();

}

int TwitterAPI::GetListMembers(const std::string& user_name,
                                const std::string& list_id,
                                std::set<std::string>& members) {

  inagist_api::CurlRequestMaker curl_request_maker;

  std::string temp_str;
  std::string reply_message;
  std::string cursor = "-1";

  bool ret_value = true;
  while (ret_value) {
    std::string url = "http://api.twitter.com/1/" + user_name + "/" \
                      + list_id + "/members.json?cursor=" + cursor;
    ret_value = curl_request_maker.GetTweets(url.c_str());

    if (ret_value) {
      curl_request_maker.GetLastWebResponse(reply_message);
      if (reply_message.size() > 0) {
        // the response is in json format
        JSONValue *json_value = JSON::Parse(reply_message.c_str());
        if (!json_value || false == json_value->IsObject()) {
          std::cout << "Error: curl reply not a json object\n";
          break;
        } else {
          // to be specific, the response is a json array
          JSONObject t_o = json_value->AsObject(); 
          if (t_o.find("users") != t_o.end() && t_o["users"]->IsArray()) {
            JSONArray user_array = t_o["users"]->AsArray();
            JSONObject user_object;
            for (unsigned int i=0; i < user_array.size(); i++) {
              JSONValue *user_value = user_array[i];
              if (false == user_value->IsObject()) {
                std::cout << "ERROR: user_value is not an object" << std::endl;
              } else {
                user_object = user_value->AsObject();
                // now lets work on the json object thus obtained
                if (user_object.find("screen_name") != user_object.end() && user_object["screen_name"]->IsString()) {
                  members.insert(user_object["screen_name"]->AsString());
                } else {
                  std::cerr << "ERROR: screen_name not found\n";
                }
              }
            }
          }
          if (t_o.find("next_cursor_str") != t_o.end() && t_o["next_cursor_str"]->IsString()) {
            cursor = t_o["next_cursor_str"]->AsString();
            if (cursor.compare("0") == 0) {
              break;
            }
          } else {
            std::cout << "could not find next_cursor_str" << std::endl;
            break;
          }
        }
        delete json_value;
      }
    }
  }

  return members.size();
}

int TwitterAPI::GetListStatuses(const std::string& user_name,
                                const std::string& list_name,
                                std::set<std::string>& tweets) {

  inagist_api::CurlRequestMaker curl_request_maker;

  std::string temp_str;
  std::string reply_message;
  std::string cursor = "-1";

  bool ret_value = true;
  //while (ret_value) {
    std::string url = "http://api.twitter.com/1/" + user_name + "/lists/" \
                      + list_name + "/statuses.json";
    ret_value = curl_request_maker.GetTweets(url.c_str());

    if (ret_value) {
      curl_request_maker.GetLastWebResponse(reply_message);
      if (reply_message.size() > 0) {
        // the response is in json format
        JSONValue *json_value = JSON::Parse(reply_message.c_str());
        if (json_value->IsArray()) {
          JSONArray tweet_array = json_value->AsArray();
          JSONObject tweet_object;
          for (unsigned int i=0; i < tweet_array.size(); i++) {
            JSONValue *tweet_value = tweet_array[i];
            if (false == tweet_value->IsObject()) {
              std::cout << "ERROR: tweet_value is not an object" << std::endl;
            } else {
              tweet_object = tweet_value->AsObject();
              // now lets work on the json object thus obtained
              if (tweet_object.find("text") != tweet_object.end() && tweet_object["text"]->IsString()) {
                tweets.insert(tweet_object["text"]->AsString());
              }
            }
          }
        }
        delete json_value;
      }
    }
  //}

  return tweets.size();
}

}
