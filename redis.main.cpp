// apps
# include "redis.app.example.h"

# include <dsn/cpp/replicated_service_app.h>

#define DSN_RUN_USE_SVCHOST

void dsn_app_registration_redis()
{

    dsn::register_app_with_type_1_replication_support< ::redisproxy::redis_service>("server");
    // register all possible service apps
    // dsn::register_app< ::redisproxy::redis_server_app>("server");
    dsn::register_app< ::redisproxy::redis_client_app>("client");
    dsn::register_app< ::redisproxy::redis_perf_test_client_app>("client.perf.redis");
}

# ifndef DSN_RUN_USE_SVCHOST

int main(int argc, char** argv)
{
    dsn_app_registration_redis();
    
    // specify what services and tools will run in config file, then run
    dsn_run(argc, argv, true);
    return 0;
}

# else

# include <dsn/internal/module_init.cpp.h>

MODULE_INIT_BEGIN(fuck)
    dsn_app_registration_redis();
MODULE_INIT_END

# endif
