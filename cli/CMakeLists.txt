add_executable(netwar_cli src/main.cpp)
target_link_libraries(netwar_cli PRIVATE netwar_sim)

# The playable terminal console (M2). FTXUI stays out of sim/: the core
# library must remain free of I/O and platform code.
include(FetchContent)

FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v6.1.9
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

add_executable(netwar src/play.cpp)
target_link_libraries(netwar PRIVATE netwar_sim ftxui::component)
target_compile_definitions(netwar PRIVATE NETWAR_VERSION="${NETWAR_VERSION}")
