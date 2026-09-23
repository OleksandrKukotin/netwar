add_library(netwar_sim STATIC
        src/graph.cpp
        src/engine.cpp
        src/scenario.cpp
        src/match.cpp
)

target_include_directories(netwar_sim PUBLIC include)
target_compile_features(netwar_sim PUBLIC cxx_std_20)
