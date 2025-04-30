#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;

class [[eosio::contract("nftcontract")]] nft_contract : public contract {
public:
  using contract::contract;

  // Action to create a new NFT
  [[eosio::action]]
  void createasset(name issuer, uint64_t collection_id, uint64_t asset_id, std::string metadata);

  // Action to create a new collection
  [[eosio::action]]
  void createcoll(name issuer, uint64_t collection_id, std::string description);

  // Action to issue an NFT to a user
  [[eosio::action]]
  void issueasset(name to, uint64_t collection_id, uint64_t asset_id, std::string memo);

  // Action to transfer an NFT between users
  [[eosio::action]]
  void transferasset(name from, name to, uint64_t collection_id, uint64_t asset_id, std::string memo);

private:
  // Table to store NFT assets
  struct [[eosio::table]] asset_entry {
    uint64_t asset_id;
    name owner;
    uint64_t collection_id;
    std::string metadata;
    uint64_t primary_key() const { return asset_id; }
    uint64_t by_collection() const { return collection_id; }
  };
  using asset_table = multi_index<"assets"_n, asset_entry,
    indexed_by<"bycoll"_n, const_mem_fun<asset_entry, uint64_t, &asset_entry::by_collection>>>;

  // Table to store collections
  struct [[eosio::table]] collection_entry {
    uint64_t collection_id;
    name issuer;
    std::string description;
    uint64_t primary_key() const { return collection_id; }
  };
  using collection_table = multi_index<"collections"_n, collection_entry>;
};