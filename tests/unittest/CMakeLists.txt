macro(declare_test TESTNAME RUN_SERVER)
    add_executable(${TESTNAME}
                   ${ARGN}
                   ${TESTNAME}.cpp
    )
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
    )

    if(${RUN_SERVER} STREQUAL "True")
        add_test(${TESTNAME}
                 ${DBUS_RUNNER}
                 --task ${CMAKE_CURRENT_BINARY_DIR}/address-book-server-test
                 --task ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME} ${TEST_ARGS} --wait-for=com.canonical.pim)
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
    ${CMAKE_BINARY_DIR}
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

declare_test(clause-test False)
declare_test(sort-clause-test False)
declare_test(fetch-hint-test False)

set(DUMMY_BACKEND_SRC
    scoped-loop.h
    scoped-loop.cpp
    dummy-backend.cpp
    dummy-backend.h)

declare_test(contactmap-test False ${DUMMY_BACKEND_SRC})

if(DBUS_RUNNER)
    set(BASE_CLIENT_TEST_SRC
        dummy-backend-defs.h
        base-client-test.h
        base-client-test.cpp)

    declare_test(addressbook-test True ${BASE_CLIENT_TEST_SRC})
    declare_test(service-life-cycle-test True ${BASE_CLIENT_TEST_SRC})
    declare_test(readonly-prop-test True ${BASE_CLIENT_TEST_SRC})
    declare_test(contact-link-test True ${BASE_CLIENT_TEST_SRC})
    declare_test(qcontacts-test True ${BASE_CLIENT_TEST_SRC})
elseif()
    message(STATUS "DBus test runner not found. Some tests will be disabled")
endif()

# server code
add_executable(address-book-server-test
    scoped-loop.h
    scoped-loop.cpp
    dummy-backend.h
    dummy-backend.cpp
    addressbook-server.cpp
)

qt5_use_modules(address-book-server-test Core Contacts Versit DBus)

target_link_libraries(address-book-server-test
                      address-book-service-lib
                      folks-dummy
                      ${CONTACTS_SERVICE_LIB}
                      ${GLIB_LIBRARIES}
                      ${GIO_LIBRARIES}
                      ${FOLKS_LIBRARIES}
)

