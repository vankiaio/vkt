/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#include <vktio/hblf_mongo_db_plugin/hblf_mongo_db_plugin.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/types.hpp>

#include <fc/io/json.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/utf8.hpp>
#include <fc/variant.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/signals2/connection.hpp>

#include <queue>
#include <thread>
#include <mutex>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

namespace fc { class variant; }

namespace eosio {

using chain::account_name;
using chain::action_name;
using chain::block_id_type;
using chain::permission_name;
using chain::transaction;
using chain::signed_transaction;
using chain::signed_block;
using chain::transaction_id_type;
using chain::packed_transaction;

static appbase::abstract_plugin& _hblf_mongo_db_plugin = app().register_plugin<hblf_mongo_db_plugin>();

struct filter_entry {
   name receiver;
   name action;
   name actor;

   friend bool operator<( const filter_entry& a, const filter_entry& b ) {
      return std::tie( a.receiver, a.action, a.actor ) < std::tie( b.receiver, b.action, b.actor );
   }

   //            receiver          action       actor
   bool match( const name& rr, const name& an, const name& ar ) const {
      return (receiver.value == 0 || receiver == rr) &&
             (action.value == 0 || action == an) &&
             (actor.value == 0 || actor == ar);
   }
};

class hblf_mongo_db_plugin_impl {
public:
   hblf_mongo_db_plugin_impl();
   ~hblf_mongo_db_plugin_impl();

   fc::optional<boost::signals2::scoped_connection> accepted_block_connection;
   fc::optional<boost::signals2::scoped_connection> applied_transaction_connection;

   void consume_blocks();

   void accepted_block( const chain::block_state_ptr& );
   void applied_transaction(const chain::transaction_trace_ptr&);
   void process_applied_transaction(const chain::transaction_trace_ptr&);
   void _process_applied_transaction(const chain::transaction_trace_ptr&);


   optional<abi_serializer> get_abi_serializer( account_name n );
   template<typename T> fc::variant to_variant_with_abi( const T& obj );

   void purge_abi_cache();

   bool add_action_trace( mongocxx::bulk_write& bulk_action_traces, const chain::action_trace& atrace,
                          const chain::transaction_trace_ptr& t,
                          bool executed, const std::chrono::milliseconds& now,
                          bool& write_ttrace );

   void update_base_col(const chain::action& act, const bsoncxx::document::view& actdoc);

   void add_pub_keys( const vector<chain::key_weight>& keys, const account_name& name,
                      const permission_name& permission, const std::chrono::milliseconds& now );
   void remove_pub_keys( const account_name& name, const permission_name& permission );
   void add_account_control( const vector<chain::permission_level_weight>& controlling_accounts,
                             const account_name& name, const permission_name& permission,
                             const std::chrono::milliseconds& now );
   void remove_account_control( const account_name& name, const permission_name& permission );

   /// @return true if act should be added to mongodb, false to skip it
   bool filter_include( const account_name& receiver, const action_name& act_name,
                        const vector<chain::permission_level>& authorization ) const;
   bool filter_include( const transaction& trx ) const;

   void init();
   void wipe_database();
   void create_expiration_index(mongocxx::collection& collection, uint32_t expire_after_seconds);

   template<typename Queue, typename Entry> void queue(Queue& queue, const Entry& e);

   bool configured{false};
   bool wipe_database_on_startup{false};
   uint32_t start_block_num = 0;
   std::atomic_bool start_block_reached{false};

   bool is_producer = false;
   bool filter_on_star = true;
   std::set<filter_entry> filter_on;
   std::set<filter_entry> filter_out;
   bool update_blocks_via_block_num = false;
   bool store_blocks = true;
   bool store_block_states = true;
   bool store_transactions = true;
   bool store_transaction_traces = true;
   bool store_action_traces = true;
   uint32_t expire_after_seconds = 0;

   std::string db_name;
   // mongocxx::instance mongo_inst;
   fc::optional<mongocxx::pool> mongo_pool;

   // consum thread
   mongocxx::collection _students;
   mongocxx::collection _student_traces;
   mongocxx::collection _teachers;
   mongocxx::collection _teacher_traces;
   mongocxx::collection _accounts;

   size_t max_queue_size = 0;
   int queue_sleep_time = 0;
   size_t abi_cache_size = 0;
   std::deque<chain::transaction_metadata_ptr> transaction_metadata_queue;
   std::deque<chain::transaction_metadata_ptr> transaction_metadata_process_queue;
   std::deque<chain::transaction_trace_ptr> transaction_trace_queue;
   std::deque<chain::transaction_trace_ptr> transaction_trace_process_queue;
   std::deque<chain::block_state_ptr> block_state_queue;
   std::deque<chain::block_state_ptr> block_state_process_queue;
   std::deque<chain::block_state_ptr> irreversible_block_state_queue;
   std::deque<chain::block_state_ptr> irreversible_block_state_process_queue;
   std::mutex mtx;
   std::condition_variable condition;
   std::thread consume_thread;
   std::atomic_bool done{false};
   std::atomic_bool startup{true};
   fc::optional<chain::chain_id_type> chain_id;
   fc::microseconds abi_serializer_max_time;

   struct by_account;
   struct by_last_access;

   struct abi_cache {
      account_name                     account;
      fc::time_point                   last_accessed;
      fc::optional<abi_serializer>     serializer;
   };

   typedef boost::multi_index_container<abi_cache,
         indexed_by<
               ordered_unique< tag<by_account>,  member<abi_cache,account_name,&abi_cache::account> >,
               ordered_non_unique< tag<by_last_access>,  member<abi_cache,fc::time_point,&abi_cache::last_accessed> >
         >
   > abi_cache_index_t;

   abi_cache_index_t abi_cache_index;

   static const action_name addstudent;
   static const action_name modstudent;
   static const action_name delstudent;
   static const action_name addteachbase;
   static const action_name modteachbase;
   static const action_name delteachbase;

   static const std::string students_col;
   static const std::string student_traces_col;
   static const std::string teachers_col;
   static const std::string teacher_traces_col;
   static const std::string accounts_col;
};

const action_name hblf_mongo_db_plugin_impl::addstudent = N(addstudent);
const action_name hblf_mongo_db_plugin_impl::modstudent = N(modstudent);
const action_name hblf_mongo_db_plugin_impl::delstudent = N(delstudent);
const action_name hblf_mongo_db_plugin_impl::addteachbase = N(addteachbase);
const action_name hblf_mongo_db_plugin_impl::modteachbase = N(modteachbase);
const action_name hblf_mongo_db_plugin_impl::delteachbase = N(delteachbase);


const std::string hblf_mongo_db_plugin_impl::students_col = "students";
const std::string hblf_mongo_db_plugin_impl::student_traces_col = "student_traces";
const std::string hblf_mongo_db_plugin_impl::teachers_col = "teachers";
const std::string hblf_mongo_db_plugin_impl::teacher_traces_col = "teacher_traces";
const std::string hblf_mongo_db_plugin_impl::accounts_col = "accounts";

bool hblf_mongo_db_plugin_impl::filter_include( const account_name& receiver, const action_name& act_name,
                                           const vector<chain::permission_level>& authorization ) const
{
   bool include = false;
   if( filter_on_star ) {
      include = true;
   } else {
      auto itr = std::find_if( filter_on.cbegin(), filter_on.cend(), [&receiver, &act_name]( const auto& filter ) {
         return filter.match( receiver, act_name, 0 );
      } );
      if( itr != filter_on.cend() ) {
         include = true;
      } else {
         for( const auto& a : authorization ) {
            auto itr = std::find_if( filter_on.cbegin(), filter_on.cend(), [&receiver, &act_name, &a]( const auto& filter ) {
               return filter.match( receiver, act_name, a.actor );
            } );
            if( itr != filter_on.cend() ) {
               include = true;
               break;
            }
         }
      }
   }

   if( !include ) { return false; }
   if( filter_out.empty() ) { return true; }

   auto itr = std::find_if( filter_out.cbegin(), filter_out.cend(), [&receiver, &act_name]( const auto& filter ) {
      return filter.match( receiver, act_name, 0 );
   } );
   if( itr != filter_out.cend() ) { return false; }

   for( const auto& a : authorization ) {
      auto itr = std::find_if( filter_out.cbegin(), filter_out.cend(), [&receiver, &act_name, &a]( const auto& filter ) {
         return filter.match( receiver, act_name, a.actor );
      } );
      if( itr != filter_out.cend() ) { return false; }
   }

   return true;
}

bool hblf_mongo_db_plugin_impl::filter_include( const transaction& trx ) const
{
   if( !filter_on_star || !filter_out.empty() ) {
      bool include = false;
      for( const auto& a : trx.actions ) {
         if( filter_include( a.account, a.name, a.authorization ) ) {
            include = true;
            break;
         }
      }
      if( !include ) {
         for( const auto& a : trx.context_free_actions ) {
            if( filter_include( a.account, a.name, a.authorization ) ) {
               include = true;
               break;
            }
         }
      }
      return include;
   }
   return true;
}


template<typename Queue, typename Entry>
void hblf_mongo_db_plugin_impl::queue( Queue& queue, const Entry& e ) {
   std::unique_lock<std::mutex> lock( mtx );
   auto queue_size = queue.size();
   if( queue_size > max_queue_size ) {
      lock.unlock();
      condition.notify_one();
      queue_sleep_time += 10;
      if( queue_sleep_time > 1000 )
         wlog("queue size: ${q}", ("q", queue_size));
      std::this_thread::sleep_for( std::chrono::milliseconds( queue_sleep_time ));
      lock.lock();
   } else {
      queue_sleep_time -= 10;
      if( queue_sleep_time < 0 ) queue_sleep_time = 0;
   }
   queue.emplace_back( e );
   lock.unlock();
   condition.notify_one();
}

void hblf_mongo_db_plugin_impl::applied_transaction( const chain::transaction_trace_ptr& t ) {
   try {
      // Traces emitted from an incomplete block leave the producer_block_id as empty.
      //
      // Avoid adding the action traces or transaction traces to the database if the producer_block_id is empty.
      // This way traces from speculatively executed transactions are not included in the Mongo database which can
      // avoid potential confusion for consumers of that database.
      //
      // Due to forks, it could be possible for multiple incompatible action traces with the same block_num and trx_id
      // to exist in the database. And if the producer double produces a block, even the block_time may not
      // disambiguate the two action traces. Without a producer_block_id to disambiguate and determine if the action
      // trace comes from an orphaned fork branching off of the blockchain, consumers of the Mongo DB database may be
      // reacting to a stale action trace that never actually executed in the current blockchain.
      //
      // It is better to avoid this potential confusion by not logging traces from speculative execution, i.e. emitted
      // from an incomplete block. This means that traces will not be recorded in speculative read-mode, but
      // users should not be using the hblf_mongo_db_plugin in that mode anyway.
      //
      // Allow logging traces if node is a producer for testing purposes, so a single nodeos can do both for testing.
      //
      // It is recommended to run hblf_mongo_db_plugin in read-mode = read-only.
      //
      if( !is_producer && !t->producer_block_id.valid() )
         return;
      // always queue since account information always gathered
      queue( transaction_trace_queue, t );
   } catch (fc::exception& e) {
      elog("FC Exception while applied_transaction ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while applied_transaction ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while applied_transaction");
   }
}


void hblf_mongo_db_plugin_impl::accepted_block( const chain::block_state_ptr& bs ) {
   try {
      if( !start_block_reached ) {
         if( bs->block_num >= start_block_num ) {
            start_block_reached = true;
         }
      }
      // if( store_blocks || store_block_states ) {
      //    queue( block_state_queue, bs );
      // }
   } catch (fc::exception& e) {
      elog("FC Exception while accepted_block ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while accepted_block ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while accepted_block");
   }
}

void hblf_mongo_db_plugin_impl::consume_blocks() {
   try {
      auto mongo_client = mongo_pool->acquire();
      auto& mongo_conn = *mongo_client;

      _students = mongo_conn[db_name][students_col];
      _student_traces = mongo_conn[db_name][student_traces_col];
      _teachers = mongo_conn[db_name][teachers_col];
      _teacher_traces = mongo_conn[db_name][teacher_traces_col];
      _accounts = mongo_conn[db_name][accounts_col];

      while (true) {
         std::unique_lock<std::mutex> lock(mtx);
         while ( transaction_metadata_queue.empty() &&
                 transaction_trace_queue.empty() &&
               //   block_state_queue.empty() &&
               //   irreversible_block_state_queue.empty() &&
                 !done ) {
            condition.wait(lock);
         }

         // capture for processing
         size_t transaction_metadata_size = transaction_metadata_queue.size();
         if (transaction_metadata_size > 0) {
            transaction_metadata_process_queue = move(transaction_metadata_queue);
            transaction_metadata_queue.clear();
         }
         size_t transaction_trace_size = transaction_trace_queue.size();
         if (transaction_trace_size > 0) {
            transaction_trace_process_queue = move(transaction_trace_queue);
            transaction_trace_queue.clear();
         }


         lock.unlock();

         if (done) {
            //ilog("draining queue, size: ${q}", ("q", transaction_metadata_size + transaction_trace_size + block_state_size + irreversible_block_size));
            ilog("draining queue, size: ${q}", ("q", transaction_metadata_size + transaction_trace_size));
         }

         // process transactions
         auto start_time = fc::time_point::now();
         auto size = transaction_trace_process_queue.size();
         while (!transaction_trace_process_queue.empty()) {
            const auto& t = transaction_trace_process_queue.front();
            process_applied_transaction(t);
            transaction_trace_process_queue.pop_front();
         }
         auto time = fc::time_point::now() - start_time;
         auto per = size > 0 ? time.count()/size : 0;
         if( time > fc::microseconds(500000) ) // reduce logging, .5 secs
            ilog( "process_applied_transaction,  time per: ${p}, size: ${s}, time: ${t}", ("s", size)("t", time)("p", per) );

         
         time = fc::time_point::now() - start_time;
         per = size > 0 ? time.count()/size : 0;
         if( time > fc::microseconds(500000) ) // reduce logging, .5 secs
            ilog( "process_irreversible_block,   time per: ${p}, size: ${s}, time: ${t}", ("s", size)("t", time)("p", per) );

         if( transaction_metadata_size == 0 &&
             transaction_trace_size == 0 &&
            //  block_state_size == 0 &&
            //  irreversible_block_size == 0 &&
             done ) {
            break;
         }
      }
      ilog("hblf_mongo_db_plugin consume thread shutdown gracefully");
   } catch (fc::exception& e) {
      elog("FC Exception while consuming block ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while consuming block ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while consuming block");
   }
}

namespace {

void handle_mongo_exception( const std::string& desc, int line_num ) {
   bool shutdown = true;
   try {
      try {
         throw;
      } catch( mongocxx::logic_error& e) {
         // logic_error on invalid key, do not shutdown
         wlog( "mongo logic error, ${desc}, line ${line}, code ${code}, ${what}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "what", e.what() ));
         shutdown = false;
      } catch( mongocxx::operation_exception& e) {
         elog( "mongo exception, ${desc}, line ${line}, code ${code}, ${details}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "details", e.code().message() ));
         if (e.raw_server_error()) {
            elog( "  raw_server_error: ${e}", ( "e", bsoncxx::to_json(e.raw_server_error()->view())));
         }
      } catch( mongocxx::exception& e) {
         elog( "mongo exception, ${desc}, line ${line}, code ${code}, ${what}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "what", e.what() ));
      } catch( bsoncxx::exception& e) {
         elog( "bsoncxx exception, ${desc}, line ${line}, code ${code}, ${what}",
               ("desc", desc)( "line", line_num )( "code", e.code().value() )( "what", e.what() ));
      } catch( fc::exception& er ) {
         elog( "mongo fc exception, ${desc}, line ${line}, ${details}",
               ("desc", desc)( "line", line_num )( "details", er.to_detail_string()));
      } catch( const std::exception& e ) {
         elog( "mongo std exception, ${desc}, line ${line}, ${what}",
               ("desc", desc)( "line", line_num )( "what", e.what()));
      } catch( ... ) {
         elog( "mongo unknown exception, ${desc}, line ${line_nun}", ("desc", desc)( "line_num", line_num ));
      }
   } catch (...) {
      std::cerr << "Exception attempting to handle exception for " << desc << " " << line_num << std::endl;
   }

   if( shutdown ) {
      // shutdown if mongo failed to provide opportunity to fix issue and restart
      app().quit();
   }
}

// custom oid to avoid monotonic throttling
// https://docs.mongodb.com/master/core/bulk-write-operations/#avoid-monotonic-throttling
bsoncxx::oid make_custom_oid() {
   bsoncxx::oid x = bsoncxx::oid();
   const char* p = x.bytes();
   std::swap((short&)p[0], (short&)p[10]);
   return x;
}

} // anonymous namespace

void hblf_mongo_db_plugin_impl::purge_abi_cache() {
   if( abi_cache_index.size() < abi_cache_size ) return;

   // remove the oldest (smallest) last accessed
   auto& idx = abi_cache_index.get<by_last_access>();
   auto itr = idx.begin();
   if( itr != idx.end() ) {
      idx.erase( itr );
   }
}

optional<abi_serializer> hblf_mongo_db_plugin_impl::get_abi_serializer( account_name n ) {
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   if( n.good()) {
      try {

         auto itr = abi_cache_index.find( n );
         if( itr != abi_cache_index.end() ) {
            abi_cache_index.modify( itr, []( auto& entry ) {
               entry.last_accessed = fc::time_point::now();
            });

            return itr->serializer;
         }

         auto account = _accounts.find_one( make_document( kvp("name", n.to_string())) );
         if(account) {
            auto view = account->view();
            abi_def abi;
            if( view.find( "abi" ) != view.end()) {
               try {
                  abi = fc::json::from_string( bsoncxx::to_json( view["abi"].get_document())).as<abi_def>();
               } catch (...) {
                  ilog( "Unable to convert account abi to abi_def for ${n}", ( "n", n ));
                  return optional<abi_serializer>();
               }

               purge_abi_cache(); // make room if necessary
               abi_cache entry;
               entry.account = n;
               entry.last_accessed = fc::time_point::now();
               abi_serializer abis;
               if( n == chain::config::system_account_name ) {
                  // redefine eosio setabi.abi from bytes to abi_def
                  // Done so that abi is stored as abi_def in mongo instead of as bytes
                  auto itr = std::find_if( abi.structs.begin(), abi.structs.end(),
                                           []( const auto& s ) { return s.name == "setabi"; } );
                  if( itr != abi.structs.end() ) {
                     auto itr2 = std::find_if( itr->fields.begin(), itr->fields.end(),
                                               []( const auto& f ) { return f.name == "abi"; } );
                     if( itr2 != itr->fields.end() ) {
                        if( itr2->type == "bytes" ) {
                           itr2->type = "abi_def";
                           // unpack setabi.abi as abi_def instead of as bytes
                           abis.add_specialized_unpack_pack( "abi_def",
                                 std::make_pair<abi_serializer::unpack_function, abi_serializer::pack_function>(
                                       []( fc::datastream<const char*>& stream, bool is_array, bool is_optional ) -> fc::variant {
                                          EOS_ASSERT( !is_array && !is_optional, chain::mongo_db_exception, "unexpected abi_def");
                                          chain::bytes temp;
                                          fc::raw::unpack( stream, temp );
                                          return fc::variant( fc::raw::unpack<abi_def>( temp ) );
                                       },
                                       []( const fc::variant& var, fc::datastream<char*>& ds, bool is_array, bool is_optional ) {
                                          EOS_ASSERT( false, chain::mongo_db_exception, "never called" );
                                       }
                                 ) );
                        }
                     }
                  }
               }
               // mongo does not like empty json keys
               // make abi_serializer use empty_name instead of "" for the action data
               for( auto& s : abi.structs ) {
                  if( s.name.empty() ) {
                     s.name = "empty_struct_name";
                  }
                  for( auto& f : s.fields ) {
                     if( f.name.empty() ) {
                        f.name = "empty_field_name";
                     }
                  }
               }
               abis.set_abi( abi, abi_serializer_max_time );
               entry.serializer.emplace( std::move( abis ) );
               abi_cache_index.insert( entry );
               return entry.serializer;
            }
         }
      } FC_CAPTURE_AND_LOG((n))
   }
   return optional<abi_serializer>();
}

template<typename T>
fc::variant hblf_mongo_db_plugin_impl::to_variant_with_abi( const T& obj ) {
   fc::variant pretty_output;
   abi_serializer::to_variant( obj, pretty_output,
                               [&]( account_name n ) { return get_abi_serializer( n ); },
                               abi_serializer_max_time );
   return pretty_output;
}

void hblf_mongo_db_plugin_impl::process_applied_transaction( const chain::transaction_trace_ptr& t ) {
   try {
      // always call since we need to capture setabi on accounts even if not storing transaction traces
      _process_applied_transaction( t );
   } catch (fc::exception& e) {
      elog("FC Exception while processing applied transaction trace: ${e}", ("e", e.to_detail_string()));
   } catch (std::exception& e) {
      elog("STD Exception while processing applied transaction trace: ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while processing applied transaction trace");
   }
}

bool
hblf_mongo_db_plugin_impl::add_action_trace( mongocxx::bulk_write& bulk_action_traces, const chain::action_trace& atrace,
                                        const chain::transaction_trace_ptr& t,
                                        bool executed, const std::chrono::milliseconds& now,
                                        bool& write_ttrace )
{
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;

   if(!executed || atrace.receiver != N(vankia.hblf)) {
      return false;
   }

   bool added = false;
   const bool in_filter = (store_action_traces || store_transaction_traces) && start_block_reached &&
                    filter_include( atrace.receiver, atrace.act.name, atrace.act.authorization );
   write_ttrace |= in_filter;
   if( start_block_reached && store_action_traces && in_filter ) {
      auto action_traces_doc = bsoncxx::builder::basic::document{};
      // improve data distributivity when using mongodb sharding
      action_traces_doc.append( kvp( "_id", make_custom_oid() ) );

      auto v = to_variant_with_abi( atrace );
      string json = fc::json::to_string( v );
      try {
         const auto& value = bsoncxx::from_json( json );
         action_traces_doc.append( bsoncxx::builder::concatenate_doc{value.view()} );
         if( executed ) {
            bsoncxx::document::element act_ele = value.view()["act"];
            if (act_ele) {
               bsoncxx::document::view actdoc{act_ele.get_document()};
               update_base_col( atrace.act, actdoc);
            }
         }
      } catch( bsoncxx::exception& ) {
         try {
            json = fc::prune_invalid_utf8( json );
            const auto& value = bsoncxx::from_json( json );
            action_traces_doc.append( bsoncxx::builder::concatenate_doc{value.view()} );
            action_traces_doc.append( kvp( "non-utf8-purged", b_bool{true} ) );
         } catch( bsoncxx::exception& e ) {
            elog( "Unable to convert action trace JSON to MongoDB JSON: ${e}", ("e", e.what()) );
            elog( "  JSON: ${j}", ("j", json) );
         }
      }
      if( t->receipt.valid() ) {
         action_traces_doc.append( kvp( "trx_status", std::string( t->receipt->status ) ) );
      }
      action_traces_doc.append( kvp( "createdAt", b_date{now} ) );

      mongocxx::model::insert_one insert_op{action_traces_doc.view()};
      bulk_action_traces.append( insert_op );
      added = true;
   }

   return added;
}


void hblf_mongo_db_plugin_impl::_process_applied_transaction( const chain::transaction_trace_ptr& t ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;

   auto trans_traces_doc = bsoncxx::builder::basic::document{};

   auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()});

   mongocxx::options::bulk_write bulk_opts;
   bulk_opts.ordered(false);
   mongocxx::bulk_write bulk_action_student_traces = _student_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_teacher_traces = _teacher_traces.create_bulk_write(bulk_opts);
   bool write_student_atraces = false;
   bool write_teacher_atraces = false;
   bool write_ttrace = false; // filters apply to transaction_traces as well
   bool executed = t->receipt.valid() && t->receipt->status == chain::transaction_receipt_header::executed;

   for( const auto& atrace : t->action_traces ) {
      try {
         if(atrace.act.name == addstudent || atrace.act.name == modstudent || atrace.act.name == delstudent){
            write_student_atraces |= add_action_trace( bulk_action_student_traces, atrace, t, executed, now, write_ttrace );
         }else if(atrace.act.name == addteachbase || atrace.act.name == modteachbase || atrace.act.name == delteachbase){
            write_teacher_atraces |= add_action_trace( bulk_action_teacher_traces, atrace, t, executed, now, write_ttrace );
         }
      } catch(...) {
         handle_mongo_exception("add action traces", __LINE__);
      }
   }

   if( !start_block_reached ) return; //< add_action_trace calls update_base_col which must be called always


   // insert action_traces
   if( write_student_atraces ) {
      try {
         if( !bulk_action_student_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_teacher_atraces ) {
      try {
         if( !bulk_action_teacher_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

}


namespace {

void create_student( mongocxx::collection& students, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "userId", userId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_student userId " << userId_ele.get_utf8().value << std::endl;
      if( !students.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert student");
      }
   } catch (...) {
      handle_mongo_exception( "create_student", __LINE__ );
   }
}

void update_student( mongocxx::collection& students, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );

   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "userId", userId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_student userId " << userId_ele.get_utf8().value << std::endl;
      if( !students.update_one( make_document( kvp("userId", userId_ele.get_value())), update.view(), update_opts )) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update student");
      }
   } catch (...) {
      handle_mongo_exception( "update_student", __LINE__ );
   }
}

void delete_student( mongocxx::collection& students, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element userId_ele = data["userId"]; 
   try {
      std::cout << "delete_student userId " << userId_ele.get_utf8().value << std::endl;
      if( !students.delete_one( make_document( kvp("userId", userId_ele.get_value())))) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete student");
      }
   } catch (...) {
      handle_mongo_exception( "delete_student", __LINE__ );
   }
}

void create_teacher( mongocxx::collection& teachers, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "userId", userId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_teacher userId " << userId_ele.get_utf8().value << std::endl;
      if( !teachers.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert teacher");
      }
   } catch (...) {
      handle_mongo_exception( "create_teacher", __LINE__ );
   }
}

void update_teacher( mongocxx::collection& teachers, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );

   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "userId", userId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_teacher userId " << userId_ele.get_utf8().value << std::endl;
      if( !teachers.update_one( make_document( kvp("userId", userId_ele.get_value())), update.view(), update_opts )) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update teacher");
      }
   } catch (...) {
      handle_mongo_exception( "update_teacher", __LINE__ );
   }
}

void delete_teacher( mongocxx::collection& teachers, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element userId_ele = data["userId"]; 
   try {
      std::cout << "delete_teacher userId " << userId_ele.get_utf8().value << std::endl;
      if( !teachers.delete_one( make_document( kvp("userId", userId_ele.get_value())))) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete teacher");
      }
   } catch (...) {
      handle_mongo_exception( "delete_teacher", __LINE__ );
   }
}

}

void hblf_mongo_db_plugin_impl::update_base_col(const chain::action& act, const bsoncxx::document::view& actdoc)
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using namespace bsoncxx::types;

   // if (act.account != chain::config::system_account_name)
   //    return;

   try {
      bsoncxx::document::element data_ele = actdoc["data"];
      bsoncxx::document::view datadoc;
      if (data_ele) {
         datadoc = bsoncxx::document::view{data_ele.get_document()};
      } else {
         std::cout << "Error: data field missing." << std::endl;
         return;
      }

      if( act.name == addstudent ) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         create_student( _students, datadoc, now );
      }else if( act.name == modstudent ) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         update_student( _students, datadoc, now );
      }else if( act.name == delstudent ) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         delete_student( _students, datadoc, now );
      }else if( act.name == addteachbase ) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         create_teacher( _teachers, datadoc, now );
      }else if( act.name == modteachbase ) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         update_teacher( _teachers, datadoc, now );
      }else if( act.name == delteachbase) {
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         delete_teacher( _teachers, datadoc, now );
      }
      
   } catch( fc::exception& e ) {
      // if unable to unpack native type, skip account creation
   }
}

hblf_mongo_db_plugin_impl::hblf_mongo_db_plugin_impl()
{
}

hblf_mongo_db_plugin_impl::~hblf_mongo_db_plugin_impl() {
   if (!startup) {
      try {
         ilog( "hblf_mongo_db_plugin shutdown in process please be patient this can take a few minutes" );
         done = true;
         condition.notify_one();

         consume_thread.join();

         mongo_pool.reset();
      } catch( std::exception& e ) {
         elog( "Exception on hblf_mongo_db_plugin shutdown of consume thread: ${e}", ("e", e.what()));
      }
   }
}

void hblf_mongo_db_plugin_impl::wipe_database() {
   ilog("mongo db wipe_database");

   auto client = mongo_pool->acquire();
   auto& mongo_conn = *client;

   auto students = mongo_conn[db_name][students_col];
   auto student_traces = mongo_conn[db_name][student_traces_col];
   auto teachers = mongo_conn[db_name][teachers_col];
   auto teacher_traces = mongo_conn[db_name][teacher_traces_col];
   auto accounts = mongo_conn[db_name][accounts_col];


   students.drop();
   student_traces.drop();
   teachers.drop();
   teacher_traces.drop();
   accounts.drop();
   
   ilog("done wipe_database");
}

void hblf_mongo_db_plugin_impl::create_expiration_index(mongocxx::collection& collection, uint32_t expire_after_seconds) {
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::kvp;

   auto indexes = collection.indexes();
   for( auto& index : indexes.list()) {
      auto key = index["key"];
      if( !key ) {
         continue;
      }
      auto field = key["createdAt"];
      if( !field ) {
         continue;
      }

      auto ttl = index["expireAfterSeconds"];
      if( ttl && ttl.get_int32() == expire_after_seconds ) {
         return;
      } else {
         auto name = index["name"].get_utf8();
         ilog( "mongo db drop ttl index for collection ${collection}", ( "collection", collection.name().to_string()));
         indexes.drop_one( name.value );
         break;
      }
   }

   mongocxx::options::index index_options{};
   index_options.expire_after( std::chrono::seconds( expire_after_seconds ));
   index_options.background( true );
   ilog( "mongo db create ttl index for collection ${collection}", ( "collection", collection.name().to_string()));
   collection.create_index( make_document( kvp( "createdAt", 1 )), index_options );
}

void hblf_mongo_db_plugin_impl::init() {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::make_document;
   using bsoncxx::builder::basic::kvp;
   // Create the native contract accounts manually; sadly, we can't run their contracts to make them create themselves
   // See native_contract_chain_initializer::prepare_database()

   ilog("init mongo");
   try {
      auto client = mongo_pool->acquire();
      auto& mongo_conn = *client;

      auto accounts = mongo_conn[db_name][accounts_col];
      if( accounts.count( make_document()) == 0 ) {
         auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

         auto doc = make_document( kvp( "name", name( chain::config::system_account_name ).to_string()),
                                   kvp( "createdAt", b_date{now} ));

         try {
            if( !accounts.insert_one( doc.view())) {
               EOS_ASSERT( false, chain::mongo_db_insert_fail, "Failed to insert account ${n}",
                           ("n", name( chain::config::system_account_name ).to_string()));
            }
         } catch (...) {
            handle_mongo_exception( "account insert", __LINE__ );
         }

         try {
            // MongoDB administrators (to enable sharding) :
            //   1. enableSharding database (default to EOS)
            //   2. shardCollection: blocks, action_traces, transaction_traces, especially action_traces
            //   3. Compound index with shard key (default to _id below), to improve query performance.

            // students indexes
            auto students = mongo_conn[db_name][students_col];
            students.create_index( bsoncxx::from_json( R"xxx({ "userId" : 1, "_id" : 1 })xxx" ));

            auto student_traces = mongo_conn[db_name][student_traces_col];
            student_traces.create_index( bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            // teachers indexes
            auto teachers = mongo_conn[db_name][teachers_col];
            teachers.create_index( bsoncxx::from_json( R"xxx({ "userId" : 1, "_id" : 1 })xxx" ));

            auto teacher_traces = mongo_conn[db_name][teacher_traces_col];
            teacher_traces.create_index( bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            // accounts indexes
            auto accounts = mongo_conn[db_name][accounts_col];
            accounts.create_index( bsoncxx::from_json( R"xxx({ "name" : 1, "_id" : 1 })xxx" ));

         } catch (...) {
            handle_mongo_exception( "create indexes", __LINE__ );
         }
      }

      if( expire_after_seconds > 0 ) {
         try {
            mongocxx::collection students = mongo_conn[db_name][students_col];
            create_expiration_index( students, expire_after_seconds );
            mongocxx::collection student_traces = mongo_conn[db_name][student_traces_col];
            create_expiration_index( student_traces, expire_after_seconds );
            mongocxx::collection teachers = mongo_conn[db_name][teachers_col];
            create_expiration_index( teachers, expire_after_seconds );
            mongocxx::collection teacher_traces = mongo_conn[db_name][teacher_traces_col];
            create_expiration_index( teacher_traces, expire_after_seconds );
            mongocxx::collection accounts = mongo_conn[db_name][accounts_col];
            create_expiration_index( accounts, expire_after_seconds );
         } catch(...) {
            handle_mongo_exception( "create expiration indexes", __LINE__ );
         }
      }
   } catch (...) {
      handle_mongo_exception( "mongo init", __LINE__ );
   }

   ilog("starting db plugin thread");

   consume_thread = std::thread( [this] {
      fc::set_os_thread_name( "mongodb" );
      consume_blocks();
   } );

   startup = false;
}

////////////
// hblf_mongo_db_plugin
////////////

hblf_mongo_db_plugin::hblf_mongo_db_plugin()
:my(new hblf_mongo_db_plugin_impl)
{
}

hblf_mongo_db_plugin::~hblf_mongo_db_plugin()
{
}

void hblf_mongo_db_plugin::set_program_options(options_description& cli, options_description& cfg)
{
   cfg.add_options()
         ("hblf-mongodb-queue-size,q", bpo::value<uint32_t>()->default_value(1024),
         "HBLF The target queue size between nodeos and MongoDB plugin thread.")
         ("hblf-mongodb-abi-cache-size", bpo::value<uint32_t>()->default_value(2048),
          "HBLF The maximum size of the abi cache for serializing data.")
         ("hblf-mongodb-wipe", bpo::bool_switch()->default_value(false),
         "HBLF Required with --replay-blockchain, --hard-replay-blockchain, or --delete-all-blocks to wipe mongo db."
         "HBLF This option required to prevent accidental wipe of mongo db.")
         ("hblf-mongodb-block-start", bpo::value<uint32_t>()->default_value(0),
         "HBLF If specified then only abi data pushed to mongodb until specified block is reached.")
         ("hblf-mongodb-uri,m", bpo::value<std::string>(),
         "HBLF MongoDB URI connection string, see: https://docs.mongodb.com/master/reference/connection-string/."
               " If not specified then plugin is disabled. Default database 'EOS' is used if not specified in URI."
               " Example: mongodb://127.0.0.1:27017/EOS")
         ("hblf-mongodb-update-via-block-num", bpo::value<bool>()->default_value(false),
          "HBLF Update blocks/block_state with latest via block number so that duplicates are overwritten.")
         ("hblf-mongodb-store-blocks", bpo::value<bool>()->default_value(true),
          "HBLF Enables storing blocks in mongodb.")
         ("hblf-mongodb-store-block-states", bpo::value<bool>()->default_value(true),
          "HBLF Enables storing block state in mongodb.")
         ("hblf-mongodb-store-transactions", bpo::value<bool>()->default_value(true),
          "HBLF Enables storing transactions in mongodb.")
         ("hblf-mongodb-store-transaction-traces", bpo::value<bool>()->default_value(true),
          "HBLF Enables storing transaction traces in mongodb.")
         ("hblf-mongodb-store-action-traces", bpo::value<bool>()->default_value(true),
          "HBLF Enables storing action traces in mongodb.")
         ("hblf-mongodb-expire-after-seconds", bpo::value<uint32_t>()->default_value(0),
          "HBLF Enables expiring data in mongodb after a specified number of seconds.")
         ("hblf-mongodb-filter-on", bpo::value<vector<string>>()->composing(),
          "HBLF Track actions which match receiver:action:actor. Receiver, Action, & Actor may be blank to include all. i.e. eosio:: or :transfer:  Use * or leave unspecified to include all.")
         ("hblf-mongodb-filter-out", bpo::value<vector<string>>()->composing(),
          "HBLF Do not track actions which match receiver:action:actor. Receiver, Action, & Actor may be blank to exclude all.")
         ;
}

void hblf_mongo_db_plugin::plugin_initialize(const variables_map& options)
{
   try {
      if( options.count( "hblf-mongodb-uri" )) {
         ilog( "initializing hblf_mongo_db_plugin" );
         my->configured = true;

         if( options.at( "replay-blockchain" ).as<bool>() || options.at( "hard-replay-blockchain" ).as<bool>() || options.at( "delete-all-blocks" ).as<bool>() ) {
            if( options.at( "mongodb-wipe" ).as<bool>()) {
               ilog( "Wiping mongo database on startup" );
               my->wipe_database_on_startup = true;
            } else if( options.count( "mongodb-block-start" ) == 0 ) {
               EOS_ASSERT( false, chain::plugin_config_exception, "--mongodb-wipe required with --replay-blockchain, --hard-replay-blockchain, or --delete-all-blocks"
                                 " --mongodb-wipe will remove all EOS collections from mongodb." );
            }
         }

         if( options.count( "abi-serializer-max-time-ms") == 0 ) {
            EOS_ASSERT(false, chain::plugin_config_exception, "--abi-serializer-max-time-ms required as default value not appropriate for parsing full blocks");
         }
         my->abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();

         if( options.count( "hblf-mongodb-queue-size" )) {
            my->max_queue_size = options.at( "hblf-mongodb-queue-size" ).as<uint32_t>();
         }
         if( options.count( "hblf-mongodb-abi-cache-size" )) {
            my->abi_cache_size = options.at( "hblf-mongodb-abi-cache-size" ).as<uint32_t>();
            EOS_ASSERT( my->abi_cache_size > 0, chain::plugin_config_exception, "hblf-mongodb-abi-cache-size > 0 required" );
         }
         if( options.count( "hblf-mongodb-block-start" )) {
            my->start_block_num = options.at( "hblf-mongodb-block-start" ).as<uint32_t>();
         }
         if( options.count( "hblf-mongodb-update-via-block-num" )) {
            my->update_blocks_via_block_num = options.at( "hblf-mongodb-update-via-block-num" ).as<bool>();
         }
         if( options.count( "hblf-mongodb-store-blocks" )) {
            my->store_blocks = options.at( "hblf-mongodb-store-blocks" ).as<bool>();
         }
         if( options.count( "hblf-mongodb-store-block-states" )) {
            my->store_block_states = options.at( "hblf-mongodb-store-block-states" ).as<bool>();
         }
         if( options.count( "hblf-mongodb-store-transactions" )) {
            my->store_transactions = options.at( "hblf-mongodb-store-transactions" ).as<bool>();
         }
         if( options.count( "hblf-mongodb-store-transaction-traces" )) {
            my->store_transaction_traces = options.at( "hblf-mongodb-store-transaction-traces" ).as<bool>();
         }
         if( options.count( "hblf-mongodb-store-action-traces" )) {
            my->store_action_traces = options.at( "hblf-mongodb-store-action-traces" ).as<bool>();
         }
         if( options.count( "hblf-mongodb-expire-after-seconds" )) {
            my->expire_after_seconds = options.at( "hblf-mongodb-expire-after-seconds" ).as<uint32_t>();
         }
         if( options.count( "hblf-mongodb-filter-on" )) {
            auto fo = options.at( "hblf-mongodb-filter-on" ).as<vector<string>>();
            my->filter_on_star = false;
            for( auto& s : fo ) {
               if( s == "*" ) {
                  my->filter_on_star = true;
                  break;
               }
               std::vector<std::string> v;
               boost::split( v, s, boost::is_any_of( ":" ));
               EOS_ASSERT( v.size() == 3, fc::invalid_arg_exception, "Invalid value ${s} for --mongodb-filter-on", ("s", s));
               filter_entry fe{v[0], v[1], v[2]};
               my->filter_on.insert( fe );
            }
         } else {
            my->filter_on_star = true;
         }
         if( options.count( "hblf-mongodb-filter-out" )) {
            auto fo = options.at( "hblf-mongodb-filter-out" ).as<vector<string>>();
            for( auto& s : fo ) {
               std::vector<std::string> v;
               boost::split( v, s, boost::is_any_of( ":" ));
               EOS_ASSERT( v.size() == 3, fc::invalid_arg_exception, "Invalid value ${s} for --mongodb-filter-out", ("s", s));
               filter_entry fe{v[0], v[1], v[2]};
               my->filter_out.insert( fe );
            }
         }
         if( options.count( "producer-name") ) {
            wlog( "mongodb plugin not recommended on producer node" );
            my->is_producer = true;
         }

         if( my->start_block_num == 0 ) {
            my->start_block_reached = true;
         }

         std::string uri_str = options.at( "hblf-mongodb-uri" ).as<std::string>();
         ilog( "connecting to ${u}", ("u", uri_str));
         mongocxx::uri uri = mongocxx::uri{uri_str};
         my->db_name = uri.database();
         if( my->db_name.empty())
            my->db_name = "EOS";
         my->mongo_pool.emplace(uri);

         // hook up to signals on controller
         chain_plugin* chain_plug = app().find_plugin<chain_plugin>();
         EOS_ASSERT( chain_plug, chain::missing_chain_plugin_exception, ""  );
         auto& chain = chain_plug->chain();
         my->chain_id.emplace( chain.get_chain_id());

         my->accepted_block_connection.emplace( chain.accepted_block.connect( [&]( const chain::block_state_ptr& bs ) {
            my->accepted_block( bs );
         } ));

         my->applied_transaction_connection.emplace(
               chain.applied_transaction.connect( [&]( std::tuple<const chain::transaction_trace_ptr&, const chain::signed_transaction&> t ) {
                  my->applied_transaction( std::get<0>(t) );
               } ));

         if( my->wipe_database_on_startup ) {
            my->wipe_database();
         }
         my->init();
      } else {
         wlog( "eosio::hblf_mongo_db_plugin configured, but no --mongodb-uri specified." );
         wlog( "hblf_mongo_db_plugin disabled." );
      }
   } FC_LOG_AND_RETHROW()
}

void hblf_mongo_db_plugin::plugin_startup()
{
}

void hblf_mongo_db_plugin::plugin_shutdown()
{
   my->accepted_block_connection.reset();
   my->applied_transaction_connection.reset();

   my.reset();
}

} // namespace eosio
