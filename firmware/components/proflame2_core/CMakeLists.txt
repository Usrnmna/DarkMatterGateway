idf_component_register(SRCS "protocol.cpp" "controller.cpp" "session.cpp"
    INCLUDE_DIRS "include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
