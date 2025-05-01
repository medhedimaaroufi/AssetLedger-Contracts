// nftcontract.hpp
// Version: 1.1 (Updated for AssetLedger whitepaper requirements, May 2025)

#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <string>

using namespace eosio;

class [[eosio::contract("nftcontract")]] nftcontract : public contract {
public:
    using contract::contract;

    // Action to create a new collection
    [[eosio::action]]
    void createcol(name issuer, uint64_t collection_id, std::string description);

    // Action to create a new NFT asset (unowned until issued)
    [[eosio::action]]
    void create(name issuer, uint64_t collection_id, uint64_t asset_id, std::string metadata);

    // Action to issue an NFT to a user, assigning ownership
    [[eosio::action]]
    void issue(name to, uint64_t collection_id, uint64_t asset_id, std::string memo);

    // Action to transfer an NFT between users
    [[eosio::action]]
    void transfer(name from, name to, uint64_t collection_id, uint64_t asset_id, std::string memo);

private:
    // Table to store NFT assets
    struct [[eosio::table]] asset_entry {
        uint64_t asset_id;
        name owner;
        uint64_t collection_id;
        std::string metadata;

        uint64_t primary_key() const { return asset_id; }
        uint64_t by_collection() const { return collection_id; }

        EOSLIB_SERIALIZE(asset_entry, (asset_id)(owner)(collection_id)(metadata))
    };
    using asset_table = multi_index<"assets"_n, asset_entry,
        indexed_by<"bycoll"_n, const_mem_fun<asset_entry, uint64_t, &asset_entry::by_collection>>>;

    // Table to store collections
    struct [[eosio::table]] collection_entry {
        uint64_t collection_id;
        name issuer;
        std::string description;

        uint64_t primary_key() const { return collection_id; }

        EOSLIB_SERIALIZE(collection_entry, (collection_id)(issuer)(description))
    };
    using collection_table = multi_index<"collections"_n, collection_entry>;
};