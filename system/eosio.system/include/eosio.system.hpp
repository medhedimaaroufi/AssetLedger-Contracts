#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/crypto.hpp>
#include <eosio/serialize.hpp>

using namespace eosio;

class [[eosio::contract("eosio.system")]] system_contract : public contract {
public:
    using contract::contract;

    // Actions
    [[eosio::action]] void init(uint64_t version, symbol core);
    [[eosio::action]] void regproducer(name producer, public_key producer_key);
    [[eosio::action]] void voteproducer(name voter, std::vector<name> producers);
    [[eosio::action]] void listprods();
    [[eosio::action]] void claimrewards(name producer);
    [[eosio::action]] void delegatebw(name from, name receiver, asset stake_net_quantity, asset stake_cpu_quantity);
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
        std::vector<name> producers;
        asset staked = asset(0, symbol("AXT", 4));
        uint64_t last_vote_time = 0;
        asset pending_voter_rewards = asset(0, symbol("AXT", 4));

        uint64_t primary_key() const { return owner.value; }

        EOSLIB_SERIALIZE(voter_info, (owner)(producers)(staked)(last_vote_time)(pending_voter_rewards))
    };
    using voter_table = multi_index<"voters"_n, voter_info>;

    struct [[eosio::table]] global_state {
        uint64_t version = 0;
        symbol core_symbol;
        uint64_t last_schedule_update = 0;

        uint64_t primary_key() const { return 0; }

        EOSLIB_SERIALIZE(global_state, (version)(core_symbol)(last_schedule_update))
    };
    using global_table = multi_index<"global"_n, global_state>;

    // Helper functions
    void schedule_producers();
    void distribute_rewards(name producer);
    void update_voter_rewards();
};