// eosio.bios.hpp
// Version: 1.1 (Updated for AssetLedger whitepaper requirements, May 2025)

#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

using namespace eosio;
using namespace std; // For std::vector in setprods

struct producer_key {
    name producer_name;
    eosio::public_key block_signing_key;

    EOSLIB_SERIALIZE(producer_key, (producer_name)(block_signing_key))
};

class [[eosio::contract("eosio.bios")]] bios : public contract {
public:
    using contract::contract;

    // Action to set privileged status for an account
    [[eosio::action]]
    void setpriv(name account, uint8_t is_priv);

    // Action to set the producer schedule (up to 20 producers as per whitepaper)
    [[eosio::action]]
    void setprods(const std::vector<producer_key>& schedule);

private:
    // Table to store the producer schedule
    struct [[eosio::table]] producer_entry {
        name producer_name;
        eosio::public_key block_signing_key;

        uint64_t primary_key() const { return producer_name.value; }

        EOSLIB_SERIALIZE(producer_entry, (producer_name)(block_signing_key))
    };
    using producer_table = multi_index<"producers"_n, producer_entry>;
};