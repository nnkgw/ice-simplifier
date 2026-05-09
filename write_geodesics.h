#pragma once

#include <Eigen/Core>

#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

// Build undirected edge adjacency graph from fine mesh with Euclidean edge weights.
static std::vector<std::vector<std::pair<int,float>>>
build_fine_graph(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F)
{
    int n = (int)V.rows();
    std::vector<std::vector<std::pair<int,float>>> g(n);
    for (int f = 0; f < (int)F.rows(); f++) {
        for (int e = 0; e < 3; e++) {
            int i = F(f, e), j = F(f, (e+1)%3);
            float d = (float)(V.row(i) - V.row(j)).norm();
            g[i].push_back({j, d});
            g[j].push_back({i, d});
        }
    }
    return g;
}

// Single-source Dijkstra on the fine mesh graph. Returns distances to all vertices.
static std::vector<float>
dijkstra_single(
    const std::vector<std::vector<std::pair<int,float>>>& g, int src)
{
    const float INF = std::numeric_limits<float>::infinity();
    std::vector<float> dist((int)g.size(), INF);
    dist[src] = 0.0f;
    std::priority_queue<std::pair<float,int>,
                        std::vector<std::pair<float,int>>,
                        std::greater<std::pair<float,int>>> pq;
    pq.push({0.0f, src});
    while (!pq.empty()) {
        float d = pq.top().first;
        int   u = pq.top().second;
        pq.pop();
        if (d > dist[u]) continue;
        for (int k = 0; k < (int)g[u].size(); k++) {
            int   v  = g[u][k].first;
            float nd = dist[u] + g[u][k].second;
            if (nd < dist[v]) {
                dist[v] = nd;
                pq.push({nd, v});
            }
        }
    }
    return dist;
}

// Write coarse geodesic distance files alongside the coarse OBJ.
//
// .ccgeo  (coarse-coarse geodesic): n_coarse x n_coarse binary float32
// .cfgeo  (coarse-fine geodesic):   n_coarse x n_fine   binary float32
//
// Binary format (both files):
//   [nrows : int32][ncols : int32]
//   row-major float32 data (nrows * ncols values)
//
// Random access: byte offset of element (r, c) = 8 + (r * ncols + c) * 4
//
// Dijkstra is run once per coarse vertex (vIdx(ci)) on the original fine mesh,
// and both files are written simultaneously.
//
// Returns false on write failure.
inline bool write_geodesics(
    const std::string&      output_obj_path,
    const Eigen::MatrixXd&  VO,
    const Eigen::MatrixXi&  FO,
    const Eigen::VectorXi&  vIdx)
{
    std::string base = output_obj_path;
    const size_t dot = base.rfind('.');
    if (dot != std::string::npos) base.resize(dot);

    const std::string ccgeo_path = base + ".ccgeo";
    const std::string cfgeo_path = base + ".cfgeo";

    const int n_coarse = (int)vIdx.rows();
    const int n_fine   = (int)VO.rows();

    std::ofstream cc(ccgeo_path, std::ios::binary);
    std::ofstream cf(cfgeo_path, std::ios::binary);
    if (!cc) { std::cerr << "Error: failed to write " << ccgeo_path << std::endl; return false; }
    if (!cf) { std::cerr << "Error: failed to write " << cfgeo_path << std::endl; return false; }

    cc.write(reinterpret_cast<const char*>(&n_coarse), sizeof(int));
    cc.write(reinterpret_cast<const char*>(&n_coarse), sizeof(int));
    cf.write(reinterpret_cast<const char*>(&n_coarse), sizeof(int));
    cf.write(reinterpret_cast<const char*>(&n_fine),   sizeof(int));

    const auto g = build_fine_graph(VO, FO);

    for (int ci = 0; ci < n_coarse; ci++) {
        const auto dist = dijkstra_single(g, vIdx(ci));

        for (int cj = 0; cj < n_coarse; cj++) {
            float d = dist[vIdx(cj)];
            cc.write(reinterpret_cast<const char*>(&d), sizeof(float));
        }
        cf.write(reinterpret_cast<const char*>(dist.data()), n_fine * sizeof(float));
    }

    std::cout << "Written: " << ccgeo_path << std::endl;
    std::cout << "Written: " << cfgeo_path << std::endl;
    return true;
}
