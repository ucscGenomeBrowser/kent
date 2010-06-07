BEGIN {ls = 0; le = 0; lc=""}

{  if (lc != $1)
      lc = $1;
   else if ($2 - le < 1000)
       print;
   ls = $2;
   le = $3;
}
