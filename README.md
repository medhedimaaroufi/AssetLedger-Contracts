# AssetLedger Contracts Development Environment (Ubuntu 20.04 + CDT)

This Docker environment is designed for developing and building EOSIO smart contracts using the EOSIO Contract Development Toolkit (CDT) on **Ubuntu 20.04**.

---

## 📁 Project Structure

The expected folder structure of the project is:

```
.
├───governance
│   ├───include         # Header files for governance contract
│   └───src             # Source files (.cpp) for governance contract
├───nft
│   ├───include         # Header files for NFT contract
│   └───src             # Source files (.cpp) for NFT contract
└───system
    ├───eosio.bios
    │   ├───include     # Header files for eosio.bios contract
    │   └───src         # Source files for eosio.bios
    ├───eosio.system
    │   ├───include     # Header files for eosio.system contract
    │   └───src         # Source files for eosio.system
    └───eosio.token
        ├───include     # Header files for eosio.token contract
        └───src         # Source files for eosio.token
```

---

## 🐳 Using the Docker Environment

### Step 1: Build the Docker Image

```bash
docker build -t eosio-cdt-env .
```

### Step 2: Run the Container

> Mount your local project directory (e.g., `C:\eos-projects`) to the container

**Windows PowerShell Example:**

```bash
docker run -it --rm -v C:\eos-projects:/project eosio-cdt-env
```

**Linux/macOS Example:**

```bash
docker run -it --rm -v $(pwd):/project eosio-cdt-env
```

> This mounts your contracts project into `/project` in the container.

---

## ⚙️ Compiling Contracts

Navigate to any module and compile using `eosio-cpp`:

```bash
cd /project/nft/src
eosio-cpp -o nftcontract.wasm nft_contract.cpp -abigen
```

Repeat for other contract modules like `nft`, `eosio.token`, etc.

---

## 📦 Included Tools

- `eosio.cdt` v1.8.0
- Build Tools: `clang`, `llvm`, `cmake`, `make`, `build-essential`
- Utilities: `git`, `curl`, `wget`, `nano`

---

## 📚 Resources

- [EOSIO CDT Documentation](https://github.com/EOSIO/eosio.cdt)
- [EOSIO Developer Portal](https://developers.eos.io/)
