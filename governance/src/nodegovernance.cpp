#include "../include/nodegovernance.hpp"

void nodegovernance::reportactiv(name user, std::string node_type, uint64_t hours, uint64_t day_index) {
  require_auth(user);
  check(node_type == "ApiNode" || node_type == "SeedNode" || node_type == "FullNode" ||
        node_type == "ValidatorNode" || node_type == "ProducerNode", "Invalid node type");

  activity_table activities(get_self(), get_self().value);
  auto itr = activities.find(user.value);
  if (itr == activities.end()) {
    activities.emplace(user, [&](auto& row) {
      row.user = user;
      row.node_type = node_type;
      row.hours = hours;
      row.day_index = day_index;
    });
  } else {
    activities.modify(itr, user, [&](auto& row) {
      row.hours += hours;
      row.day_index = day_index;
    });
  }

  update_node(user, node_type, hours);
}

void nodegovernance::checkvoting(name user, uint64_t current_time) {
  require_auth(get_self());
  node_table nodes(get_self(), get_self().value);
  auto node_itr = nodes.find(user.value);
  check(node_itr != nodes.end() && node_itr->node_type == "ValidatorNode", "Not a validator");

  voting_table votes(get_self(), get_self().value);
  votes.emplace(get_self(), [&](auto& row) {
    row.user = user;
    row.timestamp = current_time;
  });

  nodes.modify(node_itr, get_self(), [&](auto& row) {
    row.voting_sessions += 1;
  });
}

bool nodegovernance::canbevalid(name user) {
  node_table nodes(get_self(), get_self().value);
  auto itr = nodes.find(user.value);
  if (itr == nodes.end()) return false;
  return itr->days_active >= 60 && itr->node_type != "ValidatorNode" && itr->node_type != "ProducerNode";
}

bool nodegovernance::canbeprod(name user) {
  node_table nodes(get_self(), get_self().value);
  auto itr = nodes.find(user.value);
  if (itr == nodes.end() || itr->node_type != "ValidatorNode") return false;
  return itr->voting_sessions >= 45;
}

void nodegovernance::claimrewards(name user) {
  require_auth(user);
  reward_table rewards(get_self(), get_self().value);
  auto itr = rewards.find(user.value);
  if (itr == rewards.end() || itr->pending_reward.amount == 0) return;

  action(
    permission_level{get_self(), "active"_n},
    "eosio.token"_n,
    "issue"_n,
    std::make_tuple(user, itr->pending_reward, std::string("Reward for node activity"))
  ).send();

  rewards.erase(itr);
}

void nodegovernance::sponsor(name sponsor, name new_account, asset resource_cost) {
  require_auth(sponsor);
  check(resource_cost.symbol == symbol("AXT", 4), "Invalid symbol");
  check(resource_cost.amount > 0, "Resource cost must be positive");

  sponsor_table sponsors(get_self(), get_self().value);
  sponsors.emplace(sponsor, [&](auto& row) {
    row.sponsor = sponsor;
    row.new_account = new_account;
    row.resource_cost = resource_cost;
    row.reward = asset(resource_cost.amount * 1.1, resource_cost.symbol); 
  });

  action(
    permission_level{get_self(), "active"_n},
    "eosio.token"_n,
    "issue"_n,
    std::make_tuple(sponsor, asset(resource_cost.amount * 1.1, resource_cost.symbol), std::string("Sponsor reward"))
  ).send();
}

void nodegovernance::distribute() {
  require_auth(get_self());

  uint64_t current_time = current_time_point().sec_since_epoch();
  static uint64_t start_time = 0;
  if (start_time == 0) start_time = current_time;
  bool is_initial_phase = (current_time - start_time) <= 600;

  if (is_initial_phase) {
    
    asset total_axt = asset(60000 * 10000, symbol("AXT", 4)); 
    asset bp_reward = asset(total_axt.amount * 0.6, total_axt.symbol); 
    asset network_reward = asset(total_axt.amount * 0.4, total_axt.symbol); 

    action(
      permission_level{get_self(), "active"_n},
      "eosio.token"_n,
      "issue"_n,
      std::make_tuple("eosio"_n, bp_reward, std::string("Initial BP reward"))
    ).send();

    activity_table activities(get_self(), get_self().value);
    uint64_t total_hours = 0;
    for (const auto& activity : activities) {
      total_hours += activity.hours;
    }

    for (const auto& activity : activities) {
      if (total_hours > 0) {
        asset node_reward = asset((network_reward.amount * activity.hours) / total_hours, network_reward.symbol);
        action(
          permission_level{get_self(), "active"_n},
          "eosio.token"_n,
          "issue"_n,
          std::make_tuple(activity.user, node_reward, std::string("Initial network reward"))
        ).send();
      }
    }
  } else {
    // Post-10 minutes: Per-block rewards (simplified for now)
    // Requires integration with eosio.system for producer rankings
  }
}

void nodegovernance::update_node(name user, std::string node_type, uint64_t hours) {
  node_table nodes(get_self(), get_self().value);
  auto itr = nodes.find(user.value);
  if (itr == nodes.end()) {
    nodes.emplace(get_self(), [&](auto& row) {
      row.user = user;
      row.node_type = node_type;
      row.days_active = (hours >= 5) ? 1 : 0;
      row.voting_sessions = 0;
    });
  } else {
    nodes.modify(itr, get_self(), [&](auto& row) {
      if (hours >= 5) row.days_active += 1;
    });
  }

  if (node_type != "ProducerNode") {
    reward_table rewards(get_self(), get_self().value);
    auto reward_itr = rewards.find(user.value);
    asset reward = asset(hours * 10000, symbol("AXT", 4));
    if (reward_itr == rewards.end()) {
      rewards.emplace(get_self(), [&](auto& row) {
        row.user = user;
        row.pending_reward = reward;
      });
    } else {
      rewards.modify(reward_itr, get_self(), [&](auto& row) {
        row.pending_reward += reward;
      });
    }
  }
}