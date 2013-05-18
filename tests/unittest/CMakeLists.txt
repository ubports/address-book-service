macro(declare_test testname)
    add_executable(${testname} ${testname}.cpp)
    qt5_use_modules(${testname} Core Contacts Test)

    if(TEST_XML_OUTPUT)
        set(TEST_ARGS -p -xunitxml -p -o -p test_${testname}.xml)
    else()
        set(TEST_ARGS "")
    endif()

    target_link_libraries(${testname}
                          galera-common)

    add_test(${testname} ${testname})
    set_tests_properties(${testname} PROPERTIES
                         TIMEOUT ${CTEST_TESTING_TIMEOUT}
                         ENVIRONMENT "QT_QPA_PLATFORM=minimal")
endmacro(declare_test testname)

include_directories(
    ${CMAKE_SOURCE_DIR}
)

add_definitions(-DTEST_SUITE)
if(NOT CTEST_TESTING_TIMEOUT)
    set(CTEST_TESTING_TIMEOUT 60)
endif()

declare_test(clause-test)
