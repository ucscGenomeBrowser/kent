#!/bin/tcsh -efx
mkdir -p out
encodePatchTdb in/add1.ra trackDb.ra -test=out/add1.ra
diff expected/add1.ra out/add1.ra
encodePatchTdb in/add3.ra trackDb.ra -test=out/add3.ra
diff expected/add3.ra out/add3.ra
encodePatchTdb mode=replace in/replace1.ra trackDb.ra -test=out/replace1.ra
diff expected/replace1.ra out/replace1.ra
encodePatchTdb mode=replace in/replace3.ra trackDb.ra -test=out/replace3.ra
diff expected/replace3.ra out/replace3.ra
encodePatchTdb mode=replace -noComment in/replace3.ra trackDb.ra -test=out/replace3NoComment.ra
diff expected/replace3NoComment.ra out/replace3NoComment.ra
encodePatchTdb in/composite.ra trackDb.ra -test=out/composite.ra
diff expected/composite.ra out/composite.ra
