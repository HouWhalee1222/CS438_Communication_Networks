/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <fstream>

using namespace std;

#define PACKETLEN 4096

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
	int sockfd;  
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: ./http_client url\n");
	    exit(1);
	}

	// Parse url
	string url(argv[1]);
	size_t div1 = url.find("//") + 2; 
	size_t div2 = url.find("/", div1) + 1;
	string hostname = url.substr(div1, div2 - div1 - 1);
	string port = "80";
	size_t div3 = hostname.find(":");
	if (div3 != string::npos) {
		port = hostname.substr(div3 + 1);
		hostname = hostname.substr(0, div3);
	}
	string path_to_file = url.substr(div2);
	string request = "GET /" + path_to_file + " HTTP/1.1\r\n\r\n";
	// End parse url

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// Send the request to server
	if (send(sockfd, request.c_str(), request.size(), 0) == -1) {
		perror("send");
		exit(1);
	}

	ofstream output_file("output");
	int numbytes = 0;
	bool isHeader = true;
	do {
		char buffer[PACKETLEN + 1];
		numbytes = recv(sockfd, buffer, PACKETLEN, 0);
		if (numbytes == -1) {
	    	perror("recv");
			break;
		}
		buffer[numbytes] = '\0';

		// parse header
		if (isHeader) {
			string headerStr(buffer);
			size_t div = headerStr.find_first_of("\r\n");
			if (div != string::npos) headerStr = headerStr.substr(div + 4);
			output_file << headerStr;
			isHeader = false;
			continue;
		}
		
		output_file << buffer;
	} while (numbytes > 0);  // not use PACKETLEN as the condition, it is wrong!

	output_file.close();
	close(sockfd);

	return 0;
}
