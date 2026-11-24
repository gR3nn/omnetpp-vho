# Vertical handover in OMNeT++

A discrete-event simulation of vertical handover for a Computer Aided Design project.

The model moves one terminal through overlapping cellular and Wi-Fi coverage while it downloads non-real-time files with deadlines. It compares choosing the fastest access, choosing the cheapest access, and choosing by estimated consumer surplus. The modular configuration follows the course topology and makes each transfer visible in Qtenv. [Course model](https://staff.cs.upt.ro/~todinca/cad/VHO_model.html)

## Build and run

Verified on Linux with **OMNeT++ 6.5.0**, build `261029-ccd51e11a4`. The simulator uses core OMNeT++ with C++, NED, and a generated message. Qtenv needs the OMNeT++ GUI build and a display.

From the repository root, with the simulator installed at your chosen path:

```sh
source /path/to/omnetpp-6.5.0/setenv
make -j4
make smoke
make demo
```

In Qtenv, run **ModularDemo** with **F5** to see file, selection, and acknowledgment messages follow the diagram. Double-click either network to see its internal modules. Use **F4** to advance one event at a time or **F7** to finish in express mode. Run `make legacy-demo` for the earlier moving-terminal view.

## Modular course model

[ModularVho.ned](src/ModularVho.ned) defines the top-level path. `gen` sends files to `svr`, which queues them and asks `alg` to select `network1` (cellular) or `network2` (Wi-Fi). The selected network returns the file to `dest` at completion or expiry. The destination acknowledges the outcome so the server can release the next file.

Inside each network, `databuffer` forwards the file to `dly`. Separate radio and load modules update the conditions used by the delay module. It schedules a chunk according to the rate sampled at the start of that chunk. `loop` subtracts the transmitted bits and returns unfinished files to the buffer. Each file stays on the selected access. Changes between consecutive files incur setup time. An access that goes out of coverage during a file waits for coverage to return or for the deadline to expire. The model has no packet-loss or file-retry mechanism.

Run the seven deterministic cases and the 120-run policy comparison:

```sh
make modular-check
./src/vho -u Cmdenv -n src -c ModularExperiment --result-dir=results/modular-experiment
opp_scavetool export -T s -F CSV-S -o results/modular-experiment.csv results/modular-experiment/*.sca
LC_ALL=C awk -f scripts/summarize-modular.awk results/modular-experiment/*.sca
```

See the [modular report](evidence/modular.md) for the paper-to-model mapping and the policy comparison.

## Original model checks and data export

The following commands use Cmdenv and the OMNeT++ result exporter:

```sh
./src/vho -u Cmdenv -n src -c AccountingCheck
./src/vho -u Cmdenv -n src -c HandoverCheck
./src/vho -u Cmdenv -n src -c PolicyCheck
./src/vho -u Cmdenv -n src -c Experiment --result-dir=results/native-sweep
opp_scavetool export -T s -F CSV-S -o results/native-sweep.csv results/native-sweep/*.sca
```

The three check configurations produce 10 scalar files under `results/`. The full experiment produces 720 scalar files under `results/native-sweep/`. The export contains the raw per-run scalar measurements. See the results report for metric definitions and how to reproduce the aggregate tables.

## Original moving-terminal model

- [Vho.ned](src/Vho.ned) defines the two access networks, traffic source, and terminal. Defaults describe a 600 m route and a 12 m/s terminal that reverses direction at the ends. The cellular disk covers the route and the Wi-Fi disk covers its middle.
- [Access.cc](src/Access.cc) computes a rate at the sampled location. Within radius, rate is peak rate times `(1 − 0.6 × distance/radius) × (1 − variation × U)`, where `U` is a uniform draw. Each access consumes one draw per tick on its own stream, including when idle or out of coverage.
- [Traffic.cc](src/Traffic.cc) sends each [Request](src/Request.msg) with its size and absolute deadline. [Terminal.cc](src/Terminal.cc) samples and serves the FIFO head in 100 ms intervals by default. Rates are held constant over each interval. Service is clipped to the arrival and deadline, and a second queued file starts on a later tick. Expired files earn zero value and incur charges for transmitted bytes.
- [Selection.cc](src/Selection.cc) scores available accesses. Fastest maximizes sampled rate, cheapest minimizes price per MB, and exact ties favor the current access, then cellular. The surplus rule estimates `ETA = setup + remaining bits / sampled rate` and scores each option by `remaining MB × (value per MB × max(0, 1 − ETA / time left) − price per MB)`. It retains the current access if the competing advantage is at most the configured margin. If all available scores are negative, it still chooses the greatest score.
- The terminal re-evaluates during active transfer. Initial attachment is free. A switch blocks transfer until setup ends and retains any transmitted bytes. The terminal retains the last access across idle periods. Out-of-coverage access offers zero rate. With both accesses unavailable, the transfer waits until service returns or the deadline expires.
- Realized value is computed only on completed files using original size and the fraction of the original deadline budget remaining at delivery. Realized surplus subtracts all actual charges, including charges on expired files. The estimated surplus uses only the remaining part of the current file.

The project abstracts away radio packet loss, protocol signaling, contention between terminals, and rate-history prediction. It uses an instantaneous rate forecast, so a policy can make repeated switches or realize a lower score after a change in coverage. The model and numerical parameters are project assumptions.

## Research context

Ormond, Perry, and Murphy describe user-centric access selection for non-real-time transfers that considers delay and consumer surplus. The modular model implements a policy based on the full PIMRC paper linked from the course page. The original moving-terminal model uses its own simpler utility formula. The related economic-model manuscript supplies context for consumer-surplus selection under a user-specified time constraint. [PIMRC paper institutional record](https://pure.ulster.ac.uk/en/publications/network-selection-decision-in-wireless-heterogeneous-networks/) · [Economic-model author manuscript](https://citeseerx.ist.psu.edu/document?doi=10005f340190f806e4c44452df26adce5a133ae6&repid=rep1&type=pdf)

## Results and verification

See [the results report](evidence/README.md), [per-run data](evidence/runs.csv), [scenario means and intervals](evidence/summary.csv), and [paired comparisons](evidence/paired-vs-fastest.csv). All 10 deterministic simulations and 720 experiment simulations ran on the pinned version.

## Develop and verify

Start a feature branch from `develop`. Edit `omnetpp.ini` for workload and scenario settings, `src/Vho.ned` for topology and parameters, or the corresponding C++ module for model behavior. Rebuild with `make -j4` after changing C++ or message definitions. Run the relevant check in Cmdenv, then use Qtenv to inspect changes in the simulation.

1. Reimplement access sampling from the rate equation in your own code. With variation disabled, check the center, boundary, and a point just outside coverage by hand. At the cellular center expect 3 Mbps, and at its boundary expect 1.2 Mbps.
2. Derive your own selection module from a cited source or explicitly stated design. Compare policies at 300 m using a 1 MB file, rates of 3 and 12 Mbps, and prices of 0.18 and 0.26 credits/MB. The transfer times without setup are 8/3 s and 2/3 s. Compare your decisions at 1 s and 30 s with `PolicyCheck`.
3. Implement and independently verify handover interruption. Use a coverage transition with one file in progress, then compare 0 and 0.35 s setup. Check byte conservation, charges on both accesses, and that transfer resumes only after setup. Compare with `HandoverCheck`.
4. Choose a research question for your own extension, change one factor at a time, and explain the observed differences using the recorded rates, deadlines, and switches.
