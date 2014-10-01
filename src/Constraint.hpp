#ifndef CONSTRAINT_HPP
#define CONSTRAINT_HPP

#include <base/Eigen.hpp>
#include "ConstraintConfig.hpp"
#include <base/time.h>
#include <base/samples/RigidBodyState.hpp>
#include <base/samples/Joints.hpp>
#include <base/logging.h>

namespace wbc{

/**
 * @brief Class to carry constraint specific information
 */
class Constraint{
public:
    Constraint(){}
    Constraint(const ConstraintConfig& _config,
               const std::vector<std::string> &_robot_joint_names)
    {
        config = _config;
        robot_joint_names = _robot_joint_names;

        if(config.type == jnt)
            no_variables = _config.joint_names.size();
        else
            no_variables = 6;

        if(config.weights.size() != no_variables){
            LOG_ERROR("Constraint %s has %i variables, but initial_weights vector from config has size %i",
                      config.name.c_str(), no_variables, config.weights.size());
            throw std::invalid_argument("Invalid WBC config");
        }
        if(config.activation < 0 || config.activation > 1){
            LOG_ERROR("Constraint %s: Activation has to be between 0 and 1, but is %i",
                      config.name.c_str(),config.activation);
            throw std::invalid_argument("Invalid WBC config");
        }

        for(uint i = 0; i < config.weights.size(); i++)
        {
            if(config.weights[i] < 0){
                LOG_ERROR("Constraint %s: Weight values have to be between 0 and 1, but element %i has a value of %i",
                          config.name.c_str(), i, config.weights[i]);
                throw std::invalid_argument("Invalid WBC config");

            }
        }

        y_ref.resize(no_variables);
        y_solution.resize(no_variables);
        y.resize(no_variables);
        A.resize(no_variables, robot_joint_names.size());
        weights.resize(no_variables);

        reset();
    }

    base::Time time;

    /** Configuration of this constraint */
    ConstraintConfig config;

    /** Reference value for this constraint */
    base::VectorXd y_ref;

    /** Solver solution for this constraint */
    base::VectorXd y_solution;

    /** Actual constraint as executed on the robot*/
    base::VectorXd y;

    /** Constraint weights, a 0 means that the reference of the corresponding constraint variable will be ignored while computing the solution*/
    base::VectorXd weights;

    /** Between 0 .. 1. Will be multiplied with the constraint weights. Can be used to (smoothly) switch on/off the constraints */
    double activation;

    /** Can be 0 or 1. Will be multiplied with the constraint weights. If no new reference values arrives for a certain time, the constraint times out*/
    int constraint_timed_out;

     /** Constraint matrix */
    base::MatrixXd A;

    /** Joint names of the robot in correct order. These correspond to the columns of the Constraint Matrix*/
    std::vector<std::string> robot_joint_names;

    /** Number of constraint variables */
    uint no_variables;

    /** last time a new reference sample arrived*/
    base::Time last_ref_input;

    void setReference(const base::samples::RigidBodyState &ref);
    void setReference(const base::samples::Joints& ref);
    void updateTime();
    void validate();
    void reset();
};

typedef std::vector<Constraint> ConstraintsPerPrio;

}
#endif
