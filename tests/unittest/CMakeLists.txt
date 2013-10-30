macro(declare_test TESTNAME)
    declare_test_full(${TESTNAME} false)
endmacro()

macro(declare_test_with_folks TESTNAME)
    declare_test_full(${TESTNAME} true)
endmacro()

macro(declare_test_full TESTNAME USE_DUMMY_FOLKS)
    if(${USE_DUMMY_FOLKS} STREQUAL "true")
        add_executable(${TESTNAME}
                        folks-dummy-base-test.cpp
                        folks-dummy-base-test.h
                        ${TESTNAME}.cpp
        )
    else()
        add_executable(${TESTNAME}
                        ${TESTNAME}.cpp
        )
    endif()
    qt5_use_modules(${TESTNAME} Core Contacts Versit Test DBus)

    if(TEST_XML_OUTPUT)
        set(TEST_ARGS -p -xunitxml -p -o -p test_${testname}.xml)
    else()
        set(TEST_ARGS "")
    endif()

    target_link_libraries(${TESTNAME}
                          address-book-service-lib
                          folks-dummy
                          ${CONTACTS_SERVICE_LIB}
                          ${GLIB_LIBRARIES}
                          ${GIO_LIBRARIES}
                          ${FOLKS_LIBRARIES}
                          ${FOLKS_DUMMY_LIBRARIES}
    )



    if(${USE_DUMMY_FOLKS} STREQUAL "true")
        add_test(${TESTNAME}
                 ${DBUS_RUNNER}
                 --task ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME} ${TEST_ARGS})
    else()
        add_test(${TESTNAME} ${TESTNAME})
    endif()

    set(TEST_ENVIRONMENT "QT_QPA_PLATFORM=minimal\;FOLKS_BACKEND_PATH=${folks-dummy-backend_BINARY_DIR}/dummy.so")
    set_tests_properties(${TESTNAME} PROPERTIES
                          ENVIRONMENT ${TEST_ENVIRONMENT}
                          TIMEOUT ${CTEST_TESTING_TIMEOUT})
endmacro()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${folks-dummy-lib_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${FOLKS_INCLUDE_DIRS}
    ${FOLKS_DUMMY_INCLUDE_DIRS}
)

add_definitions(-DTEST_SUITE)
if(NOT CTEST_TESTING_TIMEOUT)
    set(CTEST_TESTING_TIMEOUT 60)
endif()

declare_test(clause-test)
declare_test(sort-clause-test)
declare_test(fetch-hint-test)

if(DBUS_RUNNER)
    declare_test_with_folks(contactmap-test)
    declare_test_with_folks(service-life-cycle-test)
elseif()
    message(STATUS "DBus test runner not found. Some tests will be disabled")
endif()
