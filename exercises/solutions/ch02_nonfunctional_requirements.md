# Chapter 2 Solutions — Defining Nonfunctional Requirements

Selected answers for [Chapter 2 Exercises — Defining Nonfunctional Requirements](../ch02_nonfunctional_requirements.md).

## 1.2 (a)

Sorted: `11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 16, 250, 480`

- **Mean:** (11x3 + 12x4 + 13x4 + 14x4 + 15x2 + 16 + 250 + 480) / 20 = (33+48+52+56+30+16+250+480)/20 = 965/20 = **48.25 ms**
- **Median (p50):** average of 10th and 11th values = (13 + 13)/2 = **13 ms**
- **p90:** 18th value = **16 ms** (or interpolate: 18.5th -> between 16 and 250)
- **p95:** 19th value = **250 ms**

The mean (48.25 ms) is dragged up by two outliers and does not represent what the typical user experiences. The median (13 ms) is far more representative. For an SLO you'd likely use p95 or p99 to capture those outliers.

## 1.3 (a)

P(at least one slow) = 1 - P(all fast) = 1 - 0.99^5 ~= 1 - 0.951 = **0.049 ~= 4.9%**

## 1.3 (c)

P(at least one slow) = 1 - 0.99^50 ~= 1 - 0.605 = **0.395 ~= 39.5%**

Nearly 40% of composite requests hit a tail latency event.

## 2.1 (a)

5,800 posts/sec x 200 followers = **1,160,000 timeline writes/sec**

## 2.1 (b)

30,000,000 writes for a single post. To deliver within 5 seconds: 30,000,000 / 5 = **6,000,000 writes/sec** - just from one post.

## 3.2 (a)

Overall = 0.999 x 0.999 x 0.999 = 0.997 = **99.7%** -> about **26.3 hours downtime/year**

## 3.2 (b)

App tier availability = 1 - (0.001)^2 = 0.999999 ~= **99.9999%**
Overall ~= 0.999 x 0.999999 x 0.999 ~= **99.8%** -> about **17.5 hours downtime/year**
(The bottleneck is now the LB and DB, each at 99.9%.)

## 4.3 (a)

100,000 x 200 = 20,000,000 bytes/sec = **20 MB/s** = **0.16 Gb/s**

## 4.3 (b)

20 MB/s x 8 x 3600 = **576 GB**

## 4.3 (c)

20 MB/s x 3 = **60 MB/s** total write throughput
