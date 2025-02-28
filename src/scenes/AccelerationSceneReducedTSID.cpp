#include "AccelerationSceneReducedTSID.hpp"
#include "core/RobotModel.hpp"
#include <base-logging/Logging.hpp>

#include "../tasks/JointAccelerationTask.hpp"
#include "../tasks/CartesianAccelerationTask.hpp"
#include "../tasks/CoMAccelerationTask.hpp"

#include "../constraints/RigidbodyDynamicsConstraint.hpp"
#include "../constraints/ContactsAccelerationConstraint.hpp"
#include "../constraints/JointLimitsAccelerationConstraint.hpp"
#include "../constraints/EffortLimitsAccelerationConstraint.hpp"

namespace wbc {

AccelerationSceneReducedTSID::AccelerationSceneReducedTSID(RobotModelPtr robot_model, QPSolverPtr solver, double dt) :
    WbcScene(robot_model,solver),
    hessian_regularizer(1e-8){

    bool reduced_problem = true;
    
    // for now manually adding constraint to this scene (an option would be to take them during configuration)
    constraints.resize(1);
    constraints[0].push_back(std::make_shared<RigidbodyDynamicsConstraint>(reduced_problem));
    constraints[0].push_back(std::make_shared<ContactsAccelerationConstraint>(reduced_problem));
    constraints[0].push_back(std::make_shared<JointLimitsAccelerationConstraint>(dt, reduced_problem));
    constraints[0].push_back(std::make_shared<EffortLimitsAccelerationConstraint>());
}

TaskPtr AccelerationSceneReducedTSID::createTask(const TaskConfig &config){

    if(config.type == cart)
        return std::make_shared<CartesianAccelerationTask>(config, robot_model->noOfJoints());
    else if(config.type == com)
        return std::make_shared<CoMAccelerationTask>(config, robot_model->noOfJoints());
    else if(config.type == jnt)
        return std::make_shared<JointAccelerationTask>(config, robot_model->noOfJoints());
    else{
        LOG_ERROR("Task with name %s has an invalid task type: %i", config.name.c_str(), config.type);
        throw std::invalid_argument("Invalid task config");
    }
}

const HierarchicalQP& AccelerationSceneReducedTSID::update(){

    if(!configured)
        throw std::runtime_error("AccelerationSceneReducedTSID has not been configured!. PLease call configure() before calling update() for the first time!");

    if(tasks.size() != 1){
        LOG_ERROR("Number of priorities in AccelerationSceneReducedTSID should be 1, but is %i", tasks.size());
        throw std::runtime_error("Invalid task configuration");
    }

    int prio = 0; // Only one priority is implemented here!
    uint nj = robot_model->noOfJoints();
    uint ncp = robot_model->getActiveContacts().size();

    // QP Size: (NJoints+NContacts*2*6 x NJoints+NContacts*6)
    // Variable order: (acc,f_ext)

    size_t total_eqs = 0;
    for(auto contraint : constraints[prio]) {
        contraint->update(robot_model);
        if(contraint->type() != Constraint::bounds)
            total_eqs += contraint->size();
    }
    tasks_prio[prio].resize(total_eqs, nj+ncp*6);
    tasks_prio[prio].H.setZero();
    tasks_prio[prio].g.setZero();

    ///////// Tasks

    // Walk through all tasks
    for(uint i = 0; i < tasks[prio].size(); i++){
        
        TaskPtr task = tasks[prio][i];

        task->checkTimeout();
        task->update(robot_model);

        // If the activation value is zero, also set reference to zero. Activation is usually used to switch between different
        // task phases and we don't want to store the "old" reference value, in case we switch on the task again
        if(task->activation == 0){
           task->y_ref.setZero();
           task->y_ref_root.setZero();
        }

        for(int i = 0; i < task->A.rows(); i++)
            task->Aw.row(i) = task->weights_root(i) * task->A.row(i) * task->activation * (!task->timeout);
        for(int i = 0; i < task->A.cols(); i++)
            task->Aw.col(i) = joint_weights[i] * task->Aw.col(i);

        tasks_prio[prio].H.block(0,0,nj,nj) += task->Aw.transpose()*task->Aw; // NOTE! good only if tasks involve only acceleration
        tasks_prio[prio].g.segment(0,nj) -= task->Aw.transpose()*task->y_ref_root;
    }

    tasks_prio[prio].H.block(0,0, tasks_prio[prio].nq, tasks_prio[prio].nq).diagonal().array() += hessian_regularizer;


    ///////// Constraints


    // Note already performed at the beginning of the update (but does not consider additional constraints)
    tasks_prio[prio].A.resize(total_eqs, nj+ncp*6);
    tasks_prio[prio].lower_x.resize(nj+ncp*6);
    tasks_prio[prio].upper_x.resize(nj+ncp*6);
    tasks_prio[prio].lower_y.resize(total_eqs);
    tasks_prio[prio].upper_y.resize(total_eqs);

    tasks_prio[prio].A.setZero();
    tasks_prio[prio].lower_x.setConstant(-10000);   // bounds
    tasks_prio[prio].upper_x.setConstant(+10000);   // bounds
    tasks_prio[prio].lower_y.setZero(); // inequalities
    tasks_prio[prio].upper_y.setZero(); // inequalities

    total_eqs = 0;
    for(uint i = 0; i < constraints[prio].size(); i++) {
        Constraint::Type type = constraints[prio][i]->type();
        size_t c_size = constraints[prio][i]->size();

        if(type == Constraint::bounds) {
            tasks_prio[prio].lower_x = constraints[prio][i]->lb(); // NOTE! Good only if a single bound task is admitted
            tasks_prio[prio].upper_x = constraints[prio][i]->ub();
        }
        else if (type == Constraint::equality) {
            tasks_prio[prio].A.middleRows(total_eqs, c_size) = constraints[prio][i]->A();
            tasks_prio[prio].lower_y.segment(total_eqs, c_size) = constraints[prio][i]->b();
            tasks_prio[prio].upper_y.segment(total_eqs, c_size) = constraints[prio][i]->b();
            total_eqs += c_size;
        }
        else if (type == Constraint::inequality) {
            tasks_prio[prio].A.middleRows(total_eqs, c_size) = constraints[prio][i]->A();
            tasks_prio[prio].lower_y.segment(total_eqs, c_size) = constraints[prio][i]->lb();
            tasks_prio[prio].upper_y.segment(total_eqs, c_size) = constraints[prio][i]->ub();
            total_eqs += c_size;
        }
    }

    tasks_prio.Wq = base::VectorXd::Map(joint_weights.elements.data(), robot_model->noOfJoints());
    tasks_prio.time = base::Time::now(); //  TODO: Use latest time stamp from all tasks!?
    return tasks_prio;
}

const base::commands::Joints& AccelerationSceneReducedTSID::solve(const HierarchicalQP& hqp){

    // solve
    solver_output.resize(hqp[0].nq);
    solver->solve(hqp, solver_output);

    const auto& contacts = robot_model->getActiveContacts();

    // Convert solver output: Acceleration and torque
    uint nj = robot_model->noOfJoints();
    uint na = robot_model->noOfActuatedJoints();
    uint nc = contacts.size();

    auto qdd_out = Eigen::Map<Eigen::VectorXd>(solver_output.data(), nj);
    auto fext_out = Eigen::Map<Eigen::VectorXd>(solver_output.data()+nj, 6*nc);

    // computing torques from accelerations and forces (using last na equation from dynamic equations of motion)
    Eigen::VectorXd tau_out = robot_model->jointSpaceInertiaMatrix().bottomRows(na) * qdd_out;
    for(uint c = 0; c < nc; ++c)
        tau_out += -robot_model->bodyJacobian(robot_model->worldFrame(), contacts.names[c]).transpose().bottomRows(na) * fext_out.segment<6>(c*6);
    tau_out += robot_model->biasForces().bottomRows(na);

    solver_output_joints.resize(robot_model->noOfActuatedJoints());
    solver_output_joints.names = robot_model->actuatedJointNames();
    for(uint i = 0; i < robot_model->noOfActuatedJoints(); i++){
        const std::string& name = robot_model->actuatedJointNames()[i];
        uint idx = robot_model->jointIndex(name);
        if(base::isNaN(qdd_out[idx])){
            hqp[0].print();
            throw std::runtime_error("Solver output (acceleration) for joint " + name + " is NaN");
        }
        if(base::isNaN(tau_out[idx])){
            hqp[0].print();
            throw std::runtime_error("Solver output (force/torque) for joint " + name + " is NaN");
        }
        solver_output_joints[name].acceleration = qdd_out[idx];
        uint start_idx = 6 ? robot_model->hasFloatingBase() : 0;
        solver_output_joints[name].effort = tau_out[idx-start_idx]; // tau_out does not include fb dofs.
    }
    solver_output_joints.time = base::Time::now();

    // std::cerr << "Acc:   " << qdd_out.transpose() << std::endl;
    // std::cerr << "Tau:   " << tau_out.transpose() << std::endl;
    // std::cerr << "F_ext: " << fext_out.transpose() << std::endl << std::endl;

    // Convert solver output: contact wrenches
    contact_wrenches.resize(nc);
    contact_wrenches.names = contacts.names;
    for(uint i = 0; i < nc; i++){
        contact_wrenches[i].force = fext_out.segment(i*6, 3);
        contact_wrenches[i].torque = fext_out.segment(i*6+3, 3);
    }

    contact_wrenches.time = base::Time::now();
    return solver_output_joints;
}

const TasksStatus& AccelerationSceneReducedTSID::updateTasksStatus(){

    uint nj = robot_model->noOfJoints();
    solver_output_acc = solver_output.segment(0,nj);
    const base::samples::Joints& joint_state = robot_model->jointState(robot_model->jointNames());
    robot_acc.resize(nj);
    for(size_t i = 0; i < nj; i++)
        robot_acc(i) = joint_state[i].acceleration;

    for(uint prio = 0; prio < tasks.size(); prio++){
        for(uint i = 0; i < tasks[prio].size(); i++){
            TaskPtr task = tasks[prio][i];
            const std::string &name = task->config.name;

            tasks_status[name].time       = task->time;
            tasks_status[name].config     = task->config;
            tasks_status[name].activation = task->activation;
            tasks_status[name].timeout    = task->timeout;
            tasks_status[name].weights    = task->weights;
            tasks_status[name].y_ref      = task->y_ref_root;
            if(task->config.type == cart){
                const base::MatrixXd &jac = robot_model->spaceJacobian(task->config.root, task->config.tip);
                const base::Acceleration &bias_acc = robot_model->spatialAccelerationBias(task->config.root, task->config.tip);
                tasks_status[name].y_solution = jac * solver_output_acc + bias_acc;
                tasks_status[name].y          = jac * robot_acc + bias_acc;
            }
        }
    }

    return tasks_status;
}

}
