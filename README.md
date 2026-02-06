# IoT Lighting Monitoring with Blockchain Proof

##  Project Overview
This project implements an IoT-based monitoring system for coworking space lighting, combining sensor data collection, local data storage, and blockchain anchoring to guarantee data integrity and traceability.

Sensor data such as presence detection and luminosity levels are transmitted through the network, stored locally, and a cryptographic proof of each message is recorded on a blockchain network to prevent data tampering.

The system demonstrates how blockchain technology can enhance trust, transparency, and auditability in IoT infrastructures.

---

##  Objectives
- Monitor lighting conditions in shared workspaces
- Collect and store IoT sensor data reliably
- Guarantee data integrity using blockchain proofs
- Detect tampering or modification attempts
- Provide full traceability of IoT messages

---

##  System Architecture
The system is composed of:

1. IoT sensors collecting luminosity and presence data
2. MQTT / Node-RED pipeline for message transmission
3. Node.js backend API for data processing
4. SQLite database for local storage
5. Blockchain anchoring of message hashes
6. Verification API to validate integrity

Only cryptographic proofs are stored on-chain, while full data remain in local storage.

---

##  Technologies Used
- Node.js
- Express.js API
- SQLite database
- Ethers.js
- Ganache local Ethereum blockchain
- MQTT protocol
- Node-RED
- SHA-256 / Keccak hashing

---

##  Security Concept
Instead of storing full IoT data on blockchain, the system stores only a cryptographic hash proof.

If stored data are modified later, verification fails because the recalculated hash no longer matches the blockchain record.

This guarantees data immutability and traceability while keeping blockchain usage lightweight.

---

##  API Endpoints

### Store Data Proof
`POST /proof`

Stores IoT data in database and anchors its hash on blockchain.

---

### Retrieve Stored Record
`GET /record/:id`

Returns stored IoT record.

---

### Verify Data Integrity
`POST /verify/:id`

Compares stored data hash with blockchain proof.

---

##  Demonstration Scenario
1. IoT sensor sends data.
2. Data are stored locally.
3. Hash proof is anchored on blockchain.
4. Database record is manually modified.
5. Verification endpoint detects mismatch.

This demonstrates blockchain immutability and data protection.

---

##  Academic Context
This project was developed within an academic IoT engineering project exploring:

- IoT architecture
- secure data transmission
- blockchain integration
- data traceability mechanisms

---

##  Author
Aya — IoT & AI Engineering Student
