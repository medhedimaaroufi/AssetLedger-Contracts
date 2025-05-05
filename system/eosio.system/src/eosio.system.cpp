#include "eosio.system.hpp"

void system_contract::init(uint64_t version, symbol core) {
    require_auth(get_self());

    global_table global(get_self(), get_self().value);
    check(global.begin() == global.end(), "System already initialized");

    check(core == symbol("AXT", 4), "Core symbol must be AXT with 4 decimal places");

    global.emplace(get_self(), [&](auto& g) {
        g.version = version;
        g.core_symbol = core;
        g.chain_start_time = current_time_point().sec_since_epoch();
        g.initial_phase = true;
        g.last_schedule_update = g.chain_start_time;
        g.last_node_reward_time = g.chain_start_time;
        g.treasury = asset(1000000000000, core); // Initialize treasury with 100,000 AXT
    });

    // Note: Zero-cost transactions are achieved via sponsor-funded resources (CPU, Net) for new accounts,
    // with optional subsidies configurable off-chain.
}

void system_contract::regproducer(name producer, public_key producer_key) {
    require_auth(producer);

    producer_table producers(get_self(), get_self().value);
    auto itr = producers.find(producer.value);
    if (itr == producers.end()) {
        producers.emplace(producer, [&](auto& p) {
            p.owner = producer;
            p.producer_key = producer_key;
        });
    } else {
        producers.modify(itr, producer, [&](auto& p) {
            p.producer_key = producer_key;
            p.is_active = true;
        });
    }

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(producer.value);
    if (node_itr == nodes.end()) {
        nodes.emplace(producer, [&](auto& n) {
            n.node = producer;
            n.node_type = "BP";
            n.is_active = true;
        });
    } else {
        nodes.modify(node_itr, same_payer, [&](auto& n) {
            n.node_type = "BP";
            n.is_active = true;
        });
    }

    // During initial phase, update producer schedule dynamically
    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    if (g != global.end() && g->initial_phase) {
        schedule_producers();
    }
}

void system_contract::voteproducer(name voter, vector<name> producers) {
    require_auth(voter);
    check(producers.size() <= 20, "Cannot vote for more than 20 producers");

    voter_table voters(get_self(), get_self().value);
    auto voter_itr = voters.find(voter.value);
    check(voter_itr != voters.end(), "Voter not found; must delegate bandwidth first");
    check(voter_itr->staked.amount > 0, "No staked AXT for voting");

    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");
    check(!g->initial_phase, "Voting is disabled during initial 10-minute phase");

    producer_table prod_table(get_self(), get_self().value);
    for (const auto& old_prod : voter_itr->producers) {
        auto prod_itr = prod_table.find(old_prod.value);
        if (prod_itr != prod_table.end()) {
            prod_table.modify(prod_itr, same_payer, [&](auto& p) {
                p.total_votes -= voter_itr->staked.amount / 10000.0;
                if (p.total_votes < 0) p.total_votes = 0;
            });
        }
    }

    for (const auto& prod : producers) {
        auto prod_itr = prod_table.find(prod.value);
        check(prod_itr != prod_table.end(), "Producer not registered: " + prod.to_string());
        prod_table.modify(prod_itr, same_payer, [&](auto& p) {
            p.total_votes += voter_itr->staked.amount / 10000.0;
        });
    }

    uint64_t now = current_time_point().sec_since_epoch();
    voters.modify(voter_itr, voter, [&](auto& v) {
        v.producers = producers;
        v.last_vote_time = now;
    });

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(voter.value);
    if (node_itr != nodes.end() && node_itr->node_type == "Validator") {
        action(
            permission_level{get_self(), "active"_n},
            "nodegovern"_n,
            "checkvoting"_n,
            std::make_tuple(voter, now)
        ).send();
    }

    schedule_producers();
}

void system_contract::listprods() {
    producer_table producers(get_self(), get_self().value);
    vector<producer_info> result;
    for (auto itr = producers.begin(); itr != producers.end(); ++itr) {
        if (itr->is_active) {
            result.push_back(*itr);
        }
    }

    sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.total_votes > b.total_votes;
    });

    if (result.size() > 20) result.resize(20);
    for (const auto& prod : result) {
        print("Producer: ", prod.owner, ", Votes: ", prod.total_votes, "\n");
    }
}

void system_contract::claimrewards(name producer) {
    require_auth(producer);

    producer_table producers(get_self(), get_self().value);
    auto prod_itr = producers.find(producer.value);
    check(prod_itr != producers.end(), "Producer not found: " + producer.to_string());
    check(prod_itr->pending_rewards.amount > 0, "No rewards to claim for " + producer.to_string());

    action(
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "issue"_n,
        std::make_tuple(producer, prod_itr->pending_rewards, std::string("Block production reward"))
    ).send();

    producers.modify(prod_itr, same_payer, [&](auto& p) {
        p.pending_rewards = asset(0, symbol("AXT", 4));
        p.last_reward_claim = current_time_point().sec_since_epoch();
    });

    update_voter_rewards();
}

void system_contract::delegatebw(name from, name receiver, asset stake_net_quantity, asset stake_cpu_quantity) {
    require_auth(from);
    check(stake_net_quantity.symbol == symbol("AXT", 4), "Invalid symbol for stake_net: must be AXT");
    check(stake_cpu_quantity.symbol == symbol("AXT", 4), "Invalid symbol for stake_cpu: must be AXT");
    check(stake_net_quantity.amount >= 0 && stake_cpu_quantity.amount >= 0, "Stake quantities must be non-negative");

    asset total_stake = stake_net_quantity + stake_cpu_quantity;
    check(total_stake.amount > 0, "Must stake a positive amount");

    action(
        permission_level{from, "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(from, get_self(), total_stake, std::string("Stake for bandwidth"))
    ).send();

    voter_table voters(get_self(), get_self().value);
    auto voter_itr = voters.find(from.value);
    if (voter_itr == voters.end()) {
        voters.emplace(from, [&](auto& v) {
            v.owner = from;
            v.staked = total_stake;
        });
    } else {
        voters.modify(voter_itr, same_payer, [&](auto& v) {
            v.staked += total_stake;
        });
    }
}

void system_contract::regnode(name node, string node_type) {
    require_auth(node);
    check(node_type == "API" || node_type == "Seed" || node_type == "Full" || node_type == "Validator",
          "Invalid node type: must be API, Seed, Full, or Validator");

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(node.value);
    if (node_itr == nodes.end()) {
        nodes.emplace(node, [&](auto& n) {
            n.node = node;
            n.node_type = node_type;
            n.is_active = true;
        });
    } else {
        nodes.modify(node_itr, same_payer, [&](auto& n) {
            n.node_type = node_type;
            n.is_active = true;
        });
    }
}

void system_contract::reportactive(name node, uint64_t uptime_hours) {
    require_auth(node);
    check(uptime_hours > 0, "Uptime must be positive");

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(node.value);
    check(node_itr != nodes.end(), "Node not registered: " + node.to_string());
    check(node_itr->is_active, "Node is not active: " + node.to_string());

    asset reward = asset(uptime_hours * 10 * 10000, symbol("AXT", 4)); // 10 AXT/hour
    nodes.modify(node_itr, same_payer, [&](auto& n) {
        n.uptime_hours += uptime_hours;
        n.pending_node_rewards += reward;
    });

    // Notify nodegovern to track daily activity
    action(
        permission_level{get_self(), "active"_n},
        "nodegovern"_n,
        "trackdaily"_n,
        std::make_tuple(node, uptime_hours)
    ).send();
}

void system_contract::verifyblock(name node, checksum256 block_hash) {
    require_auth(node);

    node_table nodes(get_self(), get_self().value);
    auto node_itr = nodes.find(node.value);
    check(node_itr != nodes.end(), "Node not registered: " + node.to_string());
    check(node_itr->node_type == "Validator", "Only Validators can verify blocks: " + node.to_string());
    check(node_itr->is_active, "Node is not active: " + node.to_string());

    nodes.modify(node_itr, same_payer, [&](auto& n) {
        n.verification_count += 1;
    });

    print("Block verified by ", node, ": ", block_hash, "\n");
}

void system_contract::distrnodes() {
    require_auth(get_self());
    distribute_node_rewards();
}

void system_contract::onblock(block_timestamp timestamp, name producer) {
    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");

    uint64_t now = current_time_point().sec_since_epoch();
    if (g->initial_phase && (now - g->chain_start_time >= 600)) {
        distribute_initial_rewards();
        global.modify(g, same_payer, [&](auto& s) {
            s.initial_phase = false;
        });
    }

    producer_table producers(get_self(), get_self().value);
    auto prod_itr = producers.find(producer.value);
    if (prod_itr == producers.end()) return;

    distribute_rewards(producer);

    schedule_producers();

    if (now - g->last_node_reward_time >= 24 * 3600) {
        distribute_node_rewards();
        global.modify(g, same_payer, [&](auto& s) {
            s.last_node_reward_time = now;
        });
    }
}

void system_contract::schedule_producers() {
    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");

    uint64_t now = current_time_point().sec_since_epoch();
    if (!g->initial_phase && now - g->last_schedule_update < 6 * 3600) return;

    producer_table producers(get_self(), get_self().value);
    vector<producer_info> top_producers;
    for (auto itr = producers.begin(); itr != producers.end(); ++itr) {
        if (itr->is_active) {
            top_producers.push_back(*itr);
        }
    }

    if (top_producers.empty()) return;

    sort(top_producers.begin(), top_producers.end(), [](const auto& a, const auto& b) {
        return a.total_votes > b.total_votes;
    });

    if (top_producers.size() > 20) top_producers.resize(20);

    global.modify(g, same_payer, [&](auto& s) {
        s.last_schedule_update = now;
    });

    vector<producer_key> schedule;
    for (const auto& prod : top_producers) {
        producer_key p;
        p.producer_name = prod.owner;
        p.block_signing_key = prod.producer_key;
        schedule.push_back(p);
    }
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "setprods"_n,
        std::make_tuple(schedule)
    ).send();

    print("New producer schedule applied: ", top_producers.size(), " producers\n");
}

void system_contract::distribute_rewards(name producer) {
    producer_table producers(get_self(), get_self().value);
    auto prod_itr = producers.find(producer.value);
    check(prod_itr != producers.end(), "Producer not found: " + producer.to_string());

    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");

    if (g->initial_phase) {
        asset block_reward = asset(1 * 10000, symbol("AXT", 4));
        global.modify(g, same_payer, [&](auto& s) {
            s.initial_phase_rewards += block_reward;
            s.treasury += asset(0.1 * 10000, symbol("AXT", 4)); // 0.1 AXT/block to treasury
        });
        return;
    }

    vector<producer_info> top_producers;
    for (auto itr = producers.begin(); itr != producers.end(); ++itr) {
        if (itr->is_active) {
            top_producers.push_back(*itr);
        }
    }

    sort(top_producers.begin(), top_producers.end(), [](const auto& a, const auto& b) {
        return a.total_votes > b.total_votes;
    });

    if (top_producers.size() > 20) top_producers.resize(20);

    bool is_top_producer = false;
    for (const auto& p : top_producers) {
        if (p.owner == producer) {
            is_top_producer = true;
            break;
        }
    }
    if (!is_top_producer) return;

    voter_table voters(get_self(), get_self().value);
    double total_staked_axt = 0;
    double total_votes_for_bps = 0;
    for (const auto& voter : voters) {
        total_staked_axt += voter.staked.amount / 10000.0;
    }
    for (const auto& p : top_producers) {
        total_votes_for_bps += p.total_votes;
    }

    asset total_reward = asset(1 * 10000, symbol("AXT", 4));
    double total_stake_votes = total_staked_axt + total_votes_for_bps;
    if (total_stake_votes == 0) return;

    double bp_pool_share = total_votes_for_bps / total_stake_votes;
    asset bp_pool = asset(total_reward.amount * bp_pool_share, symbol("AXT", 4));
    double producer_votes = prod_itr->total_votes;
    asset bp_reward = asset(bp_pool.amount * (producer_votes / total_votes_for_bps), symbol("AXT", 4));
    producers.modify(prod_itr, same_payer, [&](auto& p) {
        p.pending_rewards += bp_reward;
    });

    double voter_pool_share = total_staked_axt / total_stake_votes;
    asset voter_pool = asset(total_reward.amount * voter_pool_share, symbol("AXT", 4));
    if (total_votes_for_bps > 0) {
        for (auto& voter : voters) {
            for (const auto& voted_producer : voter.producers) {
                if (voted_producer == producer) {
                    double voter_share = (voter.staked.amount / 10000.0) / total_staked_axt;
                    asset voter_reward = asset(voter_pool.amount * voter_share, symbol("AXT", 4));
                    voters.modify(voter, same_payer, [&](auto& v) {
                        v.pending_voter_rewards += voter_reward;
                    });
                    break;
                }
            }
        }
    }

    // Add 0.1 AXT/block to treasury
    global.modify(g, same_payer, [&](auto& s) {
        s.treasury += asset(0.1 * 10000, symbol("AXT", 4));
    });
}

void system_contract::distribute_initial_rewards() {
    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");
    check(g->initial_phase, "Initial phase already ended");

    node_table nodes(get_self(), get_self().value);
    vector<name> active_nodes;
    for (auto itr = nodes.begin(); itr != nodes.end(); ++itr) {
        if (itr->is_active) {
            active_nodes.push_back(itr->node);
        }
    }

    if (active_nodes.empty()) return;

    asset total_initial_rewards = g->initial_phase_rewards;
    asset reward_per_node = asset(total_initial_rewards.amount / active_nodes.size(), symbol("AXT", 4));
    for (const auto& node : active_nodes) {
        action(
            permission_level{get_self(), "active"_n},
            "eosio.token"_n,
            "issue"_n,
            std::make_tuple(node, reward_per_node, std::string("Initial phase reward"))
        ).send();
    }

    global.modify(g, same_payer, [&](auto& s) {
        s.initial_phase_rewards = asset(0, symbol("AXT", 4));
    });
}

void system_contract::update_voter_rewards() {
    voter_table voters(get_self(), get_self().value);
    for (auto& voter : voters) {
        if (voter.pending_voter_rewards.amount > 0) {
            action(
                permission_level{get_self(), "active"_n},
                "eosio.token"_n,
                "issue"_n,
                std::make_tuple(voter.owner, voter.pending_voter_rewards, std::string("Voter reward"))
            ).send();

            voters.modify(voter, same_payer, [&](auto& v) {
                v.pending_voter_rewards = asset(0, symbol("AXT", 4));
            });
        }
    }
}

void system_contract::distribute_node_rewards() {
    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");

    node_table nodes(get_self(), get_self().value);
    for (auto itr = nodes.begin(); itr != nodes.end(); ++itr) {
        if (itr->is_active && itr->pending_node_rewards.amount > 0) {
            check(g->treasury.amount >= itr->pending_node_rewards.amount, "Insufficient treasury funds");
            action(
                permission_level{get_self(), "active"_n},
                "eosio.token"_n,
                "issue"_n,
                std::make_tuple(itr->node, itr->pending_node_rewards, std::string("Node activity reward"))
            ).send();

            global.modify(g, same_payer, [&](auto& s) {
                s.treasury -= itr->pending_node_rewards;
            });

            nodes.modify(itr, same_payer, [&](auto& n) {
                n.pending_node_rewards = asset(0, symbol("AXT", 4));
            });

            require_recipient(itr->node);
        }
    }
}