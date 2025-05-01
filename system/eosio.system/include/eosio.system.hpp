// eosio.system.hpp
// Version: 1.2 (Updated for AssetLedger whitepaper requirements, May 2025)

#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace eosio;
using namespace std; // For std::vector, std::sort

class [[eosio::contract("eosio.system")]] system_contract : public contract {
public:
    using contract::contract;

    // Actions
    [[eosio::action]] void init(uint64_t version, symbol core);
    [[eosio::action]] void regproducer(name producer, public_key producer_key);
    [[eosio::action]] void voteproducer(name voter, vector<name> producers);
    [[eosio::action]] void listprods();
    [[eosio::action]] void claimrewards(name producer);
    [[eosio::action]] void delegatebw(name from, name receiver, asset stake_net_quantity, asset stake_cpu_quantity);
    [[eosio::action]] void regnode(name node, string node_type); // Register API, Seed, Full, Validator nodes
    [[eosio::action]] void reportactive(name node, uint64_t uptime_hours); // Report uptime for active node rewards
    [[eosio::on_notify("eosio::onblock")]] void onblock(block_timestamp timestamp, name producer);

private:
    // Tables
    struct [[eosio::table]] producer_info {
        name owner;
        double total_votes = 0;
        eosio::public_key producer_key;
        bool is_active = true;
        uint64_t last_reward_claim = 0;
        asset pending_rewards = asset(0, symbol("AXT", 4));

        uint64_t primary_key() const { return owner.value; }

        EOSLIB_SERIALIZE(producer_info, (owner)(total_votes)(producer_key)(is_active)(last_reward_claim)(pending_rewards))
    };
    using producer_table = multi_index<"producers"_n, producer_info>;

    struct [[eosio::table]] voter_info {
        name owner;
        vector<name> producers;
        asset staked = asset(0, symbol("AXT", 4));
        uint64_t last_vote_time = 0;
        asset pending_voter_rewards = asset(0, symbol("AXT", 4));

        uint64_t primary_key() const { return owner.value; }

        EOSLIB_SERIALIZE(voter_info, (owner)(producers)(staked)(last_vote_time)(pending_voter_rewards))
    };
    using voter_table = multi_index<"voters"_n, voter_info>;

    struct [[eosio::table]] node_info {
        name node;
        string node_type; // "API", "Seed", "Full", "Validator", "BP"
        uint64_t uptime_hours = 0; // Total uptime reported
        asset pending_node_rewards = asset(0, symbol("AXT", 4));
        bool is_active = false;

        uint64_t primary_key() const { return node.value; }

        EOSLIB_SERIALIZE(node_info, (node)(node_type)(uptime_hours)(pending_node_rewards)(is_active))
    };
    using node_table = multi_index<"nodes"_n, node_info>;

    struct [[eosio::table]] global_state {
        uint64_t version = 0;
        symbol core_symbol;
        uint64_t chain_start_time = 0; // Time of chain initialization
        bool initial_phase = true; // First 10 minutes
        uint64_t last_schedule_update = 0;
        asset initial_phase_rewards = asset(0, symbol("AXT", 4)); // Accumulate rewards in first 10 minutes

        uint64_t primary_key() const { return 0; }

        EOSLIB_SERIALIZE(global_state, (version)(core_symbol)(chain_start_time)(initial_phase)(last_schedule_update)(initial_phase_rewards))
    };
    using global_table = multi_index<"global"_n, global_state>;

    // Helper functions
    void schedule_producers();
    void distribute_rewards(name producer);
    void distribute_initial_rewards();
    void update_voter_rewards();
    void distribute_node_rewards();
};