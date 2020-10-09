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
#include "ltb/util/error.hpp"

// external
#include <grpc++/server_context.h>

// standard
#include <functional>

namespace ltb::net {

//using StatusCallback = std::function<void(grpc::Status)>;
//using ErrorCallback  = std::function<void(ltb::util::Error)>;
//template <typename Response>
//using ResponseCallback = std::function<void(Response)>;

template <typename Service, typename Request, typename Response>
using UnaryAsyncRpc = void (Service::*)(grpc::ServerContext*,
                                        Request*,
                                        grpc_impl::ServerAsyncResponseWriter<Response>*,
                                        grpc::CompletionQueue*,
                                        grpc::ServerCompletionQueue*,
                                        void* tag);

struct AsyncServerRpcCallData {
    virtual ~AsyncServerRpcCallData() = default;

    virtual auto process_callbacks() -> void = 0;
};

template <typename Request, typename Response>
struct AsyncClientUnaryCallData : public AsyncServerRpcCallData {

    template <typename Service>
    explicit AsyncClientUnaryCallData(Service&                                  service,
                                      UnaryAsyncRpc<Service, Request, Response> unary_call,
                                      grpc::ServerCompletionQueue*              queue);

    ~AsyncClientUnaryCallData() override = default;

    auto process_callbacks() -> void override;

    grpc::ServerContext                            context;
    Request                                        request;
    grpc_impl::ServerAsyncResponseWriter<Response> response;
};

template <typename Request, typename Response>
template <typename Service>
AsyncClientUnaryCallData<Request, Response>::AsyncClientUnaryCallData(
    Service& service, UnaryAsyncRpc<Service, Request, Response> unary_call, grpc::ServerCompletionQueue* queue) {
    (service.*unary_call)(&context, &request, &response, queue, queue, this);
}

template <typename Request, typename Response>
auto AsyncClientUnaryCallData<Request, Response>::process_callbacks() -> void {
    //    if (response_callback) {
    //        response_callback(response);
    //    }
}

} // namespace ltb::net
