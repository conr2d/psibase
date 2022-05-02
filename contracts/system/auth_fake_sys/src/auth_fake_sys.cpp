#include <contracts/system/auth_fake_sys.hpp>

#include <psibase/crypto.hpp>
#include <psibase/native_tables.hpp>
#include <psibase/print.hpp>

using namespace psibase;

static constexpr bool enable_print = false;

namespace system_contract::auth_fake_sys
{
   void exec(AccountNumber this_contract, AccountNumber sender, auth_check& act)
   {
      // TODO: avoid copying inner raw_data (occurs in "called()" dispatcher below)
      if (enable_print)
         print("auth_check\n");
   }

   extern "C" void called(AccountNumber this_contract, AccountNumber sender)
   {
      // printf("called this_contract=%d, sender=%d\n", this_contract, sender);
      auto act  = get_current_action();
      auto data = psio::convert_from_frac<action>(act.raw_data);
      std::visit(
          [&](auto& x)
          {
             if constexpr (std::is_same_v<decltype(exec(this_contract, sender, x)), void>)
                exec(this_contract, sender, x);
             else
                set_retval(exec(this_contract, sender, x));
          },
          data);
   }

   extern "C" void __wasm_call_ctors();
   extern "C" void start(AccountNumber this_contract)
   {
      __wasm_call_ctors();
   }
}  // namespace system_contract::auth_fake_sys
