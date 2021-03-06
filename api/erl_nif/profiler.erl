-module(profiler).

-export([init/0, init_c/1, test_init/0,
         profile/1,
         new_profile/0, add_to_profile/1, add_docs_list_to_profile/1, get_profile/0, delete_profile/0,
         profile/2, profile_from_file/1, profile_handles_list/1, profile_docs_list/1,
         profile_handle_only/1, profile_handle_show_details/1, profile_handle/2,
         test/0, test_handles/0, test_file/0, test_docs/0]).

init() ->
  erlang:load_nif("../../lib/libprofiler_enif", 0).

init_c(_gist_maker_config_file_path) ->
  "NIF library not loaded".

new_profile() ->
  "NIF library not loaded".

add_to_profile(_text) ->
  "NIF library not loaded".

add_docs_list_to_profile(_list) ->
  "NIF library not loaded".

get_profile() ->
  "NIF library not loaded".

delete_profile() ->
  "NIF library not loaded".

profile(_input_type, input_value) ->
  "NIF library not loaded".

profile(_twitter_handle) ->
  profile(<<"handle+">>, _twitter_handle).

profile_handle_only(_twitter_handle) ->
  profile(<<"handle">>, _twitter_handle).

profile_handle_show_details(_twitter_handle) ->
  Tuples_list = profile(<<"handle+">>, _twitter_handle),
  case is_atom(Tuples_list) of
    false -> false;
    true -> io:format("error")
  end,
  case is_list(Tuples_list) of
    false -> false;
    true -> [io:format("~p~n",[X]) || X <- Tuples_list]
  end.

profile_handle(_twitter_handle, _output_file_name) ->
  "NIF library not implemented".

profile_handles_list(_twitter_handles_list) ->
  "NIF library not loaded to profile handles from a list".

profile_from_file(_file_name) ->
  "NIF library not loaded to profile input file contents".

profile_docs_list(_docs_list) ->
  "NIF library not loaded to profile from a list".

test_init() ->
  init_c(<<"../../configs/gist_maker.config">>).

test() ->
  Tuple = profile(<<"balajiworld">>),
  case is_atom(Tuple) of
    false -> false;
    true -> io:format("error")
  end,
  case is_list(Tuple) of
    false -> false;
    true -> io:format("error")
  end,
  case is_tuple(Tuple) of
    false -> io:format("error");
    true -> [io:format("~p~n",[X]) || X <- Tuple]
  end.

test_handles() ->
  profile_handles_list([<<"balajiworld">>, <<"balajinix">>]).
  %Tuples_list = profile_handles_list([<<"balajiworld">>, <<"balajinix">>]),
  %case is_atom(Tuples_list) of
  %  false -> false;
  %  true -> io:format("error")
  %end,
  %case is_list(Tuples_list) of
  %  false -> false;
  %  true -> [io:format("~p~n",[X]) || X <- Tuples_list]
  %end.

test_file() ->
  Tuple2_list = profile_from_file(<<"../../data/lang/tweetsource/tweets/english.txt">>),
  case is_atom(Tuple2_list) of
    false -> false;
    true -> io:format("error")
  end,
  case is_list(Tuple2_list) of
    false -> false;
    true -> [io:format("~p~n",[X]) || X <- Tuple2_list]
  end.

test_docs() ->
  profile_docs_list([<<"Test Document One">>, <<"Test Document Two">>]).
  %Tuples_list = profile_docs_list([<<"Test Document One">>, <<"Test Document Two">>]),
  %case is_atom(Tuples_list) of
  %  false -> false;
  %  true -> io:format("error")
  %end,
  %case is_list(Tuples_list) of
  %  false -> false;
  %  true -> [io:format("~p~n",[X]) || X <- Tuples_list]
  %end.

