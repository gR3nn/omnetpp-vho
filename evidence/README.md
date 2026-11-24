# Experiment results

OMNeT++ 6.5.0 build `261029-ccd51e11a4` ran 720 Cmdenv simulations. The factorial design combines three policies, deadlines of 5, 10, and 20 s, Wi-Fi prices of 0.04 and 0.26 credits/MB, handover delays of 0.1 and 0.8 s, and 20 seed sets (0–19). Each run issues twenty 4 MB files every 5 s and lasts 125 s. Cellular costs 0.18 credits/MB. Every file is completed or expired by the end of the run.

The table shows means at the higher Wi-Fi price with a 10 s deadline and a 0.1 s handover delay.

| Policy | Completed | Credits charged | Handovers | Realized surplus |
| --- | ---: | ---: | ---: | ---: |
| Fastest | 49.75% | 13.243 | 4.0 | −3.652 |
| Cheapest | 0.00% | 4.333 | 0.0 | −4.333 |
| Surplus | 36.50% | 10.691 | 10.7 | −3.079 |

Relative to fastest in this scenario, surplus reduced completion by 13.25 percentage points (paired 95% interval ±2.87 points) while increasing realized surplus by 0.573 credits (±0.561). At 0.8 s setup with the same price and deadline, realized surplus for the surplus policy fell 2.729 credits below fastest (±0.090). This sensitivity is specific to the sampled rates, pricing, and workload. At 0.04 credits/MB and 0.1 s setup, all three policies produced identical outcomes for each of the three deadlines. Wi-Fi offered both the higher sampled rate within its coverage and the lower price in those scenarios.

## CSVs

- [runs.csv](runs.csv) contains one row per simulation, including counts, credits, file bytes, completion latency sum, handover time, and realized value and surplus.
- [summary.csv](summary.csv) reports per-scenario means and two-sided Student t 95% confidence interval half-widths over 20 seeds. Mean completion latency is the average of each run's mean latency among its completed files. The count `n` excludes runs with zero completions for this metric. Blank values indicate unavailable estimates or intervals. Delivered Mbps counts only bytes in completed files and uses the full 125 s run as its denominator.
- [paired-vs-fastest.csv](paired-vs-fastest.csv) reports policy-minus-fastest differences within the same seed and scenario. The two access RNG streams consume one draw per tick regardless of policy, so matching seeds share the same rate samples. The intervals describe replication uncertainty for each individual comparison.

Run `python3 scripts/run.py --output results/experiment` after building to regenerate these CSVs and check deterministic accounting, deadlines, policy decisions, and handover interruption.
