#!/bin/sh

echo "0.64635^5" | bc -l
echo "5*0.64633^4*0.35372" | bc -l
echo "10*0.64633^3*0.35372^2" | bc -l
echo "10*0.64633^2*0.35372^3" | bc -l
echo "5*0.64633*0.35372^4" | bc -l
echo "0.35372^5" | bc -l

exit 0

 = 0.1127
> P(one base in 5 is G or C) = C(5,1) * 0.64634 * 0.35371 = 0.3084
> P(two bases in 5 are G or C) = C(5,2) * 0.64633 * 0.35372 = 0.3376
> P(three bases in 5 are G or C) = C(5,3) * 0.64632 * 0.35373 = 0.3692
> P(four bases in 5 are G or C) = C(5,4) * 0.64631 * 0.35374 = 0.0504
> P(five bases in 5 are G or C) = 0.35375 = 0.0055
