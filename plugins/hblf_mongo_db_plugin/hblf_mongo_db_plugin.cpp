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

   void update_account(const chain::action& act);

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
   mongocxx::collection _d20001;
   mongocxx::collection _d20001_traces;
   mongocxx::collection _d20002;
   mongocxx::collection _d20002_traces;
   mongocxx::collection _d80001;
   mongocxx::collection _d80001_traces;
   mongocxx::collection _d50001;
   mongocxx::collection _d50001_traces;
   mongocxx::collection _d50002;
   mongocxx::collection _d50002_traces;
   mongocxx::collection _d50003;
   mongocxx::collection _d50003_traces;
   mongocxx::collection _d50004;
   mongocxx::collection _d50004_traces;
   mongocxx::collection _d50005;
   mongocxx::collection _d50005_traces;
   mongocxx::collection _d50006;
   mongocxx::collection _d50006_traces;
   mongocxx::collection _d50007;
   mongocxx::collection _d50007_traces;
   mongocxx::collection _d50008;
   mongocxx::collection _d50008_traces;
   mongocxx::collection _d00001;
   mongocxx::collection _d00001_traces;
   mongocxx::collection _d00007;
   mongocxx::collection _d00007_traces;
   mongocxx::collection _d00005;
   mongocxx::collection _d00005_traces;
   mongocxx::collection _d00006;
   mongocxx::collection _d00006_traces;
   mongocxx::collection _d40003;
   mongocxx::collection _d40003_traces;


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

   static const action_name setabi;

   static const action_name addstudent;
   static const action_name modstudent;
   static const action_name delstudent;
   static const action_name addteachbase;
   static const action_name modteachbase;
   static const action_name delteachbase;
   static const action_name addd20001;          //体质健康信息的action
   static const action_name modd20001;        
   static const action_name deld20001;           
   static const action_name addd20002;         //体质健康信息明细的action
   static const action_name modd20002;       
   static const action_name deld20002;          
   static const action_name addd80001;         //教师综合荣誉的action
   static const action_name modd80001;       
   static const action_name deld80001;         
   static const action_name addd50001;         //教师职称评定的action
   static const action_name modd50001;       
   static const action_name deld50001;
   static const action_name addd50002;         //教师教育经历的action
   static const action_name modd50002;
   static const action_name deld50002;
   static const action_name addd50003;         //教职工联系方式的action
   static const action_name modd50003; 
   static const action_name deld50003; 
   static const action_name addd50004;         //政治面貌的action
   static const action_name modd50004;
   static const action_name deld50004;
   static const action_name addd50005;         //聘任信息的action
   static const action_name modd50005;
   static const action_name deld50005;
   static const action_name addd50006;         //语言能力的action
   static const action_name modd50006;
   static const action_name deld50006;
   static const action_name addd50007;        //教师资格的action
   static const action_name modd50007;
   static const action_name deld50007;
   static const action_name addd50008;        //其他技能的action 
   static const action_name modd50008;
   static const action_name deld50008;
   static const action_name addd00001;       //机构信息action
   static const action_name modd00001;
   static const action_name deld00001;
   static const action_name addd00007;       //学校信息action
   static const action_name modd00007;
   static const action_name deld00007;
   static const action_name addd00005;       //学生信息action
   static const action_name modd00005;
   static const action_name deld00005;
   static const action_name addd00006;       //教师信息action
   static const action_name modd00006;
   static const action_name deld00006;
   static const action_name addd40003;       //考试成绩信息action
   static const action_name modd40003;
   static const action_name deld40003;

   


   static const std::string students_col;
   static const std::string student_traces_col;
   static const std::string teachers_col;
   static const std::string teacher_traces_col;
   static const std::string d20001_col;                 //体质健康信息表名
   static const std::string d20001_traces_col;  
   static const std::string d20002_col;                //体质健康信息明细表名
   static const std::string d20002_traces_col; 
   static const std::string d80001_col;                //教师荣誉表名
   static const std::string d80001_traces_col; 
   static const std::string d50001_col;                //教师职称评定表名
   static const std::string d50001_traces_col; 
   static const std::string d50002_col;                //教师教育经历表名
   static const std::string d50002_traces_col; 
   static const std::string d50003_col;               //教职工联系方式信息
   static const std::string d50003_traces_col;
   static const std::string d50004_col;               //政治面貌 
   static const std::string d50004_traces_col;
   static const std::string d50005_col;               //聘任信息
   static const std::string d50005_traces_col;
   static const std::string d50006_col;               //语言能力
   static const std::string d50006_traces_col;
   static const std::string d50007_col;               //教师资格
   static const std::string d50007_traces_col;
   static const std::string d50008_col;               //其他技能 
   static const std::string d50008_traces_col;
   static const std::string d00001_col;               //机构信息
   static const std::string d00001_traces_col;
   static const std::string d00007_col;               //学校信息
   static const std::string d00007_traces_col;
   static const std::string d00005_col;               //学生信息
   static const std::string d00005_traces_col;
   static const std::string d00006_col;               //教师信息 
   static const std::string d00006_traces_col;
   static const std::string d40003_col;               //考试成绩信息 
   static const std::string d40003_traces_col;

   static const std::string accounts_col;
};

const action_name hblf_mongo_db_plugin_impl::setabi = chain::setabi::get_name();

const action_name hblf_mongo_db_plugin_impl::addstudent = N(addstudent);
const action_name hblf_mongo_db_plugin_impl::modstudent = N(modstudent);
const action_name hblf_mongo_db_plugin_impl::delstudent = N(delstudent);
const action_name hblf_mongo_db_plugin_impl::addteachbase = N(addteachbase);
const action_name hblf_mongo_db_plugin_impl::modteachbase = N(modteachbase);
const action_name hblf_mongo_db_plugin_impl::delteachbase = N(delteachbase);
const action_name hblf_mongo_db_plugin_impl::addd20001 = N(addsecooofir);
const action_name hblf_mongo_db_plugin_impl::modd20001 = N(modsecooofir);
const action_name hblf_mongo_db_plugin_impl::deld20001 = N(delsecooofir);
const action_name hblf_mongo_db_plugin_impl::addd20002 = N(addsecooosec);
const action_name hblf_mongo_db_plugin_impl::modd20002 = N(modsecooosec);
const action_name hblf_mongo_db_plugin_impl::deld20002 = N(delsecooosec);
const action_name hblf_mongo_db_plugin_impl::addd80001 = N(addeigooofir);
const action_name hblf_mongo_db_plugin_impl::modd80001 = N(modeigooofir);
const action_name hblf_mongo_db_plugin_impl::deld80001 = N(deleigooofir);
const action_name hblf_mongo_db_plugin_impl::addd50001 = N(addfifooofir);
const action_name hblf_mongo_db_plugin_impl::modd50001 = N(modfifooofir);
const action_name hblf_mongo_db_plugin_impl::deld50001 = N(delfifooofir);
const action_name hblf_mongo_db_plugin_impl::addd50002 = N(addfifooosec);
const action_name hblf_mongo_db_plugin_impl::modd50002 = N(modfifooosec);
const action_name hblf_mongo_db_plugin_impl::deld50002 = N(delfifooosec);
const action_name hblf_mongo_db_plugin_impl::addd50003 = N(addfifooothi);
const action_name hblf_mongo_db_plugin_impl::modd50003 = N(modfifooothi);
const action_name hblf_mongo_db_plugin_impl::deld50003 = N(delfifooothi);
const action_name hblf_mongo_db_plugin_impl::addd50004 = N(addfifooofou);
const action_name hblf_mongo_db_plugin_impl::modd50004 = N(modfifooofou);
const action_name hblf_mongo_db_plugin_impl::deld50004 = N(delfifooofou);
const action_name hblf_mongo_db_plugin_impl::addd50005 = N(addfifooofif);
const action_name hblf_mongo_db_plugin_impl::modd50005 = N(modfifooofif);
const action_name hblf_mongo_db_plugin_impl::deld50005 = N(delfifooofif);
const action_name hblf_mongo_db_plugin_impl::addd50006 = N(addfifooosix);
const action_name hblf_mongo_db_plugin_impl::modd50006 = N(modfifooosix);
const action_name hblf_mongo_db_plugin_impl::deld50006 = N(delfifooosix);
const action_name hblf_mongo_db_plugin_impl::addd50007 = N(addfifooosev);
const action_name hblf_mongo_db_plugin_impl::modd50007 = N(modfifooosev);
const action_name hblf_mongo_db_plugin_impl::deld50007 = N(delfifooosev);
const action_name hblf_mongo_db_plugin_impl::addd50008 = N(addfifoooeig);
const action_name hblf_mongo_db_plugin_impl::modd50008 = N(modfifoooeig);
const action_name hblf_mongo_db_plugin_impl::deld50008 = N(delfifoooeig);
const action_name hblf_mongo_db_plugin_impl::addd00001 = N(addzerooofir);
const action_name hblf_mongo_db_plugin_impl::modd00001 = N(modzerooofir);
const action_name hblf_mongo_db_plugin_impl::deld00001 = N(delzerooofir);
const action_name hblf_mongo_db_plugin_impl::addd00007 = N(addzerooosev);
const action_name hblf_mongo_db_plugin_impl::modd00007 = N(modzerooosev);
const action_name hblf_mongo_db_plugin_impl::deld00007 = N(delzerooosev);
const action_name hblf_mongo_db_plugin_impl::addd00005 = N(addzerooofif);
const action_name hblf_mongo_db_plugin_impl::modd00005 = N(modzerooofif);
const action_name hblf_mongo_db_plugin_impl::deld00005 = N(delzerooofif);
const action_name hblf_mongo_db_plugin_impl::addd00006 = N(addzerooosix);
const action_name hblf_mongo_db_plugin_impl::modd00006 = N(modzerooosix);
const action_name hblf_mongo_db_plugin_impl::deld00006 = N(delzerooosix);
const action_name hblf_mongo_db_plugin_impl::addd40003 = N(addfouooothi);
const action_name hblf_mongo_db_plugin_impl::modd40003 = N(modfouooothi);
const action_name hblf_mongo_db_plugin_impl::deld40003 = N(delfouooothi);






const std::string hblf_mongo_db_plugin_impl::students_col = "students";
const std::string hblf_mongo_db_plugin_impl::student_traces_col = "student_traces";
const std::string hblf_mongo_db_plugin_impl::teachers_col = "teachers";
const std::string hblf_mongo_db_plugin_impl::teacher_traces_col = "teacher_traces";
const std::string hblf_mongo_db_plugin_impl::d20001_col = "d20001";                                       //定义体质健康信息表名(mongo中存储的集合名)
const std::string hblf_mongo_db_plugin_impl::d20001_traces_col = "d20001_traces";        
const std::string hblf_mongo_db_plugin_impl::d20002_col = "d20002";                                      //定义体质健康明细表名
const std::string hblf_mongo_db_plugin_impl::d20002_traces_col = "d20002_traces";        
const std::string hblf_mongo_db_plugin_impl::d80001_col = "d80001";                                      //定义体质健康明细表名
const std::string hblf_mongo_db_plugin_impl::d80001_traces_col = "d80001_traces";      
const std::string hblf_mongo_db_plugin_impl::d50001_col = "d50001";                                     //定义教职称评定表名
const std::string hblf_mongo_db_plugin_impl::d50001_traces_col = "d50001_traces";
const std::string hblf_mongo_db_plugin_impl::d50002_col = "d50002";                                      //定义教师教育经历表名
const std::string hblf_mongo_db_plugin_impl::d50002_traces_col = "d50002_traces";
const std::string hblf_mongo_db_plugin_impl::d50003_col = "d50003";                                      //定义教职工联系方式表名
const std::string hblf_mongo_db_plugin_impl::d50003_traces_col = "d50003_traces";
const std::string hblf_mongo_db_plugin_impl::d50004_col = "d50004";                                      //定义教师政治面貌表名
const std::string hblf_mongo_db_plugin_impl::d50004_traces_col = "d50004_traces";
const std::string hblf_mongo_db_plugin_impl::d50005_col = "d50005";                                      //定义聘任信息表名
const std::string hblf_mongo_db_plugin_impl::d50005_traces_col = "d50005_traces";
const std::string hblf_mongo_db_plugin_impl::d50006_col = "d50006";                                      //定义语言能力表名
const std::string hblf_mongo_db_plugin_impl::d50006_traces_col = "d50006_traces";
const std::string hblf_mongo_db_plugin_impl::d50007_col = "d50007";                                      //定义教师资格表名
const std::string hblf_mongo_db_plugin_impl::d50007_traces_col = "d50007_traces";
const std::string hblf_mongo_db_plugin_impl::d50008_col = "d50008";                                      //定义教师其他技能表名
const std::string hblf_mongo_db_plugin_impl::d50008_traces_col = "d50008_traces";
const std::string hblf_mongo_db_plugin_impl::d00001_col = "d00001";                                      //定义机构信息表名
const std::string hblf_mongo_db_plugin_impl::d00001_traces_col = "d00001_traces";
const std::string hblf_mongo_db_plugin_impl::d00007_col = "d00007";                                      //定义学校信息表名
const std::string hblf_mongo_db_plugin_impl::d00007_traces_col = "d00007_traces";
const std::string hblf_mongo_db_plugin_impl::d00005_col = "d00005";                                      //定义学生信息表名
const std::string hblf_mongo_db_plugin_impl::d00005_traces_col = "d00005_traces";
const std::string hblf_mongo_db_plugin_impl::d00006_col = "d00006";                                      //定义教师信息表名
const std::string hblf_mongo_db_plugin_impl::d00006_traces_col = "d00006_traces";
const std::string hblf_mongo_db_plugin_impl::d40003_col = "d40003";                                      //定义考试成绩信息表名
const std::string hblf_mongo_db_plugin_impl::d40003_traces_col = "d40003_traces";

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
      _d20001 = mongo_conn[db_name][d20001_col];
      _d20001_traces = mongo_conn[db_name][d20001_traces_col];
      _d20002 = mongo_conn[db_name][d20002_col];
      _d20002_traces = mongo_conn[db_name][d20002_traces_col];
      _d80001 = mongo_conn[db_name][d80001_col];
      _d80001_traces = mongo_conn[db_name][d80001_traces_col];
      _d50001 = mongo_conn[db_name][d50001_col];
      _d50001_traces = mongo_conn[db_name][d50001_traces_col];
      _d50002 = mongo_conn[db_name][d50002_col];
      _d50002_traces = mongo_conn[db_name][d50002_traces_col];
      _d50003 = mongo_conn[db_name][d50003_col];
      _d50003_traces = mongo_conn[db_name][d50003_traces_col];
      _d50004 = mongo_conn[db_name][d50004_col];
      _d50004_traces = mongo_conn[db_name][d50004_traces_col];
      _d50005 = mongo_conn[db_name][d50005_col];
      _d50005_traces = mongo_conn[db_name][d50005_traces_col];
      _d50006 = mongo_conn[db_name][d50006_col];
      _d50006_traces = mongo_conn[db_name][d50006_traces_col];
      _d50007 = mongo_conn[db_name][d50007_col];
      _d50007_traces = mongo_conn[db_name][d50007_traces_col];
      _d50008 = mongo_conn[db_name][d50008_col];
      _d50008_traces = mongo_conn[db_name][d50008_traces_col];
      _d00001 = mongo_conn[db_name][d00001_col];
      _d00001_traces = mongo_conn[db_name][d00001_traces_col];
      _d00007 = mongo_conn[db_name][d00007_col];
      _d00007_traces = mongo_conn[db_name][d00007_traces_col];
      _d00005 = mongo_conn[db_name][d00005_col];
      _d00005_traces = mongo_conn[db_name][d00005_traces_col];
      _d00006= mongo_conn[db_name][d00006_col];
      _d00006_traces = mongo_conn[db_name][d00006_traces_col];
       _d40003= mongo_conn[db_name][d40003_col];
      _d40003_traces = mongo_conn[db_name][d40003_traces_col];
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

   if( executed && atrace.receiver == chain::config::system_account_name ) {
      update_account( atrace.act );
   }

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
     // action_traces_doc.append( kvp( "actNums",(t->action_traces.end() - t->action_traces.begin()) ) ); 
      action_traces_doc.append( kvp( "actNums",int(t->action_traces.size()) ) ); 

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
   mongocxx::bulk_write bulk_action_d20001_traces = _d20001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d20002_traces = _d20002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d80001_traces = _d80001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50001_traces = _d50001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50002_traces = _d50002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50003_traces = _d50003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50004_traces = _d50004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50005_traces = _d50005_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50006_traces = _d50006_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50007_traces = _d50007_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d50008_traces = _d50008_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00001_traces = _d00001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00007_traces = _d00007_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00005_traces = _d00005_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00006_traces = _d00006_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d40003_traces = _d40003_traces.create_bulk_write(bulk_opts);
   
   
   bool write_student_atraces = false;
   bool write_teacher_atraces = false;
   bool write_d20001_atraces = false;
   bool write_d20002_atraces = false;
   bool write_d80001_atraces = false;
   bool write_d50001_atraces = false;
   bool write_d50002_atraces = false;
   bool write_d50003_atraces = false;
   bool write_d50004_atraces = false;
   bool write_d50005_atraces = false;
   bool write_d50006_atraces = false;
   bool write_d50007_atraces = false;
   bool write_d50008_atraces = false;
   bool write_d00001_atraces = false;
   bool write_d00007_atraces = false;
   bool write_d00005_atraces = false;
   bool write_d00006_atraces = false;
   bool write_d40003_atraces = false;
   bool write_ttrace = false; // filters apply to transaction_traces as well
   bool executed = t->receipt.valid() && t->receipt->status == chain::transaction_receipt_header::executed;

   for( const auto& atrace : t->action_traces ) {
      try {
         if(atrace.act.name == addstudent || atrace.act.name == modstudent || atrace.act.name == delstudent){
            write_student_atraces |= add_action_trace( bulk_action_student_traces, atrace, t, executed, now, write_ttrace );
         }else if(atrace.act.name == addteachbase || atrace.act.name == modteachbase || atrace.act.name == delteachbase){
            write_teacher_atraces |= add_action_trace( bulk_action_teacher_traces, atrace, t, executed, now, write_ttrace );
         }else if(atrace.act.name == addd20001 || atrace.act.name == modd20001 || atrace.act.name == deld20001) {
            write_d20001_atraces |= add_action_trace(bulk_action_d20001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd20002 || atrace.act.name == modd20002 || atrace.act.name == deld20002){
             write_d20002_atraces |= add_action_trace(bulk_action_d20002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd80001 || atrace.act.name == modd80001 || atrace.act.name == deld80001){
             write_d80001_atraces |= add_action_trace(bulk_action_d80001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50001 || atrace.act.name == modd50001 || atrace.act.name == deld50001){
            write_d50001_atraces |= add_action_trace(bulk_action_d50001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50002 || atrace.act.name == modd50002|| atrace.act.name == deld50002){
            write_d50002_atraces |= add_action_trace(bulk_action_d50002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50003 || atrace.act.name == modd50003 || atrace.act.name == deld50003){
            write_d50003_atraces |= add_action_trace(bulk_action_d50003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50004 || atrace.act.name == modd50004 || atrace.act.name == deld50004){
            write_d50004_atraces |= add_action_trace(bulk_action_d50004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50005|| atrace.act.name == modd50005 || atrace.act.name == deld50005){
            write_d50005_atraces |= add_action_trace(bulk_action_d50005_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50006 || atrace.act.name == modd50006 || atrace.act.name == deld50006){
            write_d50006_atraces |= add_action_trace(bulk_action_d50006_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50007 || atrace.act.name == modd50007 || atrace.act.name == deld50007){
            write_d50007_atraces |= add_action_trace(bulk_action_d50007_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd50008 || atrace.act.name == modd50008 || atrace.act.name == deld50008){
            write_d50008_atraces |= add_action_trace(bulk_action_d50008_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00001 || atrace.act.name == modd00001 || atrace.act.name == deld00001){
               write_d00001_atraces |= add_action_trace(bulk_action_d00001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00007 || atrace.act.name == modd00007 || atrace.act.name == deld00007){
               write_d00007_atraces |= add_action_trace(bulk_action_d00007_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00005 || atrace.act.name == modd00005 || atrace.act.name == deld00005){
                write_d00005_atraces |= add_action_trace(bulk_action_d00005_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00006 || atrace.act.name == modd00006 || atrace.act.name == deld00006){
                write_d00006_atraces |= add_action_trace(bulk_action_d00006_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd40003 || atrace.act.name == modd40003 || atrace.act.name == deld40003){
               write_d40003_atraces |= add_action_trace(bulk_action_d40003_traces,atrace,t,executed,now,write_ttrace);
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
   if( write_d20001_atraces ) {
      try {
         if( !bulk_action_d20001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d20002_atraces ) {
      try {
         if( !bulk_action_d20002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
    if( write_d80001_atraces ) {
      try {
         if( !bulk_action_d80001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d50001_atraces ) {
      try {
         if( !bulk_action_d50001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d50002_atraces ) {
      try {
         if( !bulk_action_d50002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d50003_atraces ) {
      try {
         if( !bulk_action_d50003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d50004_atraces ) {
      try {
         if( !bulk_action_d50004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d50005_atraces ) {
      try {
         if( !bulk_action_d50005_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d50006_atraces ) {
      try {
         if( !bulk_action_d50006_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d50007_atraces ) {
      try {
         if( !bulk_action_d50007_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d50008_atraces ) {
      try {
         if( !bulk_action_d50008_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d00001_atraces ) {
      try {
         if( !bulk_action_d00001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d00007_atraces ) {
      try {
         if( !bulk_action_d00007_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d00005_atraces ) {
      try {
         if( !bulk_action_d00005_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d00006_atraces ) {
      try {
         if( !bulk_action_d00006_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d40003_atraces ) {
      try {
         if( !bulk_action_d40003_traces.execute() ) {
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

//创建体质健康信息
void create_d20001(mongocxx::collection& d20001,const bsoncxx::document::view& data,std::chrono::milliseconds& now)
{
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );
   bsoncxx::document::element tzjkid_ele = data["tzjkid"];
   auto update = make_document(
      kvp( "$set", make_document(   kvp( "tzjkid", tzjkid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d20001 tzjkid " << tzjkid_ele.get_utf8().value << std::endl;
      if( !d20001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert teacher");
      }
   } catch (...) {
      handle_mongo_exception( "create_d20001", __LINE__ );
   }
}

//更新体质健康信息
void update_d20001(mongocxx::collection& d20001,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element tzjkid_ele = data["tzjkid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "tzjkid", tzjkid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
                                   // printf("update这里成功执行了%d",1);
   try {
      std::cout << "update_d20001 tzjkid" << tzjkid_ele.get_utf8().value << std::endl;
      if( !d20001.update_one( make_document( kvp("tzjkid", tzjkid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d20001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d20001", __LINE__ );
   }
}
//删除体质健康信息
void delete_d20001( mongocxx::collection& d20001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element tzjkid_ele = data["tzjkid"]; 
   try {
      std::cout << "delete_d20001 tzjkid " << tzjkid_ele.get_utf8().value << std::endl;
      if( !d20001.delete_one( make_document( kvp("tzjkid", tzjkid_ele.get_value())))) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d20001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d20001", __LINE__ );
   }
}

//创建体质健康明细表
void create_d20002( mongocxx::collection& d20002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xjh_ele = data["xjh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xjh", xjh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d20002 xjh" << xjh_ele.get_utf8().value << std::endl;
      if( !d20002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         // EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert students ${n}", ("n", name));
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d20002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d20002", __LINE__ );
   }
}

//更新体质健康明细表
void update_d20002(mongocxx::collection& d20002,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xjh_ele = data["xjh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xjh", xjh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d20002 xjh" << xjh_ele.get_utf8().value << std::endl;
      if( !d20002.update_one( make_document( kvp("xjh", xjh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d20002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d20002", __LINE__ );
   }
}

//删除体质健康明细表
void delete_d20002( mongocxx::collection& d20002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xjh_ele = data["xjh"]; 
   try {
      std::cout << "delete_d20002 xjh " << xjh_ele.get_utf8().value << std::endl;
      if( !d20002.delete_one( make_document( kvp("xjh", xjh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d20002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d20002", __LINE__ );
   }
}

//创建教师荣誉信息
void create_d80001( mongocxx::collection& d80001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d80001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d80001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d80001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d80001", __LINE__ );
   }
}
//更新教师荣誉信息
void update_d80001(mongocxx::collection& d80001,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d80001 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d80001.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d80001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d80001", __LINE__ );
   }
}
//删除教师荣誉信息
void delete_d80001( mongocxx::collection& d80001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d80001 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d80001.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d80001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d80001", __LINE__ );
   }
}

//创建教师职称评定
void create_d50001( mongocxx::collection& d50001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50001", __LINE__ );
   }
}

//更新教师教师职称评定
void update_d50001(mongocxx::collection& d50001,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50001 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50001.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50001", __LINE__ );
   }
}

//删除教师职称评定
void delete_d50001( mongocxx::collection& d50001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50001 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50001.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50001", __LINE__ );
   }
}

//创建教师教育经历
void create_d50002( mongocxx::collection& d50002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50002", __LINE__ );
   }
}

//更新教师教育经历
void update_d50002(mongocxx::collection& d50002,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50002.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50002", __LINE__ );
   }
}
//删除教师教育经历
void delete_d50002( mongocxx::collection& d50002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50002 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50002.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50002", __LINE__ );
   }
}

//创建教师联系方式信息
void create_d50003( mongocxx::collection& d50003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50003 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50003", __LINE__ );
   }
}

//更新教师联系方式信息
void update_d50003(mongocxx::collection& d50003,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50003 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50003.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50003", __LINE__ );
   }
}

//删除教师联系方式信息
void delete_d50003( mongocxx::collection& d50003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50003 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50003.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50003", __LINE__ );
   }
}

//创建教师政治面貌
void create_d50004( mongocxx::collection& d50004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50004 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50004", __LINE__ );
   }
}


//更新教师政治面貌
void update_d50004(mongocxx::collection& d50004,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50004 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50004.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50004");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50004", __LINE__ );
   }
}

//删除教师政治面貌
void delete_d50004( mongocxx::collection& d50004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50004 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50004.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50004", __LINE__ );
   }
}

//创建教师聘任信息
void create_d50005( mongocxx::collection& d50005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50005 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50005.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50005");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50005", __LINE__ );
   }
}

//更新教师聘任信息
void update_d50005(mongocxx::collection& d50005,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50005 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50005.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50005");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50005", __LINE__ );
   }
}


//删除教师聘任信息
void delete_d50005( mongocxx::collection& d50005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50005 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50005.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50005");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50005", __LINE__ );
   }
}
//创建教师语言能力
void create_d50006( mongocxx::collection& d50006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50006 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50006.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50006");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50006", __LINE__ );
   }
}

//更新教师语言能力
void update_d50006(mongocxx::collection& d50006,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50006 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50006.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50006");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50006", __LINE__ );
   }
}


//删除教师语言能力
void delete_d50006( mongocxx::collection& d50006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50006 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50006.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50006");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50006", __LINE__ );
   }
}
//创建教师资格
void create_d50007( mongocxx::collection& d50007, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50007 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50007.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50007");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50007", __LINE__ );
   }
}

//更新教师资格
void update_d50007(mongocxx::collection& d50007,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50007 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50007.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50007");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50007", __LINE__ );
   }
}

//删除教师资格
void delete_d50007( mongocxx::collection& d50007, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50007 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50007.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50007");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50007", __LINE__ );
   }
}

//创建教师其他技能
void create_d50008( mongocxx::collection& d50008, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d50008 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d50008.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d50008");
      }
   } catch (...) {
      handle_mongo_exception( "create_d50008", __LINE__ );
   }
}

//更新教师其他技能
void update_d50008(mongocxx::collection& d50008,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d50008 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d50008.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d50008");
      }
   } catch (...) {
      handle_mongo_exception( "update_d50008", __LINE__ );
   }
}

//删除教师其他技能
void delete_d50008( mongocxx::collection& d50008, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d50008 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d50008.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d50008");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d50008", __LINE__ );
   }
}

//创建机构信息表
void create_d00001( mongocxx::collection& d00001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "orgId", orgId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d00001 orgId" <<orgId_ele.get_utf8().value << std::endl;
      if( !d00001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00001", __LINE__ );
   }
}

//更新机构信息
void update_d00001(mongocxx::collection& d00001,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "orgId", orgId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d00001 orgId" << orgId_ele.get_utf8().value << std::endl;
      if( !d00001.update_one( make_document( kvp("orgId", orgId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00001", __LINE__ );
   }
}

//删除机构信息表
void delete_d00001( mongocxx::collection& d00001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element orgId_ele = data["orgId"]; 
   try {
      std::cout << "delete_d00001 orgId " << orgId_ele.get_utf8().value << std::endl;
      if( !d00001.delete_one( make_document( kvp("orgId", orgId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00001", __LINE__ );
   }
}



//创建学校信息表
void create_d00007( mongocxx::collection& d00007, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "orgId", orgId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d00007 orgId" <<orgId_ele.get_utf8().value << std::endl;
      if( !d00007.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00007");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00007", __LINE__ );
   }
}

//更新学校信息
void update_d00007(mongocxx::collection& d00007,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "orgId", orgId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d00007 orgId" << orgId_ele.get_utf8().value << std::endl;
      if( !d00007.update_one( make_document( kvp("orgId", orgId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00007");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00007", __LINE__ );
   }
}

//删除学校信息表
void delete_d00007( mongocxx::collection& d00007, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element orgId_ele = data["orgId"]; 
   try {
      std::cout << "delete_d00007 orgId " << orgId_ele.get_utf8().value << std::endl;
      if( !d00007.delete_one( make_document( kvp("orgId", orgId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00007");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00007", __LINE__ );
   }
}


//创建学生信息表
void create_d00005( mongocxx::collection& d00005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
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
      std::cout << "create_d00005 userId" <<userId_ele.get_utf8().value << std::endl;
      if( !d00005.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00005");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00005", __LINE__ );
   }
}

//更新学生信息
void update_d00005(mongocxx::collection& d00005,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
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
      std::cout << "update_d00005 userId" << userId_ele.get_utf8().value << std::endl;
      if( !d00005.update_one( make_document( kvp("userId", userId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00005");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00005", __LINE__ );
   }
}

//删除学生信息表
void delete_d00005( mongocxx::collection& d00005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element userId_ele = data["userId"]; 
   try {
      std::cout << "delete_d00005userId " << userId_ele.get_utf8().value << std::endl;
      if( !d00005.delete_one( make_document( kvp("userId", userId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00005");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00005", __LINE__ );
   }
}

//创建教师信息表
void create_d00006( mongocxx::collection& d00006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
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
      std::cout << "create_d00006userId" <<userId_ele.get_utf8().value << std::endl;
      if( !d00006.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00006");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00006", __LINE__ );
   }
}

//更新教师信息
void update_d00006(mongocxx::collection& d00006,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
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
      std::cout << "update_d00006 userId" << userId_ele.get_utf8().value << std::endl;
      if( !d00006.update_one( make_document( kvp("userId", userId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00006");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00006", __LINE__ );
   }
}

//删除教师信息表
void delete_d00006( mongocxx::collection& d00006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element userId_ele = data["userId"]; 
   try {
      std::cout << "delete_d00006 userId " << userId_ele.get_utf8().value << std::endl;
      if( !d00006.delete_one( make_document( kvp("userId", userId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00006");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00006", __LINE__ );
   }
}


//创建考试成绩信息表
void create_d40003( mongocxx::collection& d40003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xscjid_ele = data["xscjid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xscjid", xscjid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "create_d40003 xscjid" <<xscjid_ele.get_utf8().value << std::endl;
      if( !d40003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d40003", __LINE__ );
   }
}

//更新考试成绩信息
void update_d40003(mongocxx::collection& d40003,const bsoncxx::document::view& data,std::chrono::milliseconds& now) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xscjid_ele = data["xscjid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xscjid", xscjid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ))));
   try {
      std::cout << "update_d4000 xscjid" << xscjid_ele.get_utf8().value << std::endl;
      if( !d40003.update_one( make_document( kvp("xscjid", xscjid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d40003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d40003", __LINE__ );
   }
}

//删除考试成绩信息表
void delete_d40003( mongocxx::collection& d40003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xscjid_ele = data["xscjid"]; 
   try {
      std::cout << "delete_d40003 xscjid " <<xscjid_ele.get_utf8().value << std::endl;
      if( !d40003.delete_one( make_document( kvp("xscjid", xscjid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40003", __LINE__ );
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
      }else if(act.name == addd20001){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d20001(_d20001,datadoc,now);
      }else if(act.name == modd20001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d20001(_d20001,datadoc,now);
      }else if(act.name == deld20001){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d20001(_d20001,datadoc,now);
      }else if(act.name == addd20002){
               std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d20002(_d20002,datadoc,now);
      }else if(act.name == modd20002)
      {
               std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d20002(_d20002,datadoc,now);
      }else if(act.name == deld20002)
      {
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d20002(_d20002,datadoc,now);
      }else if(act.name == addd80001){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d80001(_d80001,datadoc,now);
      }else if(act.name == modd80001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d80001(_d80001,datadoc,now);
      }else if(act.name == deld80001){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d80001(_d80001,datadoc,now);        
      }else if(act.name == addd50001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50001(_d50001,datadoc,now);
      }else if(act.name == modd50001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50001(_d50001,datadoc,now);
      }else if(act.name == deld50001){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50001(_d50001,datadoc,now);        
      }
      else if(act.name == addd50002){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50002(_d50002,datadoc,now);
      }else if(act.name == modd50002){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50002(_d50002,datadoc,now);
      }else if(act.name == deld50002){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50002(_d50002,datadoc,now);        
      }else if(act.name == addd50003){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50003(_d50003,datadoc,now);
      }else if(act.name == modd50003){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50003(_d50003,datadoc,now);
      }else if(act.name == deld50003){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50003(_d50003,datadoc,now);        
      }else if(act.name == addd50004){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50004(_d50004,datadoc,now);
      }else if(act.name == modd50004){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50004(_d50004,datadoc,now);
      }else if(act.name == deld50004){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50004(_d50004,datadoc,now);        
      }else if(act.name == addd50005){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50005(_d50005,datadoc,now);
      }else if(act.name == modd50005){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50005(_d50005,datadoc,now);
      }else if(act.name == deld50005){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50005(_d50005,datadoc,now);        
      }else if(act.name == addd50006){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50006(_d50006,datadoc,now);
      }else if(act.name == modd50006){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50006(_d50006,datadoc,now);
      }else if(act.name == deld50006){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50006(_d50006,datadoc,now);        
      }else if(act.name == addd50007){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50007(_d50007,datadoc,now);
      }else if(act.name == modd50007){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50007(_d50007,datadoc,now);
      }else if(act.name == deld50007){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50007(_d50007,datadoc,now);        
      }else if(act.name == addd50008){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d50008(_d50008,datadoc,now);
      }else if(act.name == modd50008){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d50008(_d50008,datadoc,now);
      }else if(act.name == deld50008){
         std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d50008(_d50008,datadoc,now);        
      }else if(act.name == addd00001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d00001(_d00001,datadoc,now);
      }else if(act.name == modd00001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d00001(_d00001,datadoc,now);
      }else if(act.name == deld00001){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d00001(_d00001,datadoc,now);
      }else if(act.name == addd00007){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d00007(_d00007,datadoc,now);
      }else if(act.name == modd00007){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               update_d00007(_d00007,datadoc,now);
      }else if(act.name == deld00007){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d00007(_d00007,datadoc,now);
      }else if(act.name == addd00005){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d00005(_d00005,datadoc,now);
      }else if(act.name == modd00005){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
              update_d00005(_d00005,datadoc,now);
      }else if(act.name == deld00005){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d00005(_d00005,datadoc,now);
      }else if(act.name == addd00006){
             std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d00006(_d00006,datadoc,now);
      }else if(act.name == modd00006){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
              update_d00006(_d00006,datadoc,now);
      }else if(act.name == deld00006){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d00006(_d00006,datadoc,now);
      }else if(act.name == addd40003){
             std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               create_d40003(_d40003,datadoc,now);
      }else if(act.name == modd40003){
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
              update_d40003(_d40003,datadoc,now);
      }else if(act.name == deld40003){
             std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
               delete_d40003(_d40003,datadoc,now);
      }
      
   } catch( fc::exception& e ) {
      // if unable to unpack native type, skip account creation
   }
}

void hblf_mongo_db_plugin_impl::update_account(const chain::action& act)
{
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;
   using namespace bsoncxx::types;

   if (act.account != chain::config::system_account_name)
      return;

   try {
     
        if( act.name == setabi ) {
         auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );
         auto setabi = act.data_as<chain::setabi>();
          abi_cache_index.erase( setabi.account );
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
   auto d20001 = mongo_conn[db_name][d20001_col];
    auto d20001_traces = mongo_conn[db_name][d20001_traces_col];
    auto d20002 = mongo_conn[db_name][ d20002_col];
    auto  d20002_traces = mongo_conn[db_name][ d20002_traces_col];
    auto d80001 = mongo_conn[db_name][d80001_col];
    auto d80001_traces = mongo_conn[db_name][d80001_traces_col];
    auto d50001 = mongo_conn[db_name][d50001_col];
    auto d50001_traces = mongo_conn[db_name][d50001_traces_col];
    auto d50002 = mongo_conn[db_name][d50002_col];
    auto d50002_traces = mongo_conn[db_name][d50002_traces_col];
    auto d50003 = mongo_conn[db_name][d50003_col];
    auto d50003_traces = mongo_conn[db_name][d50003_traces_col];
    auto d50004 = mongo_conn[db_name][d50004_col];
    auto d50004_traces = mongo_conn[db_name][d50004_traces_col];
    auto d50005 = mongo_conn[db_name][d50005_col];
    auto d50005_traces = mongo_conn[db_name][d50005_traces_col];
    auto d50006 = mongo_conn[db_name][d50006_col];
    auto d50006_traces = mongo_conn[db_name][d50006_traces_col];
    auto d50007 = mongo_conn[db_name][d50007_col];
    auto d50007_traces = mongo_conn[db_name][d50007_traces_col];
    auto d50008 = mongo_conn[db_name][d50008_col];
    auto d50008_traces = mongo_conn[db_name][d50008_traces_col];
    auto d00001 = mongo_conn[db_name][d00001_col];
    auto d00001_traces = mongo_conn[db_name][d00001_traces_col];
    auto d00007 = mongo_conn[db_name][d00007_col];
    auto d00007_traces = mongo_conn[db_name][d00007_traces_col];
    auto d00005 = mongo_conn[db_name][d00005_col];
    auto d00005_traces = mongo_conn[db_name][d00005_traces_col];
    auto d00006 = mongo_conn[db_name][d00006_col];
    auto d00006_traces = mongo_conn[db_name][d00006_traces_col];
    auto d40003 = mongo_conn[db_name][d40003_col];
    auto d40003_traces = mongo_conn[db_name][d40003_traces_col];
   
   
   students.drop();
   student_traces.drop();
   teachers.drop();
   teacher_traces.drop();
   accounts.drop();
    d20001.drop();
    d20001_traces.drop();   
    d20002.drop();
    d20002_traces.drop();
    d80001.drop();
    d80001_traces.drop();   
    d50001.drop();
    d50001_traces.drop();
    d50002.drop();
    d50002_traces.drop();   
    d50003.drop();
    d50003_traces.drop();   
    d50004.drop();
    d50004_traces.drop();   
    d50005.drop();
    d50005_traces.drop();   
    d50006.drop();
    d50006_traces.drop();   
    d50007.drop();
    d50007_traces.drop();   
    d50008.drop();
    d50008_traces.drop();      
    d00001.drop();
    d00001_traces.drop();
    d00007.drop();
    d00007_traces.drop();       
    d00005.drop();
    d00005_traces.drop();       
    d00006.drop();
    d00006_traces.drop(); 
    d40003.drop();
    d40003_traces.drop();           
    

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

            //d20001 indexes
            auto  d20001 = mongo_conn[db_name][ d20001_col];
            d20001.create_index(bsoncxx::from_json( R"xxx({ "tzjkid" : 1, "_id" : 1 })xxx" ));

            auto  d20001_traces =  mongo_conn[db_name][ d20001_traces_col];
            d20001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d20002 indexes
            auto  d20002 = mongo_conn[db_name][ d20002_col];
             d20002.create_index(bsoncxx::from_json( R"xxx({ "xjh" : 1, "_id" : 1 })xxx" ));

            auto  d20002_traces = mongo_conn[db_name][ d20002_traces_col];
            d20002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d80001 indexes
            auto  d80001 = mongo_conn[db_name][ d80001_col];
            d80001.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d80001_traces =  mongo_conn[db_name][ d80001_traces_col];
            d80001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));
           
             //d50001 indexes
            auto  d50001 = mongo_conn[db_name][ d50001_col];
            d50001.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50001_traces =  mongo_conn[db_name][ d50001_traces_col];
            d50001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50002 indexes
            auto  d50002 = mongo_conn[db_name][ d50002_col];
            d50002.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50002_traces =  mongo_conn[db_name][ d50002_traces_col];
            d50002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50003 indexes
            auto  d50003 = mongo_conn[db_name][ d50003_col];
            d50003.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50003_traces =  mongo_conn[db_name][ d50003_traces_col];
            d50003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50004 indexes
            auto  d50004 = mongo_conn[db_name][ d50004_col];
            d50004.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50004_traces =  mongo_conn[db_name][ d50004_traces_col];
            d50004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50005 indexes
            auto  d50005 = mongo_conn[db_name][ d50005_col];
            d50005.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50005_traces =  mongo_conn[db_name][ d50005_traces_col];
            d50005_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50006 indexes
            auto  d50006 = mongo_conn[db_name][ d50006_col];
            d50001.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50006_traces =  mongo_conn[db_name][ d50006_traces_col];
            d50006_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50007 indexes
            auto  d50007 = mongo_conn[db_name][ d50007_col];
            d50007.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50007_traces =  mongo_conn[db_name][ d50007_traces_col];
            d50007_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d50008 indexes
            auto  d50008 = mongo_conn[db_name][ d50008_col];
            d50008.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));

            auto  d50008_traces =  mongo_conn[db_name][ d50008_traces_col];
            d50008_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d00001 indexes
            auto  d00001 = mongo_conn[db_name][ d00001_col];
            d00001.create_index(bsoncxx::from_json( R"xxx({ "orgId" : 1, "_id" : 1 })xxx" ));

            auto  d00001_traces =  mongo_conn[db_name][ d00001_traces_col];
            d00001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d00007 indexes
            auto  d00007 = mongo_conn[db_name][ d00007_col];
            d00007.create_index(bsoncxx::from_json( R"xxx({ "orgId" : 1, "_id" : 1 })xxx" ));

            auto  d00007_traces =  mongo_conn[db_name][ d00007_traces_col];
            d00007_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d00005 indexes
            auto  d00005 = mongo_conn[db_name][ d00005_col];
            d00005.create_index(bsoncxx::from_json( R"xxx({ "userId" : 1, "_id" : 1 })xxx" ));

            auto  d00005_traces =  mongo_conn[db_name][ d00005_traces_col];
            d00005_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d00006 indexes
            auto  d00006 = mongo_conn[db_name][ d00006_col];
            d00006.create_index(bsoncxx::from_json( R"xxx({ "userId" : 1, "_id" : 1 })xxx" ));

            auto  d00006_traces =  mongo_conn[db_name][ d00006_traces_col];
            d00006_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d40003 indexes
            auto  d40003 = mongo_conn[db_name][ d40003_col];
            d40003.create_index(bsoncxx::from_json( R"xxx({ "xscjid : 1, "_id" : 1 })xxx" ));

            auto  d40003_traces =  mongo_conn[db_name][ d40003_traces_col];
            d40003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

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
            mongocxx::collection d20001 = mongo_conn[db_name][d20001_col];
            create_expiration_index( d20001, expire_after_seconds );
            mongocxx::collection d20001_traces = mongo_conn[db_name][d20001_traces_col];
            create_expiration_index( d20001_traces, expire_after_seconds );
            mongocxx::collection d20002 = mongo_conn[db_name][d20002_col];
            create_expiration_index( d20002, expire_after_seconds );
            mongocxx::collection d20002_traces = mongo_conn[db_name][d20002_traces_col];
            create_expiration_index( d20002_traces, expire_after_seconds );
            mongocxx::collection d80001 = mongo_conn[db_name][d80001_col];
            create_expiration_index( d80001, expire_after_seconds );
            mongocxx::collection d80001_traces = mongo_conn[db_name][d80001_traces_col];
            create_expiration_index( d80001_traces, expire_after_seconds );
            mongocxx::collection d50001 = mongo_conn[db_name][d50001_col];
            create_expiration_index( d50001, expire_after_seconds );
            mongocxx::collection d50001_traces = mongo_conn[db_name][d50001_traces_col];
            create_expiration_index( d50001_traces, expire_after_seconds );
            mongocxx::collection d50002 = mongo_conn[db_name][d50002_col];
            create_expiration_index( d50002, expire_after_seconds );
            mongocxx::collection d50002_traces = mongo_conn[db_name][d50002_traces_col];
            create_expiration_index( d50002_traces, expire_after_seconds );
            mongocxx::collection d50003 = mongo_conn[db_name][d50003_col];
            create_expiration_index( d50003, expire_after_seconds );
            mongocxx::collection d50003_traces = mongo_conn[db_name][d50003_traces_col];
            create_expiration_index( d50003_traces, expire_after_seconds );
            mongocxx::collection d50004 = mongo_conn[db_name][d50004_col];
            create_expiration_index( d50004, expire_after_seconds );
            mongocxx::collection d50004_traces = mongo_conn[db_name][d50004_traces_col];
            create_expiration_index( d50004_traces, expire_after_seconds );
            mongocxx::collection d50005 = mongo_conn[db_name][d50005_col];
            create_expiration_index( d50005, expire_after_seconds );
            mongocxx::collection d50005_traces = mongo_conn[db_name][d50005_traces_col];
            create_expiration_index( d50005_traces, expire_after_seconds );
            mongocxx::collection d50006 = mongo_conn[db_name][d50006_col];
            create_expiration_index( d50006, expire_after_seconds );
            mongocxx::collection d50006_traces = mongo_conn[db_name][d50006_traces_col];
            create_expiration_index( d50006_traces, expire_after_seconds );
            mongocxx::collection d50007 = mongo_conn[db_name][d50007_col];
            create_expiration_index( d50007, expire_after_seconds );
            mongocxx::collection d50007_traces = mongo_conn[db_name][d50007_traces_col];
            create_expiration_index( d50007_traces, expire_after_seconds );
            mongocxx::collection d50008 = mongo_conn[db_name][d50008_col];
            create_expiration_index( d50008, expire_after_seconds );
            mongocxx::collection d50008_traces = mongo_conn[db_name][d50008_traces_col];
            create_expiration_index( d50008_traces, expire_after_seconds );
            mongocxx::collection d00001 = mongo_conn[db_name][d00001_col];
            create_expiration_index( d00001, expire_after_seconds );
            mongocxx::collection d00001_traces = mongo_conn[db_name][d00001_traces_col];
            create_expiration_index( d00001_traces, expire_after_seconds );
            mongocxx::collection d00007 = mongo_conn[db_name][d00007_col];
            create_expiration_index( d00007, expire_after_seconds );
            mongocxx::collection d00007_traces = mongo_conn[db_name][d00007_traces_col];
            create_expiration_index( d00007_traces, expire_after_seconds );
             mongocxx::collection d00005 = mongo_conn[db_name][d00005_col];
            create_expiration_index( d00005, expire_after_seconds );
            mongocxx::collection d00005_traces = mongo_conn[db_name][d00005_traces_col];
            create_expiration_index( d00005_traces, expire_after_seconds );
             mongocxx::collection d00006 = mongo_conn[db_name][d00006_col];
            create_expiration_index( d00006, expire_after_seconds );
            mongocxx::collection d00006_traces = mongo_conn[db_name][d00006_traces_col];
            create_expiration_index( d00006_traces, expire_after_seconds );
            mongocxx::collection d40003 = mongo_conn[db_name][d40003_col];
            create_expiration_index( d40003, expire_after_seconds );
            mongocxx::collection d40003_traces = mongo_conn[db_name][d40003_traces_col];
            create_expiration_index( d40003_traces, expire_after_seconds );
            

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
