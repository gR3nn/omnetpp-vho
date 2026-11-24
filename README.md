# Vertical handover in OMNeT++

**AI-assisted reference implementation for a Computer Aided Design project.** Use it to study the model, reproduce the experiments, and check your own design. The student is responsible for independent contributions, AI disclosure, and the final submission.

The model moves one terminal through overlapping cellular and Wi-Fi coverage while it downloads non-real-time files with deadlines. It compares choosing the fastest access, choosing the cheapest access, and choosing by estimated consumer surplus.

## Build and run

Verified on Linux with **OMNeT++ 6.5.0**, build `261029-ccd51e11a4`. The simulator uses core OMNeT++ with C++, NED, and a generated message. The experiment runner needs Python 3 and its standard library. Qtenv needs the OMNeT++ GUI build and a display.

From the repository root, with the simulator installed at your chosen path:

```sh
source /path/to/omnetpp-6.5.0/setenv
make -j4
make smoke
make demo
python3 scripts/run.py
python3 scripts/run.py --output results/experiment
```

In Qtenv, run the Demo configuration with **F5** to animate the terminal and show its selected access. **F7** runs in express mode. Inspect the terminal's position, selected access, handover count, and completed count. Cmdenv writes scalars under `results/`. The experiment runner uses a fresh temporary directory for each configuration, then writes the three CSVs to the chosen output directory.

## Model

- [Vho.ned](src/Vho.ned) defines the two access networks, traffic source, and terminal. Defaults describe a 600 m route and a 12 m/s terminal that reverses direction at the ends. The cellular disk covers the route and the Wi-Fi disk covers its middle.
- [Access.cc](src/Access.cc) computes a rate at the sampled location. Within radius, rate is peak rate times `(1 − 0.6 × distance/radius) × (1 − variation × U)`, where `U` is a uniform draw. Each access consumes one draw per tick on its own stream, including when idle or out of coverage.
- [Traffic.cc](src/Traffic.cc) sends each [Request](src/Request.msg) with its size and absolute deadline. [Terminal.cc](src/Terminal.cc) samples and serves the FIFO head in 100 ms intervals by default. Rates are held constant over each interval. Service is clipped to the arrival and deadline, and a second queued file starts on a later tick. Expired files earn zero value and incur charges for transmitted bytes.
- [Selection.cc](src/Selection.cc) scores available accesses. Fastest maximizes sampled rate, cheapest minimizes price per MB, and exact ties favor the current access, then cellular. The surplus rule estimates `ETA = setup + remaining bits / sampled rate` and scores each option by `remaining MB × (value per MB × max(0, 1 − ETA / time left) − price per MB)`. It retains the current access if the competing advantage is at most the configured margin. If all available scores are negative, it still chooses the greatest score.
- The terminal re-evaluates during active transfer. Initial attachment is free. A switch blocks transfer until setup ends and retains any transmitted bytes. The terminal retains the last access across idle periods. Out-of-coverage access offers zero rate. With both accesses unavailable, the transfer waits until service returns or the deadline expires.
- Realized value is computed only on completed files using original size and the fraction of the original deadline budget remaining at delivery. Realized surplus subtracts all actual charges, including charges on expired files. The estimated surplus uses only the remaining part of the current file.

The project abstracts away radio packet loss, protocol signaling, contention between terminals, and rate-history prediction. It uses an instantaneous rate forecast, so a policy can make repeated switches or realize a lower score after a change in coverage. The model and numerical parameters are project assumptions. The supplied attachment names were available, but the files themselves were unavailable in this run.

## Research context

Ormond, Perry, and Murphy describe user-centric access selection for non-real-time transfers that considers delay and consumer surplus. The PIMRC publisher full text could not be retrieved during this run. The institutional publication record supports the research motivation. The related economic-model manuscript supplies context for consumer-surplus selection under a user-specified time constraint. [PIMRC paper institutional record](https://pure.ulster.ac.uk/en/publications/network-selection-decision-in-wireless-heterogeneous-networks/) · [Economic-model author manuscript](https://citeseerx.ist.psu.edu/document?doi=10005f340190f806e4c44452df26adce5a133ae6&repid=rep1&type=pdf)

## Results and verification

See [the results report](evidence/README.md), [per-run data](evidence/runs.csv), [scenario means and intervals](evidence/summary.csv), and [paired comparisons](evidence/paired-vs-fastest.csv). All 10 deterministic simulations and 720 experiment simulations ran on the pinned version.

## AI assistance and independent work

Codex generated the reference model, scenario configuration, build and experiment scripts, documentation, experiment outputs, and repository commits. The PR and commit history record the generated implementation.

1. Reimplement access sampling from the rate equation in your own code. With variation disabled, check the center, boundary, and a point just outside coverage by hand. At the cellular center expect 3 Mbps, and at its boundary expect 1.2 Mbps.
2. Derive your own selection module from a cited source or explicitly stated design. Compare policies at 300 m using a 1 MB file, rates of 3 and 12 Mbps, and prices of 0.18 and 0.26 credits/MB. The transfer times without setup are 8/3 s and 2/3 s. Compare your decisions at 1 s and 30 s with `PolicyCheck`.
3. Implement and independently verify handover interruption. Use a coverage transition with one file in progress, then compare 0 and 0.35 s setup. Check byte conservation, charges on both accesses, and that transfer resumes only after setup. Compare with `HandoverCheck`.
4. Choose a research question for your own extension, change one factor at a time, and explain the observed differences using the recorded rates, deadlines, and switches. Record which parts you wrote or reimplemented and which parts used AI assistance.
