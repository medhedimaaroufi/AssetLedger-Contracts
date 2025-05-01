#include "../include/eosio.system.hpp"

void system_contract::init(uint64_t version, symbol core) {
    require_auth(get_self());

    global_table global(get_self(), get_self().value);
    check(global.begin() == global.end(), "System already initialized");

    global.emplace(get_self(), [&](auto& g) {
        g.version = version;
        g.core_symbol = core;
        g.last_schedule_update = current_time_point().sec_since_epoch();
    });
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
}

void system_contract::voteproducer(name voter, std::vector<name> producers) {
    require_auth(voter);
    check(producers.size() <= 30, "Cannot vote for more than 30 producers");

    voter_table voters(get_self(), get_self().value);
    auto voter_itr = voters.find(voter.value);
    check(voter_itr != voters.end(), "Voter not found; must delegate bandwidth first");
    check(voter_itr->staked.amount > 0, "No staked AXT for voting");

    // Remove old votes
    producer_table prod_table(get_self(), get_self().value);
    for (const auto& old_prod : voter_itr->producers) {
        auto prod_itr = prod_table.find(old_prod.value);
        if (prod_itr != prod_table.end()) {
            prod_table.modify(prod_itr, same_payer, [&](auto& p) {
                p.total_votes -= voter_itr->staked.amount / 10000.0; // AXT has 4 decimals
            });
        }
    }

    // Add new votes
    for (const auto& prod : producers) {
        auto prod_itr = prod_table.find(prod.value);
        check(prod_itr != prod_table.end(), "Producer not registered");
        prod_table.modify(prod_itr, same_payer, [&](auto& p) {
            p.total_votes += voter_itr->staked.amount / 10000.0;
        });
    }

    // Update voter
    voters.modify(voter_itr, voter, [&](auto& v) {
        v.producers = producers;
        v.last_vote_time = current_time_point().sec_since_epoch();
    });

    // Schedule producers if 6 hours have passed
    schedule_producers();
}

void system_contract::listprods() {
    producer_table producers(get_self(), get_self().value);
    std::vector<producer_info> result;
    for (auto itr = producers.begin(); itr != producers.end(); ++itr) {
        if (itr->is_active) {
            result.push_back(*itr);
        }
    }

    // Sort by total_votes (descending)
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.total_votes > b.total_votes;
    });

    // Print top 20 (or fewer if less than 20 are active)
    if (result.size() > 20) result.resize(20);
    for (const auto& prod : result) {
        print("Producer: ", prod.owner, ", Votes: ", prod.total_votes, "\n");
    }
}

void system_contract::claimrewards(name producer) {
    require_auth(producer);

    producer_table producers(get_self(), get_self().value);
    auto prod_itr = producers.find(producer.value);
    check(prod_itr != producers.end(), "Producer not found");
    check(prod_itr->pending_rewards.amount > 0, "No rewards to claim");

    // Issue rewards
    action(
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "issue"_n,
        std::make_tuple(producer, prod_itr->pending_rewards, std::string("Block production reward"))
    ).send();

    // Clear pending rewards
    producers.modify(prod_itr, same_payer, [&](auto& p) {
        p.pending_rewards = asset(0, symbol("AXT", 4));
        p.last_reward_claim = current_time_point().sec_since_epoch();
    });

    // Distribute voter rewards
    update_voter_rewards();
}

void system_contract::delegatebw(name from, name receiver, asset stake_net_quantity, asset stake_cpu_quantity) {
    require_auth(from);
    check(stake_net_quantity.symbol == symbol("AXT", 4), "Invalid symbol for stake_net");
    check(stake_cpu_quantity.symbol == symbol("AXT", 4), "Invalid symbol for stake_cpu");
    check(stake_net_quantity.amount >= 0 && stake_cpu_quantity.amount >= 0, "Stake quantities must be non-negative");

    asset total_stake = stake_net_quantity + stake_cpu_quantity;
    check(total_stake.amount > 0, "Must stake a positive amount");

    // Transfer AXT to eosio for staking
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

void system_contract::onblock(block_timestamp timestamp, name producer) {
    producer_table producers(get_self(), get_self().value);
    auto prod_itr = producers.find(producer.value);
    if (prod_itr == producers.end()) return;

    // Distribute rewards for this block
    distribute_rewards(producer);

    // Schedule producers if 6 hours have passed
    schedule_producers();
}

void system_contract::schedule_producers() {
    global_table global(get_self(), get_self().value);
    auto g = global.begin();
    check(g != global.end(), "Global state not initialized");

    uint64_t now = current_time_point().sec_since_epoch();
    if (now - g->last_schedule_update < 6 * 3600) return; // 6 hours

    producer_table producers(get_self(), get_self().value);
    std::vector<producer_info> top_producers;
    for (auto itr = producers.begin(); itr != producers.end(); ++itr) {
        if (itr->is_active) {
            top_producers.push_back(*itr);
        }
    }

    if (top_producers.empty()) return;

    // Sort by total_votes (descending)
    std::sort(top_producers.begin(), top_producers.end(), [](const auto& a, const auto& b) {
        return a.total_votes > b.total_votes;
    });

    // Take top 20 (or fewer if less than 20 are active)
    if (top_producers.size() > 20) top_producers.resize(20);

    // Update producer schedule
    global.modify(g, same_payer, [&](auto& s) {
        s.last_schedule_update = now;
    });

    // Notify chain of new schedule
    print("New producer schedule: ", top_producers.size(), " producers\n");
}

void system_contract::distribute_rewards(name producer) {
    producer_table producers(get_self(), get_self().value);
    auto prod_itr = producers.find(producer.value);
    check(prod_itr != producers.end(), "Producer not found");

    // Check if producer is in top 20
    std::vector<producer_info> top_producers;
    for (auto itr = producers.begin(); itr != producers.end(); ++itr) {
        if (itr->is_active) {
            top_producers.push_back(*itr);
        }
    }

    // Sort by total_votes (descending)
    std::sort(top_producers.begin(), top_producers.end(), [](const auto& a, const auto& b) {
        return a.total_votes > b.total_votes;
    });

    // Take top 20 (or fewer if less than 20 are active)
    if (top_producers.size() > 20) top_producers.resize(20);

    bool is_top_producer = false;
    for (const auto& p : top_producers) {
        if (p.owner == producer) {
            is_top_producer = true;
            break;
        }
    }
    if (!is_top_producer) return;

    // BP reward: 30 AXT/block
    asset bp_reward = asset(30 * 10000, symbol("AXT", 4)); // 30 AXT
    producers.modify(prod_itr, same_payer, [&](auto& p) {
        p.pending_rewards += bp_reward;
    });

    // Voter reward: 15 AXT/block, distributed proportionally
    voter_table voters(get_self(), get_self().value);
    double total_votes_for_producer = prod_itr->total_votes;
    if (total_votes_for_producer > 0) {
        for (auto& voter : voters) {
            for (const auto& voted_producer : voter.producers) {
                if (voted_producer == producer) {
                    double voter_share = (voter.staked.amount / 10000.0) / total_votes_for_producer;
                    asset voter_reward = asset(15 * 10000 * voter_share, symbol("AXT", 4)); // 15 AXT total
                    voters.modify(voter, same_payer, [&](auto& v) {
                        v.pending_voter_rewards += voter_reward;
                    });
                    break;
                }
            }
        }
    }
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