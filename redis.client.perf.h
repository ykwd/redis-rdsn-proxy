# pragma once

# include "redis.client.h"
# include <boost/mpl/string.hpp>
# include <boost/mpl/vector.hpp>
# include <boost/fusion/iterator.hpp>
#include <boost/fusion/include/boost_tuple.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/include/for_each.hpp>

namespace redisproxy {
    inline std::string build_command(const std::list<std::string>& redis_cmd) {
        {
            std::string cmd = "*" + std::to_string(redis_cmd.size()) + "\r\n";

            for (const auto& cmd_part : redis_cmd)
                cmd += "$" + std::to_string(cmd_part.length()) + "\r\n" + cmd_part + "\r\n";

            return cmd;
        }
    }
    class redis_perf_test_client
        : public redis_client,
        public ::dsn::service::perf_client_helper
    {

    private:
        int _batch_size;

    public:
        redis_perf_test_client(
            ::dsn::rpc_address server)
            : redis_client(server)
        {
            constexpr auto rand_length = 12;

            auto rand_name = [rand_length](const char* prefix) -> auto
            {
                return[prefix, rand_length](int) -> std::list<std::string>
                {
                    int rand = dsn_random64(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
                    std::string ret(prefix);
                    ret += std::string(rand_length, '0');
                    for (auto i = 0; i < rand_length && rand != 0; i++)
                    {
                        *(ret.rbegin() + i) += rand % 10;
                        rand /= 10;
                    }
                    return{ ret };
                };
            };
            auto l = [](const char* literal) -> auto
            {
                return[literal](int)->std::list<std::string>
                {
                    return{ std::string(literal) };
                };
            };
            auto payload = [](int payload_bytes) -> std::list<std::string>
            {
                return{ std::string(payload_bytes, 'x') };
            };
            auto repeat = [](int num, auto&& f) -> auto
            {
                return[num, f = std::forward<decltype(f)>(f)](int payload_bytes)->std::list<std::string>
                {
                    std::list<std::string> result;
                    for (auto i = 0; i < num; i++)
                    {
                        result.splice(result.end(), f(payload_bytes));
                    }
                    return result;
                };
            };
            auto concat = [](auto&& ...fs) -> auto
            {
                return[fs...](int payload_bytes)->std::list<std::string> {
                    std::list<std::string> result;
                    auto _ = { (result.splice(result.end(), fs(payload_bytes)), dsn::none)... };
                    return result;
                };
            };
            auto write = [](const char* name, auto&& f)
            {
                return command
                {
                    name,
                    true,
                    std::forward<decltype(f)>(f)
                };
            };
            auto read = [](const char* name, auto&& f)
            {
                return command
                {
                    name,
                    false,
                    std::forward<decltype(f)>(f)
                };
            };
            cmds = {
                read("ping", l("ping")),
                write("set", concat(l("set"), rand_name("key:"), payload)),
                read("get", concat(l("get"), rand_name("key:"))),
                write("incr", concat(l("incr"), rand_name("counter:"))),
                write("lpush", concat(l("lpush"), l("mylist"), payload)),
                write("lpop", concat(l("lpop"), l("mylist"))),
                write("sadd", concat(l("sadd"), l("myset"), rand_name("element:"))),
                write("spop", concat(l("spop"), l("myset"))),
                write("lpush for lrange", concat(l("lpush"), l("mylist"), payload)),
                read("lrange_100", concat(l("lrange"), l("mylist"), l("0"), l("99"))),
                read("lrange_300", concat(l("lrange"), l("mylist"), l("0"), l("299"))),
                read("lrange_500", concat(l("lrange"), l("mylist"), l("0"), l("449"))),
                read("lrange_600", concat(l("lrange"), l("mylist"), l("0"), l("599"))),
                write("mset", concat(l("mset"), repeat(10, concat(rand_name("key:"), payload)))) };

            _batch_size = (int)dsn_config_get_value_uint64("apps.client.perf.redis", "max_batch_size", 1, "maximum batch size for perf test");
        }

        struct command
        {
            const char* name;
            bool is_write;
            std::function<std::list<std::string>(int)> f;
        };
        std::vector<command> cmds;

        template<typename F, typename ...T>
        void for_each(F&& f, T&&... t)
        {
            auto _ = { (f(std::forward<T>(t)), dsn::none)... };
        }

        void send_one(int payload_bytes, int key_space_size, const std::vector<double>& ratios) override
        {
            auto prob = (double)dsn_random32(0, 1000) / 1000.0;
            for (int i = 0; i < cmds.size(); i++)
            {
                auto& cc = cmds[i];
                if (prob <= ratios[i])
                {
                    auto hash = random64(0, 10000000) % key_space_size;
                    if (cc.is_write)
                    {
                        if (_batch_size == 1)
                        {
                            this->write(
                                build_command(cc.f(payload_bytes)),
                                [this, context = prepare_send_one()](dsn::error_code err, std::string&& resp)
                            {
                                end_send_one(context, err);
                            },
                                _timeout,
                                0,
                                hash
                                );
                        }
                        else
                        {
                            batch_string reqs;
                            for (int i = 0; i < _batch_size; i++)
                            {
                                reqs.values.push_back(build_command(cc.f(payload_bytes)));
                            }
                            this->batch_write(
                                reqs,
                                [this, context = prepare_send_one()](dsn::error_code err, std::string&& resp)
                            {
                                end_send_one(context, err);
                            },
                                _timeout,
                                0,
                                hash
                                );
                        }
                    }
                    else
                    {
                        if (_batch_size == 1)
                        {
                            this->read(
                                build_command(cc.f(payload_bytes)),
                                [this, context = prepare_send_one()](dsn::error_code err, std::string&& resp)
                            {
                                end_send_one(context, err);
                            },
                                _timeout
                                );
                        }
                        else
                        {
                            batch_string reqs;
                            for (int i = 0; i < _batch_size; i++)
                            {
                                reqs.values.push_back(build_command(cc.f(payload_bytes)));
                            }
                            this->batch_read(
                                reqs,
                                [this, context = prepare_send_one()](dsn::error_code err, std::string&& resp)
                            {
                                end_send_one(context, err);
                            },
                                _timeout,
                                0,
                                hash
                                );
                        }
                    }
                    break;
                }
            }
        }

    };

}