#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace eosio;

class [[eosio::contract("eosio.token")]] token : public contract {
public:
  using contract::contract;

  [[eosio::action]]
  void create(name issuer, asset maximum_supply) {
    require_auth(get_self());

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(maximum_supply.is_valid(), "invalid supply");
    check(maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "token with symbol already exists");

    statstable.emplace(get_self(), [&](auto& s) {
      s.supply.symbol = maximum_supply.symbol;
      s.max_supply = maximum_supply;
      s.issuer = issuer;
    });
  }

  [[eosio::action]]
  void issue(name to, asset quantity, std::string memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
    const auto& st = *existing;
    check(to != name(), "must issue to an account");

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify(st, same_payer, [&](auto& s) {
      s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);
    if (to != st.issuer) {
      transfer(st.issuer, to, quantity, memo);
    }
  }

  [[eosio::action]]
  void transfer(name from, name to, asset quantity, std::string memo) {
    check(from != to, "cannot transfer to self");
    require_auth(from);
    check(to != name(), "to account does not exist");
    auto sym = quantity.symbol;
    stats statstable(get_self(), sym.code().raw());
    const auto& st = statstable.get(sym.code().raw());

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    auto payer = has_auth(to) ? to : from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
  }

private:
  struct [[eosio::table]] account {
    asset balance;

    uint64_t primary_key() const { return balance.symbol.code().raw(); }
  };
  using accounts = multi_index<"accounts"_n, account>;

  struct [[eosio::table]] currency_stats {
    asset supply;
    asset max_supply;
    name issuer;

    uint64_t primary_key() const { return supply.symbol.code().raw(); }
  };
  using stats = multi_index<"stats"_n, currency_stats>;

  void add_balance(name owner, asset value, name ram_payer) {
    accounts acnts(get_self(), owner.value);
    auto it = acnts.find(value.symbol.code().raw());
    if (it == acnts.end()) {
      acnts.emplace(ram_payer, [&](auto& a) {
        a.balance = value;
      });
    } else {
      acnts.modify(it, ram_payer, [&](auto& a) {
        a.balance += value;
      });
    }
  }

  void sub_balance(name owner, asset value) {
    accounts acnts(get_self(), owner.value);
    const auto& from = acnts.get(value.symbol.code().raw(), "no balance object found");
    check(from.balance.amount >= value.amount, "overdrawn balance");

    acnts.modify(from, owner, [&](auto& a) {
      a.balance -= value;
    });
  }
};