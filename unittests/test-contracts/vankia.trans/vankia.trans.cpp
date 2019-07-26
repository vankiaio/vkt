#include <eosio.token/eosio.token.hpp>
#include "vankia.trans.hpp"
#include "vankia.authright.cpp"
namespace vankia
{
using namespace eosio;
using namespace std;
void accounting::deposit(account_name from, vector<account_record_content> content)
{
    require_auth(from);

    vector<account_record> acc_record;

    for (auto content_itr = content.begin(); content_itr != content.end(); ++content_itr)
    {
        account_record i_proposal = account_record(from, *content_itr);
        acc_record.push_back(i_proposal);
    }
    
    // check the asset of every accout,check the asset veroflow
    asset temp_asset(0,S(4, VKT));
    for (auto record_itr = acc_record.begin(); record_itr != acc_record.end(); ++record_itr)
    {
       eosio_assert(symbol_type(S(4, VKT)).name() == record_itr->assets.symbol.name(), "Deposit asset symbol error");
       temp_asset += record_itr->assets;
       eosio_assert(record_itr->assets.is_valid(),"Deposit asset error");
       eosio_assert(temp_asset.is_valid(),"Deposit asset error");
       eosio_assert(from!=record_itr->person_name,"Deposit cannot to self");
    }

    // check the asset of from is enough,check the asset veroflow
    asset from_banlance  = token( N(eosio.token)).get_balance(from,symbol_type(S(4, VKT)).name());
    eosio_assert(from_banlance.is_valid(),"Deposit asset error");
    eosio_assert(from_banlance>=temp_asset,"Deposit balance of from account is not enough");

    for (auto record_itr = acc_record.begin(); record_itr != acc_record.end(); ++record_itr)
    {
        //根据record_itr更新table
        account_list list(_self, record_itr->person_name);
        auto itr = list.find(record_itr->person_name);
        if (itr == list.end())
        {
            itr = list.emplace(from, [&](auto &tmp_record) {
                tmp_record.to = record_itr->person_name;
                tmp_record.assets = record_itr->assets;
                tmp_record.flag = BOOL_POSITIVE;
                tmp_record.account_time = current_time();
            });
        }
        else
        {
            list.modify(itr, 0, [&](auto &tmp_record) {
                eosio_assert(BOOL_POSITIVE == itr->flag || BOOL_NAGETIVE == itr->flag, "invalid bool_positive");
                eosio_assert(symbol_type(S(4, VKT)).name() == itr->assets.symbol.name(), "Deposit asset symbol error");
                eosio_assert(record_itr->assets.symbol.name() == itr->assets.symbol.name(), "invalid bool_positive");
                tmp_record.account_time = current_time();
                if (BOOL_POSITIVE == itr->flag)
                {
                    eosio_assert(tmp_record.assets < (tmp_record.assets + record_itr->assets), "Deposit asset overflow");
                    tmp_record.assets += record_itr->assets;
                }
                else
                {
                    tmp_record.flag = tmp_record.assets < record_itr->assets ? BOOL_POSITIVE : BOOL_NAGETIVE;
                    tmp_record.assets = tmp_record.assets < record_itr->assets ? (record_itr->assets - tmp_record.assets) : (tmp_record.assets - record_itr->assets);
                }
            });
        }
        INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {from,N(active)},
         { from, record_itr->person_name, record_itr->assets, std::string("vankia.trans deposit") } );
    }
    return;
}
void accounting::withdraw(account_name from, asset assets)
{
    require_auth(from);
    account_list list(_self, from);
    withdraw_list list_withdraw(_self, from);
    auto itr = list.find(from);
    eosio_assert(itr != list.end(), "withdraw account doesn't exist");
    eosio_assert(assets < itr->assets, "withdraw balance  doesn't enough");
    eosio_assert(BOOL_POSITIVE == itr->flag, "withdraw balance  doesn't enough");
    eosio_assert(assets.symbol.value == itr->assets.symbol.value, "withdraw currency not match");
    list.modify(itr, 0, [&](auto &tmp_record) {
        tmp_record.assets -= assets;
        tmp_record.account_time = current_time();
    });
    auto iterator = list_withdraw.find(from);
    if (iterator == list_withdraw.end())
    {
        iterator = list_withdraw.emplace(from, [&](auto &tmp_record) {
            tmp_record.withdraw_name = from;
            tmp_record.withdraw_infor.push_back(withdraw_record_content(assets, current_time()));
        });
    }
    else
    {
        list_withdraw.modify(iterator, from, [&](auto &tmp_record) {
            tmp_record.withdraw_infor.push_back(withdraw_record_content(assets, current_time()));
        });
    }
    return;
}

#if 0
void accounting::get_assets(account_name owner)
{
  ;
}
#else
asset accounting::getassets(account_name owner)
{
    account_list list(_self, owner);
    return list.get(owner).assets;
}
#endif

#if 1
void accounting::listrecord(account_name from, uint64_t seq, string hash, vector<account_record_content> trans_list)
{
    require_auth(from);
    deposit_list list(_self, _self);

    //查找该序列号对应的交易报是否已经存在无需保存
    auto itr = list.find(seq);
    eosio_assert(itr == list.end(), "transaction has exist");

    itr = list.emplace(from, [&](auto &translist) {
        translist.from = from;
        translist.seq_no = seq;
        translist.hash = hash;
        translist.list = move(trans_list);
        translist.deposit_time = current_time();
    });
    return;
}
#else
void accounting::listrecord(account_name from, uint64_t seq, vector<account_record_content> transList)
{
    require_auth(from);
    deposit_list trans_records(_self, _self);

    //查找该序列号对应的交易报是否已经存在无需保存
    auto itr = trans_records.find(seq);
    eosio_assert(itr == trans_records.end(), "transaction has exist");

    itr = trans_records.emplace(from, [&](auto &translist) {
        translist.from = from;
        translist.seq_no = seq;
        // translist.hash         = hash;
        translist.list = move(transList);
        translist.records_time = current_time();
    });
    return;
}
#endif
} // namespace vankia

EOSIO_ABI(vankia::accounting, (deposit)(withdraw)(listrecord)(authrightreg))
