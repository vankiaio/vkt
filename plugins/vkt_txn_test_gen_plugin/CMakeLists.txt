file(GLOB HEADERS "include/eosio/vkt_txn_test_gen_plugin/*.hpp")
add_library( vkt_txn_test_gen_plugin
             vkt_txn_test_gen_plugin.cpp
             ${HEADERS} )

add_dependencies(vkt_txn_test_gen_plugin eosio.token)

target_link_libraries( vkt_txn_test_gen_plugin appbase fc http_plugin chain_plugin )
target_include_directories( vkt_txn_test_gen_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_include_directories( vkt_txn_test_gen_plugin PUBLIC ${CMAKE_BINARY_DIR}/contracts )
