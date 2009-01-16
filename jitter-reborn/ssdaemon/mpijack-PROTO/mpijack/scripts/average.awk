
END {print t/n}

{ if (NR > 3) { t += $1;  n += 1; } }
