#include "../include/eosio.bios.hpp"

void bios::setpriv(name account, uint8_t is_priv) {
    // Require authorization from the contract itself (eosio)
    require_auth(get_self());

    // Validate the account exists
    check(is_account(account), "Account does not exist");

    // Use the system contract to set the privileged status
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

    // Validate the schedule
    check(schedule.size() > 0, "Producer schedule cannot be empty");
    check(schedule.size() <= 20, "Producer schedule cannot exceed 20 producers");

    // Update the producer table
    producer_table producers(get_self(), get_self().value);
    // Clear existing producers
    for (auto itr = producers.begin(); itr != producers.end();) {
        itr = producers.erase(itr);
    }

    // Add new producers to the table
    for (const auto& prod : schedule) {
        check(is_account(prod.producer_name), "Producer account does not exist");
        producers.emplace(get_self(), [&](auto& row) {
            row.producer_name = prod.producer_name;
            row.block_signing_key = prod.block_signing_key;
        });
    }

    // Notify the system to update the active producer schedule
    action(
        permission_level{get_self(), "active"_n},
        "eosio"_n,
        "setprods"_n,
        std::make_tuple(schedule)
    ).send();
}