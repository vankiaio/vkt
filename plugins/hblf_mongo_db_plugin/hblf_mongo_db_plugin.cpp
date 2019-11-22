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

   void update_base_col(const chain::action& act, const bsoncxx::document::view& actdoc,const chain::transaction_trace_ptr& t);

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
   mongocxx::collection _d30001;
   mongocxx::collection _d30001_traces;
   mongocxx::collection _d30002;
   mongocxx::collection _d30002_traces;
   mongocxx::collection _d30003;
   mongocxx::collection _d30003_traces;
   mongocxx::collection _d30004;
   mongocxx::collection _d30004_traces;
   mongocxx::collection _d40001;
   mongocxx::collection _d40001_traces;
   mongocxx::collection _d40002;
   mongocxx::collection _d40002_traces;
   mongocxx::collection _d40004;
   mongocxx::collection _d40004_traces;
   mongocxx::collection _d40005;
   mongocxx::collection _d40005_traces;
   mongocxx::collection _d40006;
   mongocxx::collection _d40006_traces;
   mongocxx::collection _d80002;
   mongocxx::collection _d80002_traces;
   mongocxx::collection _d90001;
   mongocxx::collection _d90001_traces;
   mongocxx::collection _d90002;
   mongocxx::collection _d90002_traces;
   mongocxx::collection _d90003;
   mongocxx::collection _d90003_traces;
   mongocxx::collection _d90004;
   mongocxx::collection _d90004_traces;
   mongocxx::collection _d90005;
   mongocxx::collection _d90005_traces;
   mongocxx::collection _d60001;
   mongocxx::collection _d60001_traces;
   mongocxx::collection _d60002;
   mongocxx::collection _d60002_traces;
   mongocxx::collection _d60003;
   mongocxx::collection _d60003_traces;
   mongocxx::collection _d70001;
   mongocxx::collection _d70001_traces;
   mongocxx::collection _d70002;
   mongocxx::collection _d70002_traces;
   mongocxx::collection _d70003;
   mongocxx::collection _d70003_traces;
   mongocxx::collection _d70004;
   mongocxx::collection _d70004_traces;
   mongocxx::collection _d00002;
   mongocxx::collection _d00002_traces;
   mongocxx::collection _d00003;
   mongocxx::collection _d00003_traces;
   mongocxx::collection _d00004;
   mongocxx::collection _d00004_traces;
   mongocxx::collection _d41001;
   mongocxx::collection _d41001_traces;
   mongocxx::collection _d41002;
   mongocxx::collection _d41002_traces;
   mongocxx::collection _d41003;
   mongocxx::collection _d41003_traces;
   mongocxx::collection _d11001;
   mongocxx::collection _d11001_traces;
   mongocxx::collection _d11002;
   mongocxx::collection _d11002_traces;
   mongocxx::collection _d11003;
   mongocxx::collection _d11003_traces;
   mongocxx::collection _d11004;
   mongocxx::collection _d11004_traces;
   mongocxx::collection _d11005;
   mongocxx::collection _d11005_traces;
   mongocxx::collection _d11006;
   mongocxx::collection _d11006_traces;
   mongocxx::collection _d11007;
   mongocxx::collection _d11007_traces;
   mongocxx::collection _d11008;
   mongocxx::collection _d11008_traces;
   mongocxx::collection _d11009;
   mongocxx::collection _d11009_traces;
   mongocxx::collection _d11010;
   mongocxx::collection _d11010_traces;
   mongocxx::collection _d11011;
   mongocxx::collection _d11011_traces;
   mongocxx::collection _d11012;
   mongocxx::collection _d11012_traces;

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
   static const action_name addd30001;      //师德培训
   static const action_name modd30001;
   static const action_name deld30001;
   static const action_name addd30002;     //师德考核
   static const action_name modd30002;
   static const action_name deld30002;
   static const action_name addd30003;     //师德奖励
   static const action_name modd30003;
   static const action_name deld30003;
   static const action_name addd30004;     //师德惩处
   static const action_name modd30004;
   static const action_name deld30004;
   static const action_name addd40001;       //学生考试信息action     
   static const action_name modd40001;
   static const action_name deld40001;
   static const action_name addd40002;       //考试参考科目情况action     
   static const action_name modd40002;
   static const action_name deld40002;
   static const action_name addd40004;       //考试试卷信息action     
   static const action_name modd40004;
   static const action_name deld40004;
   static const action_name addd40005;       //试题知识点action     
   static const action_name modd40005;
   static const action_name deld40005;
   static const action_name addd40006;       //学生考试试题得分action     
   static const action_name modd40006;
   static const action_name deld40006;
   static const action_name addd80002;         //教师出勤绩效的action
   static const action_name modd80002;       
   static const action_name deld80002;         
   static const action_name addd90001;         //优质课信息的action
   static const action_name modd90001;       
   static const action_name deld90001;   
   static const action_name addd90002;         //发明专利的action
   static const action_name modd90002;       
   static const action_name deld90002;   
   static const action_name addd90003;         //课题信息的action
   static const action_name modd90003;       
   static const action_name deld90003;  
   static const action_name addd90004;         //著作的action
   static const action_name modd90004;       
   static const action_name deld90004;    
   static const action_name addd90005;         //论文的action
   static const action_name modd90005;       
   static const action_name deld90005;    
   static const action_name addd60001;        //辅导学生获奖action
   static const action_name modd60001;       
   static const action_name deld60001;    
   static const action_name addd60002;       //管理情况
   static const action_name modd60002;       
   static const action_name deld60002;    
   static const action_name addd60003;        //指导青年教师
   static const action_name modd60003;       
   static const action_name deld60003;    

   static const action_name addd70001;        //进修交流
   static const action_name modd70001;       
   static const action_name deld70001;   
   static const action_name addd70002;        //国内培训
   static const action_name modd70002;       
   static const action_name deld70002;    
   static const action_name addd70003;        //海外研修
   static const action_name modd70003;       
   static const action_name deld70003;    
   static const action_name addd70004;        //交流轮岗
   static const action_name modd70004;       
   static const action_name deld70004;    
   static const action_name addd00002;       
   static const action_name modd00002;
   static const action_name deld00002;
   static const action_name addd00003;      
   static const action_name modd00003;
   static const action_name deld00003;
   static const action_name addd00004;       
   static const action_name modd00004;
   static const action_name deld00004;
   static const action_name addd41001;       
   static const action_name modd41001;
   static const action_name deld41001;
   static const action_name addd41002;       
   static const action_name modd41002;
   static const action_name deld41002;
   static const action_name addd41003;       
   static const action_name modd41003;
   static const action_name deld41003;
   static const action_name addd11001;       
   static const action_name modd11001;
   static const action_name deld11001;
   static const action_name addd11002;       
   static const action_name modd11002;
   static const action_name deld11002;
   static const action_name addd11003;       
   static const action_name modd11003;
   static const action_name deld11003;
   static const action_name addd11004;       
   static const action_name modd11004;
   static const action_name deld11004;
   static const action_name addd11005;       
   static const action_name modd11005;
   static const action_name deld11005;
   static const action_name addd11006;       
   static const action_name modd11006;
   static const action_name deld11006;
   static const action_name addd11007;       
   static const action_name modd11007;
   static const action_name deld11007;
   static const action_name addd11008;       
   static const action_name modd11008;
   static const action_name deld11008;
   static const action_name addd11009;       
   static const action_name modd11009;
   static const action_name deld11009;
   static const action_name addd11010;       
   static const action_name modd11010;
   static const action_name deld11010;
   static const action_name addd11011;       
   static const action_name modd11011;
   static const action_name deld11011;
   static const action_name addd11012;       
   static const action_name modd11012;
   static const action_name deld11012;



   



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
   static const std::string d30001_col;               //师德 培训
   static const std::string d30001_traces_col;
   static const std::string d30002_col;               //师德考核 
   static const std::string d30002_traces_col;
   static const std::string d30003_col;               //师德奖励 
   static const std::string d30003_traces_col;
   static const std::string d30004_col;               //师德惩处
   static const std::string d30004_traces_col;
   static const std::string d40001_col;               //学生考试信息
   static const std::string d40001_traces_col;
   static const std::string d40002_col;               //考试参考科目情况  
   static const std::string d40002_traces_col;
   static const std::string d40004_col;               //考试试卷信息
   static const std::string d40004_traces_col;
   static const std::string d40005_col;               //试题知识点 
   static const std::string d40005_traces_col;
   static const std::string d40006_col;               //学生考试试题得分 
   static const std::string d40006_traces_col;
   static const std::string d80002_col;                //教师出勤绩效
   static const std::string d80002_traces_col; 
   static const std::string d90001_col;              //优质课信息
   static const std::string d90001_traces_col;
   static const std::string d90002_col;              //发明专利
   static const std::string d90002_traces_col;
   static const std::string d90003_col;       //课题信息       
   static const std::string d90003_traces_col;
   static const std::string d90004_col;             //著作 
   static const std::string d90004_traces_col;
   static const std::string d90005_col;          //论文
   static const std::string d90005_traces_col;
   static const std::string d60001_col;          //辅导学生获奖
   static const std::string d60001_traces_col;
   static const std::string d60002_col;          //管理情况 
   static const std::string d60002_traces_col;
   static const std::string d60003_col;          //指导青年教师
   static const std::string d60003_traces_col;
   static const std::string d70001_col;          //进修交流
   static const std::string d70001_traces_col;
   static const std::string d70002_col;          //国内培训
   static const std::string d70002_traces_col;
   static const std::string d70003_col;          //海外研修
   static const std::string d70003_traces_col;
   static const std::string d70004_col;          //交流轮岗
   static const std::string d70004_traces_col;
   static const std::string d00002_col;               
   static const std::string d00002_traces_col;
   static const std::string d00003_col;               
   static const std::string d00003_traces_col;
   static const std::string d00004_col;               
   static const std::string d00004_traces_col;
   static const std::string d41001_col;               
   static const std::string d41001_traces_col;
   static const std::string d41002_col;               
   static const std::string d41002_traces_col;
   static const std::string d41003_col;               
   static const std::string d41003_traces_col;
   static const std::string d11001_col;               
   static const std::string d11001_traces_col;
   static const std::string d11002_col;               
   static const std::string d11002_traces_col;
   static const std::string d11003_col;               
   static const std::string d11003_traces_col;
   static const std::string d11004_col;               
   static const std::string d11004_traces_col;
   static const std::string d11005_col;               
   static const std::string d11005_traces_col;
   static const std::string d11006_col;               
   static const std::string d11006_traces_col;
   static const std::string d11007_col;               
   static const std::string d11007_traces_col;
   static const std::string d11008_col;               
   static const std::string d11008_traces_col;
   static const std::string d11009_col;               
   static const std::string d11009_traces_col;
   static const std::string d11010_col;               
   static const std::string d11010_traces_col;
   static const std::string d11011_col;               
   static const std::string d11011_traces_col;
   static const std::string d11012_col;               
   static const std::string d11012_traces_col;







   static const std::string accounts_col;
};

const action_name hblf_mongo_db_plugin_impl::setabi = chain::setabi::get_name();


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
const action_name hblf_mongo_db_plugin_impl::addd30001 = N(addthiooofir);
const action_name hblf_mongo_db_plugin_impl::modd30001 = N(modthiooofir);
const action_name hblf_mongo_db_plugin_impl::deld30001 = N(delthiooofir);
const action_name hblf_mongo_db_plugin_impl::addd30002 = N(addthiooosec);
const action_name hblf_mongo_db_plugin_impl::modd30002 = N(modthiooosec);
const action_name hblf_mongo_db_plugin_impl::deld30002 = N(delthiooosec);
const action_name hblf_mongo_db_plugin_impl::addd30003 = N(addthiooothi);
const action_name hblf_mongo_db_plugin_impl::modd30003 = N(modthiooothi);
const action_name hblf_mongo_db_plugin_impl::deld30003 = N(delthiooothi);
const action_name hblf_mongo_db_plugin_impl::addd30004 = N(addthiooofou);
const action_name hblf_mongo_db_plugin_impl::modd30004 = N(modthiooofou);
const action_name hblf_mongo_db_plugin_impl::deld30004 = N(delthiooofou);
const action_name hblf_mongo_db_plugin_impl::addd40001 = N(addfouooofir);
const action_name hblf_mongo_db_plugin_impl::modd40001 = N(modfouooofir);
const action_name hblf_mongo_db_plugin_impl::deld40001 = N(delfouooofir);
const action_name hblf_mongo_db_plugin_impl::addd40002 = N(addfouooosec);
const action_name hblf_mongo_db_plugin_impl::modd40002 = N(modfouooosec);
const action_name hblf_mongo_db_plugin_impl::deld40002 = N(delfouooosec);
const action_name hblf_mongo_db_plugin_impl::addd40004 = N(addfouooofou);
const action_name hblf_mongo_db_plugin_impl::modd40004 = N(modfouooofou);
const action_name hblf_mongo_db_plugin_impl::deld40004 = N(delfouooofou);
const action_name hblf_mongo_db_plugin_impl::addd40005 = N(addfouooofif);
const action_name hblf_mongo_db_plugin_impl::modd40005 = N(modfouooofif);
const action_name hblf_mongo_db_plugin_impl::deld40005 = N(delfouooofif);
const action_name hblf_mongo_db_plugin_impl::addd40006 = N(addfouooosix);
const action_name hblf_mongo_db_plugin_impl::modd40006 = N(modfouooosix);
const action_name hblf_mongo_db_plugin_impl::deld40006 = N(delfouooosix);
const action_name hblf_mongo_db_plugin_impl::addd80002 = N(addeigooosec);
const action_name hblf_mongo_db_plugin_impl::modd80002 = N(modeigooosec);
const action_name hblf_mongo_db_plugin_impl::deld80002 = N(deleigooosec);
const action_name hblf_mongo_db_plugin_impl::addd90001 = N(addninooofir);
const action_name hblf_mongo_db_plugin_impl::modd90001= N(modninooofir);
const action_name hblf_mongo_db_plugin_impl::deld90001 = N(delninooofir);
const action_name hblf_mongo_db_plugin_impl::addd90002 = N(addninooosec);
const action_name hblf_mongo_db_plugin_impl::modd90002 = N(modninooosec);
const action_name hblf_mongo_db_plugin_impl::deld90002 = N(delninooosec);
const action_name hblf_mongo_db_plugin_impl::addd90003 = N(addninooothi);
const action_name hblf_mongo_db_plugin_impl::modd90003 = N(modninooothi);
const action_name hblf_mongo_db_plugin_impl::deld90003 = N(delninooothi);
const action_name hblf_mongo_db_plugin_impl::addd90004 = N(addninooofou);
const action_name hblf_mongo_db_plugin_impl::modd90004 = N(modninooofou);
const action_name hblf_mongo_db_plugin_impl::deld90004 = N(delninooofou);
const action_name hblf_mongo_db_plugin_impl::addd90005 = N(addninooofif);
const action_name hblf_mongo_db_plugin_impl::modd90005 = N(modninooofif);
const action_name hblf_mongo_db_plugin_impl::deld90005 = N(delninooofif);
const action_name hblf_mongo_db_plugin_impl::addd60001 = N(addsixooofir);
const action_name hblf_mongo_db_plugin_impl::modd60001= N(modsixooofir);
const action_name hblf_mongo_db_plugin_impl::deld60001 = N(delsixooofir);
const action_name hblf_mongo_db_plugin_impl::addd60002 = N(addsixooosec);
const action_name hblf_mongo_db_plugin_impl::modd60002 = N(modsixooosec);
const action_name hblf_mongo_db_plugin_impl::deld60002 = N(delsixooosec);
const action_name hblf_mongo_db_plugin_impl::addd60003 = N(addsixooothi);
const action_name hblf_mongo_db_plugin_impl::modd60003 = N(modsixooothi);
const action_name hblf_mongo_db_plugin_impl::deld60003 = N(delsixooothi);
const action_name hblf_mongo_db_plugin_impl::addd70001 = N(addsevooofir);
const action_name hblf_mongo_db_plugin_impl::modd70001 = N(modsevooofir);
const action_name hblf_mongo_db_plugin_impl::deld70001 = N(delsevooofir);
const action_name hblf_mongo_db_plugin_impl::addd70002 = N(addsevooosec);
const action_name hblf_mongo_db_plugin_impl::modd70002 = N(modsevooosec);
const action_name hblf_mongo_db_plugin_impl::deld70002 = N(delsevooosec);
const action_name hblf_mongo_db_plugin_impl::addd70003 = N(addsevooothi);
const action_name hblf_mongo_db_plugin_impl::modd70003 = N(modsevooothi);
const action_name hblf_mongo_db_plugin_impl::deld70003= N(delsevooothi);
const action_name hblf_mongo_db_plugin_impl::addd70004 = N(addsevooofou);
const action_name hblf_mongo_db_plugin_impl::modd70004 = N(modsevooofou);
const action_name hblf_mongo_db_plugin_impl::deld70004 = N(delsevooofou);
const action_name hblf_mongo_db_plugin_impl::addd00002 = N(addzerooosec);
const action_name hblf_mongo_db_plugin_impl::modd00002 = N(modzerooosec);
const action_name hblf_mongo_db_plugin_impl::deld00002 = N(delzerooosec);
const action_name hblf_mongo_db_plugin_impl::addd00003 = N(addzerooothi);
const action_name hblf_mongo_db_plugin_impl::modd00003= N(modzerooothi);
const action_name hblf_mongo_db_plugin_impl::deld00003 = N(delzerooothi);
const action_name hblf_mongo_db_plugin_impl::addd00004 = N(addzerooofou);
const action_name hblf_mongo_db_plugin_impl::modd00004 = N(modzerooofou);
const action_name hblf_mongo_db_plugin_impl::deld00004 = N(delzerooofou);
const action_name hblf_mongo_db_plugin_impl::addd41001 = N(addfoufirfir);
const action_name hblf_mongo_db_plugin_impl::modd41001 = N(modfoufirfir);
const action_name hblf_mongo_db_plugin_impl::deld41001 = N(delfoufirfir);
const action_name hblf_mongo_db_plugin_impl::addd41002 = N(addfoufirsec);
const action_name hblf_mongo_db_plugin_impl::modd41002 = N(modfoufirsec);
const action_name hblf_mongo_db_plugin_impl::deld41002 = N(delfoufirsec);
const action_name hblf_mongo_db_plugin_impl::addd41003 = N(addfoufirthi);
const action_name hblf_mongo_db_plugin_impl::modd41003 = N(modfoufirthi);
const action_name hblf_mongo_db_plugin_impl::deld41003 = N(delfoufirthi);

const action_name hblf_mongo_db_plugin_impl::addd11001 = N(addfirfirfir);
const action_name hblf_mongo_db_plugin_impl::modd11001 = N(modfirfirfir);
const action_name hblf_mongo_db_plugin_impl::deld11001 = N(delfirfirfir);
const action_name hblf_mongo_db_plugin_impl::addd11002 = N(addfirfirsec);
const action_name hblf_mongo_db_plugin_impl::modd11002 = N(modfirfirsec);
const action_name hblf_mongo_db_plugin_impl::deld11002 = N(delfirfirsec);
const action_name hblf_mongo_db_plugin_impl::addd11003 = N(addfirfirthi);
const action_name hblf_mongo_db_plugin_impl::modd11003 = N(modfirfirthi);
const action_name hblf_mongo_db_plugin_impl::deld11003 = N(delfirfirthi);
const action_name hblf_mongo_db_plugin_impl::addd11004 = N(addfirfirfou);
const action_name hblf_mongo_db_plugin_impl::modd11004 = N(modfirfirfou);
const action_name hblf_mongo_db_plugin_impl::deld11004 = N(delfirfirfou);
const action_name hblf_mongo_db_plugin_impl::addd11005 = N(addfirfirfif);
const action_name hblf_mongo_db_plugin_impl::modd11005 = N(modfirfirfif);
const action_name hblf_mongo_db_plugin_impl::deld11005 = N(delfirfirfif);
const action_name hblf_mongo_db_plugin_impl::addd11006 = N(addfirfirsix);
const action_name hblf_mongo_db_plugin_impl::modd11006 = N(modfirfirsix);
const action_name hblf_mongo_db_plugin_impl::deld11006 = N(delfirfirsix);
const action_name hblf_mongo_db_plugin_impl::addd11007 = N(addfirfirsev);
const action_name hblf_mongo_db_plugin_impl::modd11007 = N(modfirfirsev);
const action_name hblf_mongo_db_plugin_impl::deld11007 = N(delfirfirsev);
const action_name hblf_mongo_db_plugin_impl::addd11008 = N(addfirfireig);
const action_name hblf_mongo_db_plugin_impl::modd11008 = N(modfirfireig);
const action_name hblf_mongo_db_plugin_impl::deld11008 = N(delfirfireig);
const action_name hblf_mongo_db_plugin_impl::addd11009 = N(addfirfirnin);
const action_name hblf_mongo_db_plugin_impl::modd11009 = N(modfirfirnin);
const action_name hblf_mongo_db_plugin_impl::deld11009 = N(delfirfirnin);
const action_name hblf_mongo_db_plugin_impl::addd11010 = N(addfirfirten);
const action_name hblf_mongo_db_plugin_impl::modd11010 = N(modfirfirten);
const action_name hblf_mongo_db_plugin_impl::deld11010 = N(delfirfirten);
const action_name hblf_mongo_db_plugin_impl::addd11011 = N(addfirfirele);
const action_name hblf_mongo_db_plugin_impl::modd11011 = N(modfirfirele);
const action_name hblf_mongo_db_plugin_impl::deld11011 = N(delfirfirele);
const action_name hblf_mongo_db_plugin_impl::addd11012= N(addfirfirtwe);
const action_name hblf_mongo_db_plugin_impl::modd11012 = N(modfirfirtwe);
const action_name hblf_mongo_db_plugin_impl::deld11012 = N(delfirfirtwe);












const std::string hblf_mongo_db_plugin_impl::d20001_col = "d20001";                                       //定义体质健康信息表名(mongo中存储的集合名)
const std::string hblf_mongo_db_plugin_impl::d20001_traces_col = "d20001_traces";        
const std::string hblf_mongo_db_plugin_impl::d20002_col = "d20002";                                      //定义体质健康明细表名
const std::string hblf_mongo_db_plugin_impl::d20002_traces_col = "d20002_traces";        
const std::string hblf_mongo_db_plugin_impl::d80001_col = "d80001";                                     
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
const std::string hblf_mongo_db_plugin_impl::d30001_col = "d30001";                                      //定义师德培训表名
const std::string hblf_mongo_db_plugin_impl::d30001_traces_col = "d30001_traces";
const std::string hblf_mongo_db_plugin_impl::d30002_col = "d30002";                                      //定义师德考核表名
const std::string hblf_mongo_db_plugin_impl::d30002_traces_col = "d30002_traces";
const std::string hblf_mongo_db_plugin_impl::d30003_col = "d30003";                                      //定义师德奖励表名
const std::string hblf_mongo_db_plugin_impl::d30003_traces_col = "d30003_traces";
const std::string hblf_mongo_db_plugin_impl::d30004_col = "d30004";                                      //定义师德惩处表名
const std::string hblf_mongo_db_plugin_impl::d30004_traces_col = "d30004_traces";
const std::string hblf_mongo_db_plugin_impl::d40001_col = "d40001";                                      
const std::string hblf_mongo_db_plugin_impl::d40001_traces_col = "d40001_traces";
const std::string hblf_mongo_db_plugin_impl::d40002_col = "d40002";                                      
const std::string hblf_mongo_db_plugin_impl::d40002_traces_col = "d40002_traces";
const std::string hblf_mongo_db_plugin_impl::d40004_col = "d40004";                                      
const std::string hblf_mongo_db_plugin_impl::d40004_traces_col = "d40004_traces";
const std::string hblf_mongo_db_plugin_impl::d40005_col = "d40005";                                      
const std::string hblf_mongo_db_plugin_impl::d40005_traces_col = "d40005_traces";
const std::string hblf_mongo_db_plugin_impl::d40006_col = "d40006";                                      
const std::string hblf_mongo_db_plugin_impl::d40006_traces_col = "d40006_traces";
const std::string hblf_mongo_db_plugin_impl::d80002_col = "d80002";                                      
const std::string hblf_mongo_db_plugin_impl::d80002_traces_col = "d80002_traces";
const std::string hblf_mongo_db_plugin_impl::d90001_col = "d90001";                                      
const std::string hblf_mongo_db_plugin_impl::d90001_traces_col = "d90001_traces";
const std::string hblf_mongo_db_plugin_impl::d90002_col = "d90002";                                      
const std::string hblf_mongo_db_plugin_impl::d90002_traces_col = "d90002_traces";
const std::string hblf_mongo_db_plugin_impl::d90003_col = "d90003";                                      
const std::string hblf_mongo_db_plugin_impl::d90003_traces_col = "d90003_traces";
const std::string hblf_mongo_db_plugin_impl::d90004_col = "d90004";                                      
const std::string hblf_mongo_db_plugin_impl::d90004_traces_col = "d90004_traces";
const std::string hblf_mongo_db_plugin_impl::d90005_col = "d90005";                                      
const std::string hblf_mongo_db_plugin_impl::d90005_traces_col = "d90005_traces";
const std::string hblf_mongo_db_plugin_impl::d60001_col = "d60001";                                      
const std::string hblf_mongo_db_plugin_impl::d60001_traces_col = "d60001_traces";
const std::string hblf_mongo_db_plugin_impl::d60002_col = "d60002";                                      
const std::string hblf_mongo_db_plugin_impl::d60002_traces_col = "d60002_traces";
const std::string hblf_mongo_db_plugin_impl::d60003_col = "d60003";                                      
const std::string hblf_mongo_db_plugin_impl::d60003_traces_col = "d60003_traces";
const std::string hblf_mongo_db_plugin_impl::d70001_col = "d70001";                                      
const std::string hblf_mongo_db_plugin_impl::d70001_traces_col = "d70001_traces";
const std::string hblf_mongo_db_plugin_impl::d70002_col = "d70002";                                      
const std::string hblf_mongo_db_plugin_impl::d70002_traces_col = "d70002_traces";
const std::string hblf_mongo_db_plugin_impl::d70003_col = "d70003";                                      
const std::string hblf_mongo_db_plugin_impl::d70003_traces_col = "d70003_traces";
const std::string hblf_mongo_db_plugin_impl::d70004_col = "d70004";                                      
const std::string hblf_mongo_db_plugin_impl::d70004_traces_col = "d70004_traces";
const std::string hblf_mongo_db_plugin_impl::d00002_col = "d00002";                                     
const std::string hblf_mongo_db_plugin_impl::d00002_traces_col = "d00002_traces";
const std::string hblf_mongo_db_plugin_impl::d00003_col = "d00003";                                     
const std::string hblf_mongo_db_plugin_impl::d00003_traces_col = "d00003_traces";
const std::string hblf_mongo_db_plugin_impl::d00004_col = "d00004";                                     
const std::string hblf_mongo_db_plugin_impl::d00004_traces_col = "d00004_traces";
const std::string hblf_mongo_db_plugin_impl::d41001_col = "d41001";                                     
const std::string hblf_mongo_db_plugin_impl::d41001_traces_col = "d41001_traces";
const std::string hblf_mongo_db_plugin_impl::d41002_col = "d41002";                                     
const std::string hblf_mongo_db_plugin_impl::d41002_traces_col = "d41002_traces";
const std::string hblf_mongo_db_plugin_impl::d41003_col = "d41003";                                     
const std::string hblf_mongo_db_plugin_impl::d41003_traces_col = "d41003_traces";
const std::string hblf_mongo_db_plugin_impl::d11001_col = "d11001";                                     
const std::string hblf_mongo_db_plugin_impl::d11001_traces_col = "d11001_traces";
const std::string hblf_mongo_db_plugin_impl::d11002_col = "d11002";                                     
const std::string hblf_mongo_db_plugin_impl::d11002_traces_col = "d11002_traces";
const std::string hblf_mongo_db_plugin_impl::d11003_col = "d11003";                                     
const std::string hblf_mongo_db_plugin_impl::d11003_traces_col = "d11003_traces";
const std::string hblf_mongo_db_plugin_impl::d11004_col = "d11004";                                     
const std::string hblf_mongo_db_plugin_impl::d11004_traces_col = "d11004_traces";
const std::string hblf_mongo_db_plugin_impl::d11005_col = "d11005";                                     
const std::string hblf_mongo_db_plugin_impl::d11005_traces_col = "d11005_traces";
const std::string hblf_mongo_db_plugin_impl::d11006_col = "d11006";                                     
const std::string hblf_mongo_db_plugin_impl::d11006_traces_col = "d11006_traces";
const std::string hblf_mongo_db_plugin_impl::d11007_col = "d11007";                                     
const std::string hblf_mongo_db_plugin_impl::d11007_traces_col = "d11007_traces";
const std::string hblf_mongo_db_plugin_impl::d11008_col = "d11008";                                     
const std::string hblf_mongo_db_plugin_impl::d11008_traces_col = "d11008_traces";
const std::string hblf_mongo_db_plugin_impl::d11009_col = "d11009";                                     
const std::string hblf_mongo_db_plugin_impl::d11009_traces_col = "d11009_traces";
const std::string hblf_mongo_db_plugin_impl::d11010_col = "d11010";                                     
const std::string hblf_mongo_db_plugin_impl::d11010_traces_col = "d11010_traces";
const std::string hblf_mongo_db_plugin_impl::d11011_col = "d11011";                                     
const std::string hblf_mongo_db_plugin_impl::d11011_traces_col = "d11011_traces";
const std::string hblf_mongo_db_plugin_impl::d11012_col = "d11012";                                     
const std::string hblf_mongo_db_plugin_impl::d11012_traces_col = "d11012_traces";
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
      _d30001= mongo_conn[db_name][d30001_col];
      _d30001_traces = mongo_conn[db_name][d30001_traces_col];
      _d30002= mongo_conn[db_name][d30002_col];
      _d30002_traces = mongo_conn[db_name][d30002_traces_col];
      _d30003= mongo_conn[db_name][d30003_col];
      _d30003_traces = mongo_conn[db_name][d30003_traces_col];
      _d30004= mongo_conn[db_name][d30004_col];
      _d30004_traces = mongo_conn[db_name][d30004_traces_col];
      _d40001= mongo_conn[db_name][d40001_col];
      _d40001_traces = mongo_conn[db_name][d40001_traces_col];
      _d40002= mongo_conn[db_name][d40002_col];
      _d40002_traces = mongo_conn[db_name][d40002_traces_col];
      _d40004= mongo_conn[db_name][d40004_col];
      _d40004_traces = mongo_conn[db_name][d40004_traces_col];
      _d40005= mongo_conn[db_name][d40005_col];
      _d40005_traces = mongo_conn[db_name][d40005_traces_col];
      _d40006= mongo_conn[db_name][d40006_col];
      _d40006_traces = mongo_conn[db_name][d40006_traces_col];
      _d80002 = mongo_conn[db_name][d80002_col];
      _d80002_traces = mongo_conn[db_name][d80002_traces_col];
      _d90001 = mongo_conn[db_name][d90001_col];
      _d90001_traces = mongo_conn[db_name][d90001_traces_col];
      _d90002 = mongo_conn[db_name][d90002_col];
      _d90002_traces = mongo_conn[db_name][d90002_traces_col];
      _d90003= mongo_conn[db_name][d90003_col];
      _d90003_traces = mongo_conn[db_name][d90003_traces_col];
      _d90004 = mongo_conn[db_name][d90004_col];
      _d90004_traces = mongo_conn[db_name][d90004_traces_col];
      _d90005 = mongo_conn[db_name][d90005_col];
      _d90005_traces = mongo_conn[db_name][d90005_traces_col];
      _d60001 = mongo_conn[db_name][d60001_col];
      _d60001_traces = mongo_conn[db_name][d60001_traces_col];
      _d60002 = mongo_conn[db_name][d60002_col];
      _d60002_traces = mongo_conn[db_name][d60002_traces_col];
      _d60003 = mongo_conn[db_name][d60003_col];
      _d60003_traces = mongo_conn[db_name][d60003_traces_col];
      _d70001 = mongo_conn[db_name][d70001_col];
      _d70001_traces = mongo_conn[db_name][d70001_traces_col];
      _d70002 = mongo_conn[db_name][d70002_col];
      _d70002_traces = mongo_conn[db_name][d70002_traces_col];
      _d70003 = mongo_conn[db_name][d70003_col];
      _d70003_traces = mongo_conn[db_name][d70003_traces_col];
      _d70004 = mongo_conn[db_name][d70004_col];
      _d70004_traces = mongo_conn[db_name][d70004_traces_col];
      _d00002 = mongo_conn[db_name][d00002_col];
      _d00002_traces = mongo_conn[db_name][d00002_traces_col];
      _d00003 = mongo_conn[db_name][d00003_col];
      _d00003_traces = mongo_conn[db_name][d00003_traces_col];
      _d00004 = mongo_conn[db_name][d00004_col];
      _d00004_traces = mongo_conn[db_name][d00004_traces_col];
      _d41001 = mongo_conn[db_name][d41001_col];
      _d41001_traces = mongo_conn[db_name][d41001_traces_col];
      _d41002 = mongo_conn[db_name][d41002_col];
      _d41002_traces = mongo_conn[db_name][d41002_traces_col];
      _d41003 = mongo_conn[db_name][d41003_col];
      _d41003_traces = mongo_conn[db_name][d41003_traces_col];

      _d41003 = mongo_conn[db_name][d41003_col];
      _d41003_traces = mongo_conn[db_name][d41003_traces_col];

      _d11001 = mongo_conn[db_name][d11001_col];
      _d11001_traces = mongo_conn[db_name][d11001_traces_col];
      _d11002 = mongo_conn[db_name][d11002_col];
      _d11002_traces = mongo_conn[db_name][d11002_traces_col];
      _d11003 = mongo_conn[db_name][d11003_col];
      _d11003_traces = mongo_conn[db_name][d11003_traces_col];
      _d11004 = mongo_conn[db_name][d11004_col];
      _d11004_traces = mongo_conn[db_name][d11004_traces_col];
      _d11005 = mongo_conn[db_name][d11005_col];
      _d11005_traces = mongo_conn[db_name][d11005_traces_col];
      _d11006 = mongo_conn[db_name][d11006_col];
      _d11006_traces = mongo_conn[db_name][d11006_traces_col];
      _d11007 = mongo_conn[db_name][d11007_col];
      _d11007_traces = mongo_conn[db_name][d11007_traces_col];
      _d11008 = mongo_conn[db_name][d11008_col];
      _d11008_traces = mongo_conn[db_name][d11008_traces_col];
      _d11009 = mongo_conn[db_name][d11009_col];
      _d11009_traces = mongo_conn[db_name][d11009_traces_col];
      _d11010 = mongo_conn[db_name][d11010_col];
      _d11010_traces = mongo_conn[db_name][d11010_traces_col];
      _d11011 = mongo_conn[db_name][d11011_col];
      _d11011_traces = mongo_conn[db_name][d11011_traces_col];
      _d11012 = mongo_conn[db_name][d11012_col];
      _d11012_traces = mongo_conn[db_name][d11012_traces_col];
      







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

void hblf_mongo_db_plugin_impl::process_applied_transaction( 
   const chain::transaction_trace_ptr& t ) {
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
               update_base_col( atrace.act, actdoc,t);
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
   mongocxx::bulk_write bulk_action_d30001_traces = _d30001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d30002_traces = _d30002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d30003_traces = _d30003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d30004_traces = _d30004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d40001_traces = _d40001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d40002_traces = _d40002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d40004_traces = _d40004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d40005_traces = _d40005_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d40006_traces = _d40006_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d80002_traces = _d80002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d90001_traces = _d90001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d90002_traces = _d90002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d90003_traces = _d90003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d90004_traces = _d90004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d90005_traces = _d90005_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d60001_traces = _d60001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d60002_traces = _d60002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d60003_traces = _d60003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d70001_traces = _d70001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d70002_traces = _d70002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d70003_traces = _d70003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d70004_traces = _d70004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00002_traces = _d00002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00003_traces = _d00003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d00004_traces = _d00004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d41001_traces = _d41001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d41002_traces = _d41002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d41003_traces = _d41003_traces.create_bulk_write(bulk_opts);

   mongocxx::bulk_write bulk_action_d11001_traces = _d11001_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11002_traces = _d11002_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11003_traces = _d11003_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11004_traces = _d11004_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11005_traces = _d11005_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11006_traces = _d11006_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11007_traces = _d11007_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11008_traces = _d11008_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11009_traces = _d11009_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11010_traces = _d11010_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11011_traces = _d11011_traces.create_bulk_write(bulk_opts);
   mongocxx::bulk_write bulk_action_d11012_traces = _d11012_traces.create_bulk_write(bulk_opts);

   




   
   
   

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
   bool write_d30001_atraces = false;
   bool write_d30002_atraces = false;
   bool write_d30003_atraces = false;
   bool write_d30004_atraces = false;
   bool write_d40001_atraces = false;
   bool write_d40002_atraces = false;
   bool write_d40004_atraces = false;
   bool write_d40005_atraces = false;
   bool write_d40006_atraces = false;
   bool write_d80002_atraces = false;
   bool write_d90001_atraces = false;
   bool write_d90002_atraces = false;
   bool write_d90003_atraces = false;
   bool write_d90004_atraces = false;
   bool write_d90005_atraces = false;
   bool write_d60001_atraces = false;
   bool write_d60002_atraces = false;
   bool write_d60003_atraces = false;
   bool write_d70001_atraces = false;
   bool write_d70002_atraces = false;
   bool write_d70003_atraces = false;
   bool write_d70004_atraces = false;
   bool write_d00002_atraces = false;
   bool write_d00003_atraces = false;
   bool write_d00004_atraces = false;
   bool write_d41001_atraces = false;
   bool write_d41002_atraces = false;
   bool write_d41003_atraces = false;
   bool write_d11001_atraces = false;
   bool write_d11002_atraces = false;
   bool write_d11003_atraces = false;
   bool write_d11004_atraces = false;
   bool write_d11005_atraces = false;
   bool write_d11006_atraces = false;
   bool write_d11007_atraces = false;
   bool write_d11008_atraces = false;
   bool write_d11009_atraces = false;
   bool write_d11010_atraces = false;
   bool write_d11011_atraces = false;
   bool write_d11012_atraces = false;


   

   bool write_ttrace = false; // filters apply to transaction_traces as well
   bool executed = t->receipt.valid() && t->receipt->status == chain::transaction_receipt_header::executed;

   for( const auto& atrace : t->action_traces ) {
      try {
         
        if(atrace.act.name == addd20001 || atrace.act.name == modd20001 || atrace.act.name == deld20001) {
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
         }else if(atrace.act.name == addd30001 || atrace.act.name == modd30001 || atrace.act.name == deld30001){
               write_d30001_atraces |= add_action_trace(bulk_action_d30001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd30002 || atrace.act.name == modd30002 || atrace.act.name == deld30002){
                write_d30002_atraces |= add_action_trace(bulk_action_d30002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd30003 || atrace.act.name == modd30003 || atrace.act.name == deld30003){
                write_d30003_atraces |= add_action_trace(bulk_action_d30003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd30004 || atrace.act.name == modd30004 || atrace.act.name == deld30004){
                write_d30004_atraces |= add_action_trace(bulk_action_d30004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd40001 || atrace.act.name == modd40001 || atrace.act.name == deld40001){
               write_d40001_atraces |= add_action_trace(bulk_action_d40001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd40002 || atrace.act.name == modd40002 || atrace.act.name == deld40002){
               write_d40002_atraces |= add_action_trace(bulk_action_d40002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd40004 || atrace.act.name == modd40004 || atrace.act.name == deld40004){
               write_d40004_atraces |= add_action_trace(bulk_action_d40004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd40005 || atrace.act.name == modd40005 || atrace.act.name == deld40005){
               write_d40005_atraces |= add_action_trace(bulk_action_d40005_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd40006 || atrace.act.name == modd40006 || atrace.act.name == deld40006){
               write_d40006_atraces |= add_action_trace(bulk_action_d40006_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd80002 || atrace.act.name == modd80002 || atrace.act.name == deld80002){
             write_d80002_atraces |= add_action_trace(bulk_action_d80002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd90001 || atrace.act.name == modd90001 || atrace.act.name == deld90001){
             write_d90001_atraces |= add_action_trace(bulk_action_d90001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd90002 || atrace.act.name == modd90002 || atrace.act.name == deld90002){
             write_d90002_atraces |= add_action_trace(bulk_action_d90002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd90003 || atrace.act.name == modd90003 || atrace.act.name == deld90003){
             write_d90003_atraces |= add_action_trace(bulk_action_d90003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd90004 || atrace.act.name == modd90004 || atrace.act.name == deld90004){
             write_d90004_atraces |= add_action_trace(bulk_action_d90004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd90005 || atrace.act.name == modd90005 || atrace.act.name == deld90005){
             write_d90005_atraces |= add_action_trace(bulk_action_d90005_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd60001 || atrace.act.name == modd60001 || atrace.act.name == deld60001){
             write_d60001_atraces |= add_action_trace(bulk_action_d60001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd60002 || atrace.act.name == modd60002 || atrace.act.name == deld60002){
             write_d60002_atraces |= add_action_trace(bulk_action_d60002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd60003 || atrace.act.name == modd60003 || atrace.act.name == deld60003){
             write_d60003_atraces |= add_action_trace(bulk_action_d60003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd70001 || atrace.act.name == modd70001 || atrace.act.name == deld70001){
             write_d70001_atraces |= add_action_trace(bulk_action_d70001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd70002 || atrace.act.name == modd70002 || atrace.act.name == deld70002){
             write_d70002_atraces |= add_action_trace(bulk_action_d70002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd70003 || atrace.act.name == modd70003 || atrace.act.name == deld70003){
             write_d70003_atraces |= add_action_trace(bulk_action_d70003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd70004 || atrace.act.name == modd70004 || atrace.act.name == deld70004){
             write_d70004_atraces |= add_action_trace(bulk_action_d70004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00002 || atrace.act.name == modd00002 || atrace.act.name == deld00002){
               write_d00002_atraces |= add_action_trace(bulk_action_d00002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00003 || atrace.act.name == modd00003 || atrace.act.name == deld00003){
               write_d00003_atraces |= add_action_trace(bulk_action_d00003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd00004 || atrace.act.name == modd00004 || atrace.act.name == deld00004){
               write_d00004_atraces |= add_action_trace(bulk_action_d00004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd41001 || atrace.act.name == modd41001 || atrace.act.name == deld41001){
               write_d41001_atraces |= add_action_trace(bulk_action_d41001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd41002 || atrace.act.name == modd41002 || atrace.act.name == deld41002){
               write_d41002_atraces |= add_action_trace(bulk_action_d41002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd41003 || atrace.act.name == modd41003 || atrace.act.name == deld41003){
               write_d41003_atraces |= add_action_trace(bulk_action_d41003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11001 || atrace.act.name == modd11001 || atrace.act.name == deld11001){
               write_d11001_atraces |= add_action_trace(bulk_action_d11001_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11002 || atrace.act.name == modd11002 || atrace.act.name == deld11002){
               write_d11002_atraces |= add_action_trace(bulk_action_d11002_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11003 || atrace.act.name == modd11003 || atrace.act.name == deld11003){
               write_d11003_atraces |= add_action_trace(bulk_action_d11003_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11004 || atrace.act.name == modd11004 || atrace.act.name == deld11004){
               write_d11004_atraces |= add_action_trace(bulk_action_d11004_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11005 || atrace.act.name == modd11005 || atrace.act.name == deld11005){
               write_d11005_atraces |= add_action_trace(bulk_action_d11005_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11006 || atrace.act.name == modd11006 || atrace.act.name == deld11006){
               write_d11006_atraces |= add_action_trace(bulk_action_d11006_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11007 || atrace.act.name == modd11007 || atrace.act.name == deld11007){
               write_d11007_atraces |= add_action_trace(bulk_action_d11007_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11008 || atrace.act.name == modd11008 || atrace.act.name == deld11008){
               write_d11008_atraces |= add_action_trace(bulk_action_d11008_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11009 || atrace.act.name == modd11009 || atrace.act.name == deld11009){
               write_d11009_atraces |= add_action_trace(bulk_action_d11009_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11010 || atrace.act.name == modd11010 || atrace.act.name == deld11010){
               write_d11010_atraces |= add_action_trace(bulk_action_d11010_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11011 || atrace.act.name == modd11011 || atrace.act.name == deld11011){
               write_d11011_atraces |= add_action_trace(bulk_action_d11011_traces,atrace,t,executed,now,write_ttrace);
         }else if(atrace.act.name == addd11012 || atrace.act.name == modd11012 || atrace.act.name == deld11012){
               write_d11012_atraces |= add_action_trace(bulk_action_d11012_traces,atrace,t,executed,now,write_ttrace);
         }
         
   
      } catch(...) {
         handle_mongo_exception("add action traces", __LINE__);
      }
   }

   if( !start_block_reached ) return; //< add_action_trace calls update_base_col which must be called always



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

    if( write_d80002_atraces ) {
      try {
         if( !bulk_action_d80002_traces.execute() ) {
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

    if( write_d30001_atraces ) {
      try {
         if( !bulk_action_d30001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d30002_atraces ) {
      try {
         if( !bulk_action_d30002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d30003_atraces ) {
      try {
         if( !bulk_action_d30003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d30004_atraces ) {
      try {
         if( !bulk_action_d30004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d40001_atraces ) {
      try {
         if( !bulk_action_d40001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d40002_atraces ) {
      try {
         if( !bulk_action_d40002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d40004_atraces ) {
      try {
         if( !bulk_action_d40004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d40005_atraces ) {
      try {
         if( !bulk_action_d40005_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d40006_atraces ) {
      try {
         if( !bulk_action_d40006_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d90001_atraces ) {
      try {
         if( !bulk_action_d90001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d90002_atraces ) {
      try {
         if( !bulk_action_d90002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d90003_atraces ) {
      try {
         if( !bulk_action_d90003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d90004_atraces ) {
      try {
         if( !bulk_action_d90004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d90005_atraces ) {
      try {
         if( !bulk_action_d90005_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d60001_atraces ) {
      try {
         if( !bulk_action_d60001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d60002_atraces ) {
      try {
         if( !bulk_action_d60002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d60003_atraces ) {
      try {
         if( !bulk_action_d60003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d70001_atraces ) {
      try {
         if( !bulk_action_d70001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d70002_atraces ) {
      try {
         if( !bulk_action_d70002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d70003_atraces ) {
      try {
         if( !bulk_action_d70003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d70004_atraces ) {
      try {
         if( !bulk_action_d70004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d00002_atraces ) {
      try {
         if( !bulk_action_d00002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d00003_atraces ) {
      try {
         if( !bulk_action_d00003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

    if( write_d00004_atraces ) {
      try {
         if( !bulk_action_d00004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d41001_atraces ) {
      try {
         if( !bulk_action_d41001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

if( write_d41002_atraces ) {
      try {
         if( !bulk_action_d41002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

   if( write_d41003_atraces ) {
      try {
         if( !bulk_action_d41003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }

if( write_d11001_atraces ) {
      try {
         if( !bulk_action_d11001_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11002_atraces ) {
      try {
         if( !bulk_action_d11002_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11003_atraces ) {
      try {
         if( !bulk_action_d11003_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11004_atraces ) {
      try {
         if( !bulk_action_d11004_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11005_atraces ) {
      try {
         if( !bulk_action_d11005_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11006_atraces ) {
      try {
         if( !bulk_action_d11006_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11007_atraces ) {
      try {
         if( !bulk_action_d11007_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11008_atraces ) {
      try {
         if( !bulk_action_d11008_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11009_atraces ) {
      try {
         if( !bulk_action_d11009_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11010_atraces ) {
      try {
         if( !bulk_action_d11010_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11011_atraces ) {
      try {
         if( !bulk_action_d11011_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
   if( write_d11012_atraces ) {
      try {
         if( !bulk_action_d11012_traces.execute() ) {
            EOS_ASSERT( false, chain::mongo_db_insert_fail,
                        "Bulk action traces insert failed for transaction trace: ${id}", ("id", t->id) );
         }
      } catch( ... ) {
         handle_mongo_exception( "action traces insert", __LINE__ );
      }
   }
}











namespace {

//创建体质健康信息
void create_d20001(mongocxx::collection& d20001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time)
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
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d20001(mongocxx::collection& d20001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element tzjkid_ele = data["tzjkid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "tzjkid", tzjkid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d20002( mongocxx::collection& d20002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xjh_ele = data["xjh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xjh", xjh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d20002(mongocxx::collection& d20002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xjh_ele = data["xjh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xjh", xjh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d80001( mongocxx::collection& d80001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d80001(mongocxx::collection& d80001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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

//创建教师荣誉信息
void create_d80002( mongocxx::collection& d80002, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d80002 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d80002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d80002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d80002", __LINE__ );
   }
}
//更新教师荣誉信息
void update_d80002(mongocxx::collection& d80002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d80002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d80002.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d80002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d80002", __LINE__ );
   }
}
//删除教师荣誉信息
void delete_d80002( mongocxx::collection& d80002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d80002 jsid " << jsid_ele.get_utf8().value << std::endl;
      if( !d80002.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d80002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d80002", __LINE__ );
   }
}

//创建教师职称评定
void create_d50001( mongocxx::collection& d50001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50001(mongocxx::collection& d50001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d50002( mongocxx::collection& d50002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50002(mongocxx::collection& d50002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d50003( mongocxx::collection& d50003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50003(mongocxx::collection& d50003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d50004( mongocxx::collection& d50004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50004(mongocxx::collection& d50004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d50005( mongocxx::collection& d50005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50005(mongocxx::collection& d50005,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time}))));
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
void create_d50006( mongocxx::collection& d50006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50006(mongocxx::collection& d50006,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d50007( mongocxx::collection& d50007, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50007(mongocxx::collection& d50007,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d50008( mongocxx::collection& d50008, const bsoncxx::document::view& data, std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d50008(mongocxx::collection& d50008,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d00001( mongocxx::collection& d00001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "orgId", orgId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d00001(mongocxx::collection& d00001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "orgId", orgId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d00007( mongocxx::collection& d00007, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "orgId", orgId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d00007(mongocxx::collection& d00007,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element orgId_ele = data["orgId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "orgId", orgId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void delete_d00007( mongocxx::collection& d00007, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ) {
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
void create_d00005( mongocxx::collection& d00005, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "userId", userId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d00005(mongocxx::collection& d00005,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "userId", userId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d00006( mongocxx::collection& d00006, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "userId", userId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
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
void update_d00006(mongocxx::collection& d00006,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element userId_ele = data["userId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "userId", userId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
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
void create_d40003( mongocxx::collection& d40003, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "ksid", ksid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d40003 ksid" <<ksid_ele.get_utf8().value << std::endl;
      if( !d40003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d40003", __LINE__ );
   }
}

//更新考试成绩信息
void update_d40003(mongocxx::collection& d40003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "ksid", ksid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d4000 ksid" << ksid_ele.get_utf8().value << std::endl;
      if( !d40003.update_one( make_document( kvp("ksid", ksid_ele.get_value())), update.view(), update_opts )) {
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

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   try {
      std::cout << "delete_d40003 ksid " <<ksid_ele.get_utf8().value << std::endl;
      if( !d40003.delete_one( make_document( kvp("ksid", ksid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40003", __LINE__ );
   }
}

//创建师德培训信息表
void create_d30001( mongocxx::collection& d30001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d30001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d30001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d30001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d30001", __LINE__ );
   }
}

//更新师德培训信息
void update_d30001(mongocxx::collection& d30001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d30001 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d30001.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d30001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d30001", __LINE__ );
   }
}

//删除师德培训信息表
void delete_d30001( mongocxx::collection& d30001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d30001 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d30001.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d30001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d30001", __LINE__ );
   }
}


//创建师德考核信息表
void create_d30002( mongocxx::collection& d30002, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d30002 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d30002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d30002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d30002", __LINE__ );
   }
}

//更新师德考核信息
void update_d30002(mongocxx::collection& d30002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d30002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d30002.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d30002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d30002", __LINE__ );
   }
}

//删除师德考核信息表
void delete_d30002( mongocxx::collection& d30002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d30002 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d30002.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d30002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d30002", __LINE__ );
   }
}

//创建师德奖励信息表
void create_d30003( mongocxx::collection& d30003, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d30003 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d30003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d30003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d30003", __LINE__ );
   }
}

//更新师德奖励信息
void update_d30003(mongocxx::collection& d30003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d30002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d30003.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d30003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d30003", __LINE__ );
   }
}

//删除师德奖励信息表
void delete_d30003( mongocxx::collection& d30003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d30003 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d30003.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d30003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d30003", __LINE__ );
   }
}


//创建师德惩处信息表
void create_d30004( mongocxx::collection& d30004, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d30004 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d30004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d30004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d30004", __LINE__ );
   }
}

//更新师德惩处信息
void update_d30004(mongocxx::collection& d30004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d30004 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d30004.update_one( make_document( kvp("jsid", jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d30003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d30004", __LINE__ );
   }
}

//删除师德惩处信息表
void delete_d30004( mongocxx::collection& d30004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d30004 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d30004.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d30004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d30004", __LINE__ );
   }
}

//创建学生考试信息表
void create_d40001( mongocxx::collection& d40001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "ksid", ksid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d40001 ksid" <<ksid_ele.get_utf8().value << std::endl;
      if( !d40001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d40001", __LINE__ );
   }
}

//更新考试信息
void update_d40001(mongocxx::collection& d40001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "ksid", ksid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d40001 ksid" << ksid_ele.get_utf8().value << std::endl;
      if( !d40001.update_one( make_document( kvp("ksid", ksid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d40001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d40001", __LINE__ );
   }
}

//删除考试信息表
void delete_d40001( mongocxx::collection& d40001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   try {
      std::cout << "delete_d40001 ksid " <<ksid_ele.get_utf8().value << std::endl;
      if( !d40001.delete_one( make_document( kvp("ksid", ksid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40001", __LINE__ );
   }
}

//创建考试班级科目表
void create_d40002( mongocxx::collection& d40002, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "ksid", ksid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d40002 ksid" <<ksid_ele.get_utf8().value << std::endl;
      if( !d40002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d40002", __LINE__ );
   }
}

//更新考试班级科目表
void update_d40002(mongocxx::collection& d40002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "ksid", ksid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d40002 ksid" << ksid_ele.get_utf8().value << std::endl;
      if( !d40002.update_one( make_document( kvp("ksid", ksid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d40002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d40002", __LINE__ );
   }
}

//删除考试班级科目表
void delete_d40002( mongocxx::collection& d40002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   try {
      std::cout << "delete_d40002 ksid " <<ksid_ele.get_utf8().value << std::endl;
      if( !d40002.delete_one( make_document( kvp("ksid", ksid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40002", __LINE__ );
   }
}


//创建考试试卷信息表
void create_d40004( mongocxx::collection& d40004, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element sjid_ele = data["sjid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "sjid", sjid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d40004 sjid" <<sjid_ele.get_utf8().value << std::endl;
      if( !d40004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d40004", __LINE__ );
   }
}

//更新考试试卷信息表
void update_d40004(mongocxx::collection& d40004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element sjid_ele = data["sjid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "sjid", sjid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d40004 sjid" << sjid_ele.get_utf8().value << std::endl;
      if( !d40004.update_one( make_document( kvp("sjid", sjid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d40004");
      }
   } catch (...) {
      handle_mongo_exception( "update_d40004", __LINE__ );
   }
}

//删除考试试卷信息表
void delete_d40004( mongocxx::collection& d40004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element sjid_ele = data["sjid"]; 
   try {
      std::cout << "delete_d40004 sjid " <<sjid_ele.get_utf8().value << std::endl;
      if( !d40004.delete_one( make_document( kvp("sjid", sjid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40004", __LINE__ );
   }
}


//创建学生考试试题得分表
void create_d40005( mongocxx::collection& d40005, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element zsdid_ele = data["zsdid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "zsdid", zsdid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d40005 zsdid" <<zsdid_ele.get_utf8().value << std::endl;
      if( !d40005.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40005");
      }
   } catch (...) {
      handle_mongo_exception( "create_d 40005", __LINE__ );
   }
}

//更新学生考试试题得分表
void update_d40005(mongocxx::collection& d40005,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element zsdid_ele = data["zsdid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "zsdid", zsdid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d40005 zsdid" << zsdid_ele.get_utf8().value << std::endl;
      if( !d40005.update_one( make_document( kvp("zsdid", zsdid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d40005");
      }
   } catch (...) {
      handle_mongo_exception( "update_d40005", __LINE__ );
   }
}

//删除学生考试试题得分表
void delete_d40005( mongocxx::collection& d40005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element zsdid_ele = data["zsdid"]; 
   try {
      std::cout << "delete_d40005 zsdid " <<zsdid_ele.get_utf8().value << std::endl;
      if( !d40005.delete_one( make_document( kvp("zsdid", zsdid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40005");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40005", __LINE__ );
   }
}


//创建试题知识点表
void create_d40006( mongocxx::collection& d40006, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "ksid", ksid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d40006 ksid" <<ksid_ele.get_utf8().value << std::endl;
      if( !d40006.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d40006");
      }
   } catch (...) {
      handle_mongo_exception( "create_d40006", __LINE__ );
   }
}

//更新试题知识点表
void update_d40006(mongocxx::collection& d40006,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element ksid_ele = data["ksid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "ksid", ksid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d40006 ksid" << ksid_ele.get_utf8().value << std::endl;
      if( !d40006.update_one( make_document( kvp("ksid",ksid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d40006");
      }
   } catch (...) {
      handle_mongo_exception( "update_d40006", __LINE__ );
   }
}

//删除试题知识点表
void delete_d40006( mongocxx::collection& d40006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element ksid_ele = data["ksid"]; 
   try {
      std::cout << "delete_d40006 ksid " <<ksid_ele.get_utf8().value << std::endl;
      if( !d40006.delete_one( make_document( kvp("ksid", ksid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d40006");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d40006", __LINE__ );
   }
}





//创建表
void create_d90001( mongocxx::collection& d90001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d90001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d90001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d90001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d90001", __LINE__ );
   }
}

//更新表
void update_d90001(mongocxx::collection& d90001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d90001 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d90001.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d90001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d90001", __LINE__ );
   }
}

//删除表
void delete_d90001( mongocxx::collection& d90001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d90001 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d90001.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d90001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d90001", __LINE__ );
   }
}


//创建表
void create_d90002( mongocxx::collection& d90002, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d90002 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d90002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d90002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d90002", __LINE__ );
   }
}

//更新表
void update_d90002(mongocxx::collection& d90002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d90002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d90002.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d90002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d90002", __LINE__ );
   }
}

//删除表
void delete_d90002( mongocxx::collection& d90002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d90002 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d90002.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d90002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d90002", __LINE__ );
   }
}


//创建表
void create_d90003( mongocxx::collection& d90003, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d90003 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d90003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d90003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d90003", __LINE__ );
   }
}

//更新表
void update_d90003(mongocxx::collection& d90003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d9000 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d90003.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d9000");
      }
   } catch (...) {
      handle_mongo_exception( "update_d9000", __LINE__ );
   }
}

//删除表
void delete_d90003( mongocxx::collection& d90003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d90003 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d90003.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d90003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d90003", __LINE__ );
   }
}


//创建表
void create_d90004( mongocxx::collection& d90004, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d90004 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d90004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d90004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d90004", __LINE__ );
   }
}

//更新表
void update_d90004(mongocxx::collection& d90004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d90004 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d90004.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d9000");
      }
   } catch (...) {
      handle_mongo_exception( "update_d90004", __LINE__ );
   }
}

//删除表
void delete_d90004( mongocxx::collection& d90004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d90004 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d90004.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d90004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d90004", __LINE__ );
   }
}

//创建表
void create_d90005( mongocxx::collection& d90005, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d90005 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d90005.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d90005");
      }
   } catch (...) {
      handle_mongo_exception( "create_d9000", __LINE__ );
   }
}

//更新表
void update_d90005(mongocxx::collection& d90005,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d90005 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d90005.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d9000");
      }
   } catch (...) {
      handle_mongo_exception( "update_d9000", __LINE__ );
   }
}

//删除表
void delete_d90005( mongocxx::collection& d90005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d90005 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d90005.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d90005");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d90005", __LINE__ );
   }
}





//创建表
void create_d60001( mongocxx::collection& d60001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d60001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d60001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d60001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d60001", __LINE__ );
   }
}

//更新表
void update_d60001(mongocxx::collection& d60001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d60001 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d60001.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d60001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d60001", __LINE__ );
   }
}

//删除表
void delete_d60001( mongocxx::collection& d60001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d60001 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d60001.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d60001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d60001", __LINE__ );
   }
}

//创建表
void create_d60002( mongocxx::collection& d60002, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d60002 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d60002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d60002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d60002", __LINE__ );
   }
}

//更新表
void update_d60002(mongocxx::collection& d60002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d60002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d60002.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d60002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d60002", __LINE__ );
   }
}

//删除表
void delete_d60002( mongocxx::collection& d60002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d60002 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d60002.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d60002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d60002", __LINE__ );
   }
}

//创建表
void create_d60003( mongocxx::collection& d60003, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d60003 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d60003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d60003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d60003", __LINE__ );
   }
}

//更新表
void update_d60003(mongocxx::collection& d60003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d60003 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d60003.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d60003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d60003", __LINE__ );
   }
}

//删除表
void delete_d60003( mongocxx::collection& d60003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d60003 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d60003.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d60003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d60003", __LINE__ );
   }
}







//创建表
void create_d70001( mongocxx::collection& d70001, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d70001 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d70001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d70001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d70001", __LINE__ );
   }
}

//更新表
void update_d70001(mongocxx::collection& d70001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d70001 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d70001.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d70001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d70001", __LINE__ );
   }
}

//删除表
void delete_d70001( mongocxx::collection& d70001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d70001 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d70001.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d70001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d70001", __LINE__ );
   }
}



//创建表
void create_d70002( mongocxx::collection& d70002, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d70002 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d70002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d70002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d70002", __LINE__ );
   }
}

//更新表
void update_d70002(mongocxx::collection& d70002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d70002 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d70002.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d70002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d70002", __LINE__ );
   }
}

//删除表
void delete_d70002( mongocxx::collection& d70002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d70002 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d70002.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d70002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d70002", __LINE__ );
   }
}


//创建表
void create_d70003( mongocxx::collection& d70003, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d70003 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d70003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d70004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d70003", __LINE__ );
   }
}

//更新表
void update_d70003(mongocxx::collection& d70003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d70003 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d70003.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d70003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d70003", __LINE__ );
   }
}

//删除表
void delete_d70003( mongocxx::collection& d70003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d70003 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d70003.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d70003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d70003", __LINE__ );
   }
}



//创建表
void create_d70004( mongocxx::collection& d70004, const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "jsid", jsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d70004 jsid" <<jsid_ele.get_utf8().value << std::endl;
      if( !d70004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d70004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d70004", __LINE__ );
   }
}

//更新表
void update_d70004(mongocxx::collection& d70004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element jsid_ele = data["jsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "jsid", jsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d70004 jsid" << jsid_ele.get_utf8().value << std::endl;
      if( !d70004.update_one( make_document( kvp("jsid",jsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d70004");
      }
   } catch (...) {
      handle_mongo_exception( "update_d70004", __LINE__ );
   }
}

//删除表
void delete_d70004( mongocxx::collection& d70004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element jsid_ele = data["jsid"]; 
   try {
      std::cout << "delete_d70004 jsid " <<jsid_ele.get_utf8().value << std::endl;
      if( !d70004.delete_one( make_document( kvp("jsid", jsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d70004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d70004", __LINE__ );
   }
}


//创建表
void create_d00002( mongocxx::collection& d00002,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element classId_ele = data["classId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "classId", classId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d00002 classId" <<classId_ele.get_utf8().value << std::endl;
      if( !d00002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00002", __LINE__ );
   }
}

//更新表
void update_d00002(mongocxx::collection& d00002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element classId_ele = data["classId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "classId", classId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d00002 classId" <<classId_ele.get_utf8().value << std::endl;
      if( !d00002.update_one( make_document( kvp("classId", classId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00002", __LINE__ );
   }
}

//删除表
void delete_d00002( mongocxx::collection& d00002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element classId_ele = data["classId"]; 
   try {
      std::cout << "delete_d00002 classId" << classId_ele.get_utf8().value << std::endl;
      if( !d00002.delete_one( make_document( kvp("classId", classId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00002", __LINE__ );
   }
}



//创建表
void create_d00003( mongocxx::collection& d00003,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element classId_ele = data["classId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "classId", classId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d00003 classId" <<classId_ele.get_utf8().value << std::endl;
      if( !d00003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00003", __LINE__ );
   }
}

//更新表
void update_d00003(mongocxx::collection& d00003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element classId_ele = data["classId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "classId", classId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d00003 classId" <<classId_ele.get_utf8().value << std::endl;
      if( !d00003.update_one( make_document( kvp("classId", classId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00003", __LINE__ );
   }
}

//删除表
void delete_d00003( mongocxx::collection& d00003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element classId_ele = data["classId"]; 
   try {
      std::cout << "delete_d00003 classId" << classId_ele.get_utf8().value << std::endl;
      if( !d00003.delete_one( make_document( kvp("classId", classId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00003", __LINE__ );
   }
}


//创建表
void create_d00004( mongocxx::collection& d00004,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element classId_ele = data["classId"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "classId", classId_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d00004 classId" <<classId_ele.get_utf8().value << std::endl;
      if( !d00004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d00004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d00004", __LINE__ );
   }
}

//更新表
void update_d00004(mongocxx::collection& d00004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element classId_ele = data["classId"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "classId", classId_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d00004 classId" <<classId_ele.get_utf8().value << std::endl;
      if( !d00004.update_one( make_document( kvp("classId", classId_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d00004");
      }
   } catch (...) {
      handle_mongo_exception( "update_d00004", __LINE__ );
   }
}

//删除表
void delete_d00004( mongocxx::collection& d00004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element classId_ele = data["classId"]; 
   try {
      std::cout << "delete_d00004 classId" << classId_ele.get_utf8().value << std::endl;
      if( !d00004.delete_one( make_document( kvp("classId", classId_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d00004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d00004", __LINE__ );
   }
}


//创建表
void create_d41001( mongocxx::collection& d41001,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xsid_ele = data["xsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xsid", xsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d41001 xsid" <<xsid_ele.get_utf8().value << std::endl;
      if( !d41001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d41001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d41001", __LINE__ );
   }
}

//更新表
void update_d41001(mongocxx::collection& d41001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xsid_ele = data["xsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xsid", xsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d41001 xsid" <<xsid_ele.get_utf8().value << std::endl;
      if( !d41001.update_one( make_document( kvp("xsid",xsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d41001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d41001", __LINE__ );
   }
}

//删除表
void delete_d41001( mongocxx::collection& d41001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xsid_ele = data["xsid"]; 
   try {
      std::cout << "delete_d41001 xsid" << xsid_ele.get_utf8().value << std::endl;
      if( !d41001.delete_one( make_document( kvp("xsid", xsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d41001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d41001", __LINE__ );
   }
}



//创建表
void create_d41002( mongocxx::collection& d41002,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xsid_ele = data["xsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xsid", xsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d41002 xsid" <<xsid_ele.get_utf8().value << std::endl;
      if( !d41002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d41002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d41002", __LINE__ );
   }
}

//更新表
void update_d41002(mongocxx::collection& d41002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xsid_ele = data["xsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xsid", xsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d41002 xsid" <<xsid_ele.get_utf8().value << std::endl;
      if( !d41002.update_one( make_document( kvp("xsid",xsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d41002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d41002", __LINE__ );
   }
}

//删除表
void delete_d41002( mongocxx::collection& d41002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xsid_ele = data["xsid"]; 
   try {
      std::cout << "delete_d41002 xsid" << xsid_ele.get_utf8().value << std::endl;
      if( !d41002.delete_one( make_document( kvp("xsid", xsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d41002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d41002", __LINE__ );
   }
}



//创建表
void create_d41003( mongocxx::collection& d41003,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xsid_ele = data["xsid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xsid", xsid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d41003 xsid" <<xsid_ele.get_utf8().value << std::endl;
      if( !d41003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d41003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d41003", __LINE__ );
   }
}

//更新表
void update_d41003(mongocxx::collection& d41003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xsid_ele = data["xsid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xsid", xsid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d41003 xsid" <<xsid_ele.get_utf8().value << std::endl;
      if( !d41003.update_one( make_document( kvp("xsid",xsid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d41003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d41003", __LINE__ );
   }
}

//删除表
void delete_d41003( mongocxx::collection& d41003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xsid_ele = data["xsid"]; 
   try {
      std::cout << "delete_d41003 xsid" << xsid_ele.get_utf8().value << std::endl;
      if( !d41003.delete_one( make_document( kvp("xsid", xsid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d41003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d41003", __LINE__ );
   }
}


//创建表
void create_d11001( mongocxx::collection& d11001,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element yhid_ele = data["yhid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "yhid", yhid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11001 yhid" <<yhid_ele.get_utf8().value << std::endl;
      if( !d11001.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11001");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11001", __LINE__ );
   }
}

//更新表
void update_d11001(mongocxx::collection& d11001,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element yhid_ele = data["yhid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "yhid", yhid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11001 yhid" <<yhid_ele.get_utf8().value << std::endl;
      if( !d11001.update_one( make_document( kvp("yhid",yhid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11001");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11001", __LINE__ );
   }
}

//删除表
void delete_d11001( mongocxx::collection& d11001, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element yhid_ele = data["yhid"]; 
   try {
      std::cout << "delete_d11001 yhid" << yhid_ele.get_utf8().value << std::endl;
      if( !d11001.delete_one( make_document( kvp("yhid", yhid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11001");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11001", __LINE__ );
   }
}


//创建表
void create_d11002( mongocxx::collection& d11002,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element yhid_ele = data["yhid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "yhid", yhid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11002 yhid" <<yhid_ele.get_utf8().value << std::endl;
      if( !d11002.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11002");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11002", __LINE__ );
   }
}

//更新表
void update_d11002(mongocxx::collection& d11002,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element yhid_ele = data["yhid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "yhid", yhid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11002 yhid" <<yhid_ele.get_utf8().value << std::endl;
      if( !d11002.update_one( make_document( kvp("yhid",yhid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11002");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11002", __LINE__ );
   }
}

//删除表
void delete_d11002( mongocxx::collection& d11002, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element yhid_ele = data["yhid"]; 
   try {
      std::cout << "delete_d11002 yhid" << yhid_ele.get_utf8().value << std::endl;
      if( !d11002.delete_one( make_document( kvp("yhid", yhid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11002");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11002", __LINE__ );
   }
}

//创建表
void create_d11003( mongocxx::collection& d11003,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element orgid_ele = data["orgid"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "orgid", orgid_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11003 orgid" <<orgid_ele.get_utf8().value << std::endl;
      if( !d11003.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11003");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11003", __LINE__ );
   }
}

//更新表
void update_d11003(mongocxx::collection& d11003,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element orgid_ele = data["orgid"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "orgid", orgid_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11003 orgid" <<orgid_ele.get_utf8().value << std::endl;
      if( !d11003.update_one( make_document( kvp("orgid",orgid_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11003");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11003", __LINE__ );
   }
}

//删除表
void delete_d11003( mongocxx::collection& d11003, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element orgid_ele = data["orgid"]; 
   try {
      std::cout << "delete_d11003 orgid" << orgid_ele.get_utf8().value << std::endl;
      if( !d11003.delete_one( make_document( kvp("orgid", orgid_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11003");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11003", __LINE__ );
   }
}

//创建表
void create_d11004( mongocxx::collection& d11004,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element bjmc_ele = data["bjmc"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "bjmc", bjmc_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11004 bjmc" <<bjmc_ele.get_utf8().value << std::endl;
      if( !d11004.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11004");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11004", __LINE__ );
   }
}

//更新表
void update_d11004(mongocxx::collection& d11004,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element bjmc_ele = data["bjmc"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "bjmc",bjmc_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11004 bjmc" <<bjmc_ele.get_utf8().value << std::endl;
      if( !d11004.update_one( make_document( kvp("bjmc",bjmc_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11004");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11004", __LINE__ );
   }
}

//删除表
void delete_d11004( mongocxx::collection& d11004, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element bjmc_ele = data["bjmc"]; 
   try {
      std::cout << "delete_d11004 bjmc" << bjmc_ele.get_utf8().value << std::endl;
      if( !d11004.delete_one( make_document( kvp("bjmc", bjmc_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11004");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11004", __LINE__ );
   }
}

//创建表
void create_d11005( mongocxx::collection& d11005,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xsbh_ele = data["xsbh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xsbh", xsbh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11005 xsbh" <<xsbh_ele.get_utf8().value << std::endl;
      if( !d11005.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11005");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11005", __LINE__ );
   }
}

//更新表
void update_d11005(mongocxx::collection& d11005,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xsbh_ele = data["xsbh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xsbh", xsbh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11005 xsbh" <<xsbh_ele.get_utf8().value << std::endl;
      if( !d11005.update_one( make_document( kvp("xsbh",xsbh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11005");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11005", __LINE__ );
   }
}

//删除表
void delete_d11005( mongocxx::collection& d11005, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xsbh_ele = data["xsbh"]; 
   try {
      std::cout << "delete_d1100 xsbh" << xsbh_ele.get_utf8().value << std::endl;
      if( !d11005.delete_one( make_document( kvp("xsbh", xsbh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11005");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11005", __LINE__ );
   }
}

//创建表
void create_d11006( mongocxx::collection& d11006,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element xsbh_ele = data["xsbh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "xsbh", xsbh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11006 xsbh" <<xsbh_ele.get_utf8().value << std::endl;
      if( !d11006.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11006");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11006", __LINE__ );
   }
}

//更新表
void update_d11006(mongocxx::collection& d11006,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element xsbh_ele = data["xsbh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "xsbh", xsbh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11006 xsbh" <<xsbh_ele.get_utf8().value << std::endl;
      if( !d11006.update_one( make_document( kvp("xsbh",xsbh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11006");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11006", __LINE__ );
   }
}

//删除表
void delete_d11006( mongocxx::collection& d11006, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element xsbh_ele = data["xsbh"]; 
   try {
      std::cout << "delete_d11006 xsbh" << xsbh_ele.get_utf8().value << std::endl;
      if( !d11006.delete_one( make_document( kvp("xsbh", xsbh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11006");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11006", __LINE__ );
   }
}

//创建表
void create_d11007( mongocxx::collection& d11007,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "qybh", qybh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11007 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11007.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11007");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11007", __LINE__ );
   }
}

//更新表
void update_d11007(mongocxx::collection& d11007,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "qybh", qybh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11007 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11007.update_one( make_document( kvp("qybh",qybh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11007");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11007", __LINE__ );
   }
}

//删除表
void delete_d11007( mongocxx::collection& d11007, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   try {
      std::cout << "delete_d11007 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11007.delete_one( make_document( kvp("qybh",qybh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11007");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11007", __LINE__ );
   }
}

//创建表
void create_d11008( mongocxx::collection& d11008,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "qybh", qybh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11008 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11008.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11008");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11008", __LINE__ );
   }
}

//更新表
void update_d11008(mongocxx::collection& d11008,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "qybh", qybh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11008 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11008.update_one( make_document( kvp("qybh",qybh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11008");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11008", __LINE__ );
   }
}

//删除表
void delete_d11008( mongocxx::collection& d11008, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   try {
      std::cout << "delete_d11008 qybh" << qybh_ele.get_utf8().value << std::endl;
      if( !d11008.delete_one( make_document( kvp("qybh", qybh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11008");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11008", __LINE__ );
   }
}

//创建表
void create_d11009( mongocxx::collection& d11009,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "qybh", qybh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11009 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11009.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11009");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11009", __LINE__ );
   }
}

//更新表
void update_d11009(mongocxx::collection& d11009,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "qybh", qybh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11009 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11009.update_one( make_document( kvp("qybh",qybh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11009");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11009", __LINE__ );
   }
}

//删除表
void delete_d11009( mongocxx::collection& d11009, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   try {
      std::cout << "delete_d11009 qybh" << qybh_ele.get_utf8().value << std::endl;
      if( !d11009.delete_one( make_document( kvp("qybh", qybh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11009");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11009", __LINE__ );
   }
}

//创建表
void create_d11010( mongocxx::collection& d11010,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "qybh", qybh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11010 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11010.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11010");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11010", __LINE__ );
   }
}

//更新表
void update_d11010(mongocxx::collection& d11010,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "qybh", qybh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11010 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11010.update_one( make_document( kvp("qybh",qybh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11010");
      }
   } catch (...) {
      handle_mongo_exception( "update_d1100", __LINE__ );
   }
}

//删除表
void delete_d11010( mongocxx::collection& d11010, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   try {
      std::cout << "delete_d11010 qybh" << qybh_ele.get_utf8().value << std::endl;
      if( !d11010.delete_one( make_document( kvp("qybh", qybh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11010");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11010", __LINE__ );
   }
}

//创建表
void create_d11011( mongocxx::collection& d11011,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "qybh", qybh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11011 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11011.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11011");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11011", __LINE__ );
   }
}

//更新表
void update_d11011(mongocxx::collection& d11011,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "qybh", qybh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11011 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11011.update_one( make_document( kvp("qybh",qybh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11011");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11011", __LINE__ );
   }
}

//删除表
void delete_d11011( mongocxx::collection& d11011, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   try {
      std::cout << "delete_d11011 qybh" << qybh_ele.get_utf8().value << std::endl;
      if( !d11011.delete_one( make_document( kvp("qybh",qybh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11011");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11011", __LINE__ );
   }
}

//创建表
void create_d11012( mongocxx::collection& d11012,const bsoncxx::document::view& data, std::chrono::milliseconds& now  ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( true );

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document(
         kvp( "$set", make_document(   kvp( "qybh", qybh_ele.get_value()),
                                       kvp( "data", data),
                                       kvp( "createdAt", b_date{now} ),
                                       kvp("block_time",b_date{block_time})
                                       )));
   try {
      std::cout << "create_d11012 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11012.update_one( make_document( kvp( "data", data )), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to insert d11012");
      }
   } catch (...) {
      handle_mongo_exception( "create_d11012", __LINE__ );
   }
}

//更新表
void update_d11012(mongocxx::collection& d11012,const bsoncxx::document::view& data,std::chrono::milliseconds& now ,std::chrono::milliseconds block_time) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   mongocxx::options::update update_opts{};
   update_opts.upsert( false );
    
   bsoncxx::document::element qybh_ele = data["qybh"]; 
   auto update = make_document( 
       kvp( "$set", make_document(  kvp( "qybh", qybh_ele.get_value()),
                                    kvp( "data", data),
                                    kvp( "createdAt", b_date{now} ),
                                    kvp("block_time",b_date{block_time})
                                    )));
   try {
      std::cout << "update_d11012 qybh" <<qybh_ele.get_utf8().value << std::endl;
      if( !d11012.update_one( make_document( kvp("qybh",qybh_ele.get_value())), update.view(), update_opts )) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to update d11012");
      }
   } catch (...) {
      handle_mongo_exception( "update_d11012", __LINE__ );
   }
}

//删除表
void delete_d11012( mongocxx::collection& d11012, const bsoncxx::document::view& data, std::chrono::milliseconds& now ) {
   using namespace bsoncxx::types;
   using bsoncxx::builder::basic::kvp;
   using bsoncxx::builder::basic::make_document;

   bsoncxx::document::element qybh_ele = data["qybh"]; 
   try {
      std::cout << "delete_d11012 qybh" << qybh_ele.get_utf8().value << std::endl;
      if( !d11012.delete_one( make_document( kvp("qybh",qybh_ele.get_value())))) {
         EOS_ASSERT( false, chain::mongo_db_update_fail, "Failed to delete d11012");
      }
   } catch (...) {
      handle_mongo_exception( "delete_d11012", __LINE__ );
   }
}


}

void hblf_mongo_db_plugin_impl::update_base_col(const chain::action& act, const bsoncxx::document::view& actdoc,const chain::transaction_trace_ptr& t)
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
      

      auto block_time = std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::microseconds{t->block_time.to_time_point().time_since_epoch().count()});

      std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds{fc::time_point::now().time_since_epoch().count()} );

     
       if(act.name == addd20001){
               create_d20001(_d20001,datadoc,now,block_time);
      }else if(act.name == modd20001){
               update_d20001(_d20001,datadoc,now,block_time);
      }else if(act.name == deld20001){
               delete_d20001(_d20001,datadoc,now);
      }else if(act.name == addd20002){
               create_d20002(_d20002,datadoc,now,block_time);
      }else if(act.name == modd20002){
               update_d20002(_d20002,datadoc,now,block_time);
      }else if(act.name == deld20002){
               delete_d20002(_d20002,datadoc,now);
      }else if(act.name == addd80001){
               create_d80001(_d80001,datadoc,now,block_time);
      }else if(act.name == modd80001){
               update_d80001(_d80001,datadoc,now,block_time);
      }else if(act.name == deld80001){
               delete_d80001(_d80001,datadoc,now);        
      }else if(act.name == addd50001){
               create_d50001(_d50001,datadoc,now,block_time);
      }else if(act.name == modd50001){
               update_d50001(_d50001,datadoc,now,block_time);
      }else if(act.name == deld50001){
               delete_d50001(_d50001,datadoc,now);        
      }else if(act.name == addd50002){
               create_d50002(_d50002,datadoc,now,block_time);
      }else if(act.name == modd50002){
               update_d50002(_d50002,datadoc,now,block_time);
      }else if(act.name == deld50002){
               delete_d50002(_d50002,datadoc,now);        
      }else if(act.name == addd50003){
               create_d50003(_d50003,datadoc,now,block_time);
      }else if(act.name == modd50003){
               update_d50003(_d50003,datadoc,now,block_time);
      }else if(act.name == deld50003){
               delete_d50003(_d50003,datadoc,now);        
      }else if(act.name == addd50004){
               create_d50004(_d50004,datadoc,now,block_time);
      }else if(act.name == modd50004){
               update_d50004(_d50004,datadoc,now,block_time);
      }else if(act.name == deld50004){
               delete_d50004(_d50004,datadoc,now);        
      }else if(act.name == addd50005){
               create_d50005(_d50005,datadoc,now,block_time);
      }else if(act.name == modd50005){
               update_d50005(_d50005,datadoc,now,block_time);
      }else if(act.name == deld50005){
               delete_d50005(_d50005,datadoc,now);        
      }else if(act.name == addd50006){
               create_d50006(_d50006,datadoc,now,block_time);
      }else if(act.name == modd50006){
               update_d50006(_d50006,datadoc,now,block_time);
      }else if(act.name == deld50006){
               delete_d50006(_d50006,datadoc,now);        
      }else if(act.name == addd50007){
               create_d50007(_d50007,datadoc,now,block_time);
      }else if(act.name == modd50007){
               update_d50007(_d50007,datadoc,now,block_time);
      }else if(act.name == deld50007){
               delete_d50007(_d50007,datadoc,now);        
      }else if(act.name == addd50008){
               create_d50008(_d50008,datadoc,now,block_time);
      }else if(act.name == modd50008){
               update_d50008(_d50008,datadoc,now,block_time);
      }else if(act.name == deld50008){
               delete_d50008(_d50008,datadoc,now);        
      }else if(act.name == addd00001){
               create_d00001(_d00001,datadoc,now,block_time);
      }else if(act.name == modd00001){
               update_d00001(_d00001,datadoc,now,block_time);
      }else if(act.name == deld00001){
               delete_d00001(_d00001,datadoc,now);
      }else if(act.name == addd00007){
               create_d00007(_d00007,datadoc,now,block_time);
      }else if(act.name == modd00007){
               update_d00007(_d00007,datadoc,now,block_time);
      }else if(act.name == deld00007){
               delete_d00007(_d00007,datadoc,now);
      }else if(act.name == addd00005){
               create_d00005(_d00005,datadoc,now,block_time);
      }else if(act.name == modd00005){
              update_d00005(_d00005,datadoc,now,block_time);
      }else if(act.name == deld00005){
               delete_d00005(_d00005,datadoc,now);
      }else if(act.name == addd00006){
               create_d00006(_d00006,datadoc,now,block_time);
      }else if(act.name == modd00006){
              update_d00006(_d00006,datadoc,now,block_time);
      }else if(act.name == deld00006){
               delete_d00006(_d00006,datadoc,now);
      }else if(act.name == addd40003){
               create_d40003(_d40003,datadoc,now,block_time);
      }else if(act.name == modd40003){
              update_d40003(_d40003,datadoc,now,block_time);
      }else if(act.name == deld40003){
               delete_d40003(_d40003,datadoc,now);
      }else if(act.name == addd30001){
               create_d30001(_d30001,datadoc,now,block_time);
      }else if(act.name == modd30001){
               update_d30001(_d30001,datadoc,now,block_time);
      }else if(act.name == deld30001){
               delete_d30001(_d30001,datadoc,now);
      }else if(act.name == addd30002){
               create_d30002(_d30002,datadoc,now,block_time);
      }else if(act.name == modd30002){
               update_d30002(_d30002,datadoc,now,block_time);
      }else if(act.name == deld30002){
               delete_d30002(_d30002,datadoc,now);
      }else if(act.name == addd30003){
               create_d30003(_d30003,datadoc,now,block_time);
      }else if(act.name == modd30003){
               update_d30003(_d30003,datadoc,now,block_time);
      }else if(act.name == deld30003){
               delete_d30003(_d30003,datadoc,now);
      }else if(act.name == addd30004){
               create_d30004(_d30004,datadoc,now,block_time);
      }else if(act.name == modd30004){
               update_d30004(_d30004,datadoc,now,block_time);
      }else if(act.name == deld30004){
               delete_d30004(_d30004,datadoc,now);
      }else if(act.name == addd40001){
               create_d40001(_d40001,datadoc,now,block_time);
      }else if(act.name == modd40001){
              update_d40001(_d40001,datadoc,now,block_time);
      }else if(act.name == deld40001){
               delete_d40001(_d40001,datadoc,now);
      }else if(act.name == addd40002){
               create_d40002(_d40002,datadoc,now,block_time);
      }else if(act.name == modd40002){
              update_d40002(_d40002,datadoc,now,block_time);
      }else if(act.name == deld40002){
               delete_d40002(_d40002,datadoc,now);
      }else if(act.name == addd40004){
               create_d40004(_d40004,datadoc,now,block_time);
      }else if(act.name == modd40004){
              update_d40004(_d40004,datadoc,now,block_time);
      }else if(act.name == deld40004){
               delete_d40004(_d40004,datadoc,now);
      }else if(act.name == addd40005){
               create_d40005(_d40005,datadoc,now,block_time);
      }else if(act.name == modd40005){
              update_d40005(_d40005,datadoc,now,block_time);
      }else if(act.name == deld40005){
               delete_d40005(_d40005,datadoc,now);
      }else if(act.name == addd40006){
               create_d40006(_d40006,datadoc,now,block_time);
      }else if(act.name == modd40006){
              update_d40006(_d40006,datadoc,now,block_time);
      }else if(act.name == deld40006){
               delete_d40006(_d40006,datadoc,now);
      }else if(act.name == addd80002){
               create_d80002(_d80002,datadoc,now,block_time);
      }else if(act.name == modd80002){
               update_d80002(_d80002,datadoc,now,block_time);
      }else if(act.name == deld80002){
               delete_d80002(_d80002,datadoc,now);        
      }else if(act.name == addd90001){
               create_d90001(_d90001,datadoc,now,block_time);
      }else if(act.name == modd90001){
               update_d90001(_d90001,datadoc,now,block_time);
      }else if(act.name == deld90001){
               delete_d90001(_d90001,datadoc,now);        
      }else if(act.name == addd90002){
               create_d90002(_d90002,datadoc,now,block_time);
      }else if(act.name == modd90002){
               update_d90002(_d90002,datadoc,now,block_time);
      }else if(act.name == deld90002){
               delete_d90002(_d90002,datadoc,now);        
      }else if(act.name == addd90003){
               create_d90003(_d90003,datadoc,now,block_time);
      }else if(act.name == modd90003){
               update_d90003(_d90003,datadoc,now,block_time);
      }else if(act.name == deld90003){
               delete_d90003(_d90003,datadoc,now);        
      }else if(act.name == addd90004){
               create_d90004(_d90004,datadoc,now,block_time);
      }else if(act.name == modd90004){
               update_d90004(_d90004,datadoc,now,block_time);
      }else if(act.name == deld90004){
               delete_d90004(_d90004,datadoc,now);        
      }else if(act.name == addd90005){
               create_d90005(_d90005,datadoc,now,block_time);
      }else if(act.name == modd90005){
               update_d90005(_d90005,datadoc,now,block_time);
      }else if(act.name == deld90005){
               delete_d90005(_d90005,datadoc,now);        
      }else if(act.name == addd60001){
               create_d60001(_d60001,datadoc,now,block_time);
      }else if(act.name == modd60001){
               update_d60001(_d60001,datadoc,now,block_time);
      }else if(act.name == deld60001){
               delete_d60001(_d60001,datadoc,now);        
      }else if(act.name == addd60002){
               create_d60002(_d60002,datadoc,now,block_time);
      }else if(act.name == modd60002){
               update_d60002(_d60002,datadoc,now,block_time);
      }else if(act.name == deld60002){
               delete_d60002(_d60002,datadoc,now);        
      }else if(act.name == addd60003){
               create_d60003(_d60003,datadoc,now,block_time);
      }else if(act.name == modd60003){
               update_d60003(_d60003,datadoc,now,block_time);
      }else if(act.name == deld60003){
               delete_d60003(_d60003,datadoc,now);        
      }else if(act.name == addd70001){
               create_d70001(_d70001,datadoc,now,block_time);
      }else if(act.name == modd70001){
               update_d70001(_d70001,datadoc,now,block_time);
      }else if(act.name == deld70001){
               delete_d70001(_d70001,datadoc,now);        
      }else if(act.name == addd70002){
               create_d70002(_d70002,datadoc,now,block_time);
      }else if(act.name == modd70002){
               update_d70002(_d70002,datadoc,now,block_time);
      }else if(act.name == deld70002){
               delete_d70002(_d70002,datadoc,now);        
      }else if(act.name == addd70003){
               create_d70003(_d70003,datadoc,now,block_time);
      }else if(act.name == modd70003){
               update_d70003(_d70003,datadoc,now,block_time);
      }else if(act.name == deld70003){
               delete_d70003(_d70003,datadoc,now);        
      }else if(act.name == addd70004){
               create_d70004(_d70004,datadoc,now,block_time);
      }else if(act.name == modd70004){
               update_d70004(_d70004,datadoc,now,block_time);
      }else if(act.name == deld70004){
               delete_d70004(_d70004,datadoc,now);        
      }else if(act.name == addd00002){
               create_d00002(_d00002,datadoc,now,block_time);
      }else if(act.name == modd00002){
              update_d00002(_d00002,datadoc,now,block_time);
      }else if(act.name == deld00002){
               delete_d00002(_d00002,datadoc,now);
      }else if(act.name == addd00003){
               create_d00003(_d00003,datadoc,now,block_time);
      }else if(act.name == modd00003){
              update_d00003(_d00003,datadoc,now,block_time);
      }else if(act.name == deld00003){
               delete_d00003(_d00003,datadoc,now);
      }else if(act.name == addd00004){
               create_d00004(_d00004,datadoc,now,block_time);
      }else if(act.name == modd00004){
              update_d00004(_d00004,datadoc,now,block_time);
      }else if(act.name == deld00004){
               delete_d00004(_d00004,datadoc,now);
      }else if(act.name == addd41001){
               create_d41001(_d41001,datadoc,now,block_time);
      }else if(act.name == modd41001){
              update_d41001(_d41001,datadoc,now,block_time);
      }else if(act.name == deld41001){
               delete_d41001(_d41001,datadoc,now);
      }else if(act.name == addd41002){
               create_d41002(_d41002,datadoc,now,block_time);
      }else if(act.name == modd41002){
              update_d41002(_d41002,datadoc,now,block_time);
      }else if(act.name == deld41002){
               delete_d41002(_d41002,datadoc,now);
      }else if(act.name == addd41003){
               create_d41003(_d41003,datadoc,now,block_time);
      }else if(act.name == modd41003){
              update_d41003(_d41003,datadoc,now,block_time);
      }else if(act.name == deld41003){
               delete_d41003(_d41003,datadoc,now);
      }
      else if(act.name == addd11001){
               create_d11001(_d11001,datadoc,now,block_time);
      }else if(act.name == modd11001){
              update_d11001(_d11001,datadoc,now,block_time);
      }else if(act.name == deld11001){
               delete_d11001(_d11001,datadoc,now);
      }else if(act.name == addd11002){
               create_d11002(_d11002,datadoc,now,block_time);
      }else if(act.name == modd11002){
              update_d11002(_d11002,datadoc,now,block_time);
      }else if(act.name == deld11002){
               delete_d11002(_d11002,datadoc,now);
      }else if(act.name == addd11003){
               create_d11003(_d11003,datadoc,now,block_time);
      }else if(act.name == modd11003){
              update_d11003(_d11003,datadoc,now,block_time);
      }else if(act.name == deld11003){
               delete_d11003(_d11003,datadoc,now);
      }else if(act.name == addd11004){
               create_d11004(_d11004,datadoc,now,block_time);
      }else if(act.name == modd11004){
              update_d11004(_d11004,datadoc,now,block_time);
      }else if(act.name == deld11004){
               delete_d11004(_d11004,datadoc,now);
      }else if(act.name == addd11005){
               create_d11005(_d11005,datadoc,now,block_time);
      }else if(act.name == modd11005){
              update_d11005(_d11005,datadoc,now,block_time);
      }else if(act.name == deld11005){
               delete_d11005(_d11005,datadoc,now);
      }else if(act.name == addd11006){
               create_d11006(_d11006,datadoc,now,block_time);
      }else if(act.name == modd11006){
              update_d11006(_d11006,datadoc,now,block_time);
      }else if(act.name == deld11006){
               delete_d11006(_d11006,datadoc,now);
      }else if(act.name == addd11007){
               create_d11007(_d11007,datadoc,now,block_time);
      }else if(act.name == modd11007){
              update_d11007(_d11007,datadoc,now,block_time);
      }else if(act.name == deld11007){
               delete_d11007(_d11007,datadoc,now);
      }else if(act.name == addd11008){
               create_d11008(_d11008,datadoc,now,block_time);
      }else if(act.name == modd11008){
              update_d11008(_d11008,datadoc,now,block_time);
      }else if(act.name == deld11008){
               delete_d11008(_d11008,datadoc,now);
      }else if(act.name == addd11009){
               create_d11009(_d11009,datadoc,now,block_time);
      }else if(act.name == modd11009){
              update_d11009(_d11009,datadoc,now,block_time);
      }else if(act.name == deld11009){
               delete_d11009(_d11009,datadoc,now);
      }else if(act.name == addd11010){
               create_d11010(_d11010,datadoc,now,block_time);
      }else if(act.name == modd11010){
              update_d11010(_d11010,datadoc,now,block_time);
      }else if(act.name == deld11010){
               delete_d11010(_d11010,datadoc,now);
      }else if(act.name == addd11011){
               create_d11011(_d11011,datadoc,now,block_time);
      }else if(act.name == modd11011){
              update_d11011(_d11011,datadoc,now,block_time);
      }else if(act.name == deld11011){
               delete_d11011(_d11011,datadoc,now);
      }else if(act.name == addd11012){
               create_d11012(_d11012,datadoc,now,block_time);
      }else if(act.name == modd11012){
              update_d11012(_d11012,datadoc,now,block_time);
      }else if(act.name == deld11012){
               delete_d11012(_d11012,datadoc,now);
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

    auto d30001 = mongo_conn[db_name][d30001_col];
    auto d30001_traces = mongo_conn[db_name][d30001_traces_col];
    auto d30002 = mongo_conn[db_name][d30002_col];
    auto d30002_traces = mongo_conn[db_name][d30002_traces_col];
    auto d30003 = mongo_conn[db_name][d30003_col];
    auto d30003_traces = mongo_conn[db_name][d30003_traces_col];
    auto d30004 = mongo_conn[db_name][d30004_col];
    auto d30004_traces = mongo_conn[db_name][d30004_traces_col];

    auto d40001 = mongo_conn[db_name][d40001_col];
    auto d40001_traces = mongo_conn[db_name][d40001_traces_col];
    auto d40002 = mongo_conn[db_name][d40002_col];
    auto d40002_traces = mongo_conn[db_name][d40002_traces_col];
    auto d40004 = mongo_conn[db_name][d40004_col];
    auto d40004_traces = mongo_conn[db_name][d40004_traces_col];
    auto d40005 = mongo_conn[db_name][d40005_col];
    auto d40005_traces = mongo_conn[db_name][d40005_traces_col];
    auto d40006 = mongo_conn[db_name][d40006_col];
    auto d40006_traces = mongo_conn[db_name][d40006_traces_col];
    auto d80002 = mongo_conn[db_name][d80002_col];
    auto d80002_traces = mongo_conn[db_name][d80002_traces_col];

    auto d90001 = mongo_conn[db_name][d90001_col];
    auto d90001_traces = mongo_conn[db_name][d90001_traces_col];
    auto d90002 = mongo_conn[db_name][d90002_col];
    auto d90002_traces = mongo_conn[db_name][d90002_traces_col];
    auto d90003 = mongo_conn[db_name][d90003_col];
    auto d90003_traces = mongo_conn[db_name][d90003_traces_col];
    auto d90004 = mongo_conn[db_name][d90004_col];
    auto d90004_traces = mongo_conn[db_name][d90004_traces_col];
    auto d90005 = mongo_conn[db_name][d90005_col];
    auto d90005_traces = mongo_conn[db_name][d90005_traces_col];

    auto d60001 = mongo_conn[db_name][d60001_col];
    auto d60001_traces = mongo_conn[db_name][d60001_traces_col];
    auto d60002 = mongo_conn[db_name][d60002_col];
    auto d60002_traces = mongo_conn[db_name][d60002_traces_col];
    auto d60003 = mongo_conn[db_name][d60003_col];
    auto d60003_traces = mongo_conn[db_name][d60003_traces_col];
    
    auto d70001 = mongo_conn[db_name][d70001_col];
    auto d70001_traces = mongo_conn[db_name][d70001_traces_col];
    auto d70002 = mongo_conn[db_name][d70002_col];
    auto d70002_traces = mongo_conn[db_name][d70002_traces_col];
    auto d70003 = mongo_conn[db_name][d70003_col];
    auto d70003_traces = mongo_conn[db_name][d70003_traces_col];
    auto d70004 = mongo_conn[db_name][d70004_col];
    auto d70004_traces = mongo_conn[db_name][d70004_traces_col];

    auto d00002 = mongo_conn[db_name][d00002_col];
    auto d00002_traces = mongo_conn[db_name][d00002_traces_col];
    auto d00003 = mongo_conn[db_name][d00003_col];
    auto d00003_traces = mongo_conn[db_name][d00003_traces_col];
    auto d00004 = mongo_conn[db_name][d00004_col];
    auto d00004_traces = mongo_conn[db_name][d00004_traces_col];

    auto d41001 = mongo_conn[db_name][d41001_col];
    auto d41001_traces = mongo_conn[db_name][d41001_traces_col];
    auto d41002 = mongo_conn[db_name][d41002_col];
    auto d41002_traces = mongo_conn[db_name][d41002_traces_col];
    auto d41003 = mongo_conn[db_name][d41003_col];
    auto d41003_traces = mongo_conn[db_name][d41003_traces_col];

    auto d11001 = mongo_conn[db_name][d11001_col];
    auto d11001_traces = mongo_conn[db_name][d11001_traces_col];
    auto d11002 = mongo_conn[db_name][d11002_col];
    auto d11002_traces = mongo_conn[db_name][d11002_traces_col];
    auto d11003 = mongo_conn[db_name][d11003_col];
    auto d11003_traces = mongo_conn[db_name][d11003_traces_col];
    auto d11004 = mongo_conn[db_name][d11004_col];
    auto d11004_traces = mongo_conn[db_name][d11004_traces_col];
    auto d11005 = mongo_conn[db_name][d11005_col];
    auto d11005_traces = mongo_conn[db_name][d11005_traces_col];
    auto d11006 = mongo_conn[db_name][d11006_col];
    auto d11006_traces = mongo_conn[db_name][d11006_traces_col];
    auto d11007 = mongo_conn[db_name][d11007_col];
    auto d11007_traces = mongo_conn[db_name][d11007_traces_col];
    auto d11008 = mongo_conn[db_name][d11008_col];
    auto d11008_traces = mongo_conn[db_name][d11008_traces_col];
    auto d11009 = mongo_conn[db_name][d11009_col];
    auto d11009_traces = mongo_conn[db_name][d11009_traces_col];
    auto d11010 = mongo_conn[db_name][d11010_col];
    auto d11010_traces = mongo_conn[db_name][d11010_traces_col];
    auto d11011 = mongo_conn[db_name][d11011_col];
    auto d11011_traces = mongo_conn[db_name][d11011_traces_col];
    auto d11012 = mongo_conn[db_name][d11012_col];
    auto d11012_traces = mongo_conn[db_name][d11012_traces_col];


   

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
    d30001.drop();
    d30001_traces.drop(); 
    d30002.drop();
    d30002_traces.drop(); 
    d30003.drop();
    d30003_traces.drop(); 
    d30004.drop();
    d30004_traces.drop(); 
    d40001.drop();
    d40001_traces.drop(); 
    d40002.drop();
    d40002_traces.drop(); 
    d40004.drop();
    d40004_traces.drop(); 
    d40005.drop();
    d40005_traces.drop(); 
    d40006.drop();
    d40006_traces.drop(); 
    d80002.drop();
    d80002_traces.drop();

    d90001.drop();
    d90001_traces.drop();
    d90002.drop();
    d90002_traces.drop();
    d90003.drop();
    d90003_traces.drop();
    d90004.drop();
    d90004_traces.drop();
    d90005.drop();
    d90005_traces.drop();

    d60001.drop();
    d60001_traces.drop();
    d60002.drop();
    d60002_traces.drop();
    d60003.drop();
    d60003_traces.drop();

    d70001.drop();
    d70001_traces.drop();
    d70002.drop();
    d70002_traces.drop();
    d70003.drop();
    d70003_traces.drop();
    d70004.drop();
    d70004_traces.drop();

    d00002.drop();
    d00002_traces.drop();
    d00003.drop();
    d00003_traces.drop();
    d00004.drop();
    d00004_traces.drop();

    d41001.drop();
    d41001_traces.drop();
    d41002.drop();
    d41002_traces.drop();
    d41003.drop();
    d41003_traces.drop();

    d11001.drop();
    d11001_traces.drop();
    d11002.drop();
    d11002_traces.drop();
    d11003.drop();
    d11003_traces.drop();
    d11004.drop();
    d11004_traces.drop();
    d11005.drop();
    d11005_traces.drop();
    d11006.drop();
    d11006_traces.drop();
    d11007.drop();
    d11007_traces.drop();
    d11008.drop();
    d11008_traces.drop();
    d11009.drop();
    d11009_traces.drop();
    d11010.drop();
    d11010_traces.drop();
    d11011.drop();
    d11011_traces.drop();
    d11012.drop();
    d11012_traces.drop();



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

      auto d00001_COL = mongo_conn[db_name][d00001_col];
      if( d00001_COL.count( make_document()) == 0 ) {
             
         try {
            // MongoDB administrators (to enable sharding) :
            //   1. enableSharding database (default to EOS)
            //   2. shardCollection: blocks, action_traces, transaction_traces, especially action_traces
            //   3. Compound index with shard key (default to _id below), to improve query performance.

            
            
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

            //d80002 indexes
            auto  d80002 = mongo_conn[db_name][ d80002_col];
            d80002.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));
            
            auto  d80002_traces =  mongo_conn[db_name][ d80002_traces_col];
            d80002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));
                    
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
            d50006.create_index(bsoncxx::from_json( R"xxx({ "jsid" : 1, "_id" : 1 })xxx" ));
            
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
            d40003.create_index(bsoncxx::from_json( R"xxx({ "ksid ": 1, "_id" : 1 })xxx" ));
            
            auto  d40003_traces =  mongo_conn[db_name][ d40003_traces_col];
            d40003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d30001 indexes
            auto  d30001 = mongo_conn[db_name][ d30001_col];
            d30001.create_index(bsoncxx::from_json( R"xxx({ "jsid ": 1, "_id" : 1 })xxx" ));
            
            auto  d30001_traces =  mongo_conn[db_name][d30001_traces_col];
            d30001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d30002 indexes
            auto  d30002 = mongo_conn[db_name][d30002_col];
            d30002.create_index(bsoncxx::from_json( R"xxx({ "jsid ": 1, "_id" : 1 })xxx" ));
            
            auto  d30002_traces =  mongo_conn[db_name][d30002_traces_col];
            d30002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d30003 indexes
            auto  d30003 = mongo_conn[db_name][d30003_col];
            d30003.create_index(bsoncxx::from_json( R"xxx({ "jsid ": 1, "_id" : 1 })xxx" ));
            
            auto  d30003_traces =  mongo_conn[db_name][d30003_traces_col];
            d30003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d30004 indexes
            auto  d30004 = mongo_conn[db_name][d30004_col];
            d30004.create_index(bsoncxx::from_json( R"xxx({ "jsid ": 1, "_id" : 1 })xxx" ));
            
            auto  d30004_traces =  mongo_conn[db_name][d30004_traces_col];
            d30004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d40001 indexes
            auto  d40001 = mongo_conn[db_name][d40001_col];
            d40001.create_index(bsoncxx::from_json( R"xxx({ "ksid ": 1, "_id" : 1 })xxx" ));
            
            auto  d40001_traces =  mongo_conn[db_name][d40001_traces_col];
            d40001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d40002 indexes
            auto  d40002 = mongo_conn[db_name][d40002_col];
            d40002.create_index(bsoncxx::from_json( R"xxx({ "ksid ": 1, "_id" : 1 })xxx" ));
            
            auto  d40002_traces =  mongo_conn[db_name][d40002_traces_col];
            d40002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d40004 indexes
            auto  d40004 = mongo_conn[db_name][d40004_col];
            d40004.create_index(bsoncxx::from_json( R"xxx({ "sjid ": 1, "_id" : 1 })xxx" ));
            
            auto  d40004_traces =  mongo_conn[db_name][d40004_traces_col];
            d40004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d40005 indexes
            auto  d40005 = mongo_conn[db_name][d40005_col];
            d40005.create_index(bsoncxx::from_json( R"xxx({ "ksid ": 1, "_id" : 1 })xxx" ));
            
            auto  d40005_traces =  mongo_conn[db_name][d40005_traces_col];
            d40005_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d40006 indexes
            auto  d40006= mongo_conn[db_name][d40006_col];
            d40006.create_index(bsoncxx::from_json( R"xxx({ "zsdid": 1, "_id" : 1 })xxx" ));
            
            auto  d40006_traces =  mongo_conn[db_name][d40006_traces_col];
            d40006_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d90001 indexes
            auto  d90001= mongo_conn[db_name][d90001_col];
            d90001.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d90001_traces =  mongo_conn[db_name][d90001_traces_col];
            d90001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d90002 indexes
            auto  d90002= mongo_conn[db_name][d90002_col];
            d90002.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d90002_traces =  mongo_conn[db_name][d90002_traces_col];
            d90002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d90003 indexes
            auto  d90003= mongo_conn[db_name][d90003_col];
            d90003.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d90003_traces =  mongo_conn[db_name][d90003_traces_col];
            d90003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d90004 indexes
            auto  d90004= mongo_conn[db_name][d90004_col];
            d90004.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d90004_traces =  mongo_conn[db_name][d90004_traces_col];
            d90004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d90005 indexes
            auto  d90005= mongo_conn[db_name][d90005_col];
            d90005.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d90005_traces =  mongo_conn[db_name][d90005_traces_col];
            d90005_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d60001 indexes
            auto  d60001= mongo_conn[db_name][d60001_col];
            d60001.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d60001_traces =  mongo_conn[db_name][d60001_traces_col];
            d60001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d60002 indexes
            auto  d60002= mongo_conn[db_name][d60002_col];
            d60002.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d60002_traces =  mongo_conn[db_name][d60002_traces_col];
            d60002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d60003 indexes
            auto  d60003= mongo_conn[db_name][d60003_col];
            d60003.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d60003_traces =  mongo_conn[db_name][d60003_traces_col];
            d60003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));


         //d70001 indexes
            auto  d70001= mongo_conn[db_name][d70001_col];
            d70001.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d70001_traces =  mongo_conn[db_name][d70001_traces_col];
            d70001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d70002 indexes
            auto  d70002= mongo_conn[db_name][d70002_col];
            d70002.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d70002_traces =  mongo_conn[db_name][d70002_traces_col];
            d70002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d70003 indexes
            auto  d70003= mongo_conn[db_name][d70003_col];
            d70003.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d70003_traces =  mongo_conn[db_name][d70003_traces_col];
            d70003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d7000 indexes
            auto  d70004= mongo_conn[db_name][d70004_col];
            d70004.create_index(bsoncxx::from_json( R"xxx({ "jsid": 1, "_id" : 1 })xxx" ));
            
            auto  d70004_traces =  mongo_conn[db_name][d70004_traces_col];
            d70004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d00002 indexes
            auto  d00002 = mongo_conn[db_name][ d00002_col];
            d00002.create_index(bsoncxx::from_json( R"xxx({ "classId" : 1, "_id" : 1 })xxx" ));
            
            auto  d00002_traces =  mongo_conn[db_name][ d00002_traces_col];
            d00002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d00003 indexes
            auto  d00003 = mongo_conn[db_name][ d00003_col];
            d00003.create_index(bsoncxx::from_json( R"xxx({ "classId" : 1, "_id" : 1 })xxx" ));
            
            auto  d00003_traces =  mongo_conn[db_name][ d00003_traces_col];
            d00003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d00004 indexes
            auto  d00004 = mongo_conn[db_name][ d00004_col];
            d00004.create_index(bsoncxx::from_json( R"xxx({ "classId" : 1, "_id" : 1 })xxx" ));
            
            auto  d00004_traces =  mongo_conn[db_name][ d00004_traces_col];
            d00004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d4100 indexes
            auto  d41001 = mongo_conn[db_name][ d41001_col];
            d41001.create_index(bsoncxx::from_json( R"xxx({ "xsid" : 1, "_id" : 1 })xxx" ));
            
            auto  d41001_traces =  mongo_conn[db_name][ d41001_traces_col];
            d41001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d4100 indexes
            auto  d41002= mongo_conn[db_name][ d41002_col];
            d41002.create_index(bsoncxx::from_json( R"xxx({ "xsid" : 1, "_id" : 1 })xxx" ));
            
            auto  d41002_traces =  mongo_conn[db_name][ d41002_traces_col];
            d41002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

            //d4100 indexes
            auto  d41003 = mongo_conn[db_name][ d41003_col];
            d41003.create_index(bsoncxx::from_json( R"xxx({ "xsid" : 1, "_id" : 1 })xxx" ));
            
            auto  d41003_traces =  mongo_conn[db_name][ d41003_traces_col];
            d41003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

      
            //d11001 indexes
            auto  d11001= mongo_conn[db_name][ d11001_col];
            d11001.create_index(bsoncxx::from_json( R"xxx({ "yhid":1, "_id" : 1 })xxx" ));
            
            auto  d11001_traces =  mongo_conn[db_name][ d11001_traces_col];
            d11001_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11002 indexes
            auto  d11002= mongo_conn[db_name][ d11002_col];
            d11002.create_index(bsoncxx::from_json( R"xxx({ "yhid":1, "_id" : 1 })xxx" ));
            
            auto  d11002_traces =  mongo_conn[db_name][ d11002_traces_col];
            d11002_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11003 indexes
            auto  d11003= mongo_conn[db_name][ d11003_col];
            d11003.create_index(bsoncxx::from_json( R"xxx({  "orgid":1,"_id" : 1 })xxx" ));
            
            auto  d11003_traces =  mongo_conn[db_name][ d11003_traces_col];
            d11003_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11004 indexes
            auto  d11004= mongo_conn[db_name][ d11004_col];
            d11004.create_index(bsoncxx::from_json( R"xxx({  "xxbh":1,"orgid":1,"_id" : 1 })xxx" ));
            
            auto  d11004_traces =  mongo_conn[db_name][ d11004_traces_col];
            d11004_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11005 indexes
            auto  d11005= mongo_conn[db_name][ d11005_col];
            d11005.create_index(bsoncxx::from_json( R"xxx({  "xsbh":1,"kmbh":1,"_id" : 1 })xxx" ));
            
            auto  d11005_traces =  mongo_conn[db_name][ d11005_traces_col];
            d11005_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11006 indexes
            auto  d11006= mongo_conn[db_name][ d11006_col];
            d11006.create_index(bsoncxx::from_json( R"xxx({ "orgid":1,"qybh":1,"xnbh":1, "_id" : 1 })xxx" ));
            
            auto  d11006_traces =  mongo_conn[db_name][ d11006_traces_col];
            d11006_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11007 indexes
            auto  d11007= mongo_conn[db_name][ d11007_col];
            d11007.create_index(bsoncxx::from_json( R"xxx({ "orgid":1,"qybh":1,"xnbh":1, "_id" : 1 })xxx" ));
            
            auto  d11007_traces =  mongo_conn[db_name][ d11007_traces_col];
            d11007_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d1100 indexes
            auto  d11008= mongo_conn[db_name][ d11008_col];
            d11008.create_index(bsoncxx::from_json( R"xxx({  "orgid":1,"qybh":1,"xnbh":1,"xkbh":1,"_id" : 1 })xxx" ));
            
            auto  d11008_traces =  mongo_conn[db_name][ d11008_traces_col];
            d11008_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11009 indexes
            auto  d11009= mongo_conn[db_name][ d11009_col];
            d11009.create_index(bsoncxx::from_json( R"xxx({  "qybh":1,"cxbh":1,"xnbh":1,"_id" : 1 })xxx" ));
            
            auto  d11009_traces =  mongo_conn[db_name][ d11009_traces_col];
            d11009_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11010 indexes
            auto  d11010= mongo_conn[db_name][ d11010_col];
            d11010.create_index(bsoncxx::from_json( R"xxx({  "qybh":1,"cxbh":1,"xnbh":1,"_id" : 1 })xxx" ));
            
            auto  d11010_traces =  mongo_conn[db_name][ d11010_traces_col];
            d11010_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11011 indexes
            auto  d11011= mongo_conn[db_name][ d11011_col];
            d11011.create_index(bsoncxx::from_json( R"xxx({  "qybh":1,"cxbh":1,"xnbh":1,"_id" : 1 })xxx" ));
            
            auto  d11011_traces =  mongo_conn[db_name][ d11011_traces_col];
            d11011_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

             //d11012 indexes
            auto  d11012= mongo_conn[db_name][ d11012_col];
            d11012.create_index(bsoncxx::from_json( R"xxx({  "qybh":1,"cxbh":1,"xnbh":1,"_id" : 1 })xxx" ));
            
            auto  d11012_traces =  mongo_conn[db_name][ d11012_traces_col];
            d11012_traces.create_index(bsoncxx::from_json( R"xxx({ "block_num" : 1, "_id" : 1 })xxx" ));

         

            
            


            
         } catch (...) {
            handle_mongo_exception( "create indexes", __LINE__ );
         }
      }

      if( expire_after_seconds > 0 ) {
         try {
          
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
            mongocxx::collection d80002 = mongo_conn[db_name][d80002_col];
            create_expiration_index( d80002, expire_after_seconds );
            mongocxx::collection d80002_traces = mongo_conn[db_name][d80002_traces_col];
            create_expiration_index( d80002_traces, expire_after_seconds );
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
            
            mongocxx::collection d30001 = mongo_conn[db_name][d30001_col];
            create_expiration_index( d30001, expire_after_seconds );
            mongocxx::collection d30001_traces = mongo_conn[db_name][d30001_traces_col];
            create_expiration_index( d30001_traces, expire_after_seconds );

            mongocxx::collection d30002 = mongo_conn[db_name][d30002_col];
            create_expiration_index( d30002, expire_after_seconds );
            mongocxx::collection d30002_traces = mongo_conn[db_name][d30002_traces_col];
            create_expiration_index( d30002_traces, expire_after_seconds );

            mongocxx::collection d30003 = mongo_conn[db_name][d30003_col];
            create_expiration_index( d30003, expire_after_seconds );
            mongocxx::collection d30003_traces = mongo_conn[db_name][d30003_traces_col];
            create_expiration_index( d30003_traces, expire_after_seconds );

            mongocxx::collection d30004 = mongo_conn[db_name][d30004_col];
            create_expiration_index( d30004, expire_after_seconds );
            mongocxx::collection d30004_traces = mongo_conn[db_name][d30004_traces_col];
            create_expiration_index( d30004_traces, expire_after_seconds );

            mongocxx::collection d40001 = mongo_conn[db_name][d40001_col];
            create_expiration_index( d40001, expire_after_seconds );
            mongocxx::collection d40001_traces = mongo_conn[db_name][d40001_traces_col];
            create_expiration_index( d40001_traces, expire_after_seconds );
            mongocxx::collection d40002 = mongo_conn[db_name][d40002_col];
            create_expiration_index( d40002, expire_after_seconds );
            mongocxx::collection d40002_traces = mongo_conn[db_name][d40002_traces_col];
            create_expiration_index( d40002_traces, expire_after_seconds );
            mongocxx::collection d40004 = mongo_conn[db_name][d40004_col];
            create_expiration_index( d40004, expire_after_seconds );
            mongocxx::collection d40004_traces = mongo_conn[db_name][d40004_traces_col];
            create_expiration_index( d40004_traces, expire_after_seconds );
            mongocxx::collection d40005 = mongo_conn[db_name][d40005_col];
            create_expiration_index( d40005, expire_after_seconds );
            mongocxx::collection d40005_traces = mongo_conn[db_name][d40005_traces_col];
            create_expiration_index( d40005_traces, expire_after_seconds );
            mongocxx::collection d40006 = mongo_conn[db_name][d40006_col];
            create_expiration_index( d40006, expire_after_seconds );
            mongocxx::collection d40006_traces = mongo_conn[db_name][d40006_traces_col];
            create_expiration_index( d40006_traces, expire_after_seconds );

            mongocxx::collection d90001 = mongo_conn[db_name][d90001_col];
            create_expiration_index( d90001, expire_after_seconds );
            mongocxx::collection d90001_traces = mongo_conn[db_name][d90001_traces_col];
            create_expiration_index( d90001_traces, expire_after_seconds );

            mongocxx::collection d90002 = mongo_conn[db_name][d90002_col];
            create_expiration_index( d90002, expire_after_seconds );
            mongocxx::collection d90002_traces = mongo_conn[db_name][d90002_traces_col];
            create_expiration_index( d90002_traces, expire_after_seconds );

            mongocxx::collection d90003 = mongo_conn[db_name][d90003_col];
            create_expiration_index( d90003, expire_after_seconds );
            mongocxx::collection d90003_traces = mongo_conn[db_name][d90003_traces_col];
            create_expiration_index( d90003_traces, expire_after_seconds );

            mongocxx::collection d90004 = mongo_conn[db_name][d90004_col];
            create_expiration_index( d90004, expire_after_seconds );
            mongocxx::collection d90004_traces = mongo_conn[db_name][d90004_traces_col];
            create_expiration_index( d90004_traces, expire_after_seconds );

            mongocxx::collection d90005 = mongo_conn[db_name][d90005_col];
            create_expiration_index( d90005, expire_after_seconds );
            mongocxx::collection d90005_traces = mongo_conn[db_name][d90005_traces_col];
            create_expiration_index( d90005_traces, expire_after_seconds );

            mongocxx::collection d60001 = mongo_conn[db_name][d60001_col];
            create_expiration_index( d60001, expire_after_seconds );
            mongocxx::collection d60001_traces = mongo_conn[db_name][d60001_traces_col];
            create_expiration_index( d60001_traces, expire_after_seconds );
            mongocxx::collection d60002 = mongo_conn[db_name][d60002_col];
            create_expiration_index( d60002, expire_after_seconds );
            mongocxx::collection d60002_traces = mongo_conn[db_name][d60002_traces_col];
            create_expiration_index( d60002_traces, expire_after_seconds );
            mongocxx::collection d60003 = mongo_conn[db_name][d60003_col];
            create_expiration_index( d60003, expire_after_seconds );
            mongocxx::collection d60003_traces = mongo_conn[db_name][d60003_traces_col];
            create_expiration_index( d60003_traces, expire_after_seconds );


            mongocxx::collection d70001 = mongo_conn[db_name][d70001_col];
            create_expiration_index( d70001, expire_after_seconds );
            mongocxx::collection d70001_traces = mongo_conn[db_name][d70001_traces_col];
            create_expiration_index( d70001_traces, expire_after_seconds );

            mongocxx::collection d70002 = mongo_conn[db_name][d70002_col];
            create_expiration_index( d70002, expire_after_seconds );
            mongocxx::collection d70002_traces = mongo_conn[db_name][d70002_traces_col];
            create_expiration_index( d70002_traces, expire_after_seconds );

            mongocxx::collection d70003 = mongo_conn[db_name][d70003_col];
            create_expiration_index( d70003, expire_after_seconds );
            mongocxx::collection d70003_traces = mongo_conn[db_name][d70003_traces_col];
            create_expiration_index( d70003_traces, expire_after_seconds );

            mongocxx::collection d70004 = mongo_conn[db_name][d70004_col];
            create_expiration_index( d70004, expire_after_seconds );
            mongocxx::collection d70004_traces = mongo_conn[db_name][d70004_traces_col];
            create_expiration_index( d70004_traces, expire_after_seconds );

             mongocxx::collection d00002 = mongo_conn[db_name][d00002_col];
            create_expiration_index( d00002, expire_after_seconds );
            mongocxx::collection d00002_traces = mongo_conn[db_name][d00002_traces_col];
            create_expiration_index( d00002_traces, expire_after_seconds );

             mongocxx::collection d00003 = mongo_conn[db_name][d00003_col];
            create_expiration_index( d00003, expire_after_seconds );
            mongocxx::collection d00003_traces = mongo_conn[db_name][d00003_traces_col];
            create_expiration_index( d00003_traces, expire_after_seconds );

             mongocxx::collection d00004 = mongo_conn[db_name][d00004_col];
            create_expiration_index( d00004, expire_after_seconds );
            mongocxx::collection d00004_traces = mongo_conn[db_name][d00004_traces_col];
            create_expiration_index( d00004_traces, expire_after_seconds );

            mongocxx::collection d41001 = mongo_conn[db_name][d41001_col];
            create_expiration_index( d41001, expire_after_seconds );
            mongocxx::collection d41001_traces = mongo_conn[db_name][d41001_traces_col];
            create_expiration_index( d41001_traces, expire_after_seconds );

            mongocxx::collection d41002 = mongo_conn[db_name][d41002_col];
            create_expiration_index( d41002, expire_after_seconds );
            mongocxx::collection d41002_traces = mongo_conn[db_name][d41002_traces_col];
            create_expiration_index( d41002_traces, expire_after_seconds );

            mongocxx::collection d41003 = mongo_conn[db_name][d41003_col];
            create_expiration_index( d41003, expire_after_seconds );
            mongocxx::collection d41003_traces = mongo_conn[db_name][d41003_traces_col];
            create_expiration_index( d41003_traces, expire_after_seconds );

            mongocxx::collection d11001 = mongo_conn[db_name][d11001_col];
            create_expiration_index( d11001, expire_after_seconds );
            mongocxx::collection d11001_traces = mongo_conn[db_name][d11001_traces_col];
            create_expiration_index( d11001_traces, expire_after_seconds );

            mongocxx::collection d11002 = mongo_conn[db_name][d11002_col];
            create_expiration_index( d11002, expire_after_seconds );
            mongocxx::collection d11002_traces = mongo_conn[db_name][d11002_traces_col];
            create_expiration_index( d11002_traces, expire_after_seconds );

            mongocxx::collection d11003 = mongo_conn[db_name][d11003_col];
            create_expiration_index( d11003, expire_after_seconds );
            mongocxx::collection d11003_traces = mongo_conn[db_name][d11003_traces_col];
            create_expiration_index( d11003_traces, expire_after_seconds );

            mongocxx::collection d11004 = mongo_conn[db_name][d11004_col];
            create_expiration_index( d11004, expire_after_seconds );
            mongocxx::collection d11004_traces = mongo_conn[db_name][d11004_traces_col];
            create_expiration_index( d11004_traces, expire_after_seconds );

            mongocxx::collection d11005 = mongo_conn[db_name][d11005_col];
            create_expiration_index( d11005, expire_after_seconds );
            mongocxx::collection d11005_traces = mongo_conn[db_name][d11005_traces_col];
            create_expiration_index( d11005_traces, expire_after_seconds );

            mongocxx::collection d11006 = mongo_conn[db_name][d11006_col];
            create_expiration_index( d11006, expire_after_seconds );
            mongocxx::collection d11006_traces = mongo_conn[db_name][d11006_traces_col];
            create_expiration_index( d11006_traces, expire_after_seconds );

            mongocxx::collection d11007 = mongo_conn[db_name][d11007_col];
            create_expiration_index( d11007, expire_after_seconds );
            mongocxx::collection d11007_traces = mongo_conn[db_name][d11007_traces_col];
            create_expiration_index( d11007_traces, expire_after_seconds );

            mongocxx::collection d11008 = mongo_conn[db_name][d11008_col];
            create_expiration_index( d11008, expire_after_seconds );
            mongocxx::collection d11008_traces = mongo_conn[db_name][d11008_traces_col];
            create_expiration_index( d11008_traces, expire_after_seconds );

            mongocxx::collection d11009 = mongo_conn[db_name][d11009_col];
            create_expiration_index( d11009, expire_after_seconds );
            mongocxx::collection d11009_traces = mongo_conn[db_name][d11009_traces_col];
            create_expiration_index( d11009_traces, expire_after_seconds );

            mongocxx::collection d11010 = mongo_conn[db_name][d11010_col];
            create_expiration_index( d11010, expire_after_seconds );
            mongocxx::collection d11010_traces = mongo_conn[db_name][d11010_traces_col];
            create_expiration_index( d11010_traces, expire_after_seconds );

            mongocxx::collection d11011 = mongo_conn[db_name][d11011_col];
            create_expiration_index( d11011, expire_after_seconds );
            mongocxx::collection d11011_traces = mongo_conn[db_name][d11011_traces_col];
            create_expiration_index( d11011_traces, expire_after_seconds );

            mongocxx::collection d11012 = mongo_conn[db_name][d11012_col];
            create_expiration_index( d11012, expire_after_seconds );
            mongocxx::collection d11012_traces = mongo_conn[db_name][d11012_traces_col];
            create_expiration_index( d11012_traces, expire_after_seconds );

            

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
