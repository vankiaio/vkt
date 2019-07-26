#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/crypto.h>
#include <string>

namespace vankia
{

using namespace eosio;
using namespace std;

typedef uint8_t bool_positive;
typedef uint64_t time64;

#define BOOL_POSITIVE 1
#define BOOL_NAGETIVE 0
#define IPFS_HASH_LENGHT 46

struct account_record_content
{

    account_record_content() {}

    asset assets;
    account_name person_name;

    EOSLIB_SERIALIZE(account_record_content, (assets)(person_name))
};

struct account_record : public account_record_content
{

    account_record() {}

    account_record(account_name from_name, account_record_content content)
    {
        from = from_name;
        person_name = content.person_name;
        assets = content.assets;
    }
    account_name from;
    EOSLIB_SERIALIZE_DERIVED(account_record, account_record_content, (from))
};

struct withdraw_record_content
{

    withdraw_record_content() {}
    withdraw_record_content(asset con_assets, time64 con_record_time)
    {

        assets = con_assets;
        record_time = con_record_time;
    }
    asset assets;
    time64 record_time;
    EOSLIB_SERIALIZE(withdraw_record_content, (assets)(record_time))
};

class accounting : public eosio::contract
{

    using contract::contract;

  public:
    accounting(account_name self) : contract(self) {}
    //@abi action
    void deposit(account_name from, vector<account_record_content> content);
    //@abi action
    void withdraw(account_name from, asset assets);
    //@abi action
    inline asset getassets(account_name owner);
    //@abi action
    void listrecord(account_name from, uint64_t seq, string hash, vector<account_record_content> trans_list);
    //@abi action
    void authrightreg(const uint64_t id, const account_name agent, const string ipfsvalue, const string memo, const account_name producer);

  private:
    // @abi table
    struct accountlist
    {
        account_name to;
        asset assets;
        bool_positive flag;
        time64 account_time;

        account_name primary_key() const { return to; }
        EOSLIB_SERIALIZE(accountlist, (to)(assets)(flag)(account_time))
    };
    // @abi table
    struct depositlist
    {
        uint64_t seq_no; //交易序号
        account_name from;
        string hash;
        vector<account_record_content> list;
        time64 deposit_time;
        uint64_t primary_key() const { return seq_no; }
        EOSLIB_SERIALIZE(depositlist, (seq_no)(from)(hash)(list)(deposit_time))
    };
    // @abi table
    struct withdrawlist
    {
        account_name withdraw_name;
        vector<withdraw_record_content> withdraw_infor;
        uint64_t primary_key() const { return withdraw_name; }
        EOSLIB_SERIALIZE(withdrawlist, (withdraw_name)(withdraw_infor))
    };

    /// @abi table
    struct authrightlst
    {
        uint64_t pk_id;        /// 流水ID，主键唯一的无符号64位整型
        account_name agent;    /// 代理商名称
        checksum256 ipfskey;   /// IPFS对应的sha256转换后的key
        string ipfsvalue;      /// IPFS对应的sha256转换后的value
        string memo;           /// 备注
        account_name producer; /// 生产数据的公司
        time64 regdate;        /// 登记时间

        auto primary_key() const { return pk_id; }
        key256 get_ipfskey() const { return get_ipfskey(ipfskey); }

        static key256 get_ipfskey(const checksum256 &ipfskey)
        {
            const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&ipfskey);
            //print<32>(key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]));
            return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
        }

        EOSLIB_SERIALIZE(authrightlst, (pk_id)(agent)(ipfskey)(ipfsvalue)(memo)(producer)(regdate))
    };

    typedef eosio::multi_index<N(accountlist), accountlist> account_list;
    typedef eosio::multi_index<N(depositlist), depositlist> deposit_list;
    typedef eosio::multi_index<N(withdrawlist), withdrawlist> withdraw_list;
    typedef eosio::multi_index<N(authrightlst), authrightlst, indexed_by<N(byipfskey), const_mem_fun<authrightlst, key256, &authrightlst::get_ipfskey>>> authright_list;
};

} // namespace vankia
