#ifndef EOSIO_SYSTEM_RICARDIAN_HPP
#define EOSIO_SYSTEM_RICARDIAN_HPP

static constexpr const char* init_ricardian = R"(
Initializes the system contract with a version and core symbol.
)";

static constexpr const char* regproducer_ricardian = R"(
Registers a producer with a public key.
)";

static constexpr const char* voteproducer_ricardian = R"(
Allows a voter to vote for a list of producers.
)";

static constexpr const char* listproducers_ricardian = R"(
Lists the top active producers sorted by total votes.
)";

static constexpr const char* claimrewards_ricardian = R"(
Allows a producer to claim their pending block production rewards.
)";

static constexpr const char* delegatebw_ricardian = R"(
Delegates bandwidth by staking AXT for network and CPU resources.
)";

#endif