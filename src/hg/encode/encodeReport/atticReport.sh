#!/bin/sh

hgsql $1 -e "select a.obj, a.val Attic_Type, b.val ExpId, c.val Lab,d.val Datatype, e.val Cell, f.val Rep, g.val View from metaDb a,metaDb b, metaDb c, metaDb d, metaDb e, metaDb f, metaDb g where a.obj=b.obj and b.obj=c.obj and c.obj=d.obj and d.obj=e.obj and e.obj=f.obj and f.obj=g.obj and a.var='attic' and b.var='expId' and c.var='lab' and d.var='dataType' and e.var='cell' and f.var='replicate' and g.var='view';"
