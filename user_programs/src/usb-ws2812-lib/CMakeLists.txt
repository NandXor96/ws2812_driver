cmake_minimum_required(VERSION 3.16)

# ###############################
# Generic CMake config
# ###############################

# ###############################
# Set up packages
# ###############################

# ###############################
# Modules, Libraries and Linking
# ###############################
file(GLOB usb-ws2812-client_src CONFIGURE_DEPENDS "*.c")

# Configure logging for all modules
add_compile_definitions(LOGGING_ENABLED)

# The executables
add_library(usb-ws2812-lib  SHARED ${usb-ws2812-client_src})
target_include_directories(usb-ws2812-lib PUBLIC "./")
# link all module libs with executable
# target_link_libraries(client1
# PRIVATE
# MPI::MPI_CXX)
copyTemps(usb-ws2812-lib)

# ###############################
# Tests
# ###############################
