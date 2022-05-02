#include <contracts/system/auth_fake_sys.hpp>
#include <contracts/system/transaction_sys.hpp>
#include <psibase/dispatch.hpp>

#include <psibase/crypto.hpp>
#include <psibase/native_tables.hpp>
#include <psibase/print.hpp>

using namespace psibase;

static constexpr bool enable_print = false;

static const auto& getStatus()
{
   static const auto status = kv_get<status_row>(status_row::kv_map, status_key());
   return status;
}

namespace system_contract
{
   psibase::BlockNum transaction_sys::headBlockNum() const
   {
      auto& stat = getStatus();
      if (stat && stat->head)
         return stat->head->header.blockNum;
      return 1;  // next block (currently being produced) is 2 (genesis)
   }
   psibase::TimePointSec transaction_sys::blockTime() const
   {
      auto& stat = getStatus();
      if (stat && stat->head)
         return stat->head->header.time;
      return {};
   }

   // TODO: move to another contract
   uint8_t transaction_sys::setCode(AccountNumber     contract,
                                    uint8_t           vmType,
                                    uint8_t           vmVersion,
                                    std::vector<char> code)
   {
      // TODO: validate code
      check(get_sender() == contract, "sender must match contract account");
      auto account = kv_get<account_row>(account_row::kv_map, account_key(contract));
      check(account.has_value(), "can not set code on a missing account");
      auto code_hash = sha256(code.data(), code.size());
      if (vmType == account->vmType && vmVersion == account->vmVersion &&
          code_hash == account->code_hash)
         return 0;
      if (account->code_hash != Checksum256{})
      {
         // TODO: Refund RAM? A different resource?
         auto code_obj = kv_get<code_row>(
             code_row::kv_map, code_key(account->code_hash, account->vmType, account->vmVersion));
         check(code_obj.has_value(), "missing code object");
         if (--code_obj->ref_count)
            kv_put(code_obj->kv_map, code_obj->key(), *code_obj);
         else
         {
            // TODO: erase code_obj
         }
      }

      // TODO: Bill RAM? A different resource?
      account->code_hash = code_hash;
      account->vmType    = vmType;
      account->vmVersion = vmVersion;
      kv_put(account->kv_map, account->key(), *account);

      auto code_obj = kv_get<code_row>(
          code_row::kv_map, code_key(account->code_hash, account->vmType, account->vmVersion));
      if (!code_obj)
      {
         code_obj.emplace(code_row{
             .code_hash = account->code_hash,
             .vmType    = account->vmType,
             .vmVersion = account->vmVersion,
         });
         code_obj->code.assign(code.begin(), code.end());
      }
      ++code_obj->ref_count;
      kv_put(code_obj->kv_map, code_obj->key(), *code_obj);

      return 0;
   }  // set_code

   // hard coded to account 1, which deploys transaction_sys (or avariant of) and native code
   // knows to call this method
   extern "C" [[clang::export_name("process_transaction")]] void process_transaction()
   {
      if (enable_print)
         print("process_transaction\n");

      // TODO: expiration
      // TODO: check refBlockNum, refBlockPrefix
      // TODO: check max_net_usage_words, max_cpu_usage_ms
      // TODO: resource billing
      // TODO: subjective mitigation hooks
      // TODO: limit execution time
      auto top_act = get_current_action();
      // TODO: avoid copying inner rawData during unpack
      auto trx = psio::convert_from_frac<Transaction>(top_act.rawData);

      check(trx.actions.size() > 0, "transaction has no actions");

      if (const auto& stat = getStatus())
      {
         check(stat->head->header.time <= trx.tapos.expiration, "transaction has expired");
      }

      for (auto& act : trx.actions)
      {
         auto account = kv_get<account_row>(account_row::kv_map, account_key(act.sender));
         if (!account)
            abort_message_str("unknown sender \"" + act.sender.str() + "\"");

         // actor<system_contract::auth_fake_sys> auth(system_contract::transaction_sys::contract,
         //                                            account->authContract);
         // auth->authCheck()( act, trx.claims );

         // TODO: assumes same dispatch format (abi) as auth_fake_sys
         // TODO: avoid inner rawData copy
         // TODO: authContract needs a way to opt-in to being an auth contract.
         //       Otherwise, it may misunderstand the action, and worse,
         //       sender = 1, which provides transaction.sys's authorization
         //       for a potentially-unknown action.
         psibase::Action outer = {
             .sender   = system_contract::transaction_sys::contract,
             .contract = account->authContract,
             // TODO: authContract will have to register action #
             // TODO: rename to checkauth_sys or sys_checkauth
             // TODO: checkauth return bool
             .rawData = psio::convert_to_frac(auth_fake_sys::action{auth_fake_sys::auth_check{
                 .action = act,  // act to be authorized
                 .claims = trx.claims,
             }}),
         };
         if (enable_print)
            print("call auth_check\n");
         call(outer);  // TODO: avoid copy (serializing outer)
         if (enable_print)
            print("call action\n");
         call(act);  // TODO: avoid copy (serializing)
      }
   }

}  // namespace system_contract

PSIBASE_DISPATCH(system_contract::transaction_sys)
