#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <climits>
#include <queue>
using namespace std;

void init_ft(unordered_map<int, unordered_map<int, int> > &graph, 
            set<int> &nodes,
            unordered_map<int, unordered_map<int, vector<int> > > &ft) {

    for (auto it = graph.begin(); it != graph.end(); ++it) {
        int i = it->first;
        
        for (auto it2 = nodes.begin(); it2 != nodes.end(); ++it2) {
            // src: i, dst: j
            int j = *it2;
            if (it->second.find(j) != it->second.end()) {
                ft[i][j].push_back(j);
                ft[i][j].push_back(graph[i][j]);
            }
            else {
                 ft[i][j].push_back(-1);
                 ft[i][j].push_back(INT_MAX);
            }

        }
    } 

}

void print_path(unordered_map<int, unordered_map<int, vector<int> > > &ft, set<int> &nodes, int src, int dst, string msg, ofstream &out_file) {
    // from 2 to 1 cost 6 hops 2 5 4 message here is a message from 2 to 1
    int s = src, d = dst;
    if (nodes.find(dst) != nodes.end() && ft[s][d][1] != INT_MAX) {
        int next = ft[s][d][0];
        out_file<<"from "<<s<<" to "<<d<<" cost "<<ft[s][d][1]<<" hops "<<s<<" "<<next;
        while (ft[next][d][0] != d) {
            next = ft[next][d][0];
            out_file<<" "<<next;
        }
        out_file<<" message "<<msg<<endl;
    }
    else {
        out_file<<"from "<<s<<" to "<<d<<" cost infinite hops unreachable message "<<msg<<endl;
    }

}


unordered_map<int, unordered_map<int, vector<int> > > bellman_ford(
                                    unordered_map<int, unordered_map<int, int> > &graph, 
                                    unordered_map<int, set<int> > &neighbors, 
                                    set<int> nodes,
                                    unordered_map<int, unordered_map<int, vector<int> > > ft) {
                                    
    bool updated = true;
    unordered_map<int, unordered_map<int, vector<int> > > new_ft;
    init_ft(graph, nodes, new_ft);
    // int round = 0;
    while (updated) {
        updated = false;
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            int x = *it;
            set<int> nbs = neighbors[x];

            // x: cur node, y: destination node, v: neighbor node
            for (auto it2 = nodes.begin(); it2 != nodes.end(); ++it2) {
                int y = *it2;
                bool local_updated = false;

                // maintian value for path to itself
                if (x == y) {
                    new_ft[x][y][0] = ft[x][y][0];
                    new_ft[x][y][1] = ft[x][y][1];
                    continue;
                }

                // check if need to update after recving dv from neighbors
                // int next_hop = -1, d_xy = INT_MAX;
                int next_hop = ft[x][y][0], d_xy = ft[x][y][1];

                for (auto it3 = nbs.begin(); it3 != nbs.end(); ++it3) {
                    int v = *it3;
                    int c_xv = graph[x][v], d_vy = ft[v][y][1], tmp_next_hop = v;

                    if (d_vy == INT_MAX) continue;
                    if (c_xv + d_vy < d_xy) {
                        d_xy = c_xv + d_vy;
                        next_hop = tmp_next_hop;
                        updated = true;
                        local_updated = true;
                    }
                    // Tie-break handling
                    else if (c_xv + d_vy == d_xy) {
                        d_xy = c_xv + d_vy;
                        if (next_hop > tmp_next_hop) {
                            next_hop = tmp_next_hop;
                            updated = true;
                            local_updated = true;
                        }
                    }
                }

                if (local_updated) {
                    new_ft[x][y][0] = next_hop;
                    new_ft[x][y][1] = d_xy;
                }
                else {
                    new_ft[x][y][0] = ft[x][y][0];
                    new_ft[x][y][1] = ft[x][y][1];
                }
            }
        }

        // ft = new_ft
        if (updated) {
            for (auto it = ft.begin(); it != ft.end(); ++it) {
                int x = it->first;
                for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                    int y = it2->first;
                    ft[x][y][0] = new_ft[x][y][0];
                    ft[x][y][1] = new_ft[x][y][1];
                }
            }
            
        }


    }

    return new_ft;
}





int main(int argc, char **argv)
{
    // printf("Number of arguments: %d", argc);
    if (argc != 4)
    {
        printf("Usage: ./distvec topo_file messagefile changesfile\n");
        return -1;
    }
    string output = "output.txt";
    ofstream out_file(output);
    


    ifstream topofile(argv[1]);
    string line;
    int sz = 0;
    while (getline(topofile, line)) {
        istringstream iss(line);
        int src, dst, cost;
        if (!(iss >> src >> dst >> cost)) break;
        max(sz, src);
        max(sz, dst);
    }
    topofile.close();



    // TODO: Build up initial forwarding table
    unordered_map<int, unordered_map<int, int> > graph;
    unordered_map<int, set<int> > neighbors;
    set<int> nodes;

    ifstream topo_file(argv[1]);
    while (getline(topo_file, line)) {
        istringstream iss(line);
        int src, dst, cost;
        if (!(iss >> src >> dst >> cost)) break;

        graph[src][dst] = cost;
        graph[dst][src] = cost;
        neighbors[src].insert(dst);
        neighbors[dst].insert(src);
        nodes.insert(src);
        nodes.insert(dst);
    }
    for (auto it = nodes.begin(); it != nodes.end(); ++it) graph[*it][*it] = 0;


    // Init forwarding table
    unordered_map<int, unordered_map<int, vector<int> > > ft;
    init_ft(graph, nodes, ft);

    // Update forwarding table until converge
    ft = bellman_ford(graph, neighbors, nodes, ft);

    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        int x = *it;
        for (auto it2 = nodes.begin(); it2 != nodes.end(); ++it2) {
            int y = *it2;
            if (ft[x][y][0] != -1) {
                out_file<<y<<" "<<ft[x][y][0]<<" "<<ft[x][y][1]<<"\n";
            }

        }
    }


    // TODO: Send msgs
    ifstream msg_file(argv[2]);
    // cout<<"Send msgs..."<<endl;
    while (getline(msg_file, line)) {
        int idx, src, dst;
        string msg;
        idx = line.find(' ');
        src = stoi(line.substr(0, idx));
        line = line.substr(idx+1, line.size()-idx);

        idx = line.find(' ');
        dst = stoi(line.substr(0, idx));
        msg = line.substr(idx+1, line.size()-idx);

        // TODO: Lookup forwarding table and output routing details
        // from <x> to <y> cost <path_cost> hops <hop1> <hop2> <...> message <message>
        // “from 2 to 1 cost 6 hops 2 5 4 message here is a message from 2 to 1"

        // cout<<src<<" "<<dst<<" "<<msg<<endl;
        print_path(ft, nodes, src, dst, msg, out_file);
    }
    msg_file.close();


    ifstream change_file(argv[3]);
    
    while (getline(change_file, line)) {
        istringstream iss(line);
        int src, dst, cost;
        if (!(iss >> src >> dst >> cost)) break;
        if (nodes.find(src) != nodes.end() && nodes.find(dst) != nodes.end()) {
            if (cost != -999) {
                graph[src][dst] = cost;
                graph[dst][src] = cost;
                neighbors[src].insert(dst);
                neighbors[dst].insert(src);
            }
            else {
                // delete node
                graph[src].erase(dst);
                graph[dst].erase(src);
                neighbors[src].erase(dst);
                neighbors[dst].erase(src);
            }

        }
        else {
            if (nodes.find(src) == nodes.end()) {
                nodes.insert(src);
                graph[src][src] = 0;
            }
            if (nodes.find(dst) == nodes.end()) {
                nodes.insert(dst);
                graph[dst][dst] = 0;
            }
            graph[src][dst] = cost;
            graph[dst][src] = cost;
            neighbors[src].insert(dst);
            neighbors[dst].insert(src);
        }

        // Update Forwarding Table and Print out Forwarding Table
        unordered_map<int, unordered_map<int, vector<int> > > ft;
        init_ft(graph, nodes, ft);
        ft = bellman_ford(graph, neighbors, nodes, ft);

        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            int x = *it;
            for (auto it2 = nodes.begin(); it2 != nodes.end(); ++it2) {
                int y = *it2;
                if (ft[x][y][0] != -1) {
                    out_file<<y<<" "<<ft[x][y][0]<<" "<<ft[x][y][1]<<"\n";
                }

            }
        }

        // TODO: Send msgs
        ifstream msg_file(argv[2]);
        while (getline(msg_file, line)) {
            int idx, src, dst;
            string msg;
            idx = line.find(' ');
            src = stoi(line.substr(0, idx));
            line = line.substr(idx+1, line.size()-idx);

            idx = line.find(' ');
            dst = stoi(line.substr(0, idx));
            msg = line.substr(idx+1, line.size()-idx);

            print_path(ft, nodes, src, dst, msg, out_file);
        }
        msg_file.close();

    }



    topo_file.close();
    change_file.close();
    out_file.close();

    return 0;
}

