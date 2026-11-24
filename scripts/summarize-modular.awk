# Usage: awk -f scripts/summarize-modular.awk results/modular-experiment/*.sca
function storeRun(    group, pair) {
    if (policy == "")
        return
    group = deadline SUBSEP policy
    pair = group SUBSEP seed
    if (pair in seen || submitted != 20 || pending != 0 ||
        submitted != completed + expired + declined ||
        cellular < 0 || wifi < 0 || cellular + wifi > 1 + 1e-9) {
        print "Invalid or duplicate run: " sourceFile > "/dev/stderr"
        failed = 1
        return
    }
    seen[pair] = 1
    runs[group]++
    finished[group] += completed
    late[group] += expired
    refused[group] += declined
    delay[group] += completionSeconds
    cellUse[group] += cellular
    wifiUse[group] += wifi
    credits[group] += charges
    surplus[group] += realizedSurplus
}
FNR == 1 {
    storeRun()
    sourceFile = FILENAME
    policy = deadline = seed = ""
    submitted = pending = completed = expired = declined = completionSeconds = charges = realizedSurplus = 0
    cellular = wifi = -1
}
$1 == "itervar" && $2 == "policy" { policy = $3 }
$1 == "itervar" && $2 == "deadline" { deadline = $3 }
$1 == "attr" && $2 == "repetition" { seed = $3 }
$1 != "scalar" { next }
$3 == "submitted" { submitted = $4 }
$3 == "pending" { pending = $4 }
$3 == "completed" { completed = $4 }
$3 == "expired" { expired = $4 }
$3 == "declined" { declined = $4 }
$3 == "completionSeconds" { completionSeconds = $4 }
$3 == "credits" { charges = $4 }
$3 == "realizedSurplus" { realizedSurplus = $4 }
$3 == "utilization" && $2 ~ /network1\.dly$/ { cellular = $4 }
$3 == "utilization" && $2 ~ /network2\.dly$/ { wifi = $4 }
END {
    storeRun()
    split("10s 20s", deadlines)
    split("fastest cheapest ormond", policies)
    print "deadline,policy,runs,completed_pct,expired_pct,declined_pct,mean_delay_s,cellular_use_pct,wifi_use_pct,mean_credits,mean_surplus"
    for (d = 1; d <= 2; d++) {
        for (p = 1; p <= 3; p++) {
            group = deadlines[d] SUBSEP policies[p]
            for (seed = 0; seed < 20; seed++)
                if (!((group SUBSEP seed) in seen))
                    failed = 1
            if (runs[group] != 20) {
                failed = 1
                continue
            }
            count = runs[group]
            averageDelay = finished[group] ? sprintf("%.6f", delay[group] / finished[group]) : ""
            printf "%s,%s,%d,%.4f,%.4f,%.4f,%s,%.4f,%.4f,%.6f,%.6f\n",
                deadlines[d], policies[p], count, 5 * finished[group] / count,
                5 * late[group] / count, 5 * refused[group] / count, averageDelay,
                100 * cellUse[group] / count, 100 * wifiUse[group] / count,
                credits[group] / count, surplus[group] / count
        }
    }
    exit failed
}
