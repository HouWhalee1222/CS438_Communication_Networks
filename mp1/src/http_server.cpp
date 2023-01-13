/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;

#define BACKLOG 10	 // how many pending connections queue will hold
#define PACKETLEN 4096

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	// Set Port
	if (argc != 2) {
	    fprintf(stderr,"usage: ./http_server port\n");
	    exit(1);
	}

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

			// receive header from client
			string response;
			int path_to_file_len = 0;
			char buffer[PACKETLEN];
			path_to_file_len = recv(new_fd, buffer, PACKETLEN, 0);
			if (path_to_file_len == -1) {
				perror("recv");
				exit(1);
			}

			// Decision on response code
			string buffer_str(buffer);
			size_t div1 = buffer_str.find("GET");
			size_t div2 = buffer_str.find(" HTTP/1.1");
			int file_name_len = -5 + div2;
			if (div1 == string::npos || div2 == string::npos || file_name_len <= 0) {
				response = "HTTP/1.1 400 Bad Request\r\n\r\n";
			} else {
				string path_to_file = buffer_str.substr(5, div2 - 5);
				ifstream request_file(path_to_file);
				if (request_file.good()) {
					stringstream file_strStream;
					file_strStream << request_file.rdbuf();
					string file_str = file_strStream.str();
					response = "HTTP/1.1 200 OK\r\n\r\n" + file_str;
					// response = file_str;
				} else {
					response = "HTTP/1.1 404 Not Found, Oh nooooooo~\r\n\r\n";
				}
			}

			// Send the response length - fixed size 16
			int response_len = response.size();
			
			// Send the response
			int send_len = 0;
			int send_byte = 0;
			while (send_len < response_len) {
				string send_str = response.substr(send_len, PACKETLEN);
				send_byte = send(new_fd, send_str.c_str(), send_str.size(), 0);
				if (send_byte == -1) {
					perror("send");
					exit(1);
				}
				send_len += send_byte;
			}
			
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

