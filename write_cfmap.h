#pragma once

#include <Eigen/Core>
#include <Eigen/Dense>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Write a coarse-fine correspondence file (.cfmap) alongside the coarse OBJ.
//
// Format:
//   Line 1 : n_fine  n_coarse  n_coarse_faces
//   Lines 2..(n_fine+1):  nearest_coarse_v  coarse_face  bc0  bc1  bc2
//     coarse_face == -1  : this fine vertex IS a coarse vertex
//     coarse_face >= 0   : deleted fine vertex; position reconstructed as
//                          bc0*V_c[F[f,0]] + bc1*V_c[F[f,1]] + bc2*V_c[F[f,2]]
//
// Returns false on write failure.
inline bool write_cfmap(
    const std::string&                   output_obj_path,
    int                                  n_fine,
    const Eigen::MatrixXd&               V_coarse,
    const Eigen::MatrixXi&               F_coarse,
    const Eigen::MatrixXd&               BC,
    const std::vector<std::vector<int>>& F2V,
    const std::map<int,int>&             IMV)
{
    // Derive .cfmap path from output OBJ path
    std::string path = output_obj_path;
    const size_t dot = path.rfind('.');
    if (dot != std::string::npos) path.replace(dot, std::string::npos, ".cfmap");
    else                          path += ".cfmap";

    // Build fine_vertex -> coarse_face index from F2V.
    // After the full remove_unreferenced_intrinsic, F2V[f] uses new (coarse) face
    // indices and stores original fine vertex indices as keys into BC.
    std::vector<int> fine_to_face(n_fine, -1);
    for (int f = 0; f < (int)F2V.size(); f++)
        for (int v : F2V[f])
            fine_to_face[v] = f;

    std::ofstream ofs(path);
    if (!ofs) {
        std::cerr << "Error: failed to write " << path << std::endl;
        return false;
    }

    ofs << n_fine << " " << (int)V_coarse.rows() << " " << (int)F_coarse.rows() << "\n";
    ofs << std::fixed << std::setprecision(6);

    for (int v = 0; v < n_fine; v++) {
        if (IMV.count(v)) {
            // Surviving coarse vertex
            ofs << IMV.at(v) << " -1 1.000000 0.000000 0.000000\n";
        } else {
            // Deleted fine vertex: nearest coarse = argmax of BC row
            const int f = fine_to_face[v];
            int k = 0;
            if (BC(v, 1) > BC(v, k)) k = 1;
            if (BC(v, 2) > BC(v, k)) k = 2;
            ofs << F_coarse(f, k) << " " << f << " "
                << BC(v, 0) << " " << BC(v, 1) << " " << BC(v, 2) << "\n";
        }
    }

    std::cout << "Written: " << path << std::endl;
    return true;
}
