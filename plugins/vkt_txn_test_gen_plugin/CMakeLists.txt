file(GLOB HEADERS "include/eosio/vkt_txn_test_gen_plugin/*.hpp")
add_library( vkt_txn_test_gen_plugin
             vkt_txn_test_gen_plugin.cpp
             ${HEADERS} )
             
target_link_libraries( vkt_txn_test_gen_plugin appbase fc http_plugin chain_plugin eosio_testing )
target_include_directories( vkt_txn_test_gen_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                            vkt_txn_test_gen_plugin PUBLIC ${CMAKE_SOURCE_DIR}/libraries/testing/include
                            vkt_txn_test_gen_plugin PUBLIC ${CMAKE_BINARY_DIR}/unittests/include )
