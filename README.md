IoT Lighting Monitoring with Blockchain Proof
 Project Overview

This project implements an IoT-based monitoring system for coworking space lighting, combining sensor data collection, local data storage, and blockchain anchoring to guarantee data integrity and traceability.

Sensor data (presence and luminosity) are transmitted through the network, stored locally, and a cryptographic proof of each message is recorded on a blockchain network to prevent data tampering.

The system demonstrates how blockchain can enhance trust and auditability in IoT infrastructures.

 Objectives

Monitor lighting conditions in shared spaces.

Collect and store sensor data reliably.

Guarantee data integrity using blockchain proofs.

Detect any modification or tampering of stored data.

Provide traceability of IoT messages.

 Architecture

System components:

IoT sensors send data.

Data flows through Node-RED / MQTT.

Backend API receives and stores data in SQLite.

A hash of each message is anchored on blockchain.

Verification endpoint checks data integrity.

⚙️ Technologies Used

Node.js + Express API

SQLite database

Ethers.js

Ganache (local Ethereum blockchain)

MQTT / Node-RED (IoT message handling)

SHA-256 / Keccak hashing

 Security Concept

Instead of storing data directly on blockchain, the system stores:

data locally in database

only a hash proof on blockchain

Any modification of stored data results in hash mismatch, proving tampering.

 API Endpoints
Store proof
POST /proof


Stores sensor data and anchors hash on blockchain.

Read stored record
GET /record/:id


Returns stored data.

Verify integrity
POST /verify/:id


Checks database data against blockchain proof.

🧪 Demonstration Scenario

Sensor sends data.

Data stored in DB.

Hash anchored on blockchain.

Modify DB manually.

Verification detects mismatch.

This demonstrates blockchain immutability.

 Educational Context

This project was developed as part of an academic IoT project to explore:

IoT system architecture

data integrity

blockchain integration

distributed trust mechanisms
