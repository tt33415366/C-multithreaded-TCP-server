#include "tcpserver.h"
#include <unistd.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cerrno>
#include "connectionhandler.h"

TCPServer::TCPServer():efd(-1) {
	this->start();
}

TCPServer::~TCPServer() {
	this->stop();
	if (this->efd != -1) {
		close(this->efd);
	}
}

void TCPServer::start() {
	assert(!this->m_thread.joinable());

	if (this->efd != -1) {
		close(this->efd);
	}

	this->efd = eventfd(0, 0);
	if (this->efd == -1) {
		std::cout << "eventfd() => -1, errno = " << errno << std::endl;
		return ;
	}

	// creates thread
	this->m_thread = std::thread([this]() { this->threadFunc(); });

	// sets name for thread
	pthread_setname_np(this->m_thread.native_handle(), "TCPServer");
}

void TCPServer::stop() {
	// writes to efd - it will be handled as stopping server thread
	uint64_t one = 1;
	auto ret = write(this->efd, &one, 8);
	if (ret == -1) {
		std::cout << "write => -1, errno = " << errno << std::endl;
	}
}

void TCPServer::join() {
	if (this->m_thread.joinable()) {
		this->m_thread.join();
	}
}

void TCPServer::threadFunc() {
	int sockfd;

	std::cout << "Listen on: " << PORT << std::endl;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		std::cout << "socket() => -1, errno = " << errno << std::endl;
		return ;
	}

	int reuseaddr = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
		std::cout << "setsockopt() => -1, errno = " << errno << std::endl;
	}

	struct sockaddr_in servaddr = {0, 0, 0, 0};
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);

	// binding to socket that will listen for new connections
	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		std::cout << "bind() => -1, errno = " << errno << std::endl;
		close(sockfd);
		return ;
	}

	// started listening, 50 pending connections can wait in a queue
	listen(sockfd, 50);

	// monitored file descriptors - at start there is efd and just created sockfd, 
	// POLLIN means we wait for read
	std::vector<struct pollfd> fds { {this->efd, POLLIN, 0}, {sockfd, POLLIN, 0}};
	std::unordered_map<int, ConnectionHandler> handlers;

	while (true) {
		const int TIMEOUT = 1000; // 1000ms
		int n = poll(fds.data(), fds.size(), TIMEOUT); // Checking if there was 
													   // any event on monitored 
													   // file descriptors
		if (n == -1 && errno != ETIMEDOUT && errno != EINTR) {
			std::cout << "poll() => -1, errno = " << errno << std::endl;
			break;
		}

		// n pending events
		if (n > 0) {
			if (fds[0].revents) { // handles server stop request (which is sent by
								  // TCPserver::stop())
				std::cout << "Received stop request" << std::endl;
				break;
			} else if (fds[1].revents) { // new client connected
				// accepting connection 
				int clientfd = accept(sockfd, NULL, NULL);
				std::cout << "New connection" << std::endl;
				if (clientfd != -1) {
					// insert new pollfd to monitor
					fds.push_back(pollfd {clientfd, POLLIN, 0});
					// creates ConnectionHandler object that will run in separate 
					// thread
					handlers.emplace(clientfd, clientfd);
				} else {
					std::cout << "accept => -1, errno = " << errno << std::endl;
				}

				// clearing revents
				fds[1].revents = 0;
			}

			// iterating all pollfds to check if anyone disconnected
			for (auto it = fds.begin() + 2; it != fds.end();) {
				char c;
				if (it->revents
						&& recv(it->fd, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
					// Checks if disconnected or
					std::cout << "Client disconnected" << std::endl;
					close(it->fd); // closing socket
					handlers.at(it->fd).terminate(); // terminating ConnectionHandler
													 // thread
					handlers.erase(it->fd);
					it = fds.erase(it);
				} else {
					++it;
				}
			}
		}
	}

	// cleaning section after receiving stop request
	for (auto it = fds.begin() + 1; it != fds.end(); it++) {
		close(it->fd);
		if (handlers.find(it->fd) != handlers.end()) {
			handlers.at(it->fd).terminate();
		}
	}

	std::cout << "TCP server stoped" << std::endl;
}
