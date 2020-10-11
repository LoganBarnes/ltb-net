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

// external
#include <grpc++/server_context.h>

// standard
#include <functional>

namespace ltb::net {
namespace detail {

template <typename Response>
struct AsyncUnaryWriterData {
    AsyncUnaryWriterData() : response(&context) {}

    grpc::ServerContext                            context;
    grpc_impl::ServerAsyncResponseWriter<Response> response;
};

} // namespace detail

template <typename Response>
struct AsyncUnaryWriter {
public:
    explicit AsyncUnaryWriter(std::weak_ptr<detail::AsyncUnaryWriterData<Response>> data, void* tag);

    auto cancel() -> void;
    auto finish(Response response, grpc::Status status) -> void;

private:
    std::weak_ptr<detail::AsyncUnaryWriterData<Response>> data_;
    void*                                                 tag_;
};

template <typename Service, typename Request, typename Response>
struct ServerCallbacks {
    using UnaryResponseCallback = std::function<void(Request const&, AsyncUnaryWriter<Response>)>;
};

template <typename Response>
AsyncUnaryWriter<Response>::AsyncUnaryWriter(std::weak_ptr<detail::AsyncUnaryWriterData<Response>> data, void* tag)
    : data_(std::move(data)), tag_(tag) {}

template <typename Response>
auto AsyncUnaryWriter<Response>::cancel() -> void {
    if (auto data = data_.lock()) {
        data->context.TryCancel();
    }
}

template <typename Response>
auto AsyncUnaryWriter<Response>::finish(Response response, grpc::Status status) -> void {
    if (auto data = data_.lock()) {
        data->response.Finish(response, status, tag_);
    }
}

} // namespace ltb::net
