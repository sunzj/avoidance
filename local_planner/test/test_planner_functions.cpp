#include <gtest/gtest.h>
#include <cmath>

#include "../include/local_planner/planner_functions.h"

#include "../include/local_planner/common.h"

using namespace avoidance;

TEST(PlannerFunctions, generateNewHistogramEmpty) {
  // GIVEN: an empty pointcloud
  pcl::PointCloud<pcl::PointXYZI> empty_cloud;
  Histogram histogram_output = Histogram(ALPHA_RES);
  geometry_msgs::PoseStamped location;
  location.pose.position.x = 0;
  location.pose.position.y = 0;
  location.pose.position.z = 0;

  // WHEN: we build a histogram
  generateNewHistogram(histogram_output, empty_cloud,
                       toEigen(location.pose.position));

  // THEN: the histogram should be all zeros
  for (int e = 0; e < GRID_LENGTH_E; e++) {
    for (int z = 0; z < GRID_LENGTH_Z; z++) {
      EXPECT_LE(histogram_output.get_dist(e, z), FLT_MIN);
    }
  }
}

TEST(PlannerFunctions, generateNewHistogramSpecificCells) {
  // GIVEN: a pointcloud with an object of one cell size
  Histogram histogram_output = Histogram(ALPHA_RES);
  Eigen::Vector3f location(0.0f, 0.0f, 0.0f);
  float distance = 1.0f;

  std::vector<float> e_angle_filled = {-89.9f, -30.0f, 0.0f,
                                       20.0f,  40.0f,  89.9f};
  std::vector<float> z_angle_filled = {-180.0f, -50.0f, 0.0f,
                                       59.0f,   100.0f, 175.0f};
  std::vector<Eigen::Vector3f> middle_of_cell;
  std::vector<int> e_index, z_index;

  for (auto i : e_angle_filled) {
    for (auto j : z_angle_filled) {
      PolarPoint p_pol(i, j, distance);
      middle_of_cell.push_back(polarToCartesian(p_pol, location));
      e_index.push_back(polarToHistogramIndex(p_pol, ALPHA_RES).y());
      z_index.push_back(polarToHistogramIndex(p_pol, ALPHA_RES).x());
    }
  }

  pcl::PointCloud<pcl::PointXYZI> cloud;
  for (int i = 0; i < middle_of_cell.size(); i++) {
    // put 1000 point in every occupied cell
    for (int j = 0; j < 1000; j++) {
      cloud.push_back(toXYZI(middle_of_cell[i], 0));
    }
  }

  // WHEN: we build a histogram
  generateNewHistogram(histogram_output, cloud, location);

  // THEN: the filled cells in the histogram should be one and the others be
  // zeros

  for (int e = 0; e < GRID_LENGTH_E; e++) {
    for (int z = 0; z < GRID_LENGTH_Z; z++) {
      bool e_found =
          std::find(e_index.begin(), e_index.end(), e) != e_index.end();
      bool z_found =
          std::find(z_index.begin(), z_index.end(), z) != z_index.end();
      if (e_found && z_found) {
        EXPECT_NEAR(histogram_output.get_dist(e, z), 1.f, 0.01);
      } else {
        EXPECT_LT(histogram_output.get_dist(e, z), FLT_MIN);
      }
    }
  }
}

TEST(PlannerFunctions, calculateFOV) {
  // GIVEN: the horizontal and vertical Field of View, the vehicle yaw and pitc
  float h_fov = 90.0f;
  float v_fov = 45.0f;
  float yaw_z_greater_grid_length =
      270.f;  // z_FOV_max >= GRID_LENGTH_Z && z_FOV_min >= GRID_LENGTH_Z
  float yaw_z_max_greater_grid =
      210.f;  // z_FOV_max >= GRID_LENGTH_Z && z_FOV_min < GRID_LENGTH_Z
  float yaw_z_min_smaller_zero = -140.f;  // z_FOV_min < 0 && z_FOV_max >= 0
  float yaw_z_smaller_zero = -235.f;      // z_FOV_max < 0 && z_FOV_min < 0
  float pitch = 0.0f;

  // WHEN: we calculate the Field of View
  std::vector<int> z_FOV_idx_z_greater_grid_length;
  std::vector<int> z_FOV_idx_z_max_greater_grid;
  std::vector<int> z_FOV_idx3_z_min_smaller_zero;
  std::vector<int> z_FOV_idx_z_smaller_zero;
  int e_FOV_min;
  int e_FOV_max;

  calculateFOV(h_fov, v_fov, z_FOV_idx_z_greater_grid_length, e_FOV_min,
               e_FOV_max, yaw_z_greater_grid_length, pitch);
  calculateFOV(h_fov, v_fov, z_FOV_idx_z_max_greater_grid, e_FOV_min, e_FOV_max,
               yaw_z_max_greater_grid, pitch);
  calculateFOV(h_fov, v_fov, z_FOV_idx3_z_min_smaller_zero, e_FOV_min,
               e_FOV_max, yaw_z_min_smaller_zero, pitch);
  calculateFOV(h_fov, v_fov, z_FOV_idx_z_smaller_zero, e_FOV_min, e_FOV_max,
               yaw_z_smaller_zero, pitch);

  // THEN: we expect polar histogram indexes that are in the Field of View
  std::vector<int> output_z_greater_grid_length = {
      7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
  std::vector<int> output_z_max_greater_grid = {0, 1, 2,  3,  4,  5,  6,  7,
                                                8, 9, 10, 11, 12, 57, 58, 59};
  std::vector<int> output_z_min_smaller_zero = {0, 1, 2,  3,  4,  5,  6,  7,
                                                8, 9, 10, 11, 12, 13, 14, 59};
  std::vector<int> output_z_smaller_zero = {43, 44, 45, 46, 47, 48, 49, 50,
                                            51, 52, 53, 54, 55, 56, 57, 58};

  EXPECT_EQ(18, e_FOV_max);
  EXPECT_EQ(11, e_FOV_min);

  // vector sizes:
  EXPECT_EQ(output_z_greater_grid_length.size(),
            z_FOV_idx_z_greater_grid_length.size());
  EXPECT_EQ(output_z_max_greater_grid.size(), output_z_max_greater_grid.size());
  EXPECT_EQ(output_z_min_smaller_zero.size(), output_z_min_smaller_zero.size());
  EXPECT_EQ(output_z_smaller_zero.size(), output_z_smaller_zero.size());

  for (size_t i = 0; i < z_FOV_idx_z_greater_grid_length.size(); i++) {
    EXPECT_EQ(output_z_greater_grid_length.at(i),
              z_FOV_idx_z_greater_grid_length.at(i));
  }

  for (size_t i = 0; i < z_FOV_idx_z_max_greater_grid.size(); i++) {
    EXPECT_EQ(output_z_max_greater_grid.at(i),
              z_FOV_idx_z_max_greater_grid.at(i));
  }

  for (size_t i = 0; i < z_FOV_idx3_z_min_smaller_zero.size(); i++) {
    EXPECT_EQ(output_z_min_smaller_zero.at(i),
              z_FOV_idx3_z_min_smaller_zero.at(i));
  }

  for (size_t i = 0; i < z_FOV_idx_z_smaller_zero.size(); i++) {
    EXPECT_EQ(output_z_smaller_zero.at(i), z_FOV_idx_z_smaller_zero.at(i));
  }
}

TEST(PlannerFunctionsTests, processPointcloud) {
  // GIVEN: two point clouds
  const Eigen::Vector3f position(1.5f, 1.0f, 4.5f);
  pcl::PointCloud<pcl::PointXYZ> p1;
  p1.push_back(toXYZ(position + Eigen::Vector3f(1.1f, 0.8f, 0.1f)));
  p1.push_back(toXYZ(position + Eigen::Vector3f(2.2f, 1.0f, 1.0f)));
  p1.push_back(toXYZ(position + Eigen::Vector3f(1.0f, -3.0f, 1.0f)));
  p1.push_back(toXYZ(position + Eigen::Vector3f(0.7f, 0.3f, -0.5f)));
  p1.push_back(toXYZ(position + Eigen::Vector3f(-1.0f, 1.0f, 1.0f)));
  p1.push_back(toXYZ(position + Eigen::Vector3f(-1.0f, -1.1f, 3.5f)));

  pcl::PointCloud<pcl::PointXYZ> p2;
  p2.push_back(toXYZ(
      position + Eigen::Vector3f(1.0f, 5.0f, 1.0f)));  // > histogram_box.radius
  p2.push_back(
      toXYZ(position +
            Eigen::Vector3f(100.0f, 5.0f, 1.0f)));  // > histogram_box.radius
  p2.push_back(toXYZ(
      position + Eigen::Vector3f(0.1f, 0.05f, 0.05f)));  // < min_realsense_dist

  std::vector<pcl::PointCloud<pcl::PointXYZ>> complete_cloud;
  complete_cloud.push_back(p1);
  complete_cloud.push_back(p2);
  Box histogram_box(5.0f);
  histogram_box.setBoxLimits(position, 4.5f);
  float min_realsense_dist = 0.2f;

  pcl::PointCloud<pcl::PointXYZI> processed_cloud1, processed_cloud2;
  Eigen::Vector3f memory_point(-0.4f, 0.3f, -0.4f);
  processed_cloud1.push_back(toXYZI(position + memory_point, 5));
  processed_cloud2.push_back(toXYZI(position + memory_point, 5));

  // WHEN: we filter the PointCloud with different values max_age
  processPointcloud(processed_cloud1, complete_cloud, histogram_box, position,
                    min_realsense_dist, 0.f, 0.5f);

  processPointcloud(processed_cloud2, complete_cloud, histogram_box, position,
                    min_realsense_dist, 10.f, .5f);

  // THEN: we expect the first cloud to have 6 points
  // the second cloud should contain 7 points
  EXPECT_EQ(processed_cloud1.size(), 6);
  EXPECT_EQ(processed_cloud2.size(), 7);
}

TEST(PlannerFunctions, testDirectionTree) {
  // GIVEN: the node positions in a tree and some possible vehicle positions
  float n1_x = 0.8f;
  float n2_x = 1.5f;
  float n3_x = 2.1f;
  float n4_x = 2.3f;
  Eigen::Vector3f n0(0.0f, 0.0f, 2.5f);
  Eigen::Vector3f n1(n1_x, sqrtf(1 - (n1_x * n1_x)), 2.5f);
  Eigen::Vector3f n2(n2_x, n1.y() + sqrtf(1 - powf(n2_x - n1.x(), 2)), 2.5f);
  Eigen::Vector3f n3(n3_x, n2.y() + sqrtf(1 - powf(n3_x - n2.x(), 2)), 2.5f);
  Eigen::Vector3f n4(n4_x, n3.y() + sqrtf(1 - powf(n4_x - n3.x(), 2)), 2.5f);
  const std::vector<Eigen::Vector3f> path_node_positions = {n4, n3, n2, n1, n0};

  PolarPoint p, p1, p2;
  Eigen::Vector3f postion(0.2f, 0.3f, 1.5f);
  Eigen::Vector3f postion1(1.1f, 2.3f, 2.5f);
  Eigen::Vector3f postion2(5.4f, 2.0f, 2.5f);
  Eigen::Vector3f goal(10.0f, 5.0f, 2.5f);

  // WHEN: we look for the best direction to fly towards
  bool res = getDirectionFromTree(p, path_node_positions, postion, goal);
  bool res1 = getDirectionFromTree(p1, path_node_positions, postion1, goal);
  bool res2 = getDirectionFromTree(p2, path_node_positions, postion2, goal);

  // THEN: we expect a direction in between node n1 and n2 for position, between
  // node n3 and n4 for position1, and not to get an available tree for the
  // position2
  ASSERT_TRUE(res);
  EXPECT_NEAR(45.0f, p.e, 1.f);
  EXPECT_NEAR(57.0f, p.z, 1.f);

  ASSERT_TRUE(res1);
  EXPECT_NEAR(0.0f, p1.e, 1.f);
  EXPECT_NEAR(72.0f, p1.z, 1.f);

  ASSERT_FALSE(res2);
}

TEST(PlannerFunctions, padPolarMatrixAzimuthWrapping) {
  // GIVEN: a matrix with known data. Where every cell has the value of its
  // column index.
  // And by how many lines should be padded
  int n_lines_padding = 3;
  Eigen::MatrixXf matrix;
  matrix.resize(GRID_LENGTH_E, GRID_LENGTH_Z);
  matrix.fill(1.0);
  for (int c = 0; c < matrix.cols(); c++) {
    matrix.col(c) = c * matrix.col(c);
  }

  // WHEN: we pad the matrix
  Eigen::MatrixXf matrix_padded;
  padPolarMatrix(matrix, n_lines_padding, matrix_padded);

  // THEN: the output matrix should have the right size,
  // the middle part should be equal to the original matrix,
  // and the wrapping around the azimuth angle should be correct.

  ASSERT_EQ(GRID_LENGTH_E + 2 * n_lines_padding, matrix_padded.rows());
  ASSERT_EQ(GRID_LENGTH_Z + 2 * n_lines_padding, matrix_padded.cols());

  bool middle_part_correct =
      matrix_padded.block(n_lines_padding, n_lines_padding, matrix.rows(),
                          matrix.cols()) == matrix;
  bool col_0_correct = matrix_padded.col(0) == matrix_padded.col(GRID_LENGTH_Z);
  bool col_1_correct =
      matrix_padded.col(1) == matrix_padded.col(GRID_LENGTH_Z + 1);
  bool col_2_correct =
      matrix_padded.col(2) == matrix_padded.col(GRID_LENGTH_Z + 2);
  bool col_63_correct =
      matrix_padded.col(GRID_LENGTH_Z + 3) == matrix_padded.col(3);
  bool col_64_correct =
      matrix_padded.col(GRID_LENGTH_Z + 4) == matrix_padded.col(4);
  bool col_65_correct =
      matrix_padded.col(GRID_LENGTH_Z + 5) == matrix_padded.col(5);

  EXPECT_TRUE(middle_part_correct);
  EXPECT_TRUE(col_0_correct);
  EXPECT_TRUE(col_1_correct);
  EXPECT_TRUE(col_2_correct);
  EXPECT_TRUE(col_63_correct);
  EXPECT_TRUE(col_64_correct);
  EXPECT_TRUE(col_65_correct);
}

TEST(PlannerFunctions, padPolarMatrixElevationWrapping) {
  // GIVEN: a matrix with known data. Where every cell has the value of its
  // column index.
  // And by how many lines should be padded
  int n_lines_padding = 2;
  Eigen::MatrixXf matrix;
  matrix.resize(GRID_LENGTH_E, GRID_LENGTH_Z);
  matrix.fill(0.0);
  int half_z = GRID_LENGTH_Z / 2;
  int last_z = GRID_LENGTH_Z - 1;
  int last_e = GRID_LENGTH_E - 1;

  matrix(0, 0) = 1;
  matrix(0, 1) = 2;
  matrix(1, 0) = 3;
  matrix(0, half_z) = 4;
  matrix(1, half_z + 1) = 5;
  matrix(0, last_z) = 6;
  matrix(last_e, 0) = 7;
  matrix(last_e - 1, half_z - 1) = 8;
  matrix(last_e, last_z) = 9;

  // WHEN: we pad the matrix
  Eigen::MatrixXf matrix_padded;
  padPolarMatrix(matrix, n_lines_padding, matrix_padded);

  // THEN: the output matrix should have the right size,
  // the middle part should be equal to the original matrix,
  // and the wrapping around the elevation angle should be correct.

  ASSERT_EQ(GRID_LENGTH_E + 2 * n_lines_padding, matrix_padded.rows());
  ASSERT_EQ(GRID_LENGTH_Z + 2 * n_lines_padding, matrix_padded.cols());

  bool middle_part_correct =
      matrix_padded.block(n_lines_padding, n_lines_padding, matrix.rows(),
                          matrix.cols()) == matrix;
  bool val_1 = matrix_padded(1, half_z + n_lines_padding) == 1;
  bool val_2 = matrix_padded(1, half_z + n_lines_padding + 1) == 2;
  bool val_3 = matrix_padded(0, half_z + n_lines_padding) == 3;
  bool val_4 = matrix_padded(1, 2) == 4;
  bool val_5 = matrix_padded(0, 3) == 5;
  bool val_6 = matrix_padded(1, half_z - 1 + n_lines_padding) == 6;
  bool val_7 = matrix_padded(last_e + 1 + n_lines_padding,
                             half_z + n_lines_padding) == 7;
  bool val_8 = matrix_padded(last_e + 2 + n_lines_padding,
                             GRID_LENGTH_Z + n_lines_padding - 1) == 8;
  bool val_9 = matrix_padded(last_e + 1 + n_lines_padding,
                             half_z - 1 + n_lines_padding) == 9;

  EXPECT_TRUE(middle_part_correct);
  EXPECT_TRUE(val_1);
  EXPECT_TRUE(val_2);
  EXPECT_TRUE(val_3);
  EXPECT_TRUE(val_4);
  EXPECT_TRUE(val_5);
  EXPECT_TRUE(val_6);
  EXPECT_TRUE(val_7);
  EXPECT_TRUE(val_8);
  EXPECT_TRUE(val_9);
}

TEST(PlannerFunctions, getBestCandidatesFromCostMatrix) {
  // GIVEN: a known cost matrix and the number of needed candidates
  int n_candidates = 4;
  std::vector<candidateDirection> candidate_vector;
  Eigen::MatrixXf matrix;
  matrix.resize(GRID_LENGTH_E, GRID_LENGTH_Z);
  matrix.fill(10);

  matrix(0, 2) = 1.1;
  matrix(0, 1) = 2.5;
  matrix(1, 2) = 3.8;
  matrix(1, 0) = 4.7;
  matrix(2, 2) = 4.9;

  // WHEN: calculate the candidates from the matrix
  getBestCandidatesFromCostMatrix(matrix, n_candidates, candidate_vector);

  // THEN: the output vector should have the right candidates in the right order

  ASSERT_EQ(n_candidates, candidate_vector.size());

  EXPECT_FLOAT_EQ(1.1, candidate_vector[0].cost);
  EXPECT_FLOAT_EQ(2.5, candidate_vector[1].cost);
  EXPECT_FLOAT_EQ(3.8, candidate_vector[2].cost);
  EXPECT_FLOAT_EQ(4.7, candidate_vector[3].cost);
}

TEST(PlannerFunctions, smoothPolarMatrix) {
  // GIVEN: a smoothing radius and a known cost matrix with one costly cell,
  // otherwise all zeros.
  unsigned int smooth_radius = 2;
  Eigen::MatrixXf matrix;
  Eigen::MatrixXf matrix_old;
  matrix.resize(GRID_LENGTH_E, GRID_LENGTH_Z);
  matrix.fill(0);

  int r_object = GRID_LENGTH_E / 2;
  int c_object = GRID_LENGTH_Z / 2;
  matrix(r_object, c_object) = 100;

  // WHEN: we calculate the smoothed matrix
  matrix_old = matrix;
  smoothPolarMatrix(matrix, smooth_radius);

  // THEN: The smoothed matrix should be element-wise greater-equal than the
  // input matrix
  // and the elements inside the smoothing radius around the costly cell should
  // be greater than before
  for (int r = r_object - smooth_radius; r < r_object + smooth_radius; r++) {
    for (int c = c_object - smooth_radius; c < c_object + smooth_radius; c++) {
      if (!(r == r_object && c == c_object)) {
        EXPECT_GT(matrix(r, c), matrix_old(r, c));
      }
    }
  }
  bool greater_equal = (matrix.array() >= matrix_old.array()).all();
  EXPECT_TRUE(greater_equal);
}

TEST(PlannerFunctions, smoothMatrix) {
  // GIVEN: a matrix with a single cell set
  unsigned int smooth_radius = 4;
  Eigen::MatrixXf matrix;
  matrix.resize(10, 20);
  matrix.fill(0);
  matrix(3, 16) = 100;
  matrix(6, 6) = -100;

  // WHEN: we smooth it
  Eigen::MatrixXf matrix_old = matrix;
  smoothPolarMatrix(matrix, smooth_radius);

  // THEN: it should match the matrix we expect
  Eigen::MatrixXf expected_matrix(matrix.rows(), matrix.cols());
  // clang-format off
  expected_matrix <<
   8, 0,   4,   8,  12,  16,  20,  16,  12,   8,   4, 0,  8, 16,  24,  32,  40,  32,  24, 16,
  12, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0, 12, 24,  36,  48,  60,  48,  36, 24,
  16, 0,  -4,  -8, -12, -16, -20, -16, -12,  -8,  -4, 0, 16, 32,  48,  64,  80,  64,  48, 32,
  20, 0,  -8, -16, -24, -32, -40, -32, -24, -16,  -8, 0, 20, 40,  60,  80, 100,  80,  60, 40,
  16, 0, -12, -24, -36, -48, -60, -48, -36, -24, -12, 0, 16, 32,  48,  64,  80,  64,  48, 32,
  12, 0, -16, -32, -48, -64, -80, -64, -48, -32, -16, 0, 12, 24,  36,  48,  60,  48,  36, 24,
   8, 0, -20, -40, -60, -80,-100, -80, -60, -40, -20, 0,  8, 16,  24,  32,  40,  32,  24, 16,
   4, 0, -16, -32, -48, -64, -80, -64, -48, -32, -16, 0,  4,  8,  12,  16,  20,  16,  12,  8,
   0, 0, -12, -24, -36, -48, -60, -48, -36, -24, -12, 0,  0,  0,   0,   0,   0,   0,   0,  0,
  -4, 0,  -8, -16, -24, -32, -40, -32, -24, -16,  -8, 0, -4, -8, -12, -16, -20, -16, -12, -8;
  // clang-format on

  EXPECT_LT((expected_matrix - matrix).cwiseAbs().maxCoeff(), 1e-5);
}

TEST(PlannerFunctions, getCostMatrixNoObstacles) {
  // GIVEN: a position, goal and an empty histogram
  Eigen::Vector3f position(0.f, 0.f, 0.f);
  Eigen::Vector3f goal(0.f, 5.f, 0.f);
  Eigen::Vector3f last_sent_waypoint(0.f, 1.f, 0.f);
  float heading = 0.f;
  costParameters cost_params;
  cost_params.goal_cost_param = 2.f;
  cost_params.smooth_cost_param = 1.5f;
  cost_params.height_change_cost_param = 4.f;
  cost_params.height_change_cost_param_adapted = 4.f;
  Eigen::MatrixXf cost_matrix;
  Histogram histogram = Histogram(ALPHA_RES);
  float smoothing_radius = 30.f;

  // WHEN: we calculate the cost matrix from the input data
  std::vector<uint8_t> cost_image_data;
  getCostMatrix(histogram, goal, position, heading, last_sent_waypoint,
                cost_params, false, smoothing_radius, cost_matrix,
                cost_image_data);

  // THEN: The minimum cost should be in the direction of the goal
  PolarPoint best_pol = cartesianToPolar(goal, position);
  Eigen::Vector2i best_index = polarToHistogramIndex(best_pol, ALPHA_RES);

  Eigen::MatrixXf::Index minRow, minCol;
  float min = cost_matrix.minCoeff(&minRow, &minCol);

  EXPECT_NEAR((int)minRow, best_index.y(), 1);
  EXPECT_NEAR((int)minCol, best_index.x(), 1);

  // And the cost should grow as we go away from the goal index
  int check_radius = 3;
  Eigen::MatrixXf matrix_padded;
  // extend the matrix, as the goal direction might be at the border
  padPolarMatrix(cost_matrix, check_radius, matrix_padded);

  int best_index_padded_e = (int)minRow + check_radius;
  int best_index_padded_z = (int)minCol + check_radius;

  // check that rows farther away have bigger cost. Leave out direct neighbor
  // rows as the center might be split over cells
  bool row1 = (matrix_padded.row(best_index_padded_e + 2).array() >
               matrix_padded.row(best_index_padded_e + 1).array())
                  .all();
  bool row2 = (matrix_padded.row(best_index_padded_e + 3).array() >
               matrix_padded.row(best_index_padded_e + 2).array())
                  .all();
  bool row3 = (matrix_padded.row(best_index_padded_e - 2).array() >
               matrix_padded.row(best_index_padded_e).array() - 1)
                  .all();
  bool row4 = (matrix_padded.row(best_index_padded_e - 3).array() >
               matrix_padded.row(best_index_padded_e).array() - 2)
                  .all();

  // check that columns farther away have bigger cost. Leave out direct neighbor
  // rows as the center might be split over cells
  Eigen::MatrixXf matrix_padded2 =
      matrix_padded.block(3, 3, cost_matrix.rows(),
                          cost_matrix.cols());  // cut off padded top part
  bool col1 = (matrix_padded2.col(best_index_padded_z + 10).array() >
               matrix_padded2.col(best_index_padded_z + 1).array())
                  .all();
  bool col2 = (matrix_padded2.col(best_index_padded_z + 20).array() >
               matrix_padded2.col(best_index_padded_z + 10).array())
                  .all();
  bool col3 = (matrix_padded2.col(best_index_padded_z - 10).array() >
               matrix_padded2.col(best_index_padded_z).array() - 1)
                  .all();
  bool col4 = (matrix_padded2.col(best_index_padded_z - 20).array() >
               matrix_padded2.col(best_index_padded_z).array() - 10)
                  .all();

  EXPECT_TRUE(col1);
  EXPECT_TRUE(col2);
  EXPECT_TRUE(col3);
  EXPECT_TRUE(col4);
  EXPECT_TRUE(row1);
  EXPECT_TRUE(row2);
  EXPECT_TRUE(row3);
  EXPECT_TRUE(row4);
}

TEST(PlannerFunctions, CostfunctionGoalCost) {
  // GIVEN: a scenario with two different goal locations
  Eigen::Vector3f position(0.f, 0.f, 0.f);
  Eigen::Vector3f goal_1(0.f, 5.f, 0.f);
  Eigen::Vector3f goal_2(3.f, 3.f, 0.f);
  Eigen::Vector3f last_sent_waypoint(0.f, 1.f, 0.f);
  float heading = 0.f;
  costParameters cost_params;
  cost_params.goal_cost_param = 3.f;
  cost_params.heading_cost_param = 0.5f;
  cost_params.smooth_cost_param = 1.5f;
  cost_params.height_change_cost_param = 4.f;
  cost_params.height_change_cost_param_adapted = 4.f;
  float obstacle_distance = 0.f;
  float distance_cost_1, other_costs_1, distance_cost_2, other_costs_2;

  Eigen::Vector2f candidate_1(0.f, 0.f);

  // WHEN: we calculate the cost of one cell for the same scenario but with two
  // different goals
  costFunction(candidate_1.y(), candidate_1.x(), obstacle_distance, goal_1,
               position, heading, last_sent_waypoint, cost_params,
               distance_cost_1, other_costs_1);
  costFunction(candidate_1.y(), candidate_1.x(), obstacle_distance, goal_2,
               position, heading, last_sent_waypoint, cost_params,
               distance_cost_2, other_costs_2);

  // THEN: The cost in the case where the goal is in the cell direction should
  // be lower
  EXPECT_LT(other_costs_1, other_costs_2);
}

TEST(PlannerFunctions, CostfunctionDistanceCost) {
  // GIVEN: a scenario with two different obstacle distances
  Eigen::Vector3f position(0.f, 0.f, 0.f);
  Eigen::Vector3f goal(0.f, 5.f, 0.f);
  Eigen::Vector3f last_sent_waypoint(0.f, 1.f, 0.f);
  float heading = 0.f;
  costParameters cost_params;
  cost_params.goal_cost_param = 3.f;
  cost_params.heading_cost_param = 0.5f;
  cost_params.smooth_cost_param = 1.5f;
  cost_params.height_change_cost_param = 4.f;
  cost_params.height_change_cost_param_adapted = 4.f;
  float distance_1 = 0.f;
  float distance_2 = 3.f;
  float distance_3 = 5.f;
  float distance_cost_1, distance_cost_2, distance_cost_3, other_costs;

  Eigen::Vector2f candidate_1(0.f, 0.f);

  // WHEN: we calculate the cost of one cell for the same scenario but with two
  // different obstacle distance
  costFunction(candidate_1.y(), candidate_1.x(), distance_1, goal, position,
               heading, last_sent_waypoint, cost_params, distance_cost_1,
               other_costs);
  costFunction(candidate_1.y(), candidate_1.x(), distance_2, goal, position,
               heading, last_sent_waypoint, cost_params, distance_cost_2,
               other_costs);
  costFunction(candidate_1.y(), candidate_1.x(), distance_3, goal, position,
               heading, last_sent_waypoint, cost_params, distance_cost_3,
               other_costs);

  // THEN: The distance cost for no obstacle should be zero and the distance
  // cost for the closer obstacle should be bigger
  EXPECT_LT(distance_cost_1, distance_cost_2);
  EXPECT_LT(distance_cost_3, distance_cost_2);
  EXPECT_FLOAT_EQ(distance_cost_1, 0.f);
}

TEST(PlannerFunctions, CostfunctionHeadingCost) {
  // GIVEN: a scenario with two different initial headings
  Eigen::Vector3f position(0.f, 0.f, 0.f);
  Eigen::Vector3f goal(0.f, 5.f, 0.f);
  Eigen::Vector3f last_sent_waypoint(0.f, 1.f, 0.f);
  float heading_1 = 10.f;
  float heading_2 = 30.f;
  costParameters cost_params;
  cost_params.goal_cost_param = 3.f;
  cost_params.heading_cost_param = 0.5f;
  cost_params.smooth_cost_param = 1.5f;
  cost_params.height_change_cost_param = 4.f;
  cost_params.height_change_cost_param_adapted = 4.f;
  float obstacle_distance = 0.f;
  float distance_cost, other_costs_1, other_costs_2;

  Eigen::Vector2f candidate_1(0.f, 0.f);

  // WHEN: we calculate the cost of one cell for the same scenario but with two
  // different initial headings
  costFunction(candidate_1.y(), candidate_1.x(), obstacle_distance, goal,
               position, heading_1, last_sent_waypoint, cost_params,
               distance_cost, other_costs_1);
  costFunction(candidate_1.y(), candidate_1.x(), obstacle_distance, goal,
               position, heading_2, last_sent_waypoint, cost_params,
               distance_cost, other_costs_2);

  // THEN: The cost in the case where the initial heading is closer to the
  // candidate should be lower
  EXPECT_LT(other_costs_1, other_costs_2);
}

TEST(PlannerFunctions, CostfunctionSmoothingCost) {
  // GIVEN: a scenario with two different initial headings
  Eigen::Vector3f position(0.f, 0.f, 0.f);
  Eigen::Vector3f goal(0.f, 5.f, 0.f);
  Eigen::Vector3f last_sent_waypoint_1(1.f, 2.f, 0.f);
  Eigen::Vector3f last_sent_waypoint_2(1.5f, 1.5f, 0.f);
  float heading = 0.f;
  costParameters cost_params;
  cost_params.goal_cost_param = 3.f;
  cost_params.heading_cost_param = 0.5f;
  cost_params.smooth_cost_param = 1.5f;
  cost_params.height_change_cost_param = 4.f;
  cost_params.height_change_cost_param_adapted = 4.f;
  float obstacle_distance = 0.f;
  float distance_cost, other_costs_1, other_costs_2;

  Eigen::Vector2f candidate_1(0.f, 0.f);

  // WHEN: we calculate the cost of one cell for the same scenario but with two
  // different last waypoints
  costFunction(candidate_1.y(), candidate_1.x(), obstacle_distance, goal,
               position, heading, last_sent_waypoint_1, cost_params,
               distance_cost, other_costs_1);
  costFunction(candidate_1.y(), candidate_1.x(), obstacle_distance, goal,
               position, heading, last_sent_waypoint_2, cost_params,
               distance_cost, other_costs_2);

  // THEN: The cost in the case where the last waypoint is closer to the
  // candidate should be lower
  EXPECT_LT(other_costs_1, other_costs_2);
}

TEST(Histogram, HistogramDownsampleCorrectUsage) {
  // GIVEN: a histogram of the correct resolution
  Histogram histogram = Histogram(ALPHA_RES);
  histogram.set_dist(0, 0, 1.3);
  histogram.set_dist(1, 0, 1.3);
  histogram.set_dist(0, 1, 1.3);
  histogram.set_dist(1, 1, 1.3);

  // WHEN: we downsample the histogram to have a larger bin size
  histogram.downsample();

  // THEN: The downsampled histogram should fuse four cells of the regular
  // resolution histogram into one
  for (int i = 0; i < GRID_LENGTH_E / 2; ++i) {
    for (int j = 0; j < GRID_LENGTH_Z / 2; ++j) {
      if (i == 0 && j == 0) {
        EXPECT_FLOAT_EQ(1.3, histogram.get_dist(i, j));
      } else if (i == 1 && j == 1) {
        EXPECT_FLOAT_EQ(0.0, histogram.get_dist(i, j));
      } else {
        EXPECT_FLOAT_EQ(0.0, histogram.get_dist(i, j));
      }
    }
  }
}

TEST(Histogram, HistogramUpsampleCorrectUsage) {
  // GIVEN: a histogram of the correct resolution
  Histogram histogram = Histogram(ALPHA_RES * 2);
  histogram.set_dist(0, 0, 1.3);

  // WHEN: we upsample the histogram to have regular bin size
  histogram.upsample();

  // THEN: The upsampled histogram should split every cell of the lower
  // resolution histogram into four cells
  for (int i = 0; i < GRID_LENGTH_E; ++i) {
    for (int j = 0; j < GRID_LENGTH_Z; ++j) {
      if ((i == 0 && j == 0) || (i == 1 && j == 0) || (i == 0 && j == 1) ||
          (i == 1 && j == 1)) {
        EXPECT_FLOAT_EQ(1.3, histogram.get_dist(i, j));
      } else if ((i == 2 && j == 2) || (i == 2 && j == 3) ||
                 (i == 3 && j == 2) || (i == 3 && j == 3)) {
        EXPECT_FLOAT_EQ(0.0, histogram.get_dist(i, j));
      } else {
        EXPECT_FLOAT_EQ(0.0, histogram.get_dist(i, j));
      }
    }
  }
}

TEST(Histogram, HistogramUpDownpsampleInorrectUsage) {
  // GIVEN: a histogram of the correct resolution
  Histogram low_res_histogram = Histogram(ALPHA_RES * 2);
  Histogram high_res_histogram = Histogram(ALPHA_RES);

  // THEN: We expect the functions to throw if the input histogram has the wrong
  // resolution
  EXPECT_THROW(low_res_histogram.downsample(), std::logic_error);
  EXPECT_THROW(high_res_histogram.upsample(), std::logic_error);
}

TEST(Histogram, HistogramisEmpty) {
  // GIVEN: a histogram
  Histogram histogram = Histogram(ALPHA_RES);
  // Set a cell
  histogram.set_dist(0, 0, 1.3);
  EXPECT_FALSE(histogram.isEmpty());

  // Set the cell dist to 0.f
  histogram.set_dist(0, 0, 0.f);
  EXPECT_TRUE(histogram.isEmpty());
}
