CREATE TABLE allenBrainGene (
    entrezId varchar(40) not null,	# ENTREZ gene ID
    geneSymbol varchar(40),		# gene symbol
    KEY(entrezId),
    KEY(geneSymbol)
);
