#include "eosio.bios.hpp"

void bios::setpriv(name account, uint8_t is_priv) {
    // Require authorization from the contract itself (eosio)
    require_auth(get_self());

    // Validate the account exists
    check(is_account(account), "Account does not exist: " + account.to_string());

    // Set privileged status via system contract
    // Used for accounts like eosio.token and nodegovernance
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

    // Validate schedule per AssetLedger whitepaper (max 20 producers)
    check(schedule.size() > 0, "Producer schedule cannot be empty");
    check(schedule.size() <= 20, "Producer schedule cannot exceed 20 producers");

    // Check for duplicate producer names
    std::set<name> producer_names;
    for (const auto& prod : schedule) {
        check(producer_names.insert(prod.producer_name).second, "Duplicate producer: " + prod.producer_name.to_string());
        check(prod.block_signing_key != eosio::public_key{}, "Invalid block signing key for: " + prod.producer_name.to_string());
        check(is_account(prod.producer_name), "Producer account does not exist: " + prod.producer_name.to_string());
    }

    // Update producer table
    producer_table producers(get_self(), get_self().value);
    for (auto itr = producers.begin(); itr != producers.end();) {
        itr = producers.erase(itr);
    }

    for (const auto& prod : schedule) {
        producers.emplace(get_self(), [&](auto& row) {
            row.producer_name = prod.producer_name;
            row.block_signing_key = prod.block_signing_key;
        });
    }

    // Notify system to update active producer schedule
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "setprods"_n,
        std::make_tuple(schedule)
    ).send();

    print("Updated producer schedule with ", schedule.size(), " producers\n");
}