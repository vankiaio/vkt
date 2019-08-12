/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/chain/genesis_state_origin.hpp>

// these are required to serialize a genesis_state
#include <fc/smart_ref_impl.hpp>   // required for gcc in release mode

namespace eosio { namespace chain {

genesis_state_origin::genesis_state_origin() {
   initial_timestamp = fc::time_point::from_iso_string( "2019-08-08T18:18:18" );
   initial_key = fc::variant(eosio_root_key).as<public_key_type>();
}

chain::chain_id_type genesis_state_origin::compute_chain_id() const {
   digest_type::encoder enc;
   fc::raw::pack( enc, *this );
   return chain_id_type{enc.result()};
}

} } // namespace eosio::chain
