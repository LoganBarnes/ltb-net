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
#include "async_server_data.hpp"

// external
#include <grpc++/server.h>
#include <grpc++/server_builder.h>

// standard
#include <functional>
#include <unordered_map>

namespace ltb::net {

template <typename Service>
class AsyncServer {
public:
    explicit AsyncServer(std::string const& host_address);

    auto grpc_server() -> grpc::Server&;

    /// \brief Blocks the current thread.
    auto run() -> void;

    auto shutdown() -> void;

    template <typename Response, typename Request>
    auto unary_rpc(UnaryAsyncRpc<Service, Request, Response>                                   unary_call_ptr,
                   typename ServerCallbacks<Service, Request, Response>::UnaryResponseCallback on_response = nullptr)
        -> void;

private:
    std::mutex                                   mutex_;
    Service                                      service_;
    std::unique_ptr<grpc::ServerCompletionQueue> completion_queue_;
    std::unique_ptr<grpc::Server>                server_;

    std::unordered_map<detail::AsyncServerRpcCallData*, std::unique_ptr<detail::AsyncServerRpcCallData>> rpc_call_data_;
};

template <typename Service>
AsyncServer<Service>::AsyncServer(std::string const& host_address) {
    std::lock_guard lock(mutex_);

    grpc::ServerBuilder builder;
    if (!host_address.empty()) {
        // std::cout << "S: " << host_address << std::endl;
        builder.AddListeningPort(host_address, grpc::InsecureServerCredentials());
    }
    builder.RegisterService(&service_);
    completion_queue_ = builder.AddCompletionQueue();
    server_           = builder.BuildAndStart();
}

template <typename Service>
auto AsyncServer<Service>::grpc_server() -> grpc::Server& {
    return *server_;
}

template <typename Service>
auto AsyncServer<Service>::run() -> void {
    void* raw_tag                = {};
    bool  completed_successfully = {};

    while (completion_queue_->Next(&raw_tag, &completed_successfully)) {
        std::lock_guard lock(mutex_);

        auto* call_data = static_cast<detail::AsyncServerRpcCallData*>(raw_tag);

        if (completed_successfully) {
            if (auto new_call_data = call_data->process_callbacks()) {
                auto* raw_new_call_data = new_call_data.get();
                rpc_call_data_.emplace(raw_new_call_data, std::move(new_call_data));
            } else {
                rpc_call_data_.erase(call_data);
            }
        } else {
            rpc_call_data_.erase(call_data);
        }
    }
}

template <typename Service>
auto AsyncServer<Service>::shutdown() -> void {
    std::lock_guard lock(mutex_);
    server_->Shutdown();
    completion_queue_->Shutdown();
}

template <typename Service>
template <typename Response, typename Request>
auto AsyncServer<Service>::unary_rpc(
    UnaryAsyncRpc<Service, Request, Response>                                   unary_call_ptr,
    typename ServerCallbacks<Service, Request, Response>::UnaryResponseCallback on_response) -> void {
    std::lock_guard lock(mutex_);

    auto unary_call_data
        = std::make_unique<detail::AsyncServerUnaryCallData<Service, Request, Response>>(service_,
                                                                                         unary_call_ptr,
                                                                                         completion_queue_.get(),
                                                                                         std::move(on_response));

    auto raw_unary_call_data = unary_call_data.get();
    rpc_call_data_.emplace(raw_unary_call_data, std::move(unary_call_data));
}

} // namespace ltb::net
