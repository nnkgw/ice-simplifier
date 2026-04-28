#include <igl/read_triangle_mesh.h>
#include <igl/write_triangle_mesh.h>
#include <igl/slice.h>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <build_intrinsic_info.h>
#include <coarsen_mesh.h>
#include <remove_unreferenced_intrinsic.h>
#include <connected_components.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    using namespace Eigen;

    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <input.obj> <output.obj> [n_coarse_vertices=500]" << std::endl;
        return 1;
    }

    std::string input_path  = argv[1];
    std::string output_path = argv[2];
    int n_coarse_vertices   = (argc >= 4) ? std::stoi(argv[3]) : 500;

    // Load mesh
    MatrixXd VO;
    MatrixXi FO;
    if (!igl::read_triangle_mesh(input_path, VO, FO)) {
        std::cerr << "Error: failed to read " << input_path << std::endl;
        return 1;
    }
    std::cout << "Loaded: " << VO.rows() << " vertices, " << FO.rows() << " faces" << std::endl;

    if (n_coarse_vertices <= 0) {
        std::cerr << "Error: n_coarse_vertices must be positive" << std::endl;
        return 1;
    }
    if (n_coarse_vertices >= VO.rows()) {
        std::cout << "Warning: n_coarse_vertices (" << n_coarse_vertices
                  << ") >= input vertex count (" << VO.rows() << "). Nothing to do." << std::endl;
        igl::write_triangle_mesh(output_path, VO, FO);
        return 0;
    }

    // Build intrinsic representation
    MatrixXi F = FO;
    MatrixXi G;
    MatrixXd l, A;
    MatrixXi v2fs;
    build_intrinsic_info(VO, FO, G, l, A, v2fs);

    // Check connectivity
    VectorXi v_ids, f_ids;
    int n_components;
    connected_components(FO, G, n_components, v_ids, f_ids);
    if (n_components != 1) {
        std::cout << "Warning: mesh has " << n_components << " connected components." << std::endl;
    }

    // Coarsen
    int total_removal  = VO.rows() - n_coarse_vertices;
    double weight      = 0.0; // 0=curvature error, 1=area error
    MatrixXd BC;
    std::vector<std::vector<int>> F2V;

    std::cout << "Coarsening: removing " << total_removal << " vertices..." << std::endl;
    coarsen_mesh(total_removal, weight, F, G, l, A, v2fs, BC, F2V);

    // Extract coarse mesh vertices/faces
    std::map<int,int> IMV, IMF;
    VectorXi vIdx, fIdx;
    remove_unreferenced_intrinsic(F, IMV, IMF, vIdx, fIdx);

    MatrixXd V_coarse;
    igl::slice(VO, vIdx, 1, V_coarse);
    // F is already re-indexed by remove_unreferenced_intrinsic
    MatrixXi F_coarse = F;

    std::cout << "Coarsened: " << V_coarse.rows() << " vertices, " << F_coarse.rows() << " faces" << std::endl;

    // Write output
    if (!igl::write_triangle_mesh(output_path, V_coarse, F_coarse)) {
        std::cerr << "Error: failed to write " << output_path << std::endl;
        return 1;
    }
    std::cout << "Written: " << output_path << std::endl;

    return 0;
}
