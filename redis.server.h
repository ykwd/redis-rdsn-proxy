# pragma once
# include "redis.code.definition.h"
# include <fstream>
#include "redisclient/redissyncclient.h"
#include <dsn/cpp/replicated_service_app.h>

namespace redisproxy {
    class redis_service
        :public dsn::replicated_service_app_type_1,
        public dsn::serverlet< redis_service>
    {
    public:
        redis_service() : serverlet< redis_service>("redis"), _app_info(nullptr), redisProcess(nullptr), port(0)
        {}
        virtual ~redis_service()
        {
            kill_redis();
        }

    protected:
        dsn::optional<RedisSyncClient> redis;
        boost::asio::io_service ioService;
        // all service handlers to be implemented further
        // RPC_REDIS_REDIS_WRITE 
        virtual void on_write(const std::string& args, dsn::rpc_replier< std::string>& reply)
        {
            dsn::service::zauto_lock _(_lock);
            //derror("writing ......................");
            reply(redis.unwrap().command(args).inspect());
        }
        // RPC_REDIS_REDIS_READ 
        virtual void on_read(const std::string& args, dsn::rpc_replier< std::string>& reply)
        {
            dsn::service::zauto_lock _(_lock);
            //derror("reading..........................");
            reply(redis.unwrap().command(args).inspect());
        }

        // RPC_REDIS_REDIS_BATCH_WRITE 
        virtual void on_batch_write(const batch_string& args, ::dsn::rpc_replier< batch_string>& reply)
        {
            dsn::service::zauto_lock _(_lock);
            batch_string resp;
            for (auto& arg : args.values)
            {
                resp.values.push_back(redis.unwrap().command(arg).inspect());
            }
            reply(resp);
        }

        // RPC_REDIS_REDIS_BATCH_READ 
        virtual void on_batch_read(const batch_string& args, ::dsn::rpc_replier< batch_string>& reply)
        {
            dsn::service::zauto_lock _(_lock);
            batch_string resp;
            for (auto& arg : args.values)
            {
                resp.values.push_back(redis.unwrap().command(arg).inspect());
            }
            reply(resp);
        }

    public:
        void open_service(dsn_gpid gpid)
        {
            this->register_async_rpc_handler(RPC_REDIS_REDIS_WRITE, "write", &redis_service::on_write, gpid);
            this->register_async_rpc_handler(RPC_REDIS_REDIS_READ, "read", &redis_service::on_read, gpid);
            this->register_async_rpc_handler(RPC_REDIS_REDIS_BATCH_WRITE, "batch_write", &redis_service::on_batch_write, gpid);
            this->register_async_rpc_handler(RPC_REDIS_REDIS_BATCH_READ, "batch_read", &redis_service::on_batch_read, gpid);
        }

        void close_service(dsn_gpid gpid)
        {
            this->unregister_rpc_handler(RPC_REDIS_REDIS_WRITE, gpid);
            this->unregister_rpc_handler(RPC_REDIS_REDIS_READ, gpid);
            this->unregister_rpc_handler(RPC_REDIS_REDIS_BATCH_WRITE, gpid);
            this->unregister_rpc_handler(RPC_REDIS_REDIS_BATCH_READ, gpid);
            redis.reset();
        }

        dsn::service::zlock _lock;
        dsn_app_info*        _app_info;

        const char* data_dir() const
        {
            return _app_info->data_dir;
        }
        int64_t last_committed_decree() const
        {
            return _app_info->info.type1.last_committed_decree;
        }
        int64_t last_durable_decree() const
        {
            return _app_info->info.type1.last_durable_decree;
        }
        void    set_last_durable_decree(int64_t d) const
        {
            _app_info->info.type1.last_durable_decree = d;
        }


        dsn::error_code start(int /*argc*/, char** /*argv*/) override
        {
            _app_info = dsn_get_app_info_ptr(gpid());

            {
                dsn::service::zauto_lock l(_lock);
                set_last_durable_decree(0);
                auto dump_file_path = std::string(data_dir()) + "/dump.rdb";
                dsn::utils::filesystem::remove_path(dump_file_path);
                start_redis("dump.rdb");
            }
            open_service(gpid());
            return dsn::ERR_OK;
        }

        dsn::error_code stop(bool cleanup = false) override
        {

            dsn::service::zauto_lock _(_lock);
            kill_redis();
            close_service(gpid());

            {
                dsn::service::zauto_lock l(_lock);
                if (cleanup)
                {
                    dsn_get_current_app_data_dir(gpid());

                    if (!dsn::utils::filesystem::remove_path(data_dir()))
                    {
                        dassert(false, "Fail to delete directory %s.", data_dir());
                    }
                }
            }

            return dsn::ERR_OK;
        }

        dsn::error_code checkpoint() override {
            char name[256];
            sprintf(name, "%s/checkpoint.%" PRId64, data_dir(),
                last_committed_decree()
                );
            dsn::service::zauto_lock l(_lock);

            if (last_committed_decree() == last_durable_decree())
            {
                dassert(dsn::utils::filesystem::file_exists(name),
                    "checkpoint file %s is missing!",
                    name
                    );
                return dsn::ERR_OK;
            }

            redis.unwrap().command("save");
            auto r = dsn::utils::filesystem::rename_path(std::string(data_dir()) + "/dump.rdb", name);
            dassert(r, "");
            set_last_durable_decree(last_committed_decree());
            return dsn::ERR_OK;
        }

        dsn::error_code get_checkpoint(
            int64_t /*start*/,
            void*   /*learn_request*/,
            int     /*learn_request_size*/,
            /* inout */ app_learn_state& state
            ) override {
            if (last_durable_decree() > 0)
            {
                char name[256];
                sprintf(name, "%s/checkpoint.%" PRId64,
                    data_dir(),
                    last_durable_decree()
                    );

                state.from_decree_excluded = 0;
                state.to_decree_included = last_durable_decree();
                state.files.push_back(std::string(name));

                return dsn::ERR_OK;
            }
            else
            {
                state.from_decree_excluded = 0;
                state.to_decree_included = 0;
                return dsn::ERR_OBJECT_NOT_FOUND;
            }
        }
        HANDLE redisProcess;
        unsigned short port;
        void start_redis(const std::string& load_filename = "dump.rdb")
        {
            if (redis.is_some())
            {
                kill_redis();
            }
            auto config_file_path = std::string(data_dir()) + "/config.txt";
            {
                std::ofstream config_ofstream(config_file_path.c_str());
                config_ofstream << "dbfilename " << load_filename << std::endl;
                config_ofstream << "dir " << data_dir() << std::endl;
                port = dsn_random64(10000, 60000);
                config_ofstream << "port " << port;
            }
            auto command = "redis-server.exe " + config_file_path;
            derror("redis command: %s", command.c_str());
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            if (CreateProcessA(nullptr, LPSTR(command.c_str()), nullptr, nullptr, TRUE, CREATE_NEW_CONSOLE, nullptr,
                LPSTR(data_dir()), &si, &pi))
            {
                //succeed
                CloseHandle(pi.hThread);
                redisProcess = pi.hProcess;

                auto address = boost::asio::ip::address::from_string("127.0.0.1");
                redis.reset(ioService);
                std::string errmsg;
                auto r = redis.unwrap().connect(address, port, errmsg);
                dassert(r, "");
                derror("errmsg -> %s", errmsg.c_str());
            }
            else
            {
                //failed
                dassert(false, "");
            }
        }
        void kill_redis()
        {
            system(("TASKKILL /F /T /PID " + std::to_string(GetProcessId(redisProcess))).c_str());
            CloseHandle(redisProcess);
            redis.reset();
        }

        dsn::error_code apply_checkpoint(const dsn_app_learn_state& state, dsn_chkpt_apply_mode mode) override
        {

            dsn::service::zauto_lock _(_lock);
            if (mode == DSN_CHKPT_LEARN)
            {
                kill_redis();
                dsn::utils::filesystem::rename_path(state.files[0], std::string(data_dir()) + "/dump.rdb");
                start_redis();
                set_last_durable_decree(state.to_decree_included);
                return dsn::ERR_OK;
            }
            dassert(DSN_CHKPT_COPY == mode, "invalid mode %d", (int)mode);
            dassert(state.to_decree_included > last_durable_decree(), "checkpoint's decree is smaller than current");

            char name[256];
            sprintf(name, "%s/checkpoint.%" PRId64,
                data_dir(),
                state.to_decree_included
                );
            std::string lname(name);

            if (!dsn::utils::filesystem::rename_path(state.files[0], lname))
                return dsn::ERR_CHECKPOINT_FAILED;
            set_last_durable_decree(state.to_decree_included);
            return dsn::ERR_OK;
        }
    };

}