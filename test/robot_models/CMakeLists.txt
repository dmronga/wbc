add_subdirectory(kdl)
if(USE_RBDL)
    add_subdirectory(rbdl)
endif()
if(USE_HYRODYN)
    add_subdirectory(hyrodyn)
endif()
if(USE_PINOCCHIO)
    add_subdirectory(pinocchio)
endif()


if(USE_HYRODYN AND USE_PINOCCHIO AND USE_RBDL)

   add_executable(test_model_consistency test_model_consistency.cpp ../suite.cpp  test_robot_model.cpp)
   target_link_libraries(test_model_consistency
                         wbc-robot_models-hyrodyn
                         wbc-robot_models-kdl
                         wbc-robot_models-rbdl
                         wbc-robot_models-pinocchio
                         Boost::unit_test_framework)
endif()
