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
#include <assert.h>

#include <arpa/inet.h>

#define LISTENPORT "4950"	// the port users will be connecting to

#define MAXBUFLEN 100

#define PORT "3490" // the port client will be connecting to 
// #define PORT "4950"
#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
// The funtion takes the pointer to a network struct and figures out if it is used for IPv4 or IPv6 and 
// returns the address that is used, which can then be used by other functions.
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];


	// from listener.c

	//struct sockaddr_storage their_addr;
	//socklen_t addr_len;

	//int sockfd_listener, numbytes_listener;  
	//char buf_listener[MAXDATASIZE];
	//struct addrinfo hints_listener, *servinfo_listener, *p_listener;
	//int rv_listener;
	//char s_listener[INET6_ADDRSTRLEN];


	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;  // specify the returned addrinfo type
	// servinfo is the returned address info
	// If the AI_PASSIVE flag is not set in hints.ai_flags, then the
    // returned socket addresses will be suitable for use with
    // connect(2), sendto(2), or sendmsg(2). 

	// argv[1]: host ip
	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) { // create a socket endpoint
			perror("client: socket");
			continue;
		}
		// ai_addr: a pointer to the socket address
		// return 0 if succeed
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {  // initialize connection
			close(sockfd);
			perror("client: connect");
			continue;
		}
		printf("Socket fd: %d\n",sockfd);
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	// ai->addr: A pointer to the socket address is placed in the ai_addr field
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);  // convert IPv4 and IPv6 addresses from binary to text form
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure, free all address list

	// receive message from a socket
	// return the length of the message on successful completion
	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	buf[numbytes] = '\0';

	char * pos;
	pos=strchr(buf,'\n');  // returns a pointer to the first occurrence of character in the C string str.
	// extract the length of the file
	// printf("found at %zu\n",pos-buf+1);	
	unsigned int str_position = pos-buf;
	printf("%d\n", str_position);
	//printf("%zu \n\n",str_position);
	printf("client: received %.*s bytes",str_position,buf);  // output field width
	printf("%s",buf+str_position+2);

	// close connection
	close(sockfd);  //  close() closes a file descriptor, so that it no longer refers to
    //   any file and may be reused

	// Start of listener Code

	//memset(&hints_listener, 0, sizeof hints_listener);
	//hints_listener.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	//hints_listener.ai_socktype = SOCK_DGRAM;
	//hints_listener.ai_flags = AI_PASSIVE; // use my IP

//	if ((rv_listener = getaddrinfo(NULL, LISTENPORT, &hints_listener, &servinfo_listener)) != 0) {
//		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));/
//		return 1;
//	}

	
	// loop through all the results and bind to the first we can
//	for(p_listener = servinfo_listener; p_listener != NULL; p_listener = p_listener->ai_next) {
//		if ((sockfd_listener = socket(p_listener->ai_family, p_listener->ai_socktype,
//				p_listener->ai_protocol)) == -1) {
//			perror("listener: socket");
//			continue;
//		}

//		if (bind(sockfd_listener, p_listener->ai_addr, p_listener->ai_addrlen) == -1) {
//			close(sockfd_listener);
//			perror("listener: bind");
//			continue;
//		}

//		break;
//	}

//	if (p_listener == NULL) {
//		fprintf(stderr, "listener: failed to bind socket\n");
//		return 2;
//	}

//	freeaddrinfo(servinfo_listener);

//	printf("listener: waiting to recvfrom...\n");

//	addr_len = sizeof their_addr;

//	if ((numbytes_listener = recvfrom(sockfd_listener, buf_listener, MAXBUFLEN-1 , 0,
//		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
//		perror("recvfrom");
//		exit(1);
//	}

//	printf("listener: got packet from %s\n",
//		inet_ntop(their_addr.ss_family,
//			get_in_addr((struct sockaddr *)&their_addr),
//			s_listener, sizeof s_listener));
//	printf("listener: packet is %d bytes long\n", numbytes_listener);
//	buf_listener[numbytes_listener] = '\0';
//	printf("listener: packet contains \"%s\"\n", buf_listener);

//	close(sockfd_listener);



	return 0;
}

