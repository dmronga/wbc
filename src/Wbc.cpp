#include "Wbc.hpp"
#include "TaskFrame.hpp"
#include "RobotModel.hpp"
#include "HierarchicalWDLSSolver.hpp"
#include <kdl/utilities/svd_eigen_HH.hpp>
#include <base/logging.h>

using namespace std;

Wbc::Wbc(KDL::Tree tree, const WBC_TYPE wbc_type){
    configured_ = false;
    robot_ = new RobotModel(tree);
    wbc_type_= wbc_type;
    temp_ = Eigen::VectorXd(6);
    solver_ = new HierarchicalWDLSSolver();
}

Wbc::~Wbc(){
    for(uint i = 0; i < sub_task_vector_.size(); i++ ){
        for(uint j = 0; j < sub_task_vector_[i].size(); j++)
            delete sub_task_vector_[i][j];
    }

    sub_task_vector_.clear();

    delete solver_;
    delete robot_;
}

bool Wbc::configure(const WbcConfig &config){

    if(config.empty())
        throw std::runtime_error("Empty sub task vector");

    std::vector<uint> ny_per_priority(config.size(),0); //Number of task variables per priority. This will configure the solver
    sub_task_vector_.resize(config.size());

    //Walk through all priorities
    for(uint prio = 0; prio < config.size(); prio++ ){

        //Walk through all sub tasks of this priority
        for(uint i = 0; i < config[prio].size(); i++){

            SubTaskConfig sub_task_conf = config[prio][i];
            if(sub_task_conf.task_type_ == WBC_TASK_TYPE_CARTESIAN){

                if(!robot_->addTaskFrame(sub_task_conf.root_frame_))
                    return false;
                if(!robot_->addTaskFrame(sub_task_conf.tip_frame_))
                    return false;
            }
            else{
                //Check valid config
                if(sub_task_conf.joints_.empty()){
                    if(sub_task_conf.no_task_variables_ != robot_->no_of_joints_){
                        LOG_ERROR("Invalid Sub task configuration: Joint name vector is empty, no of task variables is %i, but no of robot joints is %i",
                                  sub_task_conf.no_task_variables_, robot_->no_of_joints_)
                        throw std::invalid_argument("Invalid Sub task configuration");
                    }
                }
                else{
                    if(sub_task_conf.no_task_variables_ != sub_task_conf.joints_.size()){
                        LOG_ERROR("Invalid Sub task configuration: Joint name vector has size %i, but no of task variables is %i",
                                  sub_task_conf.joints_.size(), sub_task_conf.no_task_variables_)
                        throw std::invalid_argument("Invalid Sub task configuration");
                    }
                }
            }

            SubTask* sub_task = new SubTask(sub_task_conf, robot_->no_of_joints_);
            sub_task_vector_[prio].push_back(sub_task);
            ny_per_priority[prio] += sub_task_conf.no_task_variables_;
        }
    }

    //Resize matrices that describe the equation system
    for(uint prio = 0; prio < ny_per_priority.size(); prio++){
        Eigen::MatrixXd A(ny_per_priority[prio], robot_->no_of_joints_);
        Eigen::VectorXd y(ny_per_priority[prio]);
        Eigen::VectorXd Wy(ny_per_priority[prio]);

        Wy.setConstant(1); //Set all task weights to 1 in the beginning
        A.setZero();
        y.setZero();

        A_.push_back(A);
        y_.push_back(y);
        Wy_.push_back(Wy);
    }

    if(!solver_->configure(ny_per_priority, robot_->no_of_joints_))
        return false;

    LOG_DEBUG("Configured solver:");
    for(uint prio = 0; prio < ny_per_priority.size(); prio++)
        LOG_DEBUG("Priority: %i Task variables: %i", prio, ny_per_priority[prio]);
    LOG_DEBUG("No of robot joints: %i\n", robot_->no_of_joints_);

    solver_output_.resize(robot_->no_of_joints_);
    configured_ = true;
    return true;
}


void Wbc::updateSubTask(SubTask* sub_task){

    switch(wbc_type_){

    case WBC_TYPE_VELOCITY:{

        uint nc = sub_task->config_.no_task_variables_;
        if(sub_task->config_.task_type_ == WBC_TASK_TYPE_CARTESIAN){

            TaskFrame* tf_root = robot_->getTaskFrame(sub_task->config_.root_frame_);
            TaskFrame* tf_tip = robot_->getTaskFrame(sub_task->config_.tip_frame_);

            if(tf_root && tf_tip){ //Task is in Cartesian space
                sub_task->pose_ = tf_root->pose_.Inverse() * tf_tip->pose_;
                sub_task->task_jac_.data.setIdentity();
                sub_task->task_jac_.changeRefPoint(-sub_task->pose_.p);
                sub_task->task_jac_.changeRefFrame(tf_root->pose_);

                //Invert Task Jacobian
                KDL::svd_eigen_HH(sub_task->task_jac_.data, sub_task->Uf_, sub_task->Sf_, sub_task->Vf_, temp_);

                for (unsigned int j = 0; j < sub_task->Sf_.size(); j++){
                    if (sub_task->Sf_(j) > 0)
                        sub_task->Uf_.col(j) *= 1 / sub_task->Sf_(j);
                    else
                        sub_task->Uf_.col(j).setZero();
                }
                sub_task->H_ = (sub_task->Vf_ * sub_task->Uf_.transpose());


                ///// A = J^(-1) *J_tf_tip - J^(-1) * J_tf_root: Inverse Task Jacobian * Robot Jacobian of object frame one
                sub_task->A_ = sub_task->H_.block(0, 0, nc, 6) * tf_tip->jac_robot_.data
                        -(sub_task->H_.block(0, 0, nc, 6) * tf_root->jac_robot_.data);
            }
            else{
                LOG_ERROR("Subtask defines task frames %s and %s but at least one of them has not been added to robot model",
                          sub_task->config_.root_frame_.c_str(), sub_task->config_.tip_frame_.c_str());
                throw std::runtime_error("Invalid sub task");
            }
        }
        else if(sub_task->config_.task_type_ == WBC_TASK_TYPE_JOINT){

            if(sub_task->config_.joints_.empty())
                sub_task->A_ = Eigen::MatrixXd::Identity(nc, robot_->no_of_joints_);
            else{

                sub_task->A_ = Eigen::MatrixXd::Zero(nc, robot_->no_of_joints_);

                for(uint i = 0; i < sub_task->config_.joints_.size(); i++){
                    std::string joint_name = sub_task->config_.joints_[i];
                    uint idx = robot_->joint_index_map_[joint_name];
                    sub_task->A_(i,idx) = 1.0;
                }
            }
        }
        break;
    }
    case WBC_TYPE_TORQUE:{ //TODO
        break;
    }
    default: {
        LOG_ERROR("Invalid WBC type: %i", wbc_type_);
        throw std::invalid_argument("Invalid wbc type");
    }
    }//Switch
}

void Wbc::solve(const WbcInput& task_ref,
                const WbcInput& task_weights,
                const Eigen::VectorXd joint_weights,
                const base::samples::Joints &robot_status,
                base::commands::Joints &solver_output){

    if(!configured_)
        throw std::invalid_argument("Configure has not been called yet");

    //Check valid input
    uint nq_robot = robot_->no_of_joints_;
    if(task_ref.size() != sub_task_vector_.size() ||
       task_weights.size() != sub_task_vector_.size()){
        LOG_ERROR("Invalid input vector size: Ref size: %i, task Weight size: %i, No of priorities in wbc: %i",
                  task_ref.size(), task_weights.size(), sub_task_vector_.size());
        throw std::invalid_argument("Invalid input vector size");
    }
    if(joint_weights.size() != nq_robot){
        LOG_ERROR("Invalid input vector size: No of robot joints: %i, no of joint weights: %i",
                  robot_->no_of_joints_, joint_weights.size());
        throw std::invalid_argument("Invalid input vector size");
    }
    if(robot_status.size() != nq_robot){
        LOG_ERROR("Input robot status size. No of joints is %i but no of robot joints is %i",
                  robot_status.size(), robot_->no_of_joints_);
        throw std::invalid_argument("Invalid number of joints in robot status");
    }
    if(solver_output.size() != nq_robot){
        LOG_WARN("Solver output vector size is %i but total no of robot joints in wbc is %i. Will do a dynamic resize",
                 solver_output.size(), nq_robot);
        solver_output.resize(nq_robot);
    }

    //Update robot model: This will compute Kinematics and dynamics of all task frames
    robot_->update(robot_status);

    //Walk through all priorities and update equation system
    for(uint prio = 0; prio < sub_task_vector_.size(); prio++){

        if(task_ref[prio].size() != sub_task_vector_[prio].size() ||
           task_weights[prio].size() != sub_task_vector_[prio].size()){
                LOG_ERROR("Invalid input vector size: Priority %i defines %i sub tasks, but input size is: Ref: %i, Task Weights: %i",
                          prio, sub_task_vector_[prio].size(), task_ref[prio].size(), task_weights[prio].size());
                throw std::invalid_argument("Invalid input vector size");
            }

        //Walk through all tasks of current priority
        uint row_index = 0;
        for(uint i = 0; i < sub_task_vector_[prio].size(); i++){

            SubTask *sub_task = sub_task_vector_[prio][i];
            uint nc = sub_task->config_.no_task_variables_;

            if(nc != task_ref[prio][i].rows() ||
               nc != task_weights[prio][i].rows()){
                LOG_ERROR("Invalid input vector size: Task %i of Priority %i has %i task variables, but input size is: Ref: %i, Task Weights: %i",
                          i, prio, nc, task_ref[prio][i].rows(), task_weights[prio][i].rows());
                throw std::invalid_argument("Invalid input vector size");
            }

            //compute equations for single sub task
            updateSubTask(sub_task);

            //insert task equation into equation system of current priority
            Wy_[prio].segment(row_index, nc) = task_weights[prio][i];
            y_[prio].segment(row_index, nc) = task_ref[prio][i];
            A_[prio].block(row_index, 0, nc, nq_robot) = sub_task->A_;

            row_index += nc;
        }

        //Set weights for current priority in solver
        solver_->setTaskWeights(Wy_[prio], prio);
    }

    //Pass joint weights to solver
    solver_->setJointWeights(joint_weights);

    //Solve equation system
    solver_->solve(A_, y_, solver_output_);

    //Write output
    switch(wbc_type_){
    case WBC_TYPE_VELOCITY:{
        for(uint i = 0; i < robot_->no_of_joints_; i++)
            solver_output[i].speed = solver_output_(i);
        break;
    }
    default:{
        LOG_ERROR("Invalid WBC type: %i", wbc_type_);
        throw std::invalid_argument("Invalid wbc type");
    }
    }

}

uint Wbc::getNoOfJoints(){
    return robot_->no_of_joints_;
}
