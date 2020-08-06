#ifndef KINMATICCHAINKDL_HPP
#define KINMATICCHAINKDL_HPP

#include <memory>
#include <kdl/frameacc.hpp>
#include <kdl/chain.hpp>
#include <kdl/jacobian.hpp>
#include <kdl/jntarray.hpp>
#include <kdl/jntarrayacc.hpp>
#include <base/Time.hpp>
#include <base/samples/RigidBodyStateSE3.hpp>
#include <kdl/jntspaceinertiamatrix.hpp>

namespace base{
    namespace samples{
        class Joints;
    }
}
namespace wbc{

/**
 * @brief Helper class for storing information of a KDL chain in the robot model
*/
class KinematicChainKDL{

protected:
    base::samples::RigidBodyStateSE3 cartesian_state;

public:
    KinematicChainKDL(const KDL::Chain &chain, const std::string &root_frame, const std::string &tip_frame);

    /**
     * @brief Update all joints of the kinematic chain
     * @param joint_state Has to contain at least all joints that are included in the kinematic chain. Each entry has to have a valid position and velocity
     */
    void update(const base::samples::Joints& joint_state, const base::Vector3d& gravity = base::Vector3d(0,0,9.81));
    /** Convert and return current Cartesian state*/
    const base::samples::RigidBodyStateSE3& rigidBodyState();

    KDL::Frame pose_kdl;                             /** KDL Pose of the tip segment in root coordinate of the chain*/
    KDL::FrameVel frame_vel;                         /** Helper for the velocity fk*/
    KDL::Twist twist_kdl;                            /** KDL Pose of the tip segment in root coordinate of the chain*/
    base::Vector6d acc;                              /** Helper to store current frame acceleration*/
    KDL::Chain chain;                                /** The underlying KDL chain*/
    KDL::JntArrayVel jnt_array_vel;                  /** Vector of positions and velocities of all included joints*/
    KDL::JntArrayAcc jnt_array_acc;                  /** Vector of positions, velocities and accelerations of all included joints*/
    KDL::Jacobian jacobian;                          /** Jacobian of the Chain. Reference frame is root & reference point is tip*/
    KDL::Jacobian jacobian_dot;                      /** Derivative of Jacobian of the Chain. Reference frame & reference point is the root frame*/
    std::vector<std::string> joint_names;            /** Names of the joint included in the kinematic chain*/
    KDL::JntArray coriolis_torque;                   /** Vector of coriolis terms (Nx1)*/
    KDL::JntArray gravitation_torque;                /** Vector of gravity terms (Nx1)*/
    KDL::JntSpaceInertiaMatrix jnt_space_inertia;    /** Jointspace inertia matrix H (NxN)*/
    std::string root_frame;                          /** UID of the kinematics chain root link*/
    std::string tip_frame;                           /** UID of the kinematics chain tip link*/

};

} // namespace wbc
#endif
