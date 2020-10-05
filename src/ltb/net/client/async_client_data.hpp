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
#include <grpc++/client_context.h>

// standard
#include <deque>
#include <functional>

namespace ltb::net {

enum class ClientConnectionState {
    NoHostSpecified,
    InterprocessServerAlwaysConnected,
    NotConnected,
    AttemptingToConnect,
    Connected,
    RecoveringFromFailure,
    Shutdown,
};

enum class CallImmediately {
    Yes,
    No,
};

class AsyncClientData {
public:
    explicit AsyncClientData(grpc::CompletionQueue& completion_queue, std::weak_ptr<grpc::Channel> channel);
    virtual ~AsyncClientData();

    virtual auto process_callbacks(bool completed_successfully) -> bool = 0;
    virtual auto cancel() -> void                                       = 0;

    auto item_queued() -> bool;

protected:
    grpc::CompletionQueue&       completion_queue_;
    std::weak_ptr<grpc::Channel> channel_;
    bool                         item_queued_ = false;
};

using StatusCallback = std::function<void(grpc::Status)>;
using ErrorCallback  = std::function<void(ltb::util::Error)>;
template <typename Response>
using ResponseCallback = std::function<void(Response)>;

class AsyncClientRpcCallData : public AsyncClientData {
public:
    explicit AsyncClientRpcCallData(grpc::CompletionQueue&       completion_queue,
                                    std::weak_ptr<grpc::Channel> channel,
                                    StatusCallback               status_callback,
                                    ErrorCallback                error_callback);

    virtual ~AsyncClientRpcCallData();

protected:
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    grpc::ClientContext context_;

    // Storage for the status of the RPC upon completion.
    grpc::Status status_;

    StatusCallback status_callback_ = nullptr;
    ErrorCallback  error_callback_  = nullptr;
};

template <typename Stub, typename Request, typename Response>
using UnaryCallPtr = auto (Stub::*)(grpc::ClientContext*, Request const&, grpc::CompletionQueue*)
                         -> std::unique_ptr<grpc_impl::ClientAsyncResponseReader<Response>>;

template <typename Stub, typename Request, typename Response>
using ClientStreamCallPtr = auto (Stub::*)(grpc::ClientContext*, Response*, grpc::CompletionQueue*, void*)
                                -> std::unique_ptr<grpc_impl::ClientAsyncWriter<Request>>;

template <typename Response>
class AsyncClientUnaryCallData : public AsyncClientRpcCallData {
public:
    template <typename Stub, typename Request>
    explicit AsyncClientUnaryCallData(Stub&                                 stub,
                                      UnaryCallPtr<Stub, Request, Response> unary_call_ptr,
                                      Request const&                        request,
                                      grpc::CompletionQueue&                completion_queue,
                                      std::weak_ptr<grpc::Channel>          channel,
                                      ResponseCallback<Response>            response_callback,
                                      StatusCallback                        status_callback,
                                      ErrorCallback                         error_callback);

    ~AsyncClientUnaryCallData() override = default;

    auto process_callbacks(bool completed_successfully) -> bool override;
    auto cancel() -> void override;

private:
    Response                                                        response_;
    std::unique_ptr<grpc_impl::ClientAsyncResponseReader<Response>> response_reader_;
    ResponseCallback<Response>                                      response_callback_;
};

template <typename Request, typename Response>
class AsyncClientClientStreamCallData : public AsyncClientRpcCallData {
public:
    template <typename Stub>
    explicit AsyncClientClientStreamCallData(Stub&                                        stub,
                                             ClientStreamCallPtr<Stub, Request, Response> client_stream_call_ptr,
                                             grpc::CompletionQueue&                       completion_queue,
                                             std::weak_ptr<grpc::Channel>                 channel,
                                             ResponseCallback<Response>                   response_callback,
                                             StatusCallback                               status_callback,
                                             ErrorCallback                                error_callback);

    ~AsyncClientClientStreamCallData() override = default;

    auto process_callbacks(bool completed_successfully) -> bool override;
    auto cancel() -> void override;

private:
    std::deque<Request>                                    request_deque_;
    std::unique_ptr<grpc_impl::ClientAsyncWriter<Request>> request_writer_;
    Response                                               response_;
    ResponseCallback<Response>                             response_callback_;
};

template <typename Response>
template <typename Stub, typename Request>
AsyncClientUnaryCallData<Response>::AsyncClientUnaryCallData(Stub&                                 stub,
                                                             UnaryCallPtr<Stub, Request, Response> unary_call_ptr,
                                                             Request const&                        request,
                                                             grpc::CompletionQueue&                completion_queue,
                                                             std::weak_ptr<grpc::Channel>          channel,
                                                             ResponseCallback<Response>            response_callback,
                                                             StatusCallback                        status_callback,
                                                             ErrorCallback                         error_callback)
    : AsyncClientRpcCallData(completion_queue,
                             std::move(channel),
                             std::move(status_callback),
                             std::move(error_callback)),
      response_callback_(std::move(response_callback)) {

    if (channel_.lock()) {
        response_reader_ = (stub.*unary_call_ptr)(&context_, request, &completion_queue_);
        response_reader_->Finish(&response_, &status_, this);
        item_queued_ = true;
    }
}

template <typename Response>
auto AsyncClientUnaryCallData<Response>::process_callbacks(bool completed_successfully) -> bool {
    item_queued_ = false;
    if (completed_successfully) {
        if (response_callback_) {
            response_callback_(response_);
        }
        if (status_callback_) {
            status_callback_(status_);
        }
    } else {
        if (error_callback_) {
            error_callback_(LTB_MAKE_ERROR("Rpc could not complete."));
        }
    }
    return true;
}

template <typename Response>
auto AsyncClientUnaryCallData<Response>::cancel() -> void {
    context_.TryCancel();
}

template <typename Request, typename Response>
template <typename Stub>
AsyncClientClientStreamCallData<Request, Response>::AsyncClientClientStreamCallData(
    Stub&                                        stub,
    ClientStreamCallPtr<Stub, Request, Response> client_stream_call_ptr,
    grpc::CompletionQueue&                       completion_queue,
    std::weak_ptr<grpc::Channel>                 channel,
    ResponseCallback<Response>                   response_callback,
    StatusCallback                               status_callback,
    ErrorCallback                                error_callback)
    : AsyncClientRpcCallData(completion_queue,
                             std::move(channel),
                             std::move(status_callback),
                             std::move(error_callback)),
      response_callback_(std::move(response_callback)) {

    if (channel_.lock()) {
        request_writer_ = (stub.*client_stream_call_ptr)(&context_, &response_, &completion_queue_, this);
        item_queued_    = true;
    }
}

template <typename Request, typename Response>
auto AsyncClientClientStreamCallData<Request, Response>::process_callbacks(bool /*completed_successfully*/) -> bool {
    item_queued_ = false;
    return false;
}

template <typename Request, typename Response>
auto AsyncClientClientStreamCallData<Request, Response>::cancel() -> void {
    context_.TryCancel();
}

} // namespace ltb::net
