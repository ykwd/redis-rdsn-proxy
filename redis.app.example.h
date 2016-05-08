# pragma once
# include "redis.client.h"
# include "redis.client.perf.h"
# include "redis.server.h"

namespace redisproxy { 
// server app example
class redis_server_app : 
    public ::dsn::service_app
{
public:
    redis_server_app() {}

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        _redis_svc.open_service(gpid());
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        _redis_svc.close_service(gpid());
        return ::dsn::ERR_OK;
    }

private:
    redis_service _redis_svc;
};

// client app example
class redis_client_app : 
    public ::dsn::service_app, 
    public virtual ::dsn::clientlet
{
public:
    redis_client_app() {}
    
    ~redis_client_app() 
    {
        stop();
    }

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        if (argc < 1)
        {
            printf ("Usage: <exe> server-host:server-port or service-url\n");
            return ::dsn::ERR_INVALID_PARAMETERS;
        }

        // argv[1]: e.g., dsn://mycluster/simple-kv.instance0
        _server = ::dsn::url_host_address(argv[1]);
            
        _redis_client.reset(new redis_client(_server));
        _timer = ::dsn::tasking::enqueue_timer(LPC_REDIS_TEST_TIMER, this, [this]{on_test_timer();}, std::chrono::seconds(1));
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        _timer->cancel(true);
 
        _redis_client.reset();
        return ::dsn::ERR_OK;
    }

    void on_test_timer()
    {
        // test for service 'redis'
        {
            //sync:
            auto result = _redis_client->write_sync({});
            std::cout << "call RPC_REDIS_REDIS_WRITE end, return " << result.first.to_string() << std::endl;
            //async: 
            //_redis_client->write({});
           
        }
        {
            //sync:
            auto result = _redis_client->read_sync({});
            std::cout << "call RPC_REDIS_REDIS_READ end, return " << result.first.to_string() << std::endl;
            //async: 
            //_redis_client->read({});
           
        }
    }

private:
    ::dsn::task_ptr _timer;
    ::dsn::url_host_address _server;
    
    std::unique_ptr<redis_client> _redis_client;
};

class redis_perf_test_client_app :
    public ::dsn::service_app, 
    public virtual ::dsn::clientlet
{
public:
    redis_perf_test_client_app()
    {
        _redis_client = nullptr;
    }

    ~redis_perf_test_client_app()
    {
        stop();
    }

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        if (argc < 1)
            return ::dsn::ERR_INVALID_PARAMETERS;

        // argv[1]: e.g., dsn://mycluster/simple-kv.instance0
        _server = ::dsn::url_host_address(argv[1]);

        _redis_client = new redis_perf_test_client(_server);
        _redis_client->start_test("redis.perf-test.case.", 15);
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        if (_redis_client != nullptr)
        {
            delete _redis_client;
            _redis_client = nullptr;
        }
        
        return ::dsn::ERR_OK;
    }
    
private:
    redis_perf_test_client *_redis_client;
    ::dsn::rpc_address _server;
};

} 