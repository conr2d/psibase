#pragma once

#include <psibase/AccountNumber.hpp>

namespace psibase::net
{
   using producer_id                          = AccountNumber;
   using peer_id                              = int;
   static constexpr producer_id null_producer = {};
}  // namespace psibase::net
