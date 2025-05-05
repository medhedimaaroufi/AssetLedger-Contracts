#include "../include/nodegovern.hpp"

void nodegovernance::checkvoting(name user, uint64_t current_time) {
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
        if (row.voting_sessions >= 45 && row.node_type == "Validator") {
            row.is_candidate = true; // Mark as Candidate
        }
    });
}

void nodegovernance::canbevalid(name user) {
    require_auth(user);

    node_table nodes(get_self(), get_self().value);
    auto itr = nodes.find(user.value);
    check(itr != nodes.end(), "Node not registered: " + user.to_string());

    daily_table daily(get_self(), get_self().value);
    uint64_t active_days = 0;
    auto idx = daily.get_index<"bydaily"_n>();
    for (auto d_itr = idx.begin(); d_itr != idx.end(); ++d_itr) {
        if (d_itr->node == user && d_itr->hours_active >= 5) {
            active_days++;
        }
    }

    bool eligible = active_days >= 60 && itr->node_type != "Validator" && itr->node_type != "Producer";
    print("User ", user, " can become a Validator: ", eligible ? "true" : "false", "\n");
}

void nodegovernance::canbeprod(name user) {
    require_auth(user);

    node_table nodes(get_self(), get_self().value);
    auto itr = nodes.find(user.value);
    check(itr != nodes.end(), "Node not registered: " + user.to_string());

    bool eligible = itr->node_type == "Validator" && itr->is_candidate;
    print("User ", user, " can become a Producer: ", eligible ? "true" : "false", "\n");
}

void nodegovernance::claimrewards(name user) {
    require_auth(user);

    sponsor_table sponsors(get_self(), get_self().value);
    auto idx = sponsors.get_index<"bysponsor"_n>();
    asset total_reward = asset(0, symbol("AXT", 4));

    auto itr = idx.lower_bound(user.value);
    while (itr != idx.end() && itr->sponsor == user) {
        total_reward += itr->reward;
        itr = idx.erase(itr);
    }

    if (total_reward.amount == 0) {
        print("No sponsorship rewards to claim for ", user, "\n");
        return;
    }

    action(
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "issue"_n,
        std::make_tuple(user, total_reward, std::string("Sponsorship reward"))
    ).send();

    require_recipient(user);
}

void nodegovernance::sponsor(name sponsor, name new_account, asset resource_cost) {
    require_auth(sponsor);

    check(is_account(sponsor), "Sponsor account does not exist: " + sponsor.to_string());
    check(is_account(new_account), "New account does not exist: " + new_account.to_string());
    check(sponsor != new_account, "Cannot sponsor self");
    check(resource_cost.symbol == symbol("AXT", 4), "Resource cost must be in AXT");
    check(resource_cost.amount > 0, "Resource cost must be positive");

    action(
        permission_level{sponsor, "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(sponsor, "eosio.system"_n, resource_cost, std::string("Stake for new account"))
    ).send();

    asset half = asset(resource_cost.amount / 2, resource_cost.symbol);
    action(
        permission_level{get_self(), "active"_n},
        "eosio.system"_n,
        "delegatebw"_n,
        std::make_tuple(get_self(), new_account, half, resource_cost - half)
    ).send();

    asset reward = asset(resource_cost.amount * 1.1, resource_cost.symbol);
    sponsor_table sponsors(get_self(), get_self().value);
    sponsors.emplace(sponsor, [&](auto& row) {
        row.sponsor = sponsor;
        row.new_account = new_account;
        row.resource_cost = resource_cost;
        row.reward = reward;
    });

    require_recipient(sponsor);
    require_recipient(new_account);
}

void nodegovernance::trackdaily(name node, uint64_t uptime_hours) {
    require_auth("eosio.system"_n);

    check(uptime_hours > 0, "Uptime hours must be positive");
    check(is_account(node), "Node account does not exist: " + node.to_string());

    uint64_t now = current_time_point().sec_since_epoch();
    uint64_t today = now - (now % (24 * 3600)); // Floor to start of day

    daily_table daily(get_self(), get_self().value);
    auto idx = daily.get_index<"bydaily"_n>();
    auto itr = idx.find(node.value ^ today);

    if (itr == idx.end()) {
        daily.emplace(get_self(), [&](auto& row) {
            row.node = node;
            row.date = today;
            row.hours_active = uptime_hours;
        });
    } else {
        idx.modify(itr, same_payer, [&](auto& row) {
            row.hours_active += uptime_hours;
        });
    }

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(node.value);
    check(node_itr != nodes.end(), "Node not registered: " + node.to_string());

    if (itr != idx.end() && itr->hours_active >= 5) {
        nodes.modify(node_itr, same_payer, [&](auto& row) {
            row.days_active += 1;
        });
    }
}