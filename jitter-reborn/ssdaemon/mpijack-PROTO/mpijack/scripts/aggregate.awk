BEGIN {
        total_imiss = 0;
        total_dmiss = 0;
        total_cycles = 0;
}

{
    total_dmiss = $8 + total_dmiss;
    total_imiss = $9 + total_imiss;
    total_cycles = $10 + total_cycles;
}

END {
    total_miss = total_dmiss + total_imiss
    opm = total_cycles / total_miss
    print total_miss, total_cycles, opm
}
