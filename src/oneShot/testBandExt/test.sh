echo perfect
testBandExt perfect/a.fa perfect/b.fa
echo longInitialDelete
testBandExt longInitialDelete/a.fa longInitialDelete/b.fa
echo longInitialInsert
testBandExt longInitialInsert/a.fa longInitialInsert/b.fa
echo longMidDelete
testBandExt longMidDelete/a.fa longMidDelete/b.fa
echo longMidInsert
testBandExt longMidInsert/a.fa longMidInsert/b.fa
echo smallInitialDelete
testBandExt smallInitialDelete/a.fa smallInitialDelete/b.fa
echo smallInitialInsert
testBandExt smallInitialInsert/a.fa smallInitialInsert/b.fa
echo smallMidInsert
testBandExt smallMidInsert/a.fa smallMidInsert/b.fa
echo smallMidDelete
testBandExt smallMidDelete/a.fa smallMidDelete/b.fa
echo tiny
testBandExt tiny/a.fa tiny/b.fa
echo crash1
testBandExt crash1/a.fa crash1/b.fa -maxInsert=50
echo crash2
testBandExt crash2/a.fa crash2/b.fa -maxInsert=50
echo w1
testBandExt w1/a.fa w1/b.fa
