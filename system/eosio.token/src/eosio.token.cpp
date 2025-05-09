#include "eosio.token.hpp"

void token::create(name issuer, asset maximum_supply) {
    require_auth(get_self());

    check(is_account(issuer), "Issuer account does not exist");

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "Invalid symbol name");

    check(sym.code() == symbol_code("AXT"), "Symbol must be AXT");
    check(sym.precision() == 4, "AXT must have 4 decimal places");

    check(maximum_supply.is_valid(), "Invalid supply");
    check(maximum_supply.amount > 0, "Max supply must be positive");
    check(maximum_supply.amount <= 10000000000000000, "Max supply capped at 1 trillion AXT"); // 1T AXT

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "Token with symbol already exists");

    statstable.emplace(get_self(), [&](auto& s) {
        s.supply = asset(0, maximum_supply.symbol);
        s.max_supply = maximum_supply;
        s.issuer = issuer;
    });

    // Note: Zero-cost transactions are achieved via sponsor-funded resources (CPU, Net) for new accounts,
    // with optional subsidies configurable off-chain.
}

void token::issue(name to, asset quantity, std::string memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "Invalid symbol name");
    check(memo.size() <= 256, "Memo exceeds 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "Token with symbol does not exist");
    const auto& st = *existing;

    check(is_account(to), "Recipient account does not exist");

    require_auth(st.issuer);
    check(quantity.is_valid(), "Invalid quantity");
    check(quantity.amount > 0, "Must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "Symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "Quantity exceeds available supply");

    statstable.modify(st, same_payer, [&](auto& s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);
    if (to != st.issuer) {
        transfer(st.issuer, to, quantity, memo);
    }
}

void token::transfer(name from, name to, asset quantity, std::string memo) {
    check(from != to, "Cannot transfer to self");
    require_auth(from);
    check(is_account(to), "Recipient account does not exist");

    auto sym = quantity.symbol;
    stats statstable(get_self(), sym.code().raw());
    const auto& st = statstable.get(sym.code().raw(), "Token stats not found");

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "Invalid quantity");
    check(quantity.amount > 0, "Must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "Symbol precision mismatch");
    check(memo.size() <= 256, "Memo exceeds 256 bytes");

    auto payer = has_auth(to) ? to : from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
}

void token::add_balance(name owner, asset value, name ram_payer) {
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

void token::sub_balance(name owner, asset value) {
    accounts acnts(get_self(), owner.value);
    const auto& from = acnts.get(value.symbol.code().raw(), "No balance object found");
    check(from.balance.amount >= value.amount, "Overdrawn balance");

    if (from.balance.amount == value.amount) {
        acnts.erase(from);
    } else {
        acnts.modify(from, owner, [&](auto& a) {
            a.balance -= value;
        });
    }
}