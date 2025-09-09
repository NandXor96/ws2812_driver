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
file(GLOB usb-ws2812-example_src CONFIGURE_DEPENDS "*.c")

# Configure logging for all modules
add_compile_definitions(LOGGING_ENABLED)

# The executables
add_executable(usb-ws2812-minimal-example ${usb-ws2812-example_src})



# link all module libs with executable
target_link_libraries(usb-ws2812-minimal-example 
    PRIVATE
    usb-ws2812-lib)

# library include dir
target_include_directories(usb-ws2812-minimal-example 
  PRIVATE $<TARGET_PROPERTY:usb-ws2812-lib,INTERFACE_INCLUDE_DIRECTORIES>)


copyTemps(usb-ws2812-minimal-example )

# ###############################
# Tests
# ###############################
