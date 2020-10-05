// ///////////////////////////////////////////////////////////////////////////////////////
// LTB Networking
// Copyright (c) 2020 Logan Barnes - All Rights Reserved
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "async_client_data.hpp"
#include "ltb/net/tagger.hpp"
#include "ltb/util/atomic_data.hpp"

// external
#include <grpc++/channel.h>
#include <grpc++/create_channel.h>
#include <grpc++/server.h>

// standard
#include <functional>
#include <unordered_map>

namespace ltb::net {

template <typename Service>
class AsyncClient {
public:
    explicit AsyncClient(std::string const& host_address);
    explicit AsyncClient(grpc::Server& interprocess_server);

    using StateChangeCallback = std::function<void(ClientConnectionState)>;

    /// \brief Blocks the current thread.
    auto run() -> void;

    auto shutdown() -> void;

    auto on_state_change(StateChangeCallback callback, CallImmediately call_immediately) -> AsyncClient&;

    template <typename Response, typename Request>
    auto unary_rpc(UnaryCallPtr<typename Service::Stub, Request, Response> unary_call_ptr,
                   Request const&                                          request,
                   ResponseCallback<Response>                              on_response = nullptr,
                   StatusCallback                                          on_status   = nullptr,
                   ErrorCallback                                           on_error    = nullptr) -> void;

    template <typename Response, typename Request>
    auto client_stream_rpc(ClientStreamCallPtr<typename Service::Stub, Request, Response> client_stream_call_ptr,
                           ResponseCallback<Response>                                     on_response = nullptr,
                           StatusCallback                                                 on_status   = nullptr,
                           ErrorCallback                                                  on_error = nullptr) -> void;

private:
    std::mutex            channel_mutex_;
    grpc::CompletionQueue completion_queue_;

    struct Data {
        ClientTagger tagger;

        std::shared_ptr<grpc::Channel>          channel;
        std::unique_ptr<typename Service::Stub> stub;
        ClientConnectionState                   connection_state = ClientConnectionState::NoHostSpecified;

        std::unordered_map<AsyncClientRpcCallData*, std::unique_ptr<AsyncClientRpcCallData>> rpc_call_data;

        StateChangeCallback state_change_callback;
    } data_;

    //    util::AtomicData<Data> data_;
};

namespace detail {

auto to_client_connection_state(grpc_connectivity_state const& state) -> ClientConnectionState;
auto state_notification_deadline() -> std::chrono::time_point<std::chrono::system_clock>;

} // namespace detail

template <typename Service>
AsyncClient<Service>::AsyncClient(std::string const& host_address) {
    std::lock_guard channel_lock(channel_mutex_);
    // std::cout << "C: " << host_address << std::endl;
    data_.channel = grpc::CreateChannel(host_address, grpc::InsecureChannelCredentials());
    data_.stub    = Service::NewStub(data_.channel);

    auto grpc_state        = data_.channel->GetState(true);
    data_.connection_state = detail::to_client_connection_state(grpc_state);

    // Ask the channel to notify us when state changes by updating 'completion_queue'
    data_.channel->NotifyOnStateChange(grpc_state, detail::state_notification_deadline(), &completion_queue_, nullptr);
}

template <typename Service>
AsyncClient<Service>::AsyncClient(grpc::Server& interprocess_server) {
    std::lock_guard channel_lock(channel_mutex_);
    data_.channel          = interprocess_server.InProcessChannel({});
    data_.stub             = Service::NewStub(data_.channel);
    data_.connection_state = ClientConnectionState::InterprocessServerAlwaysConnected;
}

template <typename Service>
auto AsyncClient<Service>::run() -> void {

    void* raw_tag;
    bool  completed_successfully;

    while (completion_queue_.Next(&raw_tag, &completed_successfully)) {
        std::lock_guard channel_lock(channel_mutex_);
        std::cout << "C: " << (completed_successfully ? "Success: " : "Failure: ") << raw_tag << std::endl;

        if (raw_tag) {
            auto call_data = static_cast<AsyncClientRpcCallData*>(raw_tag);
            if (call_data->process_callbacks(completed_successfully)) {
                data_.rpc_call_data.erase(call_data);
            }
        } else if (completed_successfully && data_.channel) {
            auto grpc_state = data_.channel->GetState(true);
            auto state      = detail::to_client_connection_state(grpc_state);

            if (data_.connection_state != state && data_.state_change_callback) {
                data_.state_change_callback(state);
            }
            data_.connection_state = state;

            // Ask the channel to notify us when state changes by updating 'completion_queue_'
            data_.channel->NotifyOnStateChange(grpc_state,
                                               detail::state_notification_deadline(),
                                               &completion_queue_,
                                               nullptr);
        }
    }

    data_.connection_state = ClientConnectionState::NoHostSpecified;
    if (data_.state_change_callback) {
        data_.state_change_callback(data_.connection_state);
    }
}

template <typename Service>
auto AsyncClient<Service>::shutdown() -> void {
    std::lock_guard channel_lock(channel_mutex_);

    for (auto iter = data_.rpc_call_data.begin(); iter != data_.rpc_call_data.end(); /*no-op*/) {
        if (iter->second->item_queued()) {
            iter->second->cancel();
            ++iter;
        } else {
            iter = data_.rpc_call_data.erase(iter);
        }
    }

    //data_.rpc_call_data.clear();
    completion_queue_.Shutdown();
    data_.stub    = nullptr;
    data_.channel = nullptr;
}

template <typename Service>
auto AsyncClient<Service>::on_state_change(StateChangeCallback callback, CallImmediately call_immediately)
    -> AsyncClient& {
    std::lock_guard channel_lock(channel_mutex_);
    data_.state_change_callback = callback;
    if (call_immediately == CallImmediately::Yes) {
        data_.state_change_callback(data_.connection_state);
    }
    return *this;
}

template <typename Service>
template <typename Response, typename Request>
auto AsyncClient<Service>::unary_rpc(UnaryCallPtr<typename Service::Stub, Request, Response> unary_call_ptr,
                                     Request const&                                          request,
                                     ResponseCallback<Response>                              on_response,
                                     StatusCallback                                          on_status,
                                     ErrorCallback                                           on_error) -> void {
    std::lock_guard channel_lock(channel_mutex_);

    std::unique_ptr<AsyncClientRpcCallData> call_data;

    if (data_.channel) {
        call_data = std::make_unique<AsyncClientUnaryCallData<Response>>(*data_.stub,
                                                                         unary_call_ptr,
                                                                         request,
                                                                         completion_queue_,
                                                                         data_.channel,
                                                                         on_response,
                                                                         on_status,
                                                                         on_error);
    }

    if (call_data && call_data->item_queued()) {
        auto raw_call_data = call_data.get();
        data_.rpc_call_data.emplace(raw_call_data, std::move(call_data));
    }
}

template <typename Service>
template <typename Response, typename Request>
auto AsyncClient<Service>::client_stream_rpc(
    ClientStreamCallPtr<typename Service::Stub, Request, Response> client_stream_call_ptr,
    ResponseCallback<Response>                                     on_response,
    StatusCallback                                                 on_status,
    ErrorCallback                                                  on_error) -> void {

    std::lock_guard channel_lock(channel_mutex_);

    std::unique_ptr<AsyncClientRpcCallData> call_data;

    if (data_.channel) {
        call_data = std::make_unique<AsyncClientClientStreamCallData<Request, Response>>(*data_.stub,
                                                                                         client_stream_call_ptr,
                                                                                         completion_queue_,
                                                                                         data_.channel,
                                                                                         on_response,
                                                                                         on_status,
                                                                                         on_error);
    }

    if (call_data && call_data->item_queued()) {
        auto raw_call_data = call_data.get();
        data_.rpc_call_data.emplace(raw_call_data, std::move(call_data));
    }

    ///\todo: Return type for writing requests.
}

} // namespace ltb::net
