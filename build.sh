#!/bin/bash

# Exit on any error
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Run cmake to configure the build
cmake ..

# Build the project
make -j$(nproc)

# Create contracts directory if it doesn't exist
mkdir -p ../contracts

# Copy generated .wasm and .abi files to contracts directory
# System contracts
mkdir -p ../contracts/eosio.bios
cp ../system/eosio.bios/contracts/eosio.bios.wasm ../contracts/eosio.bios/
cp ../system/eosio.bios/contracts/eosio.bios.abi ../contracts/eosio.bios/

mkdir -p ../contracts/eosio.system
cp ../system/eosio.system/contracts/eosio.system.wasm ../contracts/eosio.system/
cp ../system/eosio.system/contracts/eosio.system.abi ../contracts/eosio.system/

mkdir -p ../contracts/eosio.token
cp ../system/eosio.token/contracts/eosio.token.wasm ../contracts/eosio.token/
cp ../system/eosio.token/contracts/eosio.token.abi ../contracts/eosio.token/

# Governance contract
mkdir -p ../contracts/governance
cp ../governance/contracts/nodegovernance.wasm ../contracts/governance/nodegovernance.wasm
cp ../governance/contracts/nodegovernance.abi ../contracts/governance/nodegovernance.abi

# NFT contract
mkdir -p ../contracts/nft_contract
cp ../nft/contracts/nftcontract.wasm ../contracts/nft_contract/nftcontract.wasm
cp ../nft/contracts/nftcontract.abi ../contracts/nft_contract/nftcontract.abi

echo "Build completed successfully. Contract artifacts are in the contracts directory."