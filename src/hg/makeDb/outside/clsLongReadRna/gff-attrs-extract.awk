

BEGIN{
    print "transcriptId", "samplesMetadata", "artifact"
    ID = ""
}

(transcriptId != "") && ($1 != transcriptId) {
    print transcriptId, samplesMetadata, artifact
}

$2=="ID" {
    transcriptId = $3
}

$2=="samplesMetadata" {
    samplesMetadata = $3
}

$2=="artifact" {
    artifact = $3
}

END {
    print transcriptId, samplesMetadata, artifact
}

