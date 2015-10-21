var visualization = d3plus.viz()
    .container("#expGraph")
                /* BRCA1 */
    //.data('http://hgwdev-kate.cse.ucsc.edu/cgi-bin/hgGtexApi?gene=ENSG00000012048.15')
    .data('http://hgwdev-kate.cse.ucsc.edu/cgi-bin/hgGtexApi?gene=' + geneId)
    .type("box")
    .height(500)
    .width(1000)
    .id("sample")
    .color("#FFFFFF")
    .x({"value": "tissue", "grid": false, 
                "label": false, "padding": 1})
    .y({"value": "rpkm", "grid": false})
    .axes({"background":
                {"color": "#FFFFFF"}
         })
    .title(geneName + ' Gene Expression from GTEx')
    .draw();
