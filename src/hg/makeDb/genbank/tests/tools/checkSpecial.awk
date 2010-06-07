# check code exceptions (special cases) in a ra file

# heuristics for bad cases don't necessarly work.

BEGIN {
    OFS="\t";
}

/^$/ {
    proc = 1;
}

proc && (items["translExcept"] ~ /:Sec/) {
    print items["acc"], items["org"], "selenocysteine","";
}
proc && ((items["translExcept"] !~ /:Sec/) && (items["translExcept"] ~ /:OTHER/)) {
    print items["acc"], items["org"], "selenocysteine", "translExcept_OTHER";
}

proc && (items["translExcept"] ~ /:Met/) {
    print items["acc"], items["org"], "non-AUG", "";
}
proc && ((items["translExcept"] !~ /:Met/) && (items["rsu"] ~ /non-AUG/)) {
    print items["acc"], items["org"], "non-AUG", "no_Met";
}

proc && (items["exception"] ~ /ribosomal_slippage/) {
    print items["acc"], items["org"], "ribosomal_slippage", "";
}
proc && ((items["exception"] !~ /ribosomal_slippage/) && (items["cno"] ~ /ribosomal frameshift/)) {
    print items["acc"], items["org"], "ribosomal_slippage", "no_exception";
}

/^$/ {
    delete items;
    proc = 0;
}
!/^$/ {
    items[$1] = $2;
    for (i=3; i<=NF; i++) {
        items[$1] = items[$1] " " $i;
    }
}
