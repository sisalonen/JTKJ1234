# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == EDIT at the beginning. Define the Project name and name for the main target (app).=========
# Define project name
set(APP_NAME JTKJ_app) 
# Define main app (app in this folder)
set(MAIN_TARGET hat_app)
#===============================================================================================

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)


# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work =======
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ================================================================================================

# == DO NOT EDIT:  SETUP pico SDK and create project ==============================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(${APP_NAME} C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# =================================================================================================

# DO NOT EDIT: Configure FreeRTOS =================================================================
# First include FreeRTOS Kernel libraries
include(FreeRTOS_Kernel_import.cmake)

# Include the configuration files. 

# Option 1 ===
# Add general include directory for all libraries of this directory. 
# Other libraries would use target_include directories
# set(FREERTOS_CONFIG_DIR "${CMAKE_SOURCE_DIR}/config")
# include_directories(${FREERTOS_CONFIG_DIR})
# ============

# Option 2. === 
# Include config only for FreeRTOS target. 
target_include_directories(FreeRTOS-Kernel INTERFACE
${CMAKE_CURRENT_LIST_DIR}/config
)

# DO NOT EDIT: Prepare the build to compile different libraries of the project ==================
# Add librabries. The TKJHAT does not use libraries from FreeRTOS
add_subdirectory(libs/TKJHAT)
# Support for usb serial communication
add_subdirectory(libs/usb-serial-debug)

# ==================
# EDIT: Prepare build to compile the main target (in root src folder) ==========================
# NOTE: If you are creating some example you would need to include these lines
# in the example CMakeLists.txt

# Add the source files to be compiled
# If you add the headers in a different directory, you should use: target_include_directories
# === Main app sources ===
file(GLOB SRC_FILES ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

add_executable(${MAIN_TARGET} ${SRC_FILES})

target_include_directories(${MAIN_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(${MAIN_TARGET}
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    TKJHAT_SDK
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(${MAIN_TARGET} pico_cyw43_arch_none)
endif()

pico_enable_stdio_usb(${MAIN_TARGET} 1)
pico_enable_stdio_uart(${MAIN_TARGET} 0)
pico_add_extra_outputs(${MAIN_TARGET})
# add url via pico_set_program_url
pico_set_program_url(${MAIN_TARGET} https://github.com/UniOulu-Ubicomp-Programming-Courses/JTKJ-PicoRTOS-ProjectP)
# ==================================================================================
