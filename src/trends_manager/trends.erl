-module(trends).
-export([init/0, init_c/1, submit/0, trends/0, deinit_c/0, getkeywords/1, gettrends/1, test/2]).
init() ->
  erlang:load_nif("./trends", 0).
init_c(_stopwords_file_path) ->
  "NIF library not loaded".
submit() ->
  "NIF library not loaded".
trends() ->
  "NIF library not loaded".
deinit_c() ->
  "NIF library not loaded".
getkeywords(_tweet) ->
  "NIF library not loaded".
gettrends(_user) ->
  "NIF library not loaded".
test(_tweet, _user) ->
  "NIF library not loaded".
