/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once
#include <appbase/application.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>

namespace eosio {

using namespace appbase;

class vkt_txn_test_gen_plugin : public appbase::plugin<vkt_txn_test_gen_plugin> {
public:
   vkt_txn_test_gen_plugin();
   ~vkt_txn_test_gen_plugin();

   APPBASE_PLUGIN_REQUIRES((http_plugin)(chain_plugin))
   virtual void set_program_options(options_description&, options_description& cfg) override;
 
   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   std::unique_ptr<struct vkt_txn_test_gen_plugin_impl> my;
};

}
