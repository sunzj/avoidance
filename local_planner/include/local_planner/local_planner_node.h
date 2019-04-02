#ifndef LOCAL_PLANNER_LOCAL_PLANNER_NODE_H
#define LOCAL_PLANNER_LOCAL_PLANNER_NODE_H

#include "local_planner/avoidance_output.h"
#include "local_planner/local_planner_visualization.h"

#ifndef DISABLE_SIMULATION
// include simulation
#include "local_planner/rviz_world_loader.h"
#endif

#include <geometry_msgs/Point.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/TransformStamped.h>
#include <mavros_msgs/Altitude.h>
#include <mavros_msgs/CompanionProcessStatus.h>
#include <mavros_msgs/Param.h>
#include <mavros_msgs/ParamGet.h>
#include <mavros_msgs/SetMode.h>
#include <mavros_msgs/State.h>
#include <mavros_msgs/Trajectory.h>
#include <pcl/filters/filter.h>
#include <pcl_conversions/pcl_conversions.h>  // fromROSMsg
#include <pcl_ros/point_cloud.h>
#include <pcl_ros/transforms.h>  // transformPointCloud
#include <ros/ros.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Range.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include <tf/transform_listener.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <Eigen/Core>
#include <boost/bind.hpp>

#include <dynamic_reconfigure/server.h>
#include <local_planner/LocalPlannerNodeConfig.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace avoidance {

class LocalPlanner;
class WaypointGenerator;

struct cameraData {
  std::string topic_;
  ros::Subscriber pointcloud_sub_;
  ros::Subscriber camera_info_sub_;
  sensor_msgs::PointCloud2 newest_cloud_msg_;

  std::unique_ptr<std::mutex> trans_ready_mutex_;
  std::unique_ptr<std::condition_variable> trans_ready_cv_;

  std::unique_ptr<std::mutex> cloud_ready_mutex_;
  std::unique_ptr<std::condition_variable> cloud_ready_cv_;
  std::thread transform_thread_;
  pcl::PointCloud<pcl::PointXYZ> pcl_cloud;

  bool received_;
  bool transformed_;
};

/**
* @brief struct to contain the parameters needed for the model based trajectory
*planning
* when MPC_AUTO_MODE is set to 1 (default) then all members are used for the
*jerk limited
* trajectory on the flight controller side
* when MPC_AUTO_MODE is set to 0, only up_accl, down_accl, xy_acc are used on
*the
* flight controller side
**/
struct ModelParameters {
  // clang-format off
  int mpc_auto_mode = 1; // Auto sub-mode - 0: default line tracking, 1 jerk-limited trajectory
  float jerk_min = 8.0f; // Velocity-based jerk limit
  float up_acc = 10.0f;   // Maximum vertical acceleration in velocity controlled modes upward
  float up_vel = 3.0f;   // Maximum vertical ascent velocity
  float down_acc = 10.0f; // Maximum vertical acceleration in velocity controlled modes down
  float down_vel = 1.0f; // Maximum vertical descent velocity
  float xy_acc = 5.0f;  // Maximum horizontal acceleration for auto mode and
                      // maximum deceleration for manual mode
  float xy_vel = 1.0f;   // Desired horizontal velocity in mission
  float takeoff_speed = 1.0f; // Takeoff climb rate
  float land_speed = 0.7f;   // Landing descend rate
  // limitations given by sensors
  float distance_sensor_max_height = 5.0f;
  float distance_sensor_max_vel = 5.0f;
  // clang-format on
};

enum class MAV_STATE {
  MAV_STATE_UNINIT,
  MAV_STATE_BOOT,
  MAV_STATE_CALIBRATIN,
  MAV_STATE_STANDBY,
  MAV_STATE_ACTIVE,
  MAV_STATE_CRITICAL,
  MAV_STATE_EMERGENCY,
  MAV_STATE_POWEROFF,
  MAV_STATE_FLIGHT_TERMINATION,
};

class LocalPlannerNode {
 public:
  LocalPlannerNode(const ros::NodeHandle& nh, const ros::NodeHandle& nh_private,
                   const bool tf_spin_thread = true);
  ~LocalPlannerNode();

  mavros_msgs::CompanionProcessStatus status_msg_;

  std::string world_path_;
  bool never_run_ = true;
  bool position_received_ = false;
  bool disable_rise_to_goal_altitude_;
  bool accept_goal_input_topic_;
  std::atomic<bool> should_exit_{false};

  std::vector<cameraData> cameras_;

  ModelParameters model_params_;

  ros::CallbackQueue pointcloud_queue_;
  ros::CallbackQueue main_queue_;

  geometry_msgs::PoseStamped hover_point_;
  geometry_msgs::PoseStamped newest_pose_;
  geometry_msgs::PoseStamped last_pose_;
  geometry_msgs::Point newest_waypoint_position_;
  geometry_msgs::Point last_waypoint_position_;
  geometry_msgs::Point newest_adapted_waypoint_position_;
  geometry_msgs::Point last_adapted_waypoint_position_;
  geometry_msgs::PoseStamped goal_msg_;

  ros::Time last_wp_time_;
  ros::Time t_status_sent_;

  std::unique_ptr<LocalPlanner> local_planner_;
  std::unique_ptr<WaypointGenerator> wp_generator_;
  LocalPlannerVisualization visualizer_;

#ifndef DISABLE_SIMULATION
  WorldVisualizer world_visualizer_;
#endif

  ros::Publisher mavros_pos_setpoint_pub_;
  ros::Publisher mavros_vel_setpoint_pub_;
  ros::Publisher mavros_obstacle_free_path_pub_;
  ros::Publisher mavros_obstacle_distance_pub_;
  ros::ServiceClient mavros_set_mode_client_;
  ros::ServiceClient get_px4_param_client_;
  ros::Publisher mavros_system_status_pub_;
  tf::TransformListener* tf_listener_;

  std::mutex running_mutex_;  ///< guard against concurrent access to input &
                              /// output data (point cloud, position, ...)

  std::mutex data_ready_mutex_;
  std::condition_variable data_ready_cv_;

  /**
  * @brief     handles threads for data publication and subscription
  **/
  void threadFunction();

  void updatePlanner();

  /**
  * @brief     checks if the transformation from the camera frame to
  *            local_origin is available at the pointcloud timestamp
  * @returns   true, if the transformation is available
  **/
  bool canUpdatePlannerInfo();

  /**
  * @brief     updates the local planner agorithm with the latest pointcloud,
  *            vehicle position, velocity, state, and distance to ground, goal,
  *            setpoint sent to the FCU
  **/
  void updatePlannerInfo();

  /**
  * @brief     computes the number of available pointclouds
  * @ returns  number of pointclouds
  **/
  size_t numReceivedClouds();

  /**
  * @brief     computes the number of transformed pointclouds
  * @ returns  number of transformed pointclouds
  **/
  size_t numTransformedClouds();

  /**
  * @brief     threads for transforming pointclouds
  **/
  void pointCloudTransformThread(int index);

  /**
  * @brief     transforms position setpoints from ROS message to MavROS message
  * @params[out] obst_avoid, position setpoint in MavROS message form
  * @params[in] pose, position setpoint computed by the planner
  **/
  void transformPoseToTrajectory(mavros_msgs::Trajectory& obst_avoid,
                                 geometry_msgs::PoseStamped pose);
  /**
  * @brief      transforms velocity setpoints from ROS message to MavROS
  *             message
  * @param[out] obst_avoid, velocity setpoint in MavROS message form
  * @param[in]  vel, velocity setpoint computd by the planner
  **/
  void transformVelocityToTrajectory(mavros_msgs::Trajectory& obst_avoid,
                                     geometry_msgs::Twist vel);

  /**
  * @brief      fills MavROS trajectory messages with NAN
  * @param      point, setpoint to be filled with NAN
  **/
  void fillUnusedTrajectoryPoint(mavros_msgs::PositionTarget& point);

  /**
  * @brief      calculates position and velocity setpoints and sends to the FCU
  * @param[in]  hover, true if the vehicle is loitering
  **/
  void calculateWaypoints(bool hover);

  /**
  * @brief      sends out a status to the FCU which will be received as a
  *heartbeat
  **/
  void publishSystemStatus();

  /**
  * @brief      check healthiness of the avoidance system to trigger failsafe in
  *             the FCU
  * @param[in]  since_last_cloud, time elapsed since the last waypoint was
  *             published to the FCU
  * @param[in]  since_start, time elapsed since staring the node
  * @param[out] planner_is_healthy, true if the planner is running without
  *errors
  * @param[out] hover, true if the vehicle is hovering
  **/
  void checkFailsafe(ros::Duration since_last_cloud, ros::Duration since_start,
                     bool& planner_is_healthy, bool& hover);

 private:
  ros::NodeHandle nh_;
  ros::NodeHandle nh_private_;

  avoidance::LocalPlannerNodeConfig rqt_param_config_;

  mavros_msgs::Altitude ground_distance_msg_;
  int path_length_ = 0;

  // Subscribers
  ros::Subscriber pose_sub_;
  ros::Subscriber velocity_sub_;
  ros::Subscriber state_sub_;
  ros::Subscriber clicked_point_sub_;
  ros::Subscriber clicked_goal_sub_;
  ros::Subscriber fcu_input_sub_;
  ros::Subscriber goal_topic_sub_;
  ros::Subscriber distance_sensor_sub_;
  ros::Subscriber px4_param_sub_;

  std::vector<float> algo_time;

  geometry_msgs::TwistStamped vel_msg_;
  bool armed_, offboard_, mission_, new_goal_;
  bool data_ready_ = false;

  dynamic_reconfigure::Server<avoidance::LocalPlannerNodeConfig>* server_;
  boost::recursive_mutex config_mutex_;

  /**
  * @brief     callaback for parameters dynamic reconfigure server
  * @param     config, struct with all the parameters
  * @param     level, bitmsak to group together reconfigurable parameters
  **/
  void dynamicReconfigureCallback(avoidance::LocalPlannerNodeConfig& config,
                                  uint32_t level);

  /**
  * @brief     subscribes to all the camera topics and camera info
  * @param     camera_topics, array with the pointcloud topics strings
  **/
  void initializeCameraSubscribers(std::vector<std::string>& camera_topics);

  /**
  * @brief     callaback for vehicle position and orientation
  * @param[in] msg, vehicle position and orientation in ENU frame
  **/
  void positionCallback(const geometry_msgs::PoseStamped& msg);

  /**
  * @brief     callaback for pointcloud
  * @param[in] msg, pointcloud message
  * @param[in] index, pointcloud instance number
  **/
  void pointCloudCallback(const sensor_msgs::PointCloud2::ConstPtr& msg,
                          int index);
  /**
  * @brief     callaback for camera information
  * @param[in] msg, camera information message
  * @param[in] index, camera info instace number
  **/
  void cameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr& msg,
                          int index);

  /**
  * @brief     callaback for vehicle velocity
  * @param[in] msg, vehicle velocity message
  **/
  void velocityCallback(const geometry_msgs::TwistStamped& msg);

  /**
  * @brief     callaback for vehicle state
  * @param[in] msg, vehicle position and orientation in ENU frame
  **/
  void stateCallback(const mavros_msgs::State& msg);

  /**
  * @brief     reads parameters from launch file and yaml file
  **/
  void readParams();

  /**
  * @brief     callaback for clicking cells in the polar histogram
  * @param[in] msg, vehicle position and orientation in ENU frame
  **/
  void clickedPointCallback(const geometry_msgs::PointStamped& msg);
  /**
  * @brief     callaback for selecting the goal by cliking on the position in
  *the Rviz visualization of the world
  * @param[in] msg, goal position
  **/
  void clickedGoalCallback(const geometry_msgs::PoseStamped& msg);
  /**
  * @brief     callaback
  * @param[in] msg,
  **/
  void updateGoalCallback(const visualization_msgs::MarkerArray& msg);
  /**
  * @brief     callaback for setting the goal from the FCU Mission Waypoints
  * @param[in] msg, current and next position goals
  **/
  void fcuInputGoalCallback(const mavros_msgs::Trajectory& msg);
  /**
  * @brief     callaback for distance to the ground
  * @param[in] msg, altitude message
  **/
  void distanceSensorCallback(const mavros_msgs::Altitude& msg);
  void px4ParamsCallback(const mavros_msgs::Param& msg);

  /**
  * @brief     callaback for vehicle state
  * @param[in] msg, vehicle position and orientation in ENU frame
  **/
  void printPointInfo(double x, double y, double z);

  /**
  * @brief     sends out emulated LaserScan data to the flight controller
  **/
  void publishLaserScan() const;
};
}
#endif  // LOCAL_PLANNER_LOCAL_PLANNER_NODE_H
