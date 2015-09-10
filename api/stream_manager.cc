/*
 * Copyright 2015 Cloudius Systems
 */

#include "stream_manager.hh"
#include "streaming/stream_manager.hh"
#include "streaming/stream_result_future.hh"
#include "api/api-doc/stream_manager.json.hh"
#include <vector>

namespace api {

namespace hs = httpd::stream_manager_json;

static void set_summaries(const std::vector<streaming::stream_summary>& from,
        json::json_list<hs::stream_summary>& to) {
    for (auto sum : from) {
        hs::stream_summary res;
        res.cf_id = boost::lexical_cast<std::string>(sum.cf_id);
        res.files = sum.files;
        res.total_size = sum.total_size;
        to.push(res);
    }
}

static hs::progress_info get_progress_info(const streaming::progress_info& info) {
    hs::progress_info res;
    res.current_bytes = info.current_bytes;
    res.direction = info.dir;
    res.file_name = info.file_name;
    res.peer = boost::lexical_cast<std::string>(info.peer);
    res.session_index = info.session_index;
    res.total_bytes = info.total_bytes;
    return res;
}

static void set_files(const std::map<sstring, streaming::progress_info>& from,
        json::json_list<hs::progress_info_mapper>& to) {
    for (auto i : from) {
        hs::progress_info_mapper m;
        m.key = i.first;
        m.value = get_progress_info(i.second);
        to.push(m);
    }
}

static hs::stream_state get_state(
        streaming::stream_result_future& result_future) {
    hs::stream_state state;
    state.description = result_future.description;
    state.plan_id = result_future.plan_id.to_sstring();
    for (auto info : result_future.get_coordinator().get()->get_all_session_info()) {
        hs::stream_info si;
        si.peer = boost::lexical_cast<std::string>(info.peer);
        si.session_index = info.session_index;
        si.state = info.state;
        si.connecting = boost::lexical_cast<std::string>(info.connecting);
        set_summaries(info.receiving_summaries, si.receiving_summaries);
        set_summaries(info.sending_summaries, si.sending_summaries);
        set_files(info.receiving_files, si.receiving_files);
        set_files(info.sending_files, si.sending_files);
    }
    return state;
}

void set_stream_manager(http_context& ctx, routes& r) {
    hs::get_current_streams.set(r,
            [] (std::unique_ptr<request> req) {
                return streaming::get_stream_manager().map_reduce0([](streaming::stream_manager& stream) {
                    std::vector<hs::stream_state> res;
                    for (auto i : stream.get_initiated_streams()) {
                        res.push_back(get_state(*i.second.get()));
                    }
                    for (auto i : stream.get_receiving_streams()) {
                        res.push_back(get_state(*i.second.get()));
                    }
                    return res;
                }, std::vector<hs::stream_state>(),concat<hs::stream_state>).
                then([](const std::vector<hs::stream_state>& res) {
                    return make_ready_future<json::json_return_type>(res);
                });
            });
}

}
