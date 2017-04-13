// barChart.js - javascript for Genome Browser barChart track

// Copyright (C) 2016 The Regents of the University of California

function barChartUiTransformChanged(name) {
// Disable view limits settings if log transform enabled

    // NOTE: selector strings are a bit complex due to dots GB vars/attributes (track.var)
    // so can't use more concise jQuery syntax

    // check log transform
    var logCheckbox = $("input[name='" + name + ".logTransform']");
    var isLogChecked = logCheckbox.attr('checked');

    // enable/disable view limits
    var maxTextbox = $("input[name='" + name + ".maxLimit']");
    maxTextbox.attr('disabled', isLogChecked);
    var maxTextLabel = $("." + name + "ViewLimitsMaxLabel");
    maxTextLabel.toggleClass("disabled", isLogChecked ? true : false);
}
