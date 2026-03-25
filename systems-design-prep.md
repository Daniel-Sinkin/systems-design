# Systems Design Interview Prep — Flow Traders (Graduate C++ SWE)

## Context

Flow Traders is a **proprietary trading firm / market maker**. Their systems are all about processing market data, making pricing decisions, and executing trades — all at extreme speed. Unlike a typical web company interview (design Twitter, design Uber), a trading firm interview will care deeply about **latency, throughput, reliability, and determinism**.

You have **1 hour, in-person**, so expect roughly:
- ~5 min: problem statement and clarifying questions
- ~20 min: high-level architecture
- ~25 min: deep dives into components
- ~10 min: trade-offs, failure modes, extensions

---

## The 8 Topics You Must Know (Priority Order)

### 1. Latency & Throughput (DDIA Ch. 2, pp. 37–42)

**Why it matters at Flow Traders:** Everything. Market making profits depend on being faster than competitors. Microsecond-level latency is the norm.

**Key concepts:**
- **Latency vs throughput** — latency is time per operation, throughput is operations per time
- **Tail latency (p99, p999)** — in trading, the *worst case* matters more than the average. A p99 spike means missed trades.
- **Response time distribution** — percentiles, not averages
- **Sources of latency:** network hops, serialization/deserialization, disk I/O, garbage collection, context switches, lock contention
- **Mechanical sympathy** — understanding hardware (CPU caches, NUMA, NIC offloading) to write fast software

**Keywords to know:** p50/p95/p99, tail latency, jitter, SLA/SLO, head-of-line blocking, bandwidth-delay product, Little's Law

---

### 2. Data Storage & Retrieval (DDIA Ch. 4, pp. 115–150)

**Why it matters at Flow Traders:** Market data (order books, trades, quotes) needs to be stored and queried efficiently. Time-series data is central.

**Key concepts:**
- **B-Trees vs LSM-Trees** — B-Trees give consistent read performance (good for lookups); LSM-Trees give high write throughput (good for ingesting market data)
- **Write-ahead logs (WAL)** — crash recovery mechanism
- **In-memory storage** — Redis, memcached, or custom in-memory structures for hot data (current order book state)
- **Column-oriented storage** — for analytics on historical market data
- **Indexes** — primary, secondary, composite. Know when they help and when they hurt.

**Keywords to know:** B-Tree, LSM-Tree, SSTable, memtable, WAL, compaction, bloom filter, column store, time-series database, OLTP vs OLAP

---

### 3. Encoding & Serialization (DDIA Ch. 5, pp. 161–191)

**Why it matters at Flow Traders:** Every message between systems must be serialized. In low-latency systems, the serialization format is a critical performance choice.

**Key concepts:**
- **Binary formats** — Protocol Buffers, FlatBuffers, Cap'n Proto, SBE (Simple Binary Encoding). These are what trading firms actually use (NOT JSON).
- **Zero-copy deserialization** — FlatBuffers/Cap'n Proto let you read data directly from the buffer without parsing. Huge for latency.
- **Schema evolution** — how to change message formats without breaking running systems (forward/backward compatibility)
- **FIX protocol** — the standard messaging protocol in financial markets

**Keywords to know:** protobuf, FlatBuffers, SBE, zero-copy, schema evolution, backward/forward compatibility, FIX protocol, endianness

---

### 4. Replication (DDIA Ch. 6, pp. 197–243)

**Why it matters at Flow Traders:** Trading systems need to be fault-tolerant without sacrificing latency. If the primary pricing engine dies, a replica must take over instantly.

**Key concepts:**
- **Single-leader replication** — one writer, many readers. Simple, common.
- **Replication lag** — reads from replicas may be stale. In trading, stale data = wrong prices = money lost.
- **Synchronous vs asynchronous replication** — sync = consistent but slow; async = fast but risk of data loss
- **Failover** — what happens when the leader dies? Split-brain problem.
- **Multi-datacenter replication** — Flow Traders operates in multiple exchanges globally

**Keywords to know:** leader/follower, replication lag, eventual consistency, failover, split-brain, quorum, WAL shipping

---

### 5. Sharding / Partitioning (DDIA Ch. 7, pp. 251–271)

**Why it matters at Flow Traders:** Market data can be naturally partitioned by instrument/symbol, exchange, or asset class.

**Key concepts:**
- **Partition by key range** — e.g., symbols A-M on shard 1, N-Z on shard 2. Simple but can create hot spots.
- **Partition by hash** — more uniform distribution, but loses range query ability
- **Hot spots** — some instruments (e.g., SPY, AAPL) have vastly more activity than others. How do you handle skew?
- **Request routing** — how does a client know which partition to talk to?

**Keywords to know:** consistent hashing, partition key, hot spot, rebalancing, scatter-gather, local vs global secondary index

---

### 6. Messaging & Event-Driven Architecture (DDIA Ch. 12, pp. 487–529)

**Why it matters at Flow Traders:** Market data feeds, order routing, and internal communication between components are all message-based.

**Key concepts:**
- **Message queues vs log-based brokers** — RabbitMQ-style (message deleted after consumption) vs Kafka-style (append-only log, replay possible)
- **Publish/subscribe** — market data feed is a classic pub/sub pattern
- **Ordering guarantees** — trades must be processed in order. How do you ensure this?
- **Backpressure** — when a consumer can't keep up, what happens?
- **Exactly-once vs at-least-once delivery** — and why exactly-once is mostly a myth

**Keywords to know:** Kafka, message broker, topic, partition, consumer group, offset, backpressure, at-least-once, idempotency, event sourcing

---

### 7. Consistency & Consensus (DDIA Ch. 10, pp. 401–440)

**Why it matters at Flow Traders:** When multiple systems need to agree on state (e.g., current position, risk limits), consistency matters enormously. Wrong state = wrong trades.

**Key concepts:**
- **Linearizability** — the strongest consistency guarantee. Reads always return the most recent write. Expensive.
- **CAP theorem** — in a network partition, you must choose consistency or availability. Trading systems usually choose consistency (better to stop trading than trade on wrong data).
- **Consensus algorithms** — Raft, Paxos (conceptual understanding, not implementation details)
- **Coordination services** — ZooKeeper, etcd for leader election and distributed locking

**Keywords to know:** linearizability, serializability, CAP theorem, Raft, Paxos, leader election, distributed lock, ZooKeeper, etcd, split-brain

---

### 8. Transactions (DDIA Ch. 8, pp. 277–335)

**Why it matters at Flow Traders:** Order management and risk systems need transactional guarantees. You can't partially fill an order and lose track of it.

**Key concepts:**
- **ACID** — Atomicity, Consistency, Isolation, Durability. Know what each actually means (not the textbook handwave).
- **Isolation levels** — Read Committed, Snapshot Isolation, Serializable. Know the anomalies each prevents.
- **Write skew & phantoms** — subtle bugs that happen under weak isolation
- **Two-phase locking vs MVCC** — pessimistic vs optimistic concurrency control
- **Distributed transactions & 2PC** — and why they're slow and fragile

**Keywords to know:** ACID, isolation levels, read committed, snapshot isolation, serializable, MVCC, 2PL, 2PC, write skew, phantom reads

---

## Trading-Specific Design Patterns

These won't be in DDIA but are essential for a trading firm interview:

### Order Book
- A data structure tracking all outstanding buy/sell orders for an instrument, sorted by price and time priority
- Needs to support fast insert, cancel, and match operations
- Typically implemented with a sorted map (price levels) + doubly-linked lists (orders at each level)

### Market Data Pipeline
- Exchange → Network interface card (NIC) → Kernel bypass → Parser → Strategy engine
- **Kernel bypass (DPDK, Solarflare OpenOnload)** — avoid the OS network stack to save microseconds
- **Multicast** — exchanges often distribute data via UDP multicast

### Tick-to-Trade Latency
- The time from receiving a market data update to sending an order
- Flow Traders likely measures this in **single-digit microseconds**
- Every component in the critical path must be optimized

### Risk Management
- Pre-trade risk checks: position limits, order size limits, price reasonableness
- Must be fast enough to not add latency to the critical path
- Often implemented as in-memory state machines

---

## Common Interview Questions for Trading Firms

1. **Design a market data distribution system** — How would you fan out market data from multiple exchanges to hundreds of internal consumers?

2. **Design an order management system (OMS)** — Track orders through their lifecycle: new → sent → partially filled → filled/cancelled.

3. **Design a price aggregation service** — Combine prices from multiple liquidity sources into a single best price.

4. **Design a risk management system** — Real-time position tracking and limit enforcement across all trading desks.

5. **Design a time-series storage system** — Store and query billions of market data ticks efficiently.

6. **Design a matching engine** — How does an exchange match buy and sell orders? (Price-time priority)

---

## DDIA Chapter Priority Map

| Priority | Chapter | Topic | Relevance to Trading |
|----------|---------|-------|---------------------|
| **Must know** | Ch. 2 | Nonfunctional Requirements | Latency, throughput, reliability |
| **Must know** | Ch. 4 | Storage and Retrieval | Market data storage, indexes |
| **Must know** | Ch. 5 | Encoding and Evolution | Serialization, wire protocols |
| **Must know** | Ch. 6 | Replication | Fault tolerance, failover |
| **Should know** | Ch. 7 | Sharding | Data partitioning strategies |
| **Should know** | Ch. 8 | Transactions | ACID, isolation, consistency |
| **Should know** | Ch. 10 | Consistency and Consensus | Distributed coordination |
| **Should know** | Ch. 12 | Stream Processing | Event-driven architecture |
| Nice to have | Ch. 1 | Trade-Offs in Architecture | Cloud vs self-hosted |
| Nice to have | Ch. 3 | Data Models | Relational vs document |
| Nice to have | Ch. 9 | Distributed Systems Problems | Network faults, clocks |
| Nice to have | Ch. 11 | Batch Processing | ETL, analytics pipelines |
| Skip for now | Ch. 13 | Philosophy of Streaming | Too high-level for interview |
| Skip for now | Ch. 14 | Doing the Right Thing | Ethics, not technical |

---

## How to Structure Your Answer in the Interview

1. **Clarify requirements** (2-3 min)
   - What instruments? (equities, futures, options, crypto?)
   - What scale? (messages per second, number of instruments)
   - What latency requirements? (microseconds? milliseconds?)
   - What's the read/write ratio?

2. **High-level architecture** (5 min)
   - Draw the major components and data flow
   - Identify the critical path (latency-sensitive) vs non-critical path

3. **Deep dive into components** (20-25 min)
   - Data structures and storage choices
   - Communication protocols
   - Failure handling
   - Scaling approach

4. **Trade-offs and extensions** (5-10 min)
   - What did you sacrifice and why?
   - How would you handle 10x growth?
   - What monitoring/observability would you add?

---

## Study Plan for This Week

| Day | Focus | DDIA Reading | Practice |
|-----|-------|-------------|----------|
| **Thu** | Latency & storage fundamentals | Ch. 2 (pp. 37-56), Ch. 4 (pp. 115-150) | Draw a market data pipeline |
| **Fri** | Serialization & replication | Ch. 5 (pp. 161-191), Ch. 6 (pp. 197-243) | Compare protobuf vs FlatBuffers |
| **Sat** | Sharding & transactions | Ch. 7 (pp. 251-271), Ch. 8 (pp. 277-335) | Design an order book |
| **Sun** | Consistency & messaging | Ch. 10 (pp. 401-440), Ch. 12 (pp. 487-529) | Design a market data system |
| **Mon** | Trading-specific patterns | Review trading patterns above | Practice explaining designs out loud |
| **Tue** | Mock interview | — | Time yourself: 1 hour, whiteboard |

---

*This is your starting point. We can drill into any of these topics in as much depth as you need.*
