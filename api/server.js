const express = require("express"); //API
const { ethers } = require("ethers"); //GANACHE
const sqlite3 = require("sqlite3").verbose(); //BDD LOCAL

const app = express();
app.use(express.json());

const provider = new ethers.JsonRpcProvider("http://127.0.0.1:7545"); //Ganache

const privateKey = "0x65793a659f0c2b624a89709d4f572af4bb7688e9359838649e89dda9f5f3f437";
const baseWallet = new ethers.Wallet(privateKey, provider);
const wallet = new ethers.NonceManager(baseWallet);

// ---- SQLite setup
const db = new sqlite3.Database("./iot.db");
db.serialize(() => {
  db.run(`
    CREATE TABLE IF NOT EXISTS iot_data (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      deviceId TEXT,
      timestamp INTEGER,
      payload_json TEXT,
      proofHash TEXT,
      txHash TEXT,
      blockNumber INTEGER,
      createdAt TEXT DEFAULT (datetime('now'))
    )
  `);
});

function canonicalStringify(obj) {
  const keys = Object.keys(obj).sort();
  const sorted = {};
  for (const k of keys) sorted[k] = obj[k];
  return JSON.stringify(sorted);
}

function run(db, sql, params=[]) {
  return new Promise((resolve, reject) => {
    db.run(sql, params, function (err) {
      if (err) reject(err);
      else resolve(this); 
    });
  });
}

function get(db, sql, params=[]) {
  return new Promise((resolve, reject) => {
    db.get(sql, params, (err, row) => {
      if (err) reject(err);
      else resolve(row);
    });
  });
}

//DB ET GN
app.post("/proof", async (req, res) => {
  try {
    const data = req.body;

    const json = canonicalStringify(data);
    const proofHash = ethers.keccak256(ethers.toUtf8Bytes(json));

    // insert first
    const ins = await run(
      db,
      `INSERT INTO iot_data (deviceId, timestamp, payload_json, proofHash)
       VALUES (?, ?, ?, ?)`,
      [data.deviceId || "dev01", data.timestamp || Date.now(), json, proofHash]
    );

    const fromAddr = await wallet.getAddress();
    const tx = await wallet.sendTransaction({
      to: fromAddr,
      value: 0n,
      data: proofHash,
      gasLimit: 80000n,
      gasPrice: 2000000000n
    });

    const receipt = await tx.wait();

    // update with tx info
    await run(
      db,
      `UPDATE iot_data SET txHash = ?, blockNumber = ? WHERE id = ?`,
      [tx.hash, receipt.blockNumber, ins.lastID]
    );

    res.json({
      id: ins.lastID,
      proofHash,
      txHash: tx.hash,
      blockNumber: receipt.blockNumber
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// 2) Read DB record
app.get("/record/:id", async (req, res) => {
  try {
    const row = await get(db, `SELECT * FROM iot_data WHERE id = ?`, [req.params.id]);
    if (!row) return res.status(404).json({ error: "Not found" });
    res.json(row);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

//   DB vs blockchain
app.post("/verify/:id", async (req, res) => {
  try {
    const row = await get(db, `SELECT * FROM iot_data WHERE id = ?`, [req.params.id]);
    if (!row) return res.status(404).json({ error: "Not found" });
    if (!row.txHash) return res.status(400).json({ error: "No txHash stored" });

    const tx = await provider.getTransaction(row.txHash);
    if (!tx) return res.status(404).json({ error: "Tx not found on chain" });

    // recompute from DB payload_json (what DB currently contains)
    const recomputed = ethers.keccak256(ethers.toUtf8Bytes(row.payload_json));
    const onChain = tx.data;

    res.json({
      id: row.id,
      txHash: row.txHash,
      onChainHash: onChain,
      recomputedHashFromDB: recomputed,
      match: onChain.toLowerCase() === recomputed.toLowerCase(),
      blockNumber: tx.blockNumber
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.listen(3000, () => console.log("API running on port 3000"));
