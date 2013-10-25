-module(moon_test).
-include_lib("eunit/include/eunit.hrl").

the_test_() ->
    {foreach,
        fun setup/0,
        fun teardown/1,
        [
            {"Starting/Stopping the VM",
                fun() -> ok end
            },
            {"Erlang -> Lua type mapping",
                fun() ->
					io:format("fuckyou-------------------------------------------------"),
						%%Script = <<"function test(fucname, pid, Arg, Type) return type(Arg) == Type end">>,
						%%
						{AA, FF} =  moon:load(vm, <<"/home/liuyang/sun/sun/test/root.lua">>),
						file:write_file("/home/liuyang/sun/sun/test/error", FF, [append, binary])
				%%		Script = <<"
				%%					global_thing_to_burn = {}
				%%					function the_only_sun(fname, pid, Args) 
				%%						global_thing_to_burn = global_thing_to_burn or {}
				%%						global_thing_to_burn[pid] = {fun=fname, args=Args} 
				%%				    end">>,
				%%		Script1 = <<"function get_sun_shine() 
				%%						for k, v in pairs(global_thing_to_burn) do 
				%%							return 'fuckyou', k
				%%						end 
				%%				     end">>,
				%%		
                %%    ?assertMatch({ok, undefined}, moon:eval(vm, Script)),

                %%    ?assertMatch({ok, undefined}, moon:eval(vm, Script1)),
				%%	?assertMatch({ok, <<"fuckyou">>}, moon:call(vm, test, [123, 123])),
				%%	?assertMatch({ok, <<"fuckyou">>}, moon:call(vm, test, [123, 123]))
					%%io:format("~p~n", moon:call(vm, test, [123, 123]))
                    %%?assertMatch({ok, true}, moon:call(vm, test, [nil, <<"nil">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [true, <<"boolean">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [false, <<"boolean">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [42, <<"number">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [42.5, <<"number">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [hello, <<"string">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [<<"hello">>, <<"string">>])),
                    %%?assertMatch({ok, true}, moon:call(vm, test, [[], <<"table">>]))
                end
            },
            {"Lua -> Erlang type mapping",
                fun() ->
                    ?assertMatch({ok, nil}, moon:eval(vm, <<"return nil">>)),
                    ?assertMatch({ok, true}, moon:eval(vm, <<"return true">>)),
                    ?assertMatch({ok, false}, moon:eval(vm, <<"return false">>)),
                    ?assertMatch({ok, 42}, moon:eval(vm, <<"return 42">>)),
                    ?assertMatch({ok, 42}, moon:eval(vm, <<"return 42.0">>)),
                    ?assertMatch({ok, 42.005}, moon:eval(vm, <<"return 42.005">>)),
                    ?assertMatch({ok, <<"hello">>}, moon:eval(vm, <<"return \"hello\"">>)),
                    ?assertMatch({ok, <<"goodbye">>}, moon:eval(vm, <<"return \"goodbye\"">>)),
                    ?assertMatch({ok, []}, moon:eval(vm, <<"return {}">>)),

                    ?assertMatch({ok, [10, 100, <<"abc">>]},
                        moon:eval(vm, <<"return {10, 100, \"abc\"}">>)),

                    ?assertMatch({ok, [{<<"another">>, <<"value">>}, {<<"yet">>, <<"value">>}]},
                        moon:eval(vm, <<"return {yet=\"value\", another=\"value\"}">>)),

                    ?assertMatch({ok, [<<"list">>, {<<"ugly">>, <<"mixed">>}]},
                        moon:eval(vm, <<"return {ugly=\"mixed\", \"list\"}">>))
                end
            }
        ]
    }.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

setup() ->
    error_logger:tty(false),
    application:start(moon),
    {ok, Res} = moon:start_vm(),
    register(vm, Res).

teardown(_) ->
    ok = moon:stop_vm(whereis(vm)),
    application:stop(moon).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
