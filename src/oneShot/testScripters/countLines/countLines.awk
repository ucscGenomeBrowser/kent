BEGIN {
   total = 0;
}

{total += 1;}

END {
   printf("total: %d\n", total);
}
