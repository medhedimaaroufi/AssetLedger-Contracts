// eosio.token.hpp
// Version: 1.1 (Updated for AssetLedger whitepaper requirements, May 2025)

#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <string> // For std::string

using namespace eosio;

class [[eosio::contract("eosio.token")]] token : public contract {
public:
    using contract::contract;

    // Action to create a new token with a maximum supply
    [[eosio::action]]
    void create(name issuer, asset maximum_supply);

    // Action to issue tokens to an account
    [[eosio::action]]
    void issue(name to, asset quantity, std::string memo);

    // Action to transfer tokens between accounts
    [[eosio::action]]
    void transfer(name from, name to, asset quantity, std::string memo);

private:
    // Table to store account balances
    struct [[eosio::table]] account {
        asset balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }

        EOSLIB_SERIALIZE(account, (balance))
    };
    using accounts = multi_index<"accounts"_n, account>;

    // Table to store token statistics (supply, max supply, issuer)
    struct [[eosio::table]] currency_stats {
        asset supply;
        asset max_supply;
        name issuer;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }

        EOSLIB_SERIALIZE(currency_stats, (supply)(max_supply)(issuer))
    };
    using stats = multi_index<"stats"_n, currency_stats>;

    // Helper function to add tokens to an account's balance
    void add_balance(name owner, asset value, name ram_payer);

    // Helper function to subtract tokens from an account's balance
    void sub_balance(name owner, asset value);
};