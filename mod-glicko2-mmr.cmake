#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is executed during module configuration
# Integrates module tests with AzerothCore's test framework
#

if (BUILD_TESTING)
    message(STATUS "Configuring mod-glicko2-mmr tests...")

    # Collect all test sources
    file(GLOB_RECURSE MODULE_TEST_SOURCES
        "${CMAKE_SOURCE_DIR}/modules/mod-glicko2-mmr/tests/*.cpp"
    )

    # Exclude the TEST_PLAN.md and other documentation
    list(FILTER MODULE_TEST_SOURCES EXCLUDE REGEX ".*\\.md$")

    if(MODULE_TEST_SOURCES)
        # Store test sources and include paths in global properties
        # These will be retrieved later when unit_tests target is created
        set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES ${MODULE_TEST_SOURCES})

        set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
            "${CMAKE_SOURCE_DIR}/modules/mod-glicko2-mmr/include"
            "${CMAKE_SOURCE_DIR}/modules/mod-glicko2-mmr/src"
            "${CMAKE_SOURCE_DIR}/modules/mod-glicko2-mmr/tests"
            "${CMAKE_SOURCE_DIR}/modules/mod-glicko2-mmr/tests/mocks"
        )

        list(LENGTH MODULE_TEST_SOURCES TEST_FILE_COUNT)
        message(STATUS "  +- Registered ${TEST_FILE_COUNT} test file(s) from mod-glicko2-mmr")
    else()
        message(STATUS "  +- No test files found in mod-glicko2-mmr/tests")
    endif()
endif()
