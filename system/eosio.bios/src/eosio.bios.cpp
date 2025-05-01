// eosio.bios.cpp
// Version: 1.1 (Updated for AssetLedger whitepaper requirements, May 2025)

#include "eosio.bios.hpp"

void bios::setpriv(name account, uint8_t is_priv) {
    // Require authorization from the contract itself (eosio)
    require_auth(get_self());

    // Validate the account exists
    check(is_account(account), "Account does not exist: " + account.to_string());

    // Use the system contract to set the privileged status
    // This allows accounts like eosio.token and nodegovernance to deploy system contracts
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "setpriv"_n,
        std::make_tuple(account, is_priv)
    ).send();
}

void bios::setprods(const std::vector<producer_key>& schedule) {
    // Require authorization from the contract itself (eosio)
    require_auth(get_self());

    // Validate the schedule per whitepaper requirements
    check(schedule.size() > 0, "Producer schedule cannot be empty");
    check(schedule.size() <= 20, "Producer schedule cannot exceed 20 producers as per AssetLedger whitepaper");

    // Check for duplicate producer names in the schedule
    std::set<name> producer_names;
    for (const auto& prod : schedule) {
        check(producer_names.insert(prod.producer_name).second, "Duplicate producer in schedule: " + prod.producer_name.to_string());
        // Validate the block signing key (ensure it's not empty)
        check(prod.block_signing_key != eosio::public_key{}, "Invalid block signing key for producer: " + prod.producer_name.to_string());
        // Validate the producer account exists
        check(is_account(prod.producer_name), "Producer account does not exist: " + prod.producer_name.to_string());
    }

    // Update the producer table
    producer_table producers(get_self(), get_self().value);
    // Clear existing producers
    for (auto itr = producers.begin(); itr != producers.end();) {
        itr = producers.erase(itr);
    }

    // Add new producers to the table
    for (const auto& prod : schedule) {
        producers.emplace(get_self(), [&](auto& row) {
            row.producer_name = prod.producer_name;
            row.block_signing_key = prod.block_signing_key;
        });
    }

    // Notify the system to update the active producer schedule
    // This enables block production by the specified producers
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "setprods"_n,
        std::make_tuple(schedule)
    ).send();
}