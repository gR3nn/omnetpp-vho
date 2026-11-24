#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
for config in ModularQueueCheck ModularOutageCheck ModularSwitchCheck ModularPolicyCheck ModularDeclineCheck; do
    if ! ./src/vho -u Cmdenv -n src -c "$config" --result-dir="$scratch" >"$scratch/log" 2>&1; then
        cat "$scratch/log"
        exit 1
    fi
done

for file in "$scratch"/*.sca; do
    awk '
        function expect(module, metric, expected,    key, delta) {
            key = module "." metric
            delta = values[key] - expected
            if (!(key in values) || delta > 1e-9 || delta < -1e-9) {
                print FILENAME ": expected " key "=" expected ", got " values[key]
                failed = 1
            }
        }
        $1 == "itervar" { variables[$2] = $3 }
        $1 == "scalar" {
            sub(/^ModularVerticalHandover\./, "", $2)
            values[$2 "." $3] = $4 + 0
        }
        END {
            expect("svr", "submitted", 2)
            expect("svr", "pending", 0)
            if (FILENAME ~ /ModularQueueCheck/) {
                expect("dest", "completed", 2)
                expect("dest", "meanCompletionSeconds", .0025)
                expect("dest", "credits", .00024)
                expect("network1.dly", "utilization", 0)
                expect("network2.dly", "utilization", .004)
                expect("network2.dly", "chunks", 4)
                expect("network2.dly", "bytesSent", 6000)
            } else if (FILENAME ~ /ModularOutageCheck/) {
                expect("dest", "completed", 0)
                expect("dest", "expired", 2)
                expect("dest", "credits", 0)
            } else if (FILENAME ~ /ModularSwitchCheck/) {
                expect("dest", "completed", 2)
                expect("svr", "handovers", 1)
                expect("network1.dly", "bytesSent", 3000)
                expect("network2.dly", "bytesSent", 3000)
                busy = values["network1.dly.busySeconds"] + values["network2.dly.busySeconds"]
                expect("dest", "completionSeconds", (variables["setup"] + 0) + busy)
            } else if (FILENAME ~ /ModularPolicyCheck/) {
                expect("dest", "completed", 2)
                if (variables["policy"] == "ormond") {
                    expect("network1.dly", "bytesSent", 240000)
                    expect("dest", "meanCompletionSeconds", .32)
                } else {
                    expect("network2.dly", "bytesSent", 240000)
                    expect("dest", "meanCompletionSeconds", .08)
                }
            } else if (FILENAME ~ /ModularDeclineCheck/) {
                expect("dest", "completed", 0)
                expect("dest", "declined", 2)
                expect("dest", "expired", 0)
            } else {
                failed = 1
            }
            exit failed
        }
    ' "$file"
done
set -- "$scratch"/*.sca
test "$#" -eq 7
printf '%s\n' 'Seven modular check runs passed.'
