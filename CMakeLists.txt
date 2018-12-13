# minimum cmake version:
cmake_minimum_required( VERSION 3.5 )

# project name:
project( MIPS_Simulation )

# runnable:
set(CMAKE_CXX_FLAGS "-std=c++0x")

# libraries:
find_package( Boost 1.58 COMPONENTS program_options REQUIRED )

# include path:
include_directories( ${Boost_INCLUDE_DIR} )

# executable:
add_executable( main main.cpp isa.cpp assembler.cpp executor.cpp)
target_link_libraries( main LINK_PUBLIC ${Boost_LIBRARIES} )
