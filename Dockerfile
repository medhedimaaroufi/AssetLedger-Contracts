# Use Ubuntu 20.04 as base
FROM ubuntu:20.04

# Install dependencies
RUN apt update && apt install -y \
    wget \
    git \
    curl \
    gnupg \
    lsb-release \
    cmake \
    make \
    build-essential \
    clang \
    llvm \
    nano \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install eosio.cdt
RUN wget https://github.com/eosio/eosio.cdt/releases/download/v1.8.0/eosio.cdt_1.8.0-1-ubuntu-20.04_amd64.deb && \
    apt update && \
    apt install ./eosio.cdt_1.8.0-1-ubuntu-20.04_amd64.deb && \
    rm eosio.cdt_1.8.0-1-ubuntu-20.04_amd64.deb

# Set working directory
WORKDIR /project

# Default command
CMD ["bash"]
