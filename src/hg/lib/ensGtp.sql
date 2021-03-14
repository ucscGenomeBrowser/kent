# This creates the table holding the relationship between
# ensemble genes, transcripts, and peptides.
CREATE TABLE ensGtp (
  gene char(28) NOT NULL,
  transcript char(31) NOT NULL,
  protein char(24) NOT NULL,
# INDICES
  INDEX(gene(28)),
  UNIQUE(transcript(31)),
  INDEX(protein(24))
) 
