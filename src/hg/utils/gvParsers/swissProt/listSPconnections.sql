select varId, kgXref.spId, mrna, geneSymbol from kgXref, dv where dv.spId=kgXref.spId;
