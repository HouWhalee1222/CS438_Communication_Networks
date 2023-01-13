#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

using namespace std;

vector<int> readInput(string fileName) {
    vector<int> parameters;
    ifstream input(fileName);
    string line;
    char sign;
    int value;
    while (getline(input, line)) {
        stringstream iss(line);
        iss >> sign;
        int paraNum = (sign == 'R') ? parameters.back() : 1;
        for (int i = 0; i < paraNum; ++i) {
            iss >> value;
            parameters.push_back(value);
        }
    }
    return parameters;
}

inline int setBackoffTime(int id, int tick, int R) {
    return (id + tick) % R;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    /* Read parameters */
    string fileName = argv[1];
    vector<int> parameters = readInput(fileName);
    int N = parameters[0], L = parameters[1], M = parameters[2], T = parameters.back();
    vector<int> R(parameters.begin() + 3, parameters.end() - 1);

    /* Start simulation */
    int tick = 0;
    int utilizedTime = 0;  // how many ticks there are packets transmitting
    int transmitPacket = -1, transmissionTime = 0;  // how many time remains for the packet to transmit
    vector<int> backoffTime(N);
    vector<int> backoffWindow(N, 0); // store index in R

    // initialize backoff time
    for (int i = 0; i < N; ++i) {
        backoffTime[i] = setBackoffTime(i, tick, R[backoffWindow[i]]);
    }

    for (tick = 0; tick < T; ++tick) {
        
        // Check whether there is packet transmitting 
        if (transmissionTime > 0) {
            --transmissionTime;
            ++utilizedTime;
            if (transmissionTime == 0) {
                // set the new backoff time for the packet
                backoffWindow[transmitPacket] = 0;
                backoffTime[transmitPacket] = setBackoffTime(transmitPacket, tick + 1, R[backoffWindow[transmitPacket]]);
                transmitPacket = -1;
            }
            continue;
        }

        vector<int> zeroPacket;  // Packets having zero backoff time at this moment
        for (int i = 0; i < N; ++i) {
            if (backoffTime[i] == 0) {
                zeroPacket.push_back(i);
            }
        }

        int countZero = zeroPacket.size();
        if (countZero == 0) {  // No packet can be sent at this time, count down each node by 1 
            for (int i = 0; i < N; ++i) {
                --backoffTime[i];
            }
        } else if (countZero == 1) {  // One packet can be sent, no collision
            transmitPacket = zeroPacket[0];
            transmissionTime = L;
            --transmissionTime;
            ++utilizedTime;
        } else {  // Collision happens
            for (int& p : zeroPacket) {
                ++backoffWindow[p];
                if (backoffWindow[p] == M) {  // exceed maximum transmit time, throw the packet
                    backoffWindow[p] = 0;
                }
                backoffTime[p] = setBackoffTime(p, tick + 1, R[backoffWindow[p]]);
            }
        }
    }

    ofstream outFile("output.txt");
    double utilizationRate = double(utilizedTime) / T;
    outFile << setprecision(2) << fixed << utilizationRate;
    outFile.close();
    return 0;
}
