awk 'BEGIN{mi=1000000;ma=-1000000} ($4 < mi){mi=$4} ($4 > ma){ma = $4} END{printf("%g\t%g\n", mi, ma )}' $1
