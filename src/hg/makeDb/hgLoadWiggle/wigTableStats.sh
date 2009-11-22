#!/bin/sh

#	$Id: wigTableStats.sh,v 1.1 2009/11/22 22:50:02 hiram Exp $

DB=$1
T=$2

if [ "x${DB}y" = "xy" -o "x${T}y" = "xy" ]; then
    echo "wigTableStats.sh - compute overall statistics for a wiggle table"
    echo
    echo "usage: wigTableStats.sh <db> <table>"
    echo "expected table is a wiggle table"
    echo "output is a summary of min, max, average, count, sumData, stdDev"
    exit 255
fi

echo -e "# db.table\tmin max mean count sumData stdDev"

echo -e -n "${DB}.${T}\t"

hgsql -N ${DB} \
-e "select lowerLimit,dataRange,validCount,sumData,sumSquares from ${T}" \
    | awk '
BEGIN { lower=3.0e+100; upper=-3.0e+100; count = 0; sumData = 0.0;
	sumSquares = 0.0 }
{
maximum = $1 + $2
if ($1 < lower) {lower = $1;}
if (maximum > upper) {upper = maximum;}
count += $3;
sumData += $4;
sumSquares += $5;
}
END {
if (count > 0) {
    mean = sumData / count;
    var = sumSquares - (sumData*sumData)/count;
    stdDev = var;
    if (count > 1) { stdDev = sqrt(var/(count-1)); }
    printf "%g %g %g %d %g %g\n", lower, upper, mean, count, sumData, stdDev
} else {
printf "empty data set\n"
}
}
'
