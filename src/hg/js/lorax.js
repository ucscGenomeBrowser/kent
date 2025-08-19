// Lorax multi-tree viewer: functions for integrating with hgc view

// Copyright (C) 2025 The Regents of the University of California

function loraxView(chrom, gStart, gEnd, rStart, rEnd) {
    console.log("Hello from lorax.js:loraxView.  chrom is", chrom,
                ", (genomiccoords) window start is", gStart, ", window end is", gEnd,
                ", (region) item start is", rStart, ", item end is", rEnd);
    console.log(document.getElementById("hgcIframe"));
}
