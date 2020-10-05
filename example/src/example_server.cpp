// ///////////////////////////////////////////////////////////////////////////////////////
//                                                                           |________|
//  Copyright (c) 2020 CloudNC Ltd - All Rights Reserved                        |  |
//                                                                              |__|
//        ____                                                                .  ||
//       / __ \                                                               .`~||$$$$
//      | /  \ \         /$$$$$$  /$$                           /$$ /$$   /$$  /$$$$$$$
//      \ \ \ \ \       /$$__  $$| $$                          | $$| $$$ | $$ /$$__  $$
//    / / /  \ \ \     | $$  \__/| $$  /$$$$$$  /$$   /$$  /$$$$$$$| $$$$| $$| $$  \__/
//   / / /    \ \__    | $$      | $$ /$$__  $$| $$  | $$ /$$__  $$| $$ $$ $$| $$
//  / / /      \__ \   | $$      | $$| $$  \ $$| $$  | $$| $$  | $$| $$  $$$$| $$
// | | / ________ \ \  | $$    $$| $$| $$  | $$| $$  | $$| $$  | $$| $$\  $$$| $$    $$
//  \ \_/ ________/ /  |  $$$$$$/| $$|  $$$$$$/|  $$$$$$/|  $$$$$$$| $$ \  $$|  $$$$$$/
//   \___/ ________/    \______/ |__/ \______/  \______/  \_______/|__/  \__/ \______/
//
// ///////////////////////////////////////////////////////////////////////////////////////
#include "example_server.hpp"

#include <grpc++/server_builder.h>
#include <thread>

namespace ltb::example {

ExampleService::~ExampleService() {}

grpc::Status
ExampleService::DispatchAction(grpc::ServerContext* /*context*/, Action const* request, util::Result* response) {
    std::cout << "S: Action: " << request->ShortDebugString() << std::endl;
    response->mutable_success();
    return grpc::Status::OK;
}

grpc::Status ExampleService::PokeUser(grpc::ServerContext* /*context*/,
                                      grpc::ServerReader<User::Id>* reader,
                                      util::Result*                 response) {
    User::Id request;
    while (reader->Read(&request)) {
    }
    response->mutable_success();
    return grpc::Status::OK;
}

grpc::Status ExampleService::SearchMessages(grpc::ServerContext*                 context,
                                            google::protobuf::StringValue const* request,
                                            grpc::ServerWriter<UserMessage>*     writer) {
    return Service::SearchMessages(context, request, writer);
}

grpc::Status ExampleService::GetMessages(grpc::ServerContext*                                         context,
                                         grpc::ServerReaderWriter<ChatMessageResult, ChatMessage_Id>* stream) {
    return Service::GetMessages(context, stream);
}

grpc::Status ExampleService::GetUpdates(grpc::ServerContext*           context,
                                        google::protobuf::Empty const* request,
                                        grpc::ServerWriter<Update>*    writer) {
    return Service::GetUpdates(context, request, writer);
}

ExampleServer::ExampleServer(std::string const& host_address) {
    grpc::ServerBuilder builder;
    if (!host_address.empty()) {
        // std::cout << "S: " << host_address << std::endl;
        builder.AddListeningPort(host_address, grpc::InsecureServerCredentials());
    }
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
}

auto ExampleServer::grpc_server() -> grpc::Server& {
    return *server_;
}

auto ExampleServer::shutdown() -> void {
    server_->Shutdown();
}

} // namespace ltb::example
