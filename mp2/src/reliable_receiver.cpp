/* 
 * File:   reliable_receiver.cpp
 * Author: jingyul9
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <list>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
using namespace std;

#define PACKETSIZE 1280
#define PACKETBYTE 5
#define BASE 100
#define TIMEOUT 30  // in ms
#define SOCKETTIMEOUT 50 // in ms

int decodePacketNum(vector<char>& buffer) {
    int num = 0;
    int b = 1;
    for (int i = 0; i < PACKETBYTE; ++i) {
        num += int(buffer[i]) * b;
        b *= BASE;
    }
    return num;
}

void encodePacketNum(int num, vector<char>& buffer) {
    for (int i = 0; i < PACKETBYTE; ++i) {
        buffer[i] = char(num % BASE);
        num /= BASE;
    }
}

void diep(string s) {
    perror(s.c_str());
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    // Open the file
    ofstream outFile(destinationFile);
    if (!outFile.is_open()) {
        printf("Could not open file to receive.");
        exit(1);
    }

    // Build socket connection
    struct sockaddr_in si_me, si_other;  // si_me: socket sever listens, si_other: socket at the other end, address family, port, ip
    int slen;
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1) {
        diep("socket");
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    bind(s, (struct sockaddr*)&si_me, sizeof(si_me));  // socket s should bind to si_me

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000 * SOCKETTIMEOUT;  // in ms
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) {
        diep("Timeout Error");
    }

    // Set TCP parameters
    vector<char> recv_buffer(PACKETSIZE + PACKETBYTE, 0);
    list<pair<int, vector<char>>> temp_buffer;  // store out-of-order packet, file length - content
    int ackPrv = 0;  // last packet written to file, 0 is none
    long long totalBytes = 0;  // Total bytes written to file
    auto programStartTime = chrono::high_resolution_clock::now();

	/* Now receive data and send acknowledgements */
    while (true) {
        
        // Receive data
        slen = sizeof(si_other);  // data about sender store in si_other, room for slen bytes
        int numbytes = recvfrom(s, &recv_buffer[0], PACKETSIZE + PACKETBYTE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen);
        if (numbytes == -1) {
            // diep("recvfrom()");
            // cout << "No receive data this loop" << endl;
            continue;
        }
        // Check whether it is terminate signal
        if (numbytes == 8 && string(recv_buffer.begin(), recv_buffer.begin() + 8) == "##STOP##") break;
        
        if (numbytes == 0) {
            // cout << "No further received, break, current successful byte: " << totalBytes << endl;
            break;
        }

        // Handle packet
        int pktNum = decodePacketNum(recv_buffer);  // current received packet
        // cout << "Current received: " << pktNum << endl;
        if (pktNum == ackPrv + 1) {  // If packet number matches the next packet (in order), write to file

            // Write the packet to output file
            for (int i = PACKETBYTE; i < numbytes; ++i) {
                outFile << recv_buffer[i];
                ++totalBytes;
            }
            ++ackPrv;  // the packet has been successfully written to file
            // cout << "Successful write: " << ackPrv << endl;

            // Check whether the content in list can write to file
            while (!temp_buffer.empty()) {
                auto it = temp_buffer.begin();
                int pktNumList = decodePacketNum((*it).second);
                if (pktNumList == ackPrv + 1) {  // if the order match, write to file
                    for (int i = PACKETBYTE; i < (*it).first; ++i) {
                        outFile << (*it).second[i];
                        ++totalBytes;
                    }
                    ++ackPrv;
                    // cout << "Successful write list element: " << ackPrv << endl;
                    temp_buffer.erase(it);
                } else break;
            }

        } else if (pktNum > ackPrv + 1) {  // If the received packet number is larger, write the buffer to a list
            bool same = false;
            auto pktListElement = temp_buffer.begin();
            int pktListNum = decodePacketNum((*pktListElement).second);
            while (pktListElement != temp_buffer.end() && pktNum >= pktListNum) {
                if (pktNum == pktListNum) {
                    same = true;
                    break;
                }
                ++pktListElement;
                pktListNum = decodePacketNum((*pktListElement).second);
            }
            if (!same) {
                temp_buffer.insert(pktListElement, {numbytes, recv_buffer});
                // cout << "Inserted: " << pktNum << "; Current list:" << endl;
                // for (auto it = temp_buffer.begin(); it != temp_buffer.end(); ++it) {
                //     cout << int(decodePacketNum((*it).second)) << " ";
                // }
                // cout << endl;
            } 

        } else {  // The received packet is smaller than or equal to previous ack, ignore
            // cout << "This should rarely happen" << " Received: " << pktNum << " Previous written to file: " << ackPrv << endl;
        }

        // Send ack
        vector<char> ackSendBuffer(PACKETBYTE, 0);
        encodePacketNum(ackPrv, ackSendBuffer);
        int ackbytes = sendto(s, &ackSendBuffer[0], PACKETBYTE, 0, (struct sockaddr*) &si_other, slen);
        if (ackbytes == -1) {
            diep("send ack");
        }

        // cout << "Send ack: " << ackPrv << endl;
        // cout << "Received packet: " << pktNum << " Bytes:" << numbytes << " Total bytes in file now: " << totalBytes << " List length: " << temp_buffer.size() << endl << endl;

    }

    outFile.close();
    cout << "Total bytes:" << totalBytes << endl;
    auto programEndTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(programEndTime - programStartTime);
    cout << "Receiver cost time: " << duration.count() / 1000 << endl;
    cout << "Throughput: " << double(totalBytes) * 8 / 1000 / duration.count() << " Mbps" << endl;
	printf("%s received.\n", destinationFile);
    close(s);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

