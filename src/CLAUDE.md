# CLAUDE.md — UCSC Genome Browser Kent Source Tree

For source code changes (C, JavaScript, HTML, Python, CGIs, trackDb, build system),
load the **`edit-kent-code`** skill. For building a new genome browser track
(data pipelines, trackDb stanzas, makeDocs, description pages), load the
**`make-track`** skill.

Binaries you build from this tree install under `~/bin/$MACHTYPE/` — invoke them
from there, not by bare name (the system PATH resolves to production binaries).
