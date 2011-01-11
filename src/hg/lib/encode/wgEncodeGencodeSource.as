table wgEncodeGencodeSource
"Gencode metadata table of the source of Gencode Gene transcripts"
   (
    string transcriptId; "Transcript ID for Gencode gene"
    enum('ensembl', 'shares_CDS', 'shares_CDS_and_UTR') source; "Source of transcript"
   )
