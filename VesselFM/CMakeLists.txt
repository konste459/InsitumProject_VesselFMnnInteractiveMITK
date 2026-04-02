project(MitkVesselFM)

include(files.cmake)

mitk_create_module(
  DEPENDS
    MitkCore
    MitkSegmentation
    MitkMultilabel
  PACKAGE_DEPENDS
    Qt6|Core
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(VESSELFM_UPSTREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/vesselFM")
set(VESSELFM_ENV_DIR "${CMAKE_BINARY_DIR}/python/vesselfm_env")
if(WIN32)
  set(VESSELFM_ENV_PYTHON "${VESSELFM_ENV_DIR}/Scripts/python.exe")
else()
  set(VESSELFM_ENV_PYTHON "${VESSELFM_ENV_DIR}/bin/python")
endif()

if(NOT EXISTS "${VESSELFM_UPSTREAM_DIR}/setup.py")
  message(FATAL_ERROR
    "vesselFM upstream repo not found at: ${VESSELFM_UPSTREAM_DIR}\n"
    "Expected setup.py there. Please place the upstream Python package under external/vesselFM."
  )
endif()

add_custom_command(
  OUTPUT "${VESSELFM_ENV_PYTHON}"
  COMMAND "${Python3_EXECUTABLE}" -m venv "${VESSELFM_ENV_DIR}"
  COMMENT "Creating VesselFM Python virtual environment"
)

add_custom_target(VesselFM_CreatePythonEnv ALL
  DEPENDS "${VESSELFM_ENV_PYTHON}"
)

add_custom_target(VesselFM_InstallPythonPackage ALL
  COMMAND "${VESSELFM_ENV_PYTHON}" -m pip install --upgrade pip setuptools wheel
  COMMAND "${VESSELFM_ENV_PYTHON}" -m pip install -e "${VESSELFM_UPSTREAM_DIR}"
  DEPENDS VesselFM_CreatePythonEnv
  COMMENT "Installing upstream vesselFM into managed Python environment"
)

add_dependencies(MitkVesselFM VesselFM_InstallPythonPackage)

install(DIRECTORY "${VESSELFM_ENV_DIR}/"
        DESTINATION "python/vesselfm_env")