table gencodeGeneClass
"Class for Gencode genes"
   (
    string geneId; "Gene ID for Gencode gene"
    string name; "Transcript ID for Gencode gene"
    string transcriptType; "Transcript type for Gencode gene"
    int level;  "Gencode level"
    
    string class; "Class of gene.  enum('Antisense', 'Antisense_val', 'Artifact', 'Known', 'Novel_CDS', 'Novel_transcript', 'Novel_transcript_val', 'Putative', 'Putative_val', 'TEC', 'Processed_pseudogene', 'Unprocessed_pseudogene', 'Pseudogene_fragment', 'Undefined')"
    string ottGeneId; "Otter Gene id for Gencode gene"
    string ottTranscriptId; "Otter Transcript id for Gencode gene"
   )
