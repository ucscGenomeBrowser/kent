# This creates the table holding the relationship between
# ensemble genes, transcripts, and peptides.
CREATE TABLE ensGtp (
  gene char(21) NOT NULL,
  transcript char(21) NOT NULL,
  protein char(24) NOT NULL,
# INDICES
  INDEX(gene(21)),
  UNIQUE(transcript(21)),
  INDEX(protein(24))
) 
