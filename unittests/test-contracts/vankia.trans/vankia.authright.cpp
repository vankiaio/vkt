namespace vankia
{

// using authright::authright_table;
/// @abi action
void accounting::authrightreg(const uint64_t id, const account_name agent, const string ipfsvalue, const string memo, const account_name producer)
{
  checksum256 cksum256ipfskey;
  //获取授权，如果没有授权，Action调用会中止，事务会回滚
  require_auth(agent);

  //实例化address数据表（multi_index），参数用于建立对表的访问权限
  //如果访问自己的合约则具有读写权限，访问其他人的合约则具有只读权限
  authright_list authrighttbl(_self, _self);

  //multi_index的find函数通过主键（primary_key）查询数据，返回迭代器itr
  //auto关键字会自动匹配类型
  auto itr = authrighttbl.find(id);
  //如果判断条件不成立，则终止执行并打印错误信息
  eosio_assert(itr == authrighttbl.end(), "primary key id for account already exists");

  //由于IPFS生成的HASH值为46bytes，为了增加索引便于查找，调用sha256转换。
  sha256(ipfsvalue.c_str(), IPFS_HASH_LENGHT, &cksum256ipfskey);

  auto ipfskeyidx = authrighttbl.get_index<N(byipfskey)>();
  auto itr2 = ipfskeyidx.find(authrightlst::get_ipfskey(cksum256ipfskey));
  //如果判断条件不成立，则终止执行并打印错误信息
  eosio_assert(itr2 == ipfskeyidx.end(), "ipfs sha256 for account already exists");

  authrighttbl.emplace(_self, [&](auto &ar) {
    ar.pk_id = id;
    ar.agent = agent;
    ar.ipfskey = cksum256ipfskey;
    ar.ipfsvalue = ipfsvalue;
    ar.memo = memo;
    ar.producer = producer;
    //获取当前时间
    ar.regdate = current_time();
  });
}

} // namespace vankia

// EOSIO_ABI(vankia::authright, (reg))
