# Chapter 2 Exercises — Defining Nonfunctional Requirements

Difficulty: * = warm-up, ** = interview-level, *** = deep thinking


---

## Section 1: Performance — Latency, Throughput, and Response Time

### Exercise 1.1 — Terminology Precision (*)

The book defines four distinct time-related concepts: *response time*, *service time*, *queueing delay*, and *latency* (in the narrow sense). For each of the following scenarios, identify which concept is being described:

(a) A market data packet takes 0.3 ms to travel from the exchange to your server's NIC.

(b) Your order matching function takes 1.2 µs to execute once it starts.

(c) A request sits in a thread pool queue for 5 ms because all worker threads are busy.

(d) A user clicks "submit order" and sees confirmation 47 ms later.

(e) After your service finishes computing the response, it waits 2 ms for the outbound network buffer to drain.

#### My Solution

(a) (Network) Latency
(b) service time
(c) queueing delay
(d) Reponse time
(e) Queueing Delay

### Exercise 1.2 — Percentile Calculations (**)

A service recorded the following 20 response times (in ms) over 1 minute:

```
12, 15, 11, 14, 13, 250, 16, 12, 14, 11,
13, 15, 12, 14, 480, 11, 13, 12, 14, 13
```

(a) Compute the mean, median (p50), p90, and p95.

(b) Which metric would you put in an SLO and why? Would the mean be suitable?

(c) A junior engineer proposes to compute percentiles separately for each of 4 backend servers and then average them. Why is this mathematically wrong? What should you do instead?

#### My Solution
(a) Won't compute compute but mean is just summing and dividing by number of elements (here 20), others I just sort and from sorted take the 11th element for median, 19th for p90, 20th for p95 (maybe I've shifted one to the right when I shouldn't have, in that case 10, 18, 19)

(b) Uptime (how often the service can be used), Response time (how long we wait on average), 95th percentile REsponse time (how long we wait in reasonable bad case)

(c) Suppose we have one server with value 1e10 and the values on others are around 1-100, average is skewed by outliars but percentiles are stable under them. You should merge the lists and then take percentile of those. There's also an algorithm for finding median of K-sorted lists, you have numbers a1, ..., ak and know that a1 + a2 + ... ak = N / 2 (where N is the total number of elements) and you can can find the ranges of those values where this can hold and you check those for the median or other percentiles.


### Exercise 1.3 — Tail Latency Amplification (**)

Your web application fans out each user request to 5 independent backend services in parallel. Each backend has a p99 latency of 50 ms (i.e., 1% of calls take >= 50 ms).

(a) What is the probability that at least one of the 5 backend calls takes >= 50 ms?

(b) If the web application waits for all 5 to complete before responding, what is the effective p99 of the *composite* request? (You may assume calls are independent.)

(c) This effect is called "tail latency amplification." If you fan out to N=50 services (as some large systems do), what is the probability that at least one call hits the tail?

#### My Solution
(a) The probabilities are independent, we have a $p = 0.01$ chance per backend call, so our probability is $1 - (1 - p)^4 = 1 - 0.99^4$ where $0.99^4$ is the probability that none of them take more then 50ms.

(b) I think it would be the max over the individual latencies, how to compute that I don't know.

(c) Same computation as in (a); 1 - 0.99^50.

### Exercise 1.4 — Throughput vs Latency Under Load (**)

The chapter shows a graph where response time stays flat at low throughput and then rises sharply as throughput approaches max capacity (Figure 2-3).

(a) Explain intuitively why this happens, using the concept of queueing.

(b) A system has a max throughput of 10,000 requests/sec. At 5,000 req/s the p50 latency is 2 ms. At 9,000 req/s the p50 is 25 ms. At 9,500 req/s the p50 is 200 ms. Sketch what happens at 10,500 req/s. What is this called?

(c) Name three mechanisms the book describes that prevent a system from entering this state.

#### My Solution
(a) If we have nothing in the queue we can immediately process the request, so we get the normal performance time. If each request can be processed in 1 second, it would just be a second, if we can process 2 requests in a second and we queue up 3 requests per second we finish the first two requests in the first second, then we have 4 requests to do in the second second, those get processed and then we have 5 requests to process in the third second and so on. Because queues process in a FIFO way our newly queued tasks take K / 2 seconds where K is the number of requests already queued up.

(b) At 10500 req/s we are making more requests than we can process and we accumulate more and more requests as time goes on, so we have to drop those and they make re-try requests and the system can't process them all. I don't remember the term.

### Exercise 1.5 — SLO Design (**)

You are designing the SLO for a new market data distribution service at a trading firm. The service receives raw data from exchanges and distributes it to internal consumers.

(a) Would you define the SLO in terms of mean response time? If not, which percentile and why?

(b) An SLO states: "p99 latency < 5 ms". An SLA states: "If p99 exceeds 5 ms for more than 5 minutes in any 1-hour window, the provider owes a credit." What is the difference between these two concepts?

(c) Why might Amazon use p99.9 for its internal services while a less latency-sensitive business might only use p95?


#### My Solution
(a) Mean response time is not that relevant, we need to measure bad response times and handle those, even if mean response time suffers, so our SLO should be 99th percentile response time, as we need reliable systems. (I guess we could even take worst response time but that one is very "un-smooth"/unstable as random jitters and network delays and so on can make this worse despite us not being able to do anything about it. Basically random acts of god make that metric horrible, so we shoud stick to a more stable metric like 99th percentile).

(b) SLAs are contractual obligations with particular constraints and penalties. They are guaranteed to hold, while SLOs are targets for the architecture but there is no legally binding aspect to them.

(c) Optimising for p99.9 will have a cost (either more expensive hardware or worse mean response time), which for the less latency-sensitive business might be added costs without benefits, so they'd do a more forgiving metric, which also allows them to focus on mean response time.

---

## Section 2: The Social Network Case Study — Fan-out and Materialization

### Exercise 2.1 — Fan-out Arithmetic (**)

The chapter gives these numbers for a Twitter-like service:
- 500 million posts/day ≈ 5,800 posts/sec (peak: 150,000/sec)
- Average user: 200 followers
- Celebrity: up to 100 million followers

**Approach 1 (pull/query on read):** When a user opens their timeline, query the `follows` and `posts` tables with a JOIN.

**Approach 2 (push/fan-out on write):** When a user posts, write a copy to each follower's timeline cache.

(a) Under Approach 2, compute the average number of timeline writes per second at the 5,800 posts/sec rate.

(b) What happens when a celebrity with 30 million followers posts? How many writes does that single post generate? At what rate must the system process them if the goal is delivery within 5 seconds?

(c) The book describes a hybrid approach for celebrities. Explain it. Why is this a good trade-off?

#### My Solution
(a) We have to write one post copy to each follower, so on average we have 5800 users writing, each have on average have 200 followers so we on average have $200 * 5800 = 1,160,000$ writes per second.

(b) Each follower gets one write to its cache so 30 millions writes for that one post, so it must be processed in (30 million writes) / (N writes / second) = 5 seconds, i.e. 6 million writes per second.

(c) There are few celebrities with a lot of followers, so logic can optimise for them in ways which scale badly across number of posters but scale well by number of recipients per post.

### Exercise 2.2 — Pull vs Push Trade-offs (*)

Fill in the table:

| Property | Pull (query on read) | Push (fan-out on write) |
| --- | --- | --- |
| Write cost per post | 1 write; store the post once | O(number of followers); duplicate to each follower's timeline |
| Read cost per timeline | Higher; query and merge posts from followed accounts | Lower; read the already materialized timeline |
| Staleness of timeline | Fresh at read time | Can be stale until fan-out finishes |
| Celebrity post handling | No extra handling needed | Expensive; usually needs a hybrid or special-case path |
| Storage overhead | Low; little or no duplication | High; duplicate posts across many follower timelines |


### Exercise 2.3 — Applying to Trading (***)

A market data distribution system receives price updates from an exchange (50,000 updates/sec across 10,000 instruments) and needs to deliver them to 200 internal consumers. Each consumer only cares about a subset of instruments (typically 500-2,000).

(a) Design two approaches — one pull-based and one push-based — mirroring the social network case study. What are the trade-offs?

(b) Which approach would a latency-sensitive trading firm prefer and why?

(c) What data structure would you use to track which consumers care about which instruments? What is the fan-out factor?


---

## Section 3: Reliability and Fault Tolerance

### Exercise 3.1 — Fault vs Failure (*)

Classify each of the following as a *fault* or a *failure*. If it's a fault, state whether it could escalate to a failure and under what conditions.

(a) One of three database replicas goes offline.

(b) A user submits a trade and gets a timeout error.

(c) A network switch in rack 7 loses power.

(d) The pricing engine returns stale prices because it lost its market data feed but doesn't know it.

(e) A deployment introduces a bug that only triggers under a specific rare input pattern.


### Exercise 3.2 — Redundancy and SPOF (**)

You have a system with three components in series: load balancer → application server → database.

(a) If each component has 99.9% uptime independently, what is the overall system availability? (Express as a percentage and as minutes of downtime per year.)

(b) You add a second application server behind the load balancer. Assuming the load balancer routes around a dead server instantly, what is the new availability of the app tier? What is the new overall system availability?

(c) What is still a single point of failure (SPOF) in this design? How would you address it?


### Exercise 3.3 — Hardware vs Software Faults (**)

The book distinguishes hardware faults (mostly independent, uncorrelated) from software faults (often correlated, affecting many nodes simultaneously).

(a) Give two examples of each from the chapter.

(b) Why are correlated faults more dangerous than independent faults?

(c) The chapter mentions that a firmware bug caused all SSDs of a certain model to fail at exactly 32,768 hours of operation. Why is this type of fault particularly insidious compared to random hardware failure?

(d) What is *chaos engineering* and how does it relate to fault tolerance?


### Exercise 3.4 — Metastable Failures (***)

The chapter describes a scenario where a system becomes overloaded, clients time out and retry, creating even more load — a *retry storm*. Even after the original load spike subsides, the system stays overloaded. This is called a *metastable failure*.

(a) Draw a feedback diagram showing how retries amplify load.

(b) The chapter mentions three mechanisms to prevent this: *exponential backoff*, *circuit breakers*, and *load shedding*. Explain each in 1-2 sentences.

(c) Design a simple retry policy for a trading system that sends orders to an exchange. What constraints differ from a typical web service?


---

## Section 4: Scalability

### Exercise 4.1 — Understanding Load (*)

(a) Define *linear scalability*. If a system has linear scalability and currently handles 10,000 req/s on 4 servers, how many servers do you need for 25,000 req/s?

(b) What does it mean when cost grows faster than linearly with load? Give an example from the chapter.

(c) The chapter describes three scaling architectures: shared-memory, shared-disk, and shared-nothing. Match each to its best use case and main limitation.


### Exercise 4.2 — Scaling the Social Network (**)

Your social network has grown from 10 million to 100 million users.

(a) The materialized timeline approach stores one copy of each post per follower. Estimate how much additional storage this requires per day if the average post is 200 bytes and the average fan-out is 200.

(b) At what point might you need to shard the timeline cache? How would you shard it? (By user ID? By user ID hash? Something else?)

(c) A colleague says "just use a bigger server" (vertical scaling). Under what conditions is this actually the right call, per the chapter's advice?


### Exercise 4.3 — Back-of-the-Envelope Estimation (**)

A trading firm's market data system processes 100,000 messages per second. Each message is 200 bytes.

(a) What is the data rate in MB/s and Gb/s?

(b) If you need to store 8 hours of data for replay, how much storage do you need?

(c) If each message must be written to 3 replicas, what is the total write throughput the storage layer must support?

(d) You want to add a rolling 10-minute percentile computation over message processing latencies (one latency value per message). If you sort the window each time, what is the computational cost? What data structure does the book suggest instead?


---

## Section 5: Maintainability

### Exercise 5.1 — The Three Principles (*)

The chapter identifies three sub-properties of maintainability: *operability*, *simplicity*, and *evolvability*.

(a) For each, give a concrete example of a design decision that improves it and one that hurts it.

(b) The chapter argues that the *majority of software cost* is not initial development but ongoing maintenance. Why does this matter for how you design a system today?

(c) What is the difference between *essential complexity* and *accidental complexity*? Give an example of each in the context of a trading system.


### Exercise 5.2 — Irreversibility (***)

The chapter mentions that one major factor making change difficult in large systems is *irreversibility*.

(a) You are migrating a trading firm's order management system from Oracle to PostgreSQL. Why is this change hard to reverse?

(b) Describe two architectural strategies that would make this migration more reversible (the chapter hints at these).

(c) How does the concept of *evolvability* relate to the encoding and schema evolution topics in Chapter 5?


---

## Section 6: Implementation Exercises

### Exercise 6.1 — Percentile Calculator (C++) (**)

Implement a class `PercentileTracker` that:
- Accepts response time samples via `void record(double ms)`
- Returns the current p50, p95, and p99 over a sliding window of the last N samples
- Is efficient enough to handle 100,000 samples/sec

Think about: What data structure would you use? What is the time complexity of each operation? The naive approach (sort the window each time) is O(N log N) per query — can you do better?

Starter code:

```cpp
#include "common.hpp"

namespace ds_sysdes {

class PercentileTracker {
public:
    explicit PercentileTracker(usize window_size);

    void record(f64 ms);
    auto percentile(f64 p) const -> f64;  // p in [0, 1], e.g. 0.99 for p99

    auto p50() const -> f64 { return percentile(0.50); }
    auto p95() const -> f64 { return percentile(0.95); }
    auto p99() const -> f64 { return percentile(0.99); }

private:
    // Your data structure here
};

} // namespace ds_sysdes
```


### Exercise 6.2 — Fan-out Simulator (C++) (***)

Simulate the social network fan-out problem. Model:
- N users, each following F random other users
- Posts arrive at a configurable rate
- Implement both pull (query on read) and push (fan-out on write) strategies
- Measure and compare: write amplification, read latency, memory usage

This is deliberately open-ended — the goal is to build intuition about the trade-offs, not to build a production system.


### Exercise 6.3 — Exponential Backoff with Jitter (C++) (*)

Implement a retry policy with exponential backoff and jitter:

```cpp
#include "common.hpp"

namespace ds_sysdes {

class RetryPolicy {
public:
    RetryPolicy(u32 max_retries, f64 base_delay_ms, f64 max_delay_ms);

    // Returns the delay (in ms) before the i-th retry, or std::nullopt if
    // max retries exceeded.
    auto next_delay(u32 attempt) -> std::optional<f64>;

private:
    u32 max_retries_;
    f64 base_delay_ms_;
    f64 max_delay_ms_;
};

} // namespace ds_sysdes
```

The delay for attempt `i` should be: `min(base_delay * 2^i + jitter, max_delay)` where jitter is a random value in `[0, base_delay * 2^i)`.

Why is jitter important? What happens without it when many clients retry simultaneously?

For selected answers, see [solutions/ch02_nonfunctional_requirements.md](solutions/ch02_nonfunctional_requirements.md).
