# pragma once
# include "redis.code.definition.h"
# include <iostream>


namespace redisproxy { 
class redis_client 
    : public virtual ::dsn::clientlet
{
public:
    redis_client(::dsn::rpc_address server) { _server = server; }
    redis_client() { }
    virtual ~redis_client() {}
    
 
    // ---------- call RPC_REDIS_REDIS_WRITE ------------
    // - synchronous 
    std::pair< ::dsn::error_code, std::string> write_sync(
        const std::string& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0), 
        uint64_t hash = 0,
        dsn::optional< ::dsn::rpc_address> server_addr = dsn::none
        )
    {
        return ::dsn::rpc::wait_and_unwrap< std::string>(
            ::dsn::rpc::call(
                server_addr.unwrap_or(_server),
                RPC_REDIS_REDIS_WRITE,
                args,
                nullptr,
                dsn::empty_callback,
                hash,
                timeout,
                0
                )
            );
    }
    
    // - asynchronous with on-stack std::string and std::string  
    template<typename TCallback>
    ::dsn::task_ptr write(
        const std::string& args,
        TCallback&& callback,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
        int reply_hash = 0,
        uint64_t hash = 0,
        dsn::optional< ::dsn::rpc_address> server_addr = dsn::none
        )
    {
        return ::dsn::rpc::call(
                    server_addr.unwrap_or(_server), 
                    RPC_REDIS_REDIS_WRITE, 
                    args,
                    this,
                    std::forward<TCallback>(callback),
                    hash, 
                    timeout, 
                    reply_hash
                    );
    }
 
    // ---------- call RPC_REDIS_REDIS_READ ------------
    // - synchronous 
    std::pair< ::dsn::error_code, std::string> read_sync(
        const std::string& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0), 
        uint64_t hash = 0,
        dsn::optional< ::dsn::rpc_address> server_addr = dsn::none
        )
    {
        return ::dsn::rpc::wait_and_unwrap< std::string>(
            ::dsn::rpc::call(
                server_addr.unwrap_or(_server),
                RPC_REDIS_REDIS_READ,
                args,
                nullptr,
                dsn::empty_callback,
                hash,
                timeout,
                0
                )
            );
    }
    
    // - asynchronous with on-stack std::string and std::string  
    template<typename TCallback>
    ::dsn::task_ptr read(
        const std::string& args,
        TCallback&& callback,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
        int reply_hash = 0,
        uint64_t hash = 0,
        dsn::optional< ::dsn::rpc_address> server_addr = dsn::none
        )
    {
        return ::dsn::rpc::call(
                    server_addr.unwrap_or(_server), 
                    RPC_REDIS_REDIS_READ, 
                    args,
                    this,
                    std::forward<TCallback>(callback),
                    hash, 
                    timeout, 
                    reply_hash
                    );
    }

private:
    ::dsn::rpc_address _server;
};

} 