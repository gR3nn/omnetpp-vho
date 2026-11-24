# Modular vertical handover experiment

## Literature policy

The `ormond` policy uses Ormond, Perry, and Murphy, *Network Selection Decision in Wireless Heterogeneous Networks*, PIMRC 2005, sections II–IV. It predicts rate from the last five completed file transfers on each network. An unobserved link uses the minimum rate that would finish by the deadline. A prediction too slow for the deadline is discarded after six decisions in which it was too slow. The policy then uses the same minimum-rate default until new observations arrive. For eligible links, the policy maximizes estimated willingness to pay minus the price of the file.

Figure 3 keeps utility at its maximum up to the ideal delay, decreases it linearly to 25% of the maximum at the deadline, and drops it to zero after the deadline. The paper's example uses a 5 s ideal delay and a 20 s deadline. The experiment here uses a 2 s ideal delay and varies the deadline. The same realized utility is used to evaluate all three policies. When no link qualifies, this implementation uses the no-selection outcome reported in section IV-C. Figure 1 instead shows a cheapest-network fallback when every available link misses the predicted deadline. [Course page and full paper links](https://staff.cs.upt.ro/~todinca/cad/VHO_model.html) · [Publication record](https://pure.ulster.ac.uk/en/publications/network-selection-decision-in-wireless-heterogeneous-networks/)

The experiment uses a moving terminal with cellular and Wi-Fi rates, a FIFO, and setup between accesses. Queue age and setup count against the remaining deadline. Observed file throughput uses the time from the first chunk's start to the last chunk's completion, including any outage wait between chunks. The original paper's scenario uses two WLANs and a semi-stationary terminal in ns-2. The results below apply to the OMNeT++ scenario.

## Comparison

OMNeT++ 6.5.0 ran three policies, two deadlines, and 20 paired load seeds for 120 runs. Each run generates twenty 4 MB files, one every 5 s, and observes both links for 125 s. Cellular costs 0.18 credits/MB and Wi-Fi costs 0.26 credits/MB. Each condition generator consumes its own RNG on a fixed cadence independent of the policy.

| Deadline | Policy | Completed | Mean delay of completed files | Cellular utilization | Wi-Fi utilization |
| --- | --- | ---: | ---: | ---: | ---: |
| 10 s | Fastest | 40% | 6.66 s | 35.49% | 34.88% |
| 10 s | Cheapest | 0% | — | 83.94% | 0.00% |
| 10 s | Ormond | 0% | — | 83.94% | 0.00% |
| 20 s | Fastest | 45% | 16.65 s | 46.76% | 31.61% |
| 20 s | Cheapest | 5% | 19.09 s | 91.95% | 0.00% |
| 20 s | Ormond | 25% | 16.64 s | 70.99% | 14.17% |

At 10 s, the Ormond policy starts with the cheaper link because both default predictions place completion at the deadline. In these runs no file completes on cellular before the 10 s deadline, so the policy gains no completed-transfer estimate that would change its decision. At 20 s it completes more files than cheapest and costs less than fastest (8.01 versus 11.97 credits per run), with a lower completion rate. Mean delay describes completed files only. The policies complete different sets of files, so similar means at 20 s can coexist with very different completion rates.

Utilization is each link's chunk transmission time divided by 125 s. It excludes setup, idle time, and outage waits. Expired files can consume bandwidth and incur charges for completed chunks. The model has no whole-file retries. [Full aggregate CSV](modular-summary.csv)
