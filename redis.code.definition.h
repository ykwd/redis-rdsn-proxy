# pragma once
# include <dsn/service_api_cpp.h>
# include "redis.types.h"

namespace redisproxy { 
    // define your own thread pool using DEFINE_THREAD_POOL_CODE(xxx)
    // define RPC task code for service 'redis'
    DEFINE_TASK_CODE_RPC(RPC_REDIS_REDIS_WRITE, TASK_PRIORITY_COMMON, ::dsn::THREAD_POOL_DEFAULT)
    DEFINE_TASK_CODE_RPC(RPC_REDIS_REDIS_READ, TASK_PRIORITY_COMMON, ::dsn::THREAD_POOL_DEFAULT)
    // test timer task code
    DEFINE_TASK_CODE(LPC_REDIS_TEST_TIMER, TASK_PRIORITY_COMMON, ::dsn::THREAD_POOL_DEFAULT)
} 
