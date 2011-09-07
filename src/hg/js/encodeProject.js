// encodeProject - javascript utilities for ENCODE-specific things 
// such as controlled vocabulary and experiment table

// Formatted: jsbeautify.py -j
// Syntax checked: jslint --indent=4 --plusplus=true --strict=false --browser=true
/*global $ */

var encodeProject = (function () {
    var server = "genome.ucsc.edu",
        assembly = "hg19",
        cgi = "/cgi-bin/hgApi?";

    function cmpNoCase(a, b) {
        // Helper function for case-insensitive sort
        var A, B;
        A = a.toUpperCase();
        B = b.toUpperCase();
        if (A < B) {
            return -1;
        }
        if (A > B) {
            return 1;
        }
        return 0;
    }

    // TODO: modularize by extending Array.sort ?
    function cmpCV(a, b) {
        // Helper function for case-insensitive sort of CV objects
        //  Use label if any, otherwise the term
        A = (a.label !== undefined ? a.label.toUpperCase() : a.term.toUpperCase());
        B = (b.label !== undefined ? b.label.toUpperCase() : b.term.toUpperCase());
        if (A < B) {
            return -1;
        }
        if (A > B) {
            return 1;
        }
        return 0;
    }

    return {
        setup: function (settings) {
            // Change defaults
            if (settings.server) {
                server = settings.server;
            }
            if (settings.assembly) {
                server = settings.assembly;
            }
        },

        getServer: function () {
            // Get currently set server 
            return server;
        },

        getDataGroups: function (dataTypes) {
            // Return sorted array of dataGroup objects each having a .label,
            // .dataTypes, 
            // and an array of dataTypes, alphasorted, with 'Other' last
            var dataGroupHash = {},
                dataGroups = [],
                otherGroup,
                group;
            $.each(dataTypes, function (i, dataType) {
                group = dataType.dataGroup;
                if (!group) {
                    return true;
                }
                if (!dataGroupHash[group]) {
                    dataGroupHash[group] = {
                        label: group,
                        dataTypes: []
                    };
                }
                dataGroupHash[group].dataTypes.push(dataType.label);
            });
            $.each(dataGroupHash, function (key, item) {
                if (key === "Other") {
                    otherGroup = item;
                } else {
                    dataGroups.push(item);
                }
            });
            dataGroups.sort(cmpCV);
            dataGroups.push(otherGroup);
            $.each(dataGroups, function (i, group) {
                if (!dataGroups[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                dataGroups[i].dataTypes.sort(cmpNoCase);
            });
            return dataGroups;
        },

        getCellTiers: function (cellTypes) {
            // Return sorted array of cellTier objects each having a .term,
            // with tier number, .celltypes, 
            // and an array of cell types, alphasorted
            var cellTiers = [],
                tier;
            $.each(cellTypes, function (i, cellType) {
                tier = cellType.tier;
                // ignore untiered cell types (all human should have a tier)
                if (!tier) {
                    return true;
                }
                if (!cellTiers[tier]) {
                    cellTiers[tier] = {
                        term: tier,
                        cellTypes: []
                    };
                }
                cellTiers[tier].cellTypes.push(cellType.term);
            });
            cellTiers.sort(cmpCV);
            $.each(cellTiers, function (i, tier) {
                if (!cellTiers[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                cellTiers[i].cellTypes.sort(cmpNoCase);
            });
            return cellTiers;
        },

        isHistone: function (target) {
            // Helper function, returns true if antibody target histone modification

            return target.match(/^H[234]/);
        },

        antibodyFromExp: function (experiment) {
            // Get antibody from expVars field of experiment
            var match = experiment.expVars.match(/antibody=(\S+)/);
            if (match) {
                return match[1];
            }
        },

        targetFromAntibody: function (antibody, antibodyCV) {
            // Get target for antibody from antibody controlled vocab
            if (antibodyCV[antibody]) {
                return antibodyCV[antibody].target;
            }
        },

        getAntibodyGroups: function (antibodies) {
            // Return sorted array of antibodyGroup objects each having a .label
            // with group name (Histone Modification or Transcription Factor), 
            // and an array of antibody targets, alphasorted
            var antibodyGroups = [],
                antibodyGroupHash = {},
                group,
                target;
            $.each(antibodies, function (i, antibody) {
                group = encodeProject.isHistone(antibody.target) ? 
                        "Histone Modification" : "Transcription Factor";
                if (!antibodyGroupHash[group]) {
                    antibodyGroupHash[group] = {
                        label: group,
                        targets: [],
                        targetHash: {}
                    };
                }
                target = antibody.target;
                if (antibodyGroupHash[group].targetHash[target] === undefined) {
                    antibodyGroupHash[group].targetHash[target] = target;
                    antibodyGroupHash[group].targets.push(target);
                }
            });
            $.each(antibodyGroupHash, function (key, item) {
                antibodyGroups.push(item);
            });
            antibodyGroups.sort(cmpCV);
            $.each(antibodyGroups, function (i, group) {
                if (!antibodyGroups[i]) {
                    // for some reason there's  __ element here (not my property)
                    return true;
                }
                antibodyGroups[i].targets.sort(cmpNoCase);
            });
            return antibodyGroups;
        },

        serverRequests: {
            // Requests for data from server API
            experiment: "cmd=encodeExperiments",

            dataType: "cmd=cv&type=dataType",

            cellType: "cmd=cv&type=cellType",

            antibody: "cmd=cv&type=antibody"
        },

        loadAllFromServer: function (requests, handler) {
            // Execute requests to server via ajax
            var serverData = [],
                count = requests.length;

            $.each(requests, function (i, request) {
                $.getJSON("http://" + server + cgi + "db=" + assembly + "&" + request, function (data) {
                    serverData[i] = data;
                    if (--count === 0) {
                        handler(serverData);
                    }
                });
            });
        }
    };
}());
