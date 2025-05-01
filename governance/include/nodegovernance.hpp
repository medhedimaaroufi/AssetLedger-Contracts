#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;

class [[eosio::contract("nodegovernance")]] nodegovernance : public contract {
public:
    using contract::contract;

    [[eosio::action]] void checkvoting(name user, uint64_t current_time);
    [[eosio::action]] void canbevalid(name user);
    [[eosio::action]] void canbeprod(name user);
    [[eosio::action]] void claimrewards(name user);
    [[eosio::action]] void sponsor(name sponsor, name new_account, asset resource_cost);
    [[eosio::action]] void trackdaily(name node, uint64_t uptime_hours);

private:
    struct [[eosio::table]] voting_entry {
        name user;
        uint64_t timestamp;
        uint64_t primary_key() const { return user.value ^ timestamp; }
        EOSLIB_SERIALIZE(voting_entry, (user)(timestamp))
    };
    using voting_table = multi_index<"voting"_n, voting_entry>;

    struct [[eosio::table]] node_entry {
        name user;
        std::string node_type;
        uint64_t days_active;
        uint64_t voting_sessions;
        bool is_candidate; // Explicit Candidate status
        uint64_t primary_key() const { return user.value; }
        EOSLIB_SERIALIZE(node_entry, (user)(node_type)(days_active)(voting_sessions)(is_candidate))
    };
    using node_table = multi_index<"nodes"_n, node_entry>;

    struct [[eosio::table]] sponsor_entry {
        name sponsor;
        name new_account;
        asset resource_cost;
        asset reward;
        uint64_t primary_key() const { return sponsor.value ^ new_account.value; }
        uint64_t by_sponsor() const { return sponsor.value; }
        EOSLIB_SERIALIZE(sponsor_entry, (sponsor)(new_account)(resource_cost)(reward))
    };
    using sponsor_table = multi_index<"sponsors"_n, sponsor_entry,
        indexed_by<"bysponsor"_n, const_mem_fun<sponsor_entry, uint64_t, &sponsor_entry::by_sponsor>>>;

    struct [[eosio::table]] daily_activity {
        name node;
        uint64_t date; // Seconds since epoch, floored to day
        uint64_t hours_active;
        uint64_t primary_key() const { return node.value ^ date; }
        EOSLIB_SERIALIZE(daily_activity, (node)(date)(hours_active))
    };
    using daily_table = multi_index<"dailyact"_n, daily_activity>;
};