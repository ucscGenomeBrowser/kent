BEGIN {ls = 0; le = 0}

{ if ($2 == ls || $3 == le)
     print;
  ls = $2;
  le = $3;
}
