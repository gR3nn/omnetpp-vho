#!/usr/bin/env python3
"""Run deterministic checks and summarize the paired-seed experiment."""
import argparse
import csv
from collections import defaultdict
from math import isclose, sqrt
from pathlib import Path
import shlex
from statistics import mean, stdev
import subprocess
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
METRICS = ("completionRate", "credits", "meanCompletionSeconds", "handovers",
           "handoverInterruptionSeconds", "deliveredMbps")


def run(config):
    with TemporaryDirectory(prefix="vho-") as directory:
        command = ["./src/vho", "-u", "Cmdenv", "-n", "src", "-c", config,
                   f"--result-dir={directory}", "--cmdenv-status-frequency=0s"]
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        return [read_run(path) for path in sorted(Path(directory).glob("*.sca"))]


def read_run(path):
    run = {}
    for line in path.read_text().splitlines():
        if line.startswith(("itervar ", "attr repetition ")):
            _, key, value = shlex.split(line)
            run[key] = value
        if line.startswith("scalar VerticalHandover.terminal "):
            _, _, key, value = line.split()
            run[key] = float(value)
    assert run["submitted"] == run["completed"] + run["expired"] + run["pending"], run
    assert run["completedBytes"] <= run["transferredBytes"] + 1e-6, run
    return run


def check():
    accounting = {row["limit"]: row for row in run("AccountingCheck")}
    short, long = accounting["0.5s"], accounting["2s"]
    assert (short["completed"], short["expired"]) == (0, 1), short
    assert isclose(short["credits"], .5 * 3e6 * (1 - .6 * 300 / 310) * .18 / 8e6), short
    assert (long["completed"], long["expired"]) == (1, 0), long
    assert isclose(long["credits"], .18 * .12), long

    handovers = {row["setup"]: row for row in run("HandoverCheck")}
    assert len(handovers) == 2
    for row in handovers.values():
        assert row["completed"] == row["handovers"] == 1, row
        assert isclose(row["transferredBytes"], 1e6), row
        assert isclose(row["handoverInterruptionSeconds"], float(row["setup"][:-1])), row
    assert handovers["0.35s"]["completionSeconds"] > handovers["0s"]["completionSeconds"], handovers

    policies = run("PolicyCheck")
    assert len(policies) == 6
    for row in policies:
        uses_wifi = row["policy"] == "fastest" or (
            row["policy"] == "surplus" and row["deadline"] == "1s")
        completes = uses_wifi or row["deadline"] == "30s"
        duration = 8 / (12 if uses_wifi else 3)
        charge = (.26 if uses_wifi else .18) * min(1, float(row["deadline"][:-1]) / duration)
        assert row["completed"] == int(completes) and row["handovers"] == 0, row
        assert isclose(row["credits"], charge), row
        assert isclose(row["completionSeconds"], duration if completes else 0), row
    print("10 deterministic simulations passed")


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def interval(values):
    # Two-sided Student t multiplier for 19 degrees of freedom.
    assert len(values) == 20
    return {"mean": mean(values), "ci95HalfWidth": 2.093024 * stdev(values) / sqrt(len(values))}


def experiment(output):
    rows = run("Experiment")
    assert len(rows) == 720
    groups = defaultdict(dict)
    for row in rows:
        assert row["submitted"] == 20 and row["pending"] == 0, row
        row["seed"] = int(row.pop("repetition"))
        row["completionRate"] = row["completed"] / row["submitted"]
        row["meanCompletionSeconds"] = row["completionSeconds"] / row["completed"] if row["completed"] else ""
        row["deliveredMbps"] = row["completedBytes"] * 8 / 125e6
        key = tuple(row[field] for field in ("deadline", "price", "setup", "policy"))
        assert row["seed"] not in groups[key]
        groups[key][row["seed"]] = row
    summary, paired = [], []
    for (deadline, price, setup, policy), seeds in sorted(groups.items()):
        assert set(seeds) == set(range(20))
        case = dict(deadline=deadline, price=price, setup=setup, policy=policy)
        baseline = groups[(deadline, price, setup, "fastest")]
        for metric in METRICS:
            values = [row[metric] for row in seeds.values() if row[metric] != ""]
            stats = interval(values) if len(values) == 20 else {
                "mean": mean(values) if values else "", "ci95HalfWidth": ""}
            summary.append(dict(case, metric=metric, n=len(values), **stats))
            if policy != "fastest" and metric in ("completionRate", "credits", "handovers"):
                differences = [row[metric] - baseline[seed][metric] for seed, row in seeds.items()]
                paired.append(dict(case, metric=metric, n=20, **interval(differences)))
    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "runs.csv", rows)
    write_csv(output / "summary.csv", summary)
    write_csv(output / "paired-vs-fastest.csv", paired)
    print(f"{len(rows)} simulations summarized in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="also run the full sweep and save CSVs here")
    args = parser.parse_args()
    check()
    if args.output:
        experiment(args.output)
