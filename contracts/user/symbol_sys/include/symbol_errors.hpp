#pragma once

#include <string_view>

namespace UserContract
{
   namespace Errors
   {
      constexpr std::string_view symbolDNE              = "Symbol does not exist";
      constexpr std::string_view symbolAlreadyExists    = "Symbol already exists";
      constexpr std::string_view symbolUnavailable      = "Symbol unavailable";
      constexpr std::string_view missingTokenIssuerAuth = "Missing token issuer auth";
      constexpr std::string_view priceTooLow            = "Symbol price below minimum";
      constexpr std::string_view creditSymbolRequired =
          "Symbol must be credited to symbol contract";
      constexpr std::string_view buyerIsSeller     = "Buyer cannot be seller";
      constexpr std::string_view insufficientFunds = "Insufficient funds to complete purchase";
      constexpr std::string_view invalidSymbol =
          "Symbol may only contain 3 to 7 uppercase alphabetic characters";
      constexpr std::string_view uninit = "Contract not properly initialized";
   }  // namespace Errors
}  // namespace UserContract