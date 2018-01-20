PROJECT(librvth-tests)

# Top-level src directory.
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../..)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/../..)

# Certificate verification test.
ADD_EXECUTABLE(CertVerifyTest CertVerifyTest.cpp)
TARGET_LINK_LIBRARIES(CertVerifyTest rvth)
TARGET_LINK_LIBRARIES(CertVerifyTest gtest)
DO_SPLIT_DEBUG(CertVerifyTest)
SET_WINDOWS_SUBSYSTEM(CertVerifyTest CONSOLE)
ADD_TEST(NAME CertVerifyTest COMMAND CertVerifyTest)
