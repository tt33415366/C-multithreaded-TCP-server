#include "connectionhandler.h"
#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <iostream>
#include <pthread.h>

ConnectionHandler::ConnectionHandler(int fd): fd(fd) {
        assert(!this->m_thread.joinable());

        // creating thread that handles received messages
        this->m_thread = std::thread([this]() { this->threadFunc(); });
        pthread_setname_np(this->m_thread.native_handle(), "ConnectionHandler");
}

ConnectionHandler::~ConnectionHandler() {
        this->stop();
}

void ConnectionHandler::stop() {
        if (this->m_thread.joinable()) {
                this->m_thread.join();
        }
}

void ConnectionHandler::threadFunc() {
        while (!this->m_terminate) {
                std::string msg = this->readMessage();
                std::cout << "Received message: " << msg << std::endl;
                this->sendMessage("Thank you for your message " + msg);
        }
}

std::string ConnectionHandler::readMessage() {
        std::string msg(1024, '\0'); // buffer with 1024 length which is filled with
                                     //  NULL character
        int readBytes = recv(this->fd, (void *)msg.data(), msg.size(), 0);
        if (readBytes < 1) {
                std::cout << "Error in readMessage, readBytes: " << readBytes << std::endl;
                return "";
        }

        return msg;
}

void ConnectionHandler::sendMessage(const std::string& msg) {
        int n = send(this->fd, msg.c_str(), msg.size(), 0);
        if (n != static_cast<int>(msg.size())) {
                std::cout << "Error while sending message, message size: " << msg.size() << " bytes sent: " << n << std::endl;
        }
}

void ConnectionHandler::terminate() {
        this->m_terminate = true;
}
