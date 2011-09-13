TestCase("encodeProject", {
    setUp: function () {
    },

    tearDown: function () {
    },

    // test methods
    "test server setup": function () {
        var host = "myhost";
        encodeProject.setup({ server: host });
        assertEquals(encodeProject.getServer(), host);
    },

    "test getting antibody from experiment ": function () {
        var exp = {
                    expVars: "antibody=Pol2 localization=nucleus"
                };
        var ab = encodeProject.antibodyFromExp(exp);
        assertEquals(ab, "Pol2");
    },

    "test getDataGroups": function () {
        var dataGroups,
            dataTypes = [{ term: "AffyExonArray", label: "Exon-array", dataGroup: "RNA Profiling"}, 
                         { term: "DnaPet", label: "DNA-PET", dataGroup: "Other"},
                         { term: "DnaseSeq", label: "DNase-seq", dataGroup: "Open Chromatin"},
                         { term: "DNaseDgf", label: "DNase-DGF", dataGroup: "Open Chromatin"}
                        ];
        dataGroups = encodeProject.getDataGroups(dataTypes);
        //assertEquals(dataGroups[0].name, "Open Chromatin");
        assertEquals(dataGroups[0].dataTypes[0], "DNase-DGF");
    },

    "test getCellTiers": function () {
        var cellTiers,
            cellTypes = [{ term: "K562", tier: "1"}, 
                         { term: "AF349", tier: "3"},
                         { term: "GM12878", tier: "1"},
                         { term: "HeLa", tier: "2"}
                        ];

        cellTiers = encodeProject.getCellTiers(cellTypes);
        assertEquals(cellTiers[0].cellTypes[0], "GM12878");
        //assertEquals(cellTiers[0].name, "1");
    }

});

