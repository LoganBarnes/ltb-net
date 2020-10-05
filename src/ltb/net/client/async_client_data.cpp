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
#include "async_client_data.hpp"

namespace ltb::net {

AsyncClientData::AsyncClientData(grpc::CompletionQueue& completion_queue, std::weak_ptr<grpc::Channel> channel)
    : completion_queue_(completion_queue), channel_(std::move(channel)) {}

AsyncClientData::~AsyncClientData() = default;

auto AsyncClientData::item_queued() -> bool {
    return item_queued_;
}

AsyncClientRpcCallData::AsyncClientRpcCallData(grpc::CompletionQueue&       completion_queue,
                                               std::weak_ptr<grpc::Channel> channel,
                                               StatusCallback               status_callback,
                                               ErrorCallback                error_callback)
    : AsyncClientData(completion_queue, std::move(channel)),
      status_callback_(status_callback),
      error_callback_(error_callback) {}

AsyncClientRpcCallData::~AsyncClientRpcCallData() = default;

} // namespace ltb::net
