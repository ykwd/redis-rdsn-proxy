# pragma once
# include <dsn/service_api_cpp.h>
# include <dsn/cpp/serialization.h>


# include "thrift/redis_types.h" 

using namespace dsn;

namespace redisproxy { 
    GENERATED_TYPE_SERIALIZATION(batch_string, THRIFT)

} 