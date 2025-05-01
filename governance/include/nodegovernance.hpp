// nodegovernance.hpp
// Version: 1.1 (Updated for AssetLedger whitepaper requirements, May 2025)

#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;

class [[eosio::contract("nodegovernance")]] nodegovernance : public contract {
public:
    using contract::contract;

    // Action to record voting session participation for Validators
    [[eosio::action]]
    void checkvoting(name user, uint64_t current_time);

    // Action to check Validator eligibility
    [[eosio::action]]
    void canbevalid(name user);

    // Action to check Producer eligibility
    [[eosio::action]]
    void canbeprod(name user);

    // Action to claim sponsorship rewards
    [[eosio::action]]
    void claimrewards(name user);

    // Action to sponsor a new account
    [[eosio::action]]
    void sponsor(name sponsor, name new_account, asset resource_cost);

private:
    // Table to store voting participation
    struct [[eosio::table]] voting_entry {
        name user;
        uint64_t timestamp;

        uint64_t primary_key() const { return user.value ^ timestamp; }

        EOSLIB_SERIALIZE(voting_entry, (user)(timestamp))
    };
    using voting_table = multi_index<"voting"_n, voting_entry>;

    // Table to store node activity for eligibility tracking
    struct [[eosio::table]] node_entry {
        name user;
        std::string node_type;
        uint64_t days_active;
        uint64_t voting_sessions;

        uint64_t primary_key() const { return user.value; }

        EOSLIB_SERIALIZE(node_entry, (user)(node_type)(days_active)(voting_sessions))
    };
    using node_table = multi_index<"nodes"_n, node_entry>;

    // Table to store sponsorship rewards
    struct [[eosio::table]] sponsor_entry {
        name sponsor;
        name new_account;
        asset resource_cost;
        asset reward;

        uint64_t primary_key() const { return sponsor.value ^ new_account.value; }

        EOSLIB_SERIALIZE(sponsor_entry, (sponsor)(new_account)(resource_cost)(reward))
    };
    using sponsor_table = multi_index<"sponsors"_n, sponsor_entry>;
};