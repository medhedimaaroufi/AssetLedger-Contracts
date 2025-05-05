#include "../include/nft_contract.hpp"

void nftcontract::createcol(name issuer, uint64_t collection_id, std::string description, std::string metadata_schema, std::string industry) {
    require_auth(issuer);

    check(is_account(issuer), "Issuer account does not exist: " + issuer.to_string());
    check(description.size() <= 512, "Description exceeds 512 bytes");
    check(metadata_schema.size() <= 1024, "Metadata schema exceeds 1024 bytes");
    check(industry.size() <= 64, "Industry tag exceeds 64 bytes");
    check(!industry.empty(), "Industry tag cannot be empty");

    collection_table collections(get_self(), get_self().value);
    auto coll_itr = collections.find(collection_id);
    check(coll_itr == collections.end(), "Collection ID " + std::to_string(collection_id) + " already exists");

    collections.emplace(issuer, [&](auto& row) {
        row.collection_id = collection_id;
        row.issuer = issuer;
        row.description = description;
        row.metadata_schema = metadata_schema;
        row.industry = industry;
    });
}

void nftcontract::create(name issuer, uint64_t collection_id, uint64_t asset_id, std::string metadata) {
    require_auth(issuer);

    check(is_account(issuer), "Issuer account does not exist: " + issuer.to_string());
    check(metadata.size() <= 1024, "Metadata exceeds 1024 bytes");

    collection_table collections(get_self(), get_self().value);
    auto coll_itr = collections.find(collection_id);
    check(coll_itr != collections.end(), "Collection ID " + std::to_string(collection_id) + " does not exist");
    check(coll_itr->issuer == issuer, "Only the collection issuer can create assets");

    // Placeholder: Validate metadata against metadata_schema (e.g., JSON schema)
    check(!coll_itr->metadata_schema.empty() || metadata.size() > 0, "Metadata must comply with collection schema");

    asset_table assets(get_self(), get_self().value);
    auto asset_itr = assets.find(asset_id);
    check(asset_itr == assets.end(), "Asset ID " + std::to_string(asset_id) + " already exists");

    assets.emplace(issuer, [&](auto& row) {
        row.asset_id = asset_id;
        row.owner = name{};
        row.collection_id = collection_id;
        row.metadata = metadata;
    });
}

void nftcontract::issue(name to, uint64_t collection_id, uint64_t asset_id, std::string memo) {
    collection_table collections(get_self(), get_self().value);
    auto coll_itr = collections.find(collection_id);
    check(coll_itr != collections.end(), "Collection ID " + std::to_string(collection_id) + " does not exist");

    require_auth(coll_itr->issuer);
    check(memo.size() <= 256, "Memo exceeds 256 bytes");
    check(is_account(to), "Recipient account does not exist: " + to.to_string());

    asset_table assets(get_self(), get_self().value);
    auto asset_itr = assets.find(asset_id);
    check(asset_itr != assets.end(), "Asset ID " + std::to_string(asset_id) + " does not exist");
    check(asset_itr->owner == name{}, "Asset ID " + std::to_string(asset_id) + " already issued");
    check(asset_itr->collection_id == collection_id, "Asset does not belong to collection ID " + std::to_string(collection_id));

    assets.modify(asset_itr, coll_itr->issuer, [&](auto& row) {
        row.owner = to;
    });

    require_recipient(to);
}

void nftcontract::transfer(name from, name to, uint64_t collection_id, uint64_t asset_id, std::string memo) {
    require_auth(from);
    check(memo.size() <= 256, "Memo exceeds 256 bytes");
    check(is_account(from), "Sender account does not exist: " + from.to_string());
    check(is_account(to), "Recipient account does not exist: " + to.to_string());
    check(from != to, "Cannot transfer to self");

    asset_table assets(get_self(), get_self().value);
    auto asset_itr = assets.find(asset_id);
    check(asset_itr != assets.end(), "Asset ID " + std::to_string(asset_id) + " does not exist");
    check(asset_itr->owner == from, "Sender does not own asset ID " + std::to_string(asset_id));
    check(asset_itr->collection_id == collection_id, "Asset does not belong to collection ID " + std::to_string(collection_id));

    assets.modify(asset_itr, from, [&](auto& row) {
        row.owner = to;
    });

    require_recipient(from);
    require_recipient(to);
}