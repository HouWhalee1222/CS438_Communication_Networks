/* 
 * File:   reliable_sender.cpp
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <string>
#include <list>
#include <fstream>
#include <iostream>
#include <vector>
#include <deque>
#include <chrono>
#include "math.h"
using namespace std;

#define SLOWSTART 1
#define CONGESTIONAVOIDANCE 2
#define FASTRECOVERY 3

#define PACKETSIZE 1280
#define PACKETBYTE 5
#define BASE 100  // Use 5 bytes to represent packet number
#define INITSST 128
#define MINSST 32
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

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    
    //Open the file
    ifstream inFile(filename);
    if (!inFile.is_open()) {
        printf("Could not open file to send.");
        exit(1);
    }

    // Build socket connection
    struct sockaddr_in si_other;  // the socket the server is listening, data about the receiver, si_me in receiver
    int slen;
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1) {
        diep("socket");
    }

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000 * SOCKETTIMEOUT;  // in ms
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) {
        diep("Timeout Error");
    }

    // Set TCP parameters
    vector<char> send_buffer(PACKETSIZE + PACKETBYTE, 0);
    deque<int> slidWindow; slidWindow.push_back(1);  // start from packet 1
    deque<pair<int, vector<char>>> retransmit_buffer;  // buffer for current slide window, int is file length
    deque<pair<int, chrono::high_resolution_clock::time_point>> timestamp;  // packet number - timestamp
    int state = SLOWSTART;
    int pktNum = 0;  // The last packet sent, 0 represents no packet sent
    double cw = 1;  // slide window size
    int sst = INITSST;
    int dupAck = 1;
    int ackPrv = 0;  // ack number, the last successfully received packet, 0 represents none
    int timeoutCount = 0;  // how many timeout happens

	/* Send data and receive acknowledgements on s*/
    while (true) {
        // Send packet according to sliding window
        while (bytesToTransfer > 0 && pktNum < slidWindow.back()) {

            // Determine how many bytes to read from file
            int numbytesread = PACKETSIZE;
            if (bytesToTransfer < PACKETSIZE) numbytesread = bytesToTransfer;
            
            // Add packet number to the start of buffer
            encodePacketNum(pktNum + 1, send_buffer);  // current packet number should add 1
            inFile.read(&send_buffer[PACKETBYTE], numbytesread);

            // Buffer data to transmit buffer
            retransmit_buffer.push_back({numbytesread, send_buffer});

            // Send data
            slen = sizeof (si_other);  // receiver specified in si_other
            int numbytes = sendto(s, &send_buffer[0], numbytesread + PACKETBYTE, 0, (struct sockaddr*) &si_other, slen);
            if (numbytes == -1) {
                diep("sendto()");
            }

            ++pktNum;            
            bytesToTransfer -= numbytesread;
            
            // Record send timestamp
            auto timestampPkt = chrono::high_resolution_clock::now();
            timestamp.push_back({pktNum, timestampPkt});
            
            // cout << "Packet sent: " << pktNum << " bytes = " << numbytes << " BytesToTransfer: " << bytesToTransfer << " cw=" << cw << " sst = " << sst << " ack=" << ackPrv << " dupack = "<< dupAck << endl;
        }
        
        // If no bytes to transfer and receive the last packet, jump out
        if (bytesToTransfer == 0 && ackPrv == pktNum) break;
        
        // Check timeout
        auto currentTime = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(currentTime - timestamp.front().second);
        // cout << "Current timeout: " << duration.count() << " First Packet: " << timestamp.front().first << " Last packet in timeout buffer: " << timestamp.back().first << " timestamp size: " << timestamp.size() << endl;
        if (duration.count() > TIMEOUT) {  // timeout happens
            ++timeoutCount;

            sst = max(int(cw / 2), MINSST);
            cw = sst / 2;
            dupAck = 1;
            state = SLOWSTART;

            // Record the current packet to be retransmitted
            retransmit_buffer.push_back(retransmit_buffer.front());

            // Retransmit cw base
            slen = sizeof (si_other);  // receiver specified in si_other
            int numbytes = sendto(s, &retransmit_buffer.front().second[0], retransmit_buffer.front().first + PACKETBYTE, 0, (struct sockaddr*) &si_other, slen);
            if (numbytes == -1) {
                diep("retransmit");
            }
            // cout << "Timeout is: "<< duration.count() << " Retransmit Packet sent: " << decodePacketNum(retransmit_buffer.front().second) << " bytes=" << numbytes << " cw=" << cw << " ack=" << ackPrv << " dupack="<< dupAck << endl;

            // record time
            auto timestampPkt = chrono::high_resolution_clock::now();
            timestamp.push_back({timestamp.front().first, timestampPkt});
            timestamp.pop_front();

            // pop out related content in retransmit buffer
            retransmit_buffer.pop_front();
        }

        // Receive ack
        vector<char> ackRecvBuffer(PACKETBYTE, 0);
        int ackbytes = recvfrom(s, &ackRecvBuffer[0], PACKETBYTE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen);
        if (ackbytes == -1) {
            // diep("receive ack");
            // cout << "No receive ack this loop" << endl;
            continue;
        }
        int ackRecv = decodePacketNum(ackRecvBuffer);  // current received ack
        // cout << "Receive Ack from receiver: " << ackRecv << " Window size cw: " << cw << endl;
        
        // Update ack
        if (ackRecv == ackPrv) {  // dup ack
            if (state == FASTRECOVERY) {
                ++cw;
            }
            ++dupAck;

        } else if (ackRecv > ackPrv) {  // received ack must >= current ack unless delay
            ackPrv = ackRecv;
            dupAck = 1;

        } else {
            // cout << "This should not happen unless delay. Received: " << ackRecv << " Previous ack:" << ackPrv << endl;
            continue;
        }

        // Update retransmit buffer
        while (!retransmit_buffer.empty() && decodePacketNum(retransmit_buffer.front().second) <= ackPrv) {
            retransmit_buffer.pop_front();
        }

        // Update timeout queue
        while (!timestamp.empty() && timestamp.front().first <= ackPrv) {
            timestamp.pop_front();
        }

        // Check dup ack, if dup ack is 3, enter fast recovery state
        if (state != FASTRECOVERY && dupAck == 3) {
            state = FASTRECOVERY;
            sst = max(int(cw / 2), MINSST);
            cw += 3;  // do not set cw as sst

            // Record the current packet to be retransmitted
            retransmit_buffer.push_back(retransmit_buffer.front());

            // Retransmit cw base
            slen = sizeof (si_other);  // receiver specified in si_other
            int numbytes = sendto(s, &retransmit_buffer.front().second[0], retransmit_buffer.front().first + PACKETBYTE, 0, (struct sockaddr*) &si_other, slen);

            if (numbytes == -1) {
                diep("retransmit");
            }
            // cout << "Dup ack Retransmit Packet sent: " << decodePacketNum(retransmit_buffer.front().second) << " bytes=" << numbytes << " cw=" << cw << " ack=" << ackPrv << " dupack="<< dupAck << endl;
            auto timestampPkt = chrono::high_resolution_clock::now();
            timestamp.push_back({timestamp.front().first, timestampPkt});
            timestamp.pop_front();

            // pop out related content in retransmit buffer
            retransmit_buffer.pop_front();
        }

        // Make sure base of the slide window is correct
        bool newAckAtFastRecovery = false;
        while (slidWindow.front() <= ackPrv) {
            slidWindow.push_back(slidWindow.back() + 1);
            slidWindow.pop_front();
            if (state == SLOWSTART) {
                ++cw;
                if (cw >= sst) state = CONGESTIONAVOIDANCE;
            } else if (state == CONGESTIONAVOIDANCE) {
                cw += 1 / floor(cw);
            } else {
                newAckAtFastRecovery = true;
            }
        }

        if (newAckAtFastRecovery) {
            cw = sst;
            state = CONGESTIONAVOIDANCE;
        }

        // Make sure length of the slide window is correct
        while (slidWindow.size() < floor(cw)) {
            slidWindow.push_back(slidWindow.back() + 1);
        }
        while (slidWindow.size() > floor(cw)) {
            slidWindow.pop_back();
        }
        // cout << "Current cw size: " << cw << " Current cw window: " << slidWindow.front() << " - " << slidWindow.back() << " Current State: " << state << endl << endl;
    }

    // Send signal to terminate transmission
    string stop = "##STOP##";
    send_buffer.assign(stop.begin(), stop.end());
    slen = sizeof (si_other);
    int numbytes = sendto(s, &send_buffer[0], stop.size(), 0, (struct sockaddr*) &si_other, slen);
    if (numbytes == -1) {
        diep("close connection sendto()");
    }
    // cout << "stop signal sent: " << numbytes << endl; 
    // cout << "Timeout: " << timeoutCount << " Rate: " << double(timeoutCount) / pktNum << endl;

    inFile.close();
    printf("Closing the socket\n");
    close(s);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    return (EXIT_SUCCESS);
}


