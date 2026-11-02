# Neural Amp Modeler's core (MIT, github.com/sdatkinson/NeuralAmpModelerCore)
# and Eigen (MPL 2.0), fetched at configure time, pinned and checked by
# SHA-256: Krunch plays amp captures (.nam files) with it (instruments/krunch/
# docs/CAPTURES.md). Neither is copied into this repository.

include(FetchContent)

FetchContent_Declare(nam_core
    URL https://github.com/sdatkinson/NeuralAmpModelerCore/archive/refs/tags/v0.6.0.tar.gz
    URL_HASH SHA256=d55f3c49278a06e5bf5978ea60ae8384603b843d7fdee22a0e9ba71c95108450
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR no-build) # sources only: their own build files aren't used
# The commit NAM v0.6.0 pins as its Eigen submodule.
FetchContent_Declare(nam_eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/bc3b39870ecb690a623a3f49149a358b95c5781d/eigen-bc3b39870ecb690a623a3f49149a358b95c5781d.tar.gz
    URL_HASH SHA256=5f6557d1aca6e959a4b955c55f2935261fd30de59c0c79e35bc6d5cbe1de22d1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR no-build)
FetchContent_MakeAvailable(nam_core nam_eigen)

file(GLOB NAM_SOURCES ${nam_core_SOURCE_DIR}/NAM/*.cpp ${nam_core_SOURCE_DIR}/NAM/wavenet/*.cpp)
add_library(nam_core STATIC ${NAM_SOURCES})
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(nam_core SYSTEM PUBLIC
    ${nam_core_SOURCE_DIR}
    ${nam_core_SOURCE_DIR}/NAM
    ${nam_core_SOURCE_DIR}/Dependencies/nlohmann
    ${nam_eigen_SOURCE_DIR})
# Single precision throughout (the audio path's), and Eigen kept from allocating on the audio thread where it can.
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT EIGEN_MPL2_ONLY)
target_compile_features(nam_core PUBLIC cxx_std_20)
# Release-speed even in a debug build: the networks are too slow otherwise.
target_compile_options(nam_core PRIVATE -O3 -w)
