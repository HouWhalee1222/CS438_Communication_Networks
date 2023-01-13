#include <iostream>
#include <string>
#include <fstream>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <algorithm>

using namespace std;

void generateTopo(string path, unordered_map<int, unordered_map<int, int> >& graph, set<int>& nodes) {
    ifstream topofile(path);
    int start, end, dist;
    while (topofile >> start >> end >> dist) {
        graph[start][end] = dist;
        graph[end][start] = dist;
        nodes.insert(start); nodes.insert(end);
    }
    topofile.close();
}

void generateMsg(string path, vector<vector<string> >& allMsg) {
    ifstream msgfile(path);
    string line;
    while (getline(msgfile, line)) {
        int split1 = line.find(' ');
        int split2 = line.find(' ', split1 + 1);
        string start = line.substr(0, split1);
        string end = line.substr(split1 + 1, split2 - split1 - 1);
        string msg = line.substr(split2 + 1);
        allMsg.push_back({start, end, msg});
    }
    msgfile.close();
}

void generateChange(string path, vector<vector<int>>& allChanges) {
    ifstream changefile(path);
    int start, end, dist;
    while (changefile >> start >> end >> dist) {
        allChanges.push_back({start, end, dist});
    }
    changefile.close();
}

void applyChange(unordered_map<int, unordered_map<int, int> >& graph, vector<int>& change) {
    int start = change[0], end = change[1], cost = change[2];
    if (cost == -999) {
        graph[start].erase(end);
        graph[end].erase(start);
    } else {
        graph[start][end] = cost;
        graph[end][start] = cost;
    }
}

void linkStateDijkstra(unordered_map<int, unordered_map<int, int> >& graph, unordered_map<int, unordered_map<int, vector<int> > >& forwardingTable) {
    for (auto& [node, neighbor] : graph) {  // construct forwarding table for node one by one
        unordered_set<int> visited;  // visited node;
        priority_queue<pair<int, int>, vector<pair<int, int> >, greater<pair<int, int> > > pq;  // next possible traversed node {dist, node}
        visited.insert(node);
        forwardingTable[node][node] = {node, node, 0};  // self to self dist is 0
        for (auto& [neighborNode, dist] : neighbor) {  // iterate all neighboring nodes
            forwardingTable[node][neighborNode] = {neighborNode, node, dist};
            pq.push({dist, neighborNode});
        }
        while (!pq.empty()) {
            auto [dist, curNode] = pq.top();  // iterate the closest node to the visited node
            pq.pop();
            if (visited.count(curNode)) continue;
            visited.insert(curNode);
            int nextHop = forwardingTable[node][curNode][0], node2CurNode = forwardingTable[node][curNode][2]; 
            for (auto& [neighborOfCurNode, curNode2Neighbor] : graph[curNode]) {  // iteate all neighboring node of current node
                if (visited.count(neighborOfCurNode)) continue; // if neighbor of the current node is visited, it means closest path already found
                int node2NeighborOfCurNew = node2CurNode + curNode2Neighbor;  // new distance
                if (!forwardingTable[node].count(neighborOfCurNode)) {  // if no distance recorded, which means infinite
                    forwardingTable[node][neighborOfCurNode] = {nextHop, curNode, node2NeighborOfCurNew};
                    pq.push({node2NeighborOfCurNew, neighborOfCurNode});
                } else {
                    int node2NeighborOfCurOri = forwardingTable[node][neighborOfCurNode][2];
                    if (node2NeighborOfCurOri > node2NeighborOfCurNew) {  // if new distance is smaller, update
                        forwardingTable[node][neighborOfCurNode] = {nextHop, curNode, node2NeighborOfCurNew};
                        pq.push({node2NeighborOfCurNew, neighborOfCurNode});
                    } else if (node2NeighborOfCurOri == node2NeighborOfCurNew && curNode < forwardingTable[node][neighborOfCurNode][1]) {  // tie, choose the one with smaller previous node
                        forwardingTable[node][neighborOfCurNode] = {nextHop, curNode, node2NeighborOfCurNew};
                        pq.push({node2NeighborOfCurNew, neighborOfCurNode});
                    }
                }
            }
        }
    }
}

bool findPath(int start, int end, unordered_map<int, unordered_map<int, vector<int> > >& forwardingTable, vector<int>& hops) {
    if (!forwardingTable[start].count(end)) return false;  // no path between start and end
    int prevNode = forwardingTable[start][end][1];
    hops.push_back(prevNode);
    while (prevNode != start) {
        prevNode = forwardingTable[start][prevNode][1];
        hops.push_back(prevNode);
    }
    reverse(hops.begin(), hops.end());
    return true;
}

void outputOnce(ofstream& outFile, set<int>& nodes, unordered_map<int, unordered_map<int, vector<int> > >& forwardingTable, vector<vector<string> >& allMsg) {
    for (auto it1 = nodes.begin(); it1 != nodes.end(); ++it1) {  // iterate node
        for (auto it2 = nodes.begin(); it2 != nodes.end(); ++it2) {
            if (forwardingTable[*it1].count(*it2)) {
                outFile << *it2 << " " << forwardingTable[*it1][*it2][0] << " " << forwardingTable[*it1][*it2][2] << endl;
            }
        }
    }
    for (auto& msgInfo : allMsg) {
        int start = stoi(msgInfo[0]), end = stoi(msgInfo[1]);
        vector<int> hops;
        if (findPath(start, end, forwardingTable, hops)) {
            outFile << "from " << start << " to " << end << " cost " << forwardingTable[start][end][2] << " hops ";
            for (int& hop : hops) {
                outFile << hop << " ";
            }
        } else {
            outFile << "from " << start << " to " << end << " cost infinite hops unreachable ";
        }
        outFile << "message " << msgInfo[2] << endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    // node number - neighbor number - dist
    unordered_map<int, unordered_map<int, int> > graph;  
    set<int> nodes;  // keep node order
    vector<vector<string> > allMsg;
    vector<vector<int>> allChanges;

    string topofilePath = argv[1];
    string msgfilePath = argv[2];
    string changefilePath = argv[3];
    
    generateTopo(topofilePath, graph, nodes);
    generateMsg(msgfilePath, allMsg);
    generateChange(changefilePath, allChanges);

    ofstream outFile("output.txt");

    for (int i = 0; i <= allChanges.size(); ++i) {
        // node number - dst node - {nexthop, previous node, pathcost}
        unordered_map<int, unordered_map<int, vector<int> > > forwardingTable;  
        if (i > 0) {
            applyChange(graph, allChanges[i - 1]);
        }
        linkStateDijkstra(graph, forwardingTable);
        outputOnce(outFile, nodes, forwardingTable, allMsg);
    }
    
    outFile.close();
    return 0;
}
