#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;

class [[eosio::contract("nodegovernance")]] nodegovernance : public contract {
public:
  using contract::contract;

  // Action to report node activity
  [[eosio::action]]
  void reportactiv(name user, std::string node_type, uint64_t hours, uint64_t day_index);

  // Action to record voting session participation
  [[eosio::action]]
  void checkvoting(name user, uint64_t current_time);

  // Action to check Validator eligibility
  [[eosio::action]]
  bool canbevalid(name user);

  // Action to check Producer eligibility
  [[eosio::action]]
  bool canbeprod(name user);

  // Action to claim rewards
  [[eosio::action]]
  void claimrewards(name user);

  // Action to sponsor a new account
  [[eosio::action]]
  void sponsor(name sponsor, name new_account, asset resource_cost);

  // Action to distribute rewards
  [[eosio::action]]
  void distribute();

private:
  // Tables
  struct [[eosio::table]] activity_entry {
    name user;
    std::string node_type;
    uint64_t hours;
    uint64_t day_index;
    uint64_t primary_key() const { return user.value; }
  };
  using activity_table = multi_index<"activity"_n, activity_entry>;

  struct [[eosio::table]] voting_entry {
    name user;
    uint64_t timestamp;
    uint64_t primary_key() const { return user.value ^ timestamp; }
  };
  using voting_table = multi_index<"voting"_n, voting_entry>;

  struct [[eosio::table]] node_entry {
    name user;
    std::string node_type;
    uint64_t days_active;
    uint64_t voting_sessions;
    uint64_t primary_key() const { return user.value; }
  };
  using node_table = multi_index<"nodes"_n, node_entry>;

  struct [[eosio::table]] reward_entry {
    name user;
    asset pending_reward;
    uint64_t primary_key() const { return user.value; }
  };
  using reward_table = multi_index<"rewards"_n, reward_entry>;

  struct [[eosio::table]] sponsor_entry {
    name sponsor;
    name new_account;
    asset resource_cost;
    asset reward;
    uint64_t primary_key() const { return sponsor.value ^ new_account.value; }
  };
  using sponsor_table = multi_index<"sponsors"_n, sponsor_entry>;

  // Helper function to update node table and rewards
  void update_node(name user, std::string node_type, uint64_t hours);
};