//#include "Wbc.hpp"
//#include "Constraint.hpp"
//#include "TaskFrame.hpp"
//#include <map>
//#include <base-logging/Logging.hpp>

//namespace wbc{

//Constraint* Wbc::getConstraint(const std::string& name){

//    for(size_t i = 0; i < constraints.size(); i++){
//        for(size_t j = 0; j < constraints[i].size(); j++){
//            if(constraints[i][j]->config.name == name)
//                return constraints[i][j];
//        }
//    }
//    LOG_ERROR("No such constraint: %s", name.c_str());
//    throw std::invalid_argument("Invalid constraint name");
//}

//std::vector<ConstraintsPerPrio> Wbc::getConstraints(){
//    constraint_vector.resize(constraints.size());
//    for(size_t i = 0; i < constraints.size(); i++){
//        constraint_vector[i].resize(constraints[i].size());
//        for(size_t j = 0; j < constraints[i].size(); j++)
//            constraint_vector[i][j] = *constraints[i][j];
//    }
//    return constraint_vector;
//}

//bool Wbc::hasConstraint(const std::string &name){
//    for(size_t i = 0; i < constraints.size(); i++){
//        for(size_t j = 0; j < constraints[i].size(); j++){
//            if(constraints[i][j]->config.name == name)
//                return true;
//        }
//    }
//    return false;
//}

//std::vector<int> Wbc::getConstraintVariablesPerPrio(){
//    std::vector<int> nc_pp(constraints.size());
//    for(size_t i = 0; i < constraints.size(); i++){
//        nc_pp[i] = 0;
//        for(size_t j = 0; j < constraints[i].size(); j++)
//            nc_pp[i] += constraints[i][j]->no_variables;
//    }
//    return nc_pp;
//}

//void Wbc::sortConfigByPriority(const std::vector<ConstraintConfig>& config, std::vector< std::vector<ConstraintConfig> >& sorted_config){

//    // Get highest prio
//    int max_prio = 0;
//    for(uint i = 0; i < config.size(); i++){

//        if(config[i].priority > max_prio)
//            max_prio = config[i].priority;
//    }
//    sorted_config.resize(max_prio + 1);

//    for(uint i = 0; i < config.size(); i++)
//        sorted_config[config[i].priority].push_back(config[i]);

//    // Erase empty priorities
//    for(uint i = 0; i < sorted_config.size(); i++){
//        if(sorted_config[i].empty())
//        {
//            sorted_config.erase(sorted_config.begin() + i, sorted_config.begin() + i + 1);
//            i--;
//        }
//    }
//}

//}
