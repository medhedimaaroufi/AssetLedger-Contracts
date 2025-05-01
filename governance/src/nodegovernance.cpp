// nodegovernance.cpp
// Version: 1.2 (Updated for AssetLedger whitepaper requirements, May 2025)

#include "nodegovernance.hpp"

void nodegovernance::checkvoting(name user, uint64_t current_time) {
    // Only callable by eosio.system (e.g., during voteproducer)
    require_auth("eosio.system"_n);

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(user.value);
    check(node_itr != nodes.end(), "Node not registered: " + user.to_string());
    check(node_itr->node_type == "Validator", "User is not a Validator: " + user.to_string());

    voting_table votes(get_self(), get_self().value);
    votes.emplace(get_self(), [&](auto& row) {
        row.user = user;
        row.timestamp = current_time;
    });

    nodes.modify(node_itr, same_payer, [&](auto& row) {
        row.voting_sessions += 1;
    });
}

void nodegovernance::canbevalid(name user) {
    require_auth(user);

    node_table nodes(get_self(), get_self().value);
    auto itr = nodes.find(user.value);
    bool eligible = itr != nodes.end() && itr->days_active >= 60 &&
                    itr->node_type != "Validator" && itr->node_type != "Producer";
    print("User ", user, " can become a Validator: ", eligible ? "true" : "false", "\n");
}

void nodegovernance::canbeprod(name user) {
    require_auth(user);

    node_table nodes(get_self(), get_self().value);
    auto itr = nodes.find(user.value);
    bool eligible = itr != nodes.end() && itr->node_type == "Validator" && itr->voting_sessions >= 45;
    print("User ", user, " can become a Producer: ", eligible ? "true" : "false", "\n");
}

void nodegovernance::claimrewards(name user) {
    require_auth(user);

    sponsor_table sponsors(get_self(), get_self().value);
    auto idx = sponsors.get_index<"byaccount"_n>(); // Note: Requires a secondary index
    auto itr = idx.find(user.value);
    if (itr == idx.end() || itr->reward.amount == 0) {
        print("No sponsorship rewards to claim for ", user, "\n");
        return;
    }

    action(
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "issue"_n,
        std::make_tuple(user, itr->reward, std::string("Sponsorship reward"))
    ).send();

    sponsors.erase(itr);

    // Notify the user
    require_recipient(user);
}

void nodegovernance::sponsor(name sponsor, name new_account, asset resource_cost) {
    require_auth(sponsor);

    check(is_account(sponsor), "Sponsor account does not exist: " + sponsor.to_string());
    check(is_account(new_account), "New account does not exist: " + new_account.to_string());
    check(sponsor != new_account, "Cannot sponsor self");
    check(resource_cost.symbol == symbol("AXT", 4), "Resource cost must be in AXT");
    check(resource_cost.amount > 0, "Resource cost must be positive");

    // Transfer AXT to eosio.system for staking (CPU/Net)
    action(
        permission_level{sponsor, "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(sponsor, "eosio.system"_n, resource_cost, std::string("Stake for new account"))
    ).send();

    // Delegate bandwidth to the new account (split evenly between CPU and Net)
    asset half = asset(resource_cost.amount / 2, resource_cost.symbol);
    action(
        permission_level{get_self(), "active"_n},
        "eosio.system"_n,
        "delegatebw"_n,
        std::make_tuple(get_self(), new_account, half, resource_cost - half)
    ).send();

    // Calculate and record sponsorship reward (staked amount + 10% incentive)
    asset reward = asset(resource_cost.amount * 1.1, resource_cost.symbol);
    sponsor_table sponsors(get_self(), get_self().value);
    sponsors.emplace(sponsor, [&](auto& row) {
        row.sponsor = sponsor;
        row.new_account = new_account;
        row.resource_cost = resource_cost;
        row.reward = reward;
    });

    // Notify both parties
    require_recipient(sponsor);
    require_recipient(new_account);
}