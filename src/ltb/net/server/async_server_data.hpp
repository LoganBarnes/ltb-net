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
#include "async_server_rpc.hpp"
#include "async_server_unary_writer.hpp"
#include "ltb/net/tagger.hpp"

// standard
#include <functional>

namespace ltb::net {

template <typename Service, typename Request, typename Response>
using UnaryAsyncRpc = void (Service::*)(grpc::ServerContext*,
                                        Request*,
                                        grpc_impl::ServerAsyncResponseWriter<Response>*,
                                        grpc::CompletionQueue*,
                                        grpc::ServerCompletionQueue*,
                                        void* tag);
template <typename Service, typename Request, typename Response>
using ClientStreamAsyncRpc = void (Service::*)(grpc::ServerContext*,
                                               grpc_impl::ServerAsyncReader<Response, Request>*,
                                               grpc::CompletionQueue*,
                                               grpc::ServerCompletionQueue*,
                                               void* tag);
template <typename Service, typename Request, typename Response>
using ServerStreamAsyncRpc = void (Service::*)(grpc::ServerContext*,
                                               Request*,
                                               grpc_impl::ServerAsyncWriter<Response>*,
                                               grpc::CompletionQueue*,
                                               grpc::ServerCompletionQueue*,
                                               void* tag);

template <typename Service, typename Request, typename Response>
using BidirectionalStreamAsyncRpc = void (Service::*)(grpc::ServerContext*,
                                                      grpc_impl::ServerAsyncReaderWriter<Response, Request>*,
                                                      grpc::CompletionQueue*,
                                                      grpc::ServerCompletionQueue*,
                                                      void* tag);

namespace detail {

template <typename Service, typename Request, typename Response>
struct AsyncServerUnaryCallData : public AsyncServerRpc {

    explicit AsyncServerUnaryCallData(Service&                                                           service,
                                      UnaryAsyncRpc<Service, Request, Response>                          unary_call,
                                      grpc::ServerCompletionQueue*                                       queue,
                                      ServerTagger&                                                      tagger,
                                      typename ServerCallbacks<Service, Request, Response>::UnaryConnect on_connect,
                                      typename ServerCallbacks<Service, Request, Response>::Disconnect   on_disconnect);

    ~AsyncServerUnaryCallData() override = default;

    auto clone() -> std::unique_ptr<AsyncServerRpc> override;
    auto invoke_connection_callback() -> void override;
    auto invoke_disconnect_callback() -> void override;

private:
    Service&                                                           service_;
    UnaryAsyncRpc<Service, Request, Response>                          unary_call_;
    grpc::ServerCompletionQueue*                                       queue_;
    ServerTagger&                                                      tagger_;
    Request                                                            request_;
    std::shared_ptr<ServerAsyncResponseWriter<Response>>               writer_data_;
    typename ServerCallbacks<Service, Request, Response>::UnaryConnect on_connect_;
    typename ServerCallbacks<Service, Request, Response>::Disconnect   on_disconnect_;
};

template <typename Service, typename Request, typename Response>
AsyncServerUnaryCallData<Service, Request, Response>::AsyncServerUnaryCallData(
    Service&                                                           service,
    UnaryAsyncRpc<Service, Request, Response>                          unary_call,
    grpc::ServerCompletionQueue*                                       queue,
    ServerTagger&                                                      tagger,
    typename ServerCallbacks<Service, Request, Response>::UnaryConnect on_connect,
    typename ServerCallbacks<Service, Request, Response>::Disconnect   on_disconnect)
    : service_(service),
      unary_call_(unary_call),
      queue_(queue),
      tagger_(tagger),
      writer_data_(std::make_shared<ServerAsyncResponseWriter<Response>>()),
      on_connect_(std::move(on_connect)),
      on_disconnect_(std::move(on_disconnect)) {

    (service.*unary_call)(&writer_data_->context,
                          &request_,
                          &writer_data_->writer,
                          queue,
                          queue,
                          tagger_.make_tag(this, ServerTagLabel::NewRpc));
}

template <typename Service, typename Request, typename Response>
auto AsyncServerUnaryCallData<Service, Request, Response>::clone() -> std::unique_ptr<AsyncServerRpc> {
    return std::make_unique<AsyncServerUnaryCallData<Service, Request, Response>>(service_,
                                                                                  unary_call_,
                                                                                  queue_,
                                                                                  tagger_,
                                                                                  on_connect_,
                                                                                  on_disconnect_);
}

template <typename Service, typename Request, typename Response>
auto AsyncServerUnaryCallData<Service, Request, Response>::invoke_connection_callback() -> void {
    auto tag = tagger_.make_tag(this, ServerTagLabel::Done);
    if (on_connect_) {
        on_connect_(request_, AsyncServerUnaryWriter<Response>{writer_data_, tag});
    } else {
        writer_data_->writer.FinishWithError(grpc::Status{grpc::StatusCode::UNIMPLEMENTED, "RPC not implemented."},
                                             tag);
    }
}

template <typename Service, typename Request, typename Response>
auto AsyncServerUnaryCallData<Service, Request, Response>::invoke_disconnect_callback() -> void {
    if (on_disconnect_) {
        on_disconnect_(this);
    }
}

} // namespace detail
} // namespace ltb::net
