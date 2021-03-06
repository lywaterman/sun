#include "lua.hpp"
#include "utils.hpp"
#include "errors.hpp"
#include "lua_utils.hpp"

#include <dlfcn.h>
#include <unistd.h>

extern "C"
{
	#include "luasocket/luasocket.h"
}
#include "pbc-c.h"
#include "luamongo/interface.h"

extern "C"
{
	#include "cjson/interface.h"
}

extern "C"
{
	#include "luaxml/interface.h"
	#include "lanes/lanes.h"
}

namespace lua {

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// task handlers:
template <class return_type>
class base_handler : public boost::static_visitor<return_type>
{
public :
    typedef typename boost::static_visitor<return_type>::result_type result_type;

    base_handler(vm_t & vm) : vm_(vm) {};
    virtual return_type operator()(vm_t::tasks::load_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::eval_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::call_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::resp_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::quit_t const&) { throw quit_tag(); }

    vm_t & vm() { return vm_; };
    vm_t const& vm() const { return vm_; }

protected :
    ~base_handler() {}

private :
    vm_t & vm_;
};

/////////////////////////////////////////////////////////////////////////////

class result_handler : public base_handler<erlcpp::term_t>
{
public :
    using base_handler<erlcpp::term_t>::operator();
    result_handler(vm_t & vm) : base_handler<erlcpp::term_t>(vm) {};
    erlcpp::term_t operator()(vm_t::tasks::resp_t const& resp) { return resp.term; }
};

void dispatch(vm_t & vm)
{
	stack_guard_t guard(vm);
	try
	{
		lua_getglobal( vm.state(), "debug" );
		lua_getfield( vm.state(), -1, "traceback" );
		lua_remove( vm.state(), -2 );

		lua_getglobal(vm.state(), "get_sun_shine");

		if (lua_isnil(vm.state(), -1)) 
			return;

		if (lua_pcall(vm.state(), 0, LUA_MULTRET, -2))
			//if (lua_pcall(vm().state(), call.args.size(), LUA_MULTRET, 0))
		{
			throw std::runtime_error(lua_tostring(vm.state(), -1));
		}
		else
		{
			erlcpp::tuple_t result(2);
			result[0] = erlcpp::atom_t("ok");
			lua_remove(vm.state(), 1);
			
			if (lua_gettop(vm.state()) == 0)
			{
				enif_fprintf(stderr, "get top is 000000\n");
				return;
			}
			lua_Integer pid =  lua_tointeger(vm.state(), -1);
			enif_fprintf(stderr, "get_top: %d\n", lua_gettop(vm.state()));
			
			lua_pop(vm.state(), 1);

			enif_fprintf(stderr, "get_top: %d\n", lua_gettop(vm.state()));

			result[1] = lua::stack::pop_all(vm.state());

			erlcpp::lpid_t caller;
			caller.ptr()->pid = (ERL_NIF_TERM)(pid);

			enif_fprintf(stderr, "call get_sun_shine: %d\n", pid);

			send_result_caller(vm, "moon_response", result, caller);
		}
	}
	catch( std::exception & ex )
	{
		erlcpp::tuple_t result(2);
		result[0] = erlcpp::atom_t("error_lua");
		result[1] = erlcpp::atom_t(ex.what());

		enif_fprintf(stderr, "*** exception when call get_sun_shine : %s\n", ex.what());
	}

}
struct call_handler : public base_handler<void>
{
    using base_handler<void>::operator();
    call_handler(vm_t & vm) : base_handler<void>(vm) {};

    // Loading file:
    void operator()(vm_t::tasks::load_t const& load)
    {
        stack_guard_t guard(vm());
        try
        {
            std::string file(load.file.data(), load.file.data() + load.file.size());
            if (luaL_dofile(vm().state(), file.c_str()))
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("error_lua");
                result[1] = lua::stack::pop(vm().state());
                send_result_caller(vm(), "moon_response", result, load.caller);
            }
            else
            {
                erlcpp::atom_t result("ok");
                send_result_caller(vm(), "moon_response", result, load.caller);
            }
        }
        catch( std::exception & ex )
        {
            erlcpp::tuple_t result(2);
            result[0] = erlcpp::atom_t("error_lua");
            result[1] = erlcpp::atom_t(ex.what());
            send_result_caller(vm(), "moon_response", result, load.caller);
        }
    }

    // Evaluating arbitrary code:
    void operator()(vm_t::tasks::eval_t const& eval)
    {
        stack_guard_t guard(vm());
        try
        {
            if ( luaL_loadbuffer(vm().state(), eval.code.data(), eval.code.size(), "line") ||
                    lua_pcall(vm().state(), 0, LUA_MULTRET, 0) )
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("error_lua");
                result[1] = lua::stack::pop(vm().state());
                send_result_caller(vm(), "moon_response", result, eval.caller);
            }
            else
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("ok");
                result[1] = lua::stack::pop_all(vm().state());
                send_result_caller(vm(), "moon_response", result, eval.caller);
            }
        }
        catch( std::exception & ex )
        {
            erlcpp::tuple_t result(2);
            result[0] = erlcpp::atom_t("error_lua");
            result[1] = erlcpp::atom_t(ex.what());
            send_result_caller(vm(), "moon_response", result, eval.caller);
        }
    }

    // Calling arbitrary function:
    void operator()(vm_t::tasks::call_t const& call)
    {
        stack_guard_t guard(vm());
        try
        {
			lua_getglobal( vm().state(), "debug" );
			lua_getfield( vm().state(), -1, "traceback" );
			lua_remove( vm().state(), -2 );
			

            //lua_getglobal(vm().state(), call.fun.c_str());
            lua_getglobal(vm().state(), "the_only_sun");
            
			lua_pushstring(vm().state(), call.fun.c_str());
			lua_pushinteger(vm().state(), call.caller.to_int());

			lua::stack::push(vm().state(), call.args);

            if (lua_pcall(vm().state(), 1 + 2, LUA_MULTRET, -2-1+2))
            //if (lua_pcall(vm().state(), call.args.size(), LUA_MULTRET, 0))
            {
                //erlcpp::tuple_t result(2);
                //result[0] = erlcpp::atom_t("error_lua");
                //result[1] = lua::stack::pop(vm().state());
                //send_result_caller(vm(), "moon_response", result, call.caller);
            }
            else
            {
                //erlcpp::tuple_t result(2);
                //result[0] = erlcpp::atom_t("ok");
				//lua_remove(vm().state(), 1);
                //result[1] = lua::stack::pop_all(vm().state());
                //send_result_caller(vm(), "moon_response", result, call.caller);
            }
        }
        catch( std::exception & ex )
        {
            erlcpp::tuple_t result(2);
            result[0] = erlcpp::atom_t("error_lua");
            result[1] = erlcpp::atom_t(ex.what());
            send_result_caller(vm(), "moon_response", result, call.caller);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

int erlang_call(vm_t & vm)
{
    bool exception_caught = false; // because lua_error makes longjump
    try
    {
        stack_guard_t guard(vm);

        erlcpp::term_t args = lua::stack::pop_all(vm.state());

        send_result(vm, "moon_callback", args);
        erlcpp::term_t result = perform_task<result_handler>(vm);

        lua::stack::push(vm.state(), result);

        guard.dismiss();
        return 1;
    }
    catch(std::exception & ex)
    {
        lua::stack::push(vm.state(), erlcpp::atom_t(ex.what()));
        exception_caught = true;
    }

    if (exception_caught) {
        lua_error(vm.state());
    }

    return 0;
}

extern "C"
{
    static int erlang_call(lua_State * vm)
    {
        int index = lua_upvalueindex(1);
        assert(lua_islightuserdata(vm, index));
        void * data = lua_touserdata(vm, index);
        assert(data);
        return erlang_call(*static_cast<vm_t*>(data));
    }

    static const struct luaL_Reg erlang_lib[] =
    {
        {"call", erlang_call},
        {NULL, NULL}
    };
}

vm_t::vm_t(erlcpp::lpid_t const& pid)
    : pid_(pid)
    , luastate_(luaL_newstate(), lua_close)
{
	stack_guard_t guard(*this);

//	char ff[256] = {0,};
//	getcwd(ff, 256);
//
//	printf("%s\n", "11111111111111111111111");
//	printf("%s\n", ff);

	void* handle = dlopen("/usr/local/lib/libluajit-5.1.so", RTLD_NOW | RTLD_GLOBAL); 	
	//assert(handle != NULL);

	luaL_openlibs(luastate_.get());
	luaopen_debug(luastate_.get());

	luaopen_cjson(luastate_.get());
	luaopen_cjson_safe(luastate_.get());

	luaopen_LuaXML_lib(luastate_.get());

	luaopen_lanes_core(luastate_.get());

	mongo_bsontypes_register(luastate_.get());
	mongo_connection_register(luastate_.get());
	mongo_replicaset_register(luastate_.get());
	mongo_cursor_register(luastate_.get());
	mongo_query_register(luastate_.get());
	mongo_gridfs_register(luastate_.get());
	mongo_gridfile_register(luastate_.get());
	mongo_gridfschunk_register(luastate_.get());

	luaopen_socket_core(luastate_.get());	
	
	luaopen_protobuf_c(luastate_.get());

	lua_newtable(luastate_.get());

	lua_pushstring(luastate_.get(), "call");
	lua_pushlightuserdata(luastate_.get(), this);
	lua_pushcclosure(luastate_.get(), erlang_call, 1);
	
	lua_settable(luastate_.get(), -3);

	lua_setglobal(luastate_.get(), "erlang");
}

vm_t::~vm_t()
{
//     enif_fprintf(stderr, "*** destruct the vm\n");
}

/////////////////////////////////////////////////////////////////////////////

boost::shared_ptr<vm_t> vm_t::create(ErlNifResourceType* res_type, erlcpp::lpid_t const& pid)
{
    enif_fprintf(stdout, "vm_t create------------------------------------------------------------------\n");
    void * buf = enif_alloc_resource(res_type, sizeof(vm_t));
    // TODO: may leak, need to guard agaist
    boost::shared_ptr<vm_t> result(new (buf) vm_t(pid), enif_release_resource);

    if(enif_thread_create(NULL, &result->tid_, vm_t::thread_run, result.get(), NULL) != 0) {
        result.reset();
    }

    return result;
}

void vm_t::destroy(ErlNifEnv* env, void* obj)
{
    static_cast<vm_t*>(obj)->stop();
    static_cast<vm_t*>(obj)->~vm_t();
}

void vm_t::run()
{
    try
    {
        for(;;)
        {
            perform_task<call_handler>(*this);
			dispatch(*this);
		}
    }
    catch(quit_tag) {}
    catch(std::exception & ex)
    {
        enif_fprintf(stderr, "*** exception in vm thread: %s\n", ex.what());
    }
    catch(...) {}
}

void vm_t::stop()
{
    queue_.push(tasks::quit_t());
    enif_thread_join(tid_, NULL);
};

void vm_t::add_task(task_t const& task)
{
    queue_.push(task);
}

vm_t::task_t vm_t::get_task()
{
    return queue_.pop();
}

lua_State* vm_t::state()
{
    return luastate_.get();
}

lua_State const* vm_t::state() const
{
    return luastate_.get();
}

void* vm_t::thread_run(void * vm)
{
    static_cast<vm_t*>(vm)->run();
    return 0;
}

/////////////////////////////////////////////////////////////////////////////

}
