#!/bin/sh

#	$Id: wigTableStats.sh,v 1.4 2009/11/25 19:55:25 hiram Exp $

DB=$1
T=$2

if [ "x${DB}y" = "xy" -o "x${T}y" = "xy" ]; then
    echo "wigTableStats.sh - compute overall statistics for a wiggle table"
    echo
    echo "usage: wigTableStats.sh <db> <table> [other tables]"
    echo "expected tables are wiggle tables"
    echo "output is a summary of min, max, average, count, sumData, stdDev, viewLimits"
    echo "the recommended viewLimits are: mean +- 5*stdDev limited by min,max"
    echo "you will want to round those numbers to reasonable nearby values."
    exit 255
fi

echo -e "# db.table\tmin max mean count sumData stdDev viewLimits"
shift		# eliminate the database argument

for T in $*
do
echo -e -n "${DB}.${T}\t"

hgsql -N ${DB} \
-e "select lowerLimit,dataRange,validCount,sumData,sumSquares from ${T}" \
    | awk '
function abs(value) { if (value < 0) {return -value;} else {return value;} }
function viewUpper(min, max, mean, stdDev,  fiveDev, range, upper) {
fiveDev = 5 * stdDev;
range = abs(max-min);
upper = mean + fiveDev;
if (upper > max) upper = max;
return upper;
}
function viewLower(min, max, mean, stdDev,  fiveDev, range, lower) {
fiveDev = 5 * stdDev;
range = abs(max-min);
lower = mean - fiveDev;
if (lower < min) lower = min;
return lower;
}
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
    vLower = viewLower(lower, upper, mean, stdDev);
    vUpper = viewUpper(lower, upper, mean, stdDev);
    printf "%g %g %g %d %g %g viewLimits=%g:%g\n",
	lower, upper, mean, count, sumData, stdDev, vLower, vUpper
} else {
printf "empty data set\n"
}
}
'

done
