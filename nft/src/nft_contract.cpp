#include "../include/nft_contract.hpp"

void nft_contract::createcol(name issuer, uint64_t collection_id, std::string description) {
  // Require authorization from the issuer
  require_auth(issuer);

  // Ensure the issuer is a valid account
  check(is_account(issuer), "Issuer account does not exist");

  // Check if the collection already exists
  collection_table collections(get_self(), get_self().value);
  auto coll_itr = collections.find(collection_id);
  check(coll_itr == collections.end(), "Collection already exists");

  // Create the new collection
  collections.emplace(issuer, [&](auto& row) {
    row.collection_id = collection_id;
    row.issuer = issuer;
    row.description = description;
  });
}

void nft_contract::create(name issuer, uint64_t collection_id, uint64_t asset_id, std::string metadata) {
  // Require authorization from the issuer
  require_auth(issuer);

  // Ensure the issuer is a valid account
  check(is_account(issuer), "Issuer account does not exist");

  // Check if the collection exists
  collection_table collections(get_self(), get_self().value);
  auto coll_itr = collections.find(collection_id);
  check(coll_itr != collections.end(), "Collection does not exist");
  check(coll_itr->issuer == issuer, "Only the collection issuer can create assets");

  // Check if the asset already exists
  asset_table assets(get_self(), get_self().value);
  auto asset_itr = assets.find(asset_id);
  check(asset_itr == assets.end(), "Asset already exists");

  // Create the new asset (not owned yet)
  assets.emplace(issuer, [&](auto& row) {
    row.asset_id = asset_id;
    row.owner = name{}; // No owner until issued
    row.collection_id = collection_id;
    row.metadata = metadata;
  });
}

void nft_contract::issue(name to, uint64_t collection_id, uint64_t asset_id, std::string memo) {
  // Check if the collection exists
  collection_table collections(get_self(), get_self().value);
  auto coll_itr = collections.find(collection_id);
  check(coll_itr != collections.end(), "Collection does not exist");

  // Require authorization from the collection issuer
  require_auth(coll_itr->issuer);

  // Ensure the recipient account exists
  check(is_account(to), "Recipient account does not exist");

  // Check if the asset exists and is unowned
  asset_table assets(get_self(), get_self().value);
  auto asset_itr = assets.find(asset_id);
  check(asset_itr != assets.end(), "Asset does not exist");
  check(asset_itr->owner == name{}, "Asset already issued");
  check(asset_itr->collection_id == collection_id, "Asset does not belong to this collection");

  // Issue the asset to the user
  assets.modify(asset_itr, coll_itr->issuer, [&](auto& row) {
    row.owner = to;
  });
}

void nft_contract::transfer(name from, name to, uint64_t collection_id, uint64_t asset_id, std::string memo) {
  // Require authorization from the current owner
  require_auth(from);

  // Ensure the sender and recipient accounts exist
  check(is_account(from), "Sender account does not exist");
  check(is_account(to), "Recipient account does not exist");
  check(from != to, "Cannot transfer to self");

  // Check if the asset exists and is owned by the sender
  asset_table assets(get_self(), get_self().value);
  auto asset_itr = assets.find(asset_id);
  check(asset_itr != assets.end(), "Asset does not exist");
  check(asset_itr->owner == from, "Sender does not own this asset");
  check(asset_itr->collection_id == collection_id, "Asset does not belong to this collection");

  // Transfer the asset to the new owner
  assets.modify(asset_itr, from, [&](auto& row) {
    row.owner = to;
  });
}