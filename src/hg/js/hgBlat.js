// hgBlat.js - client-side support for the hgBlat "Create a stable custom track"
// (autoBigPsl) results page.  hgBlat.c emits an inline hgBlatData object with this
// request's parameters, then includes this file.

/* global $, hgBlatData, luckyLocation, catchErrorOrDispatch, errorHandler */

var ctBlat = '';  // name of the custom track created by buildBigPslCt; saved for later rename/delete

function buildBigPslCtSuccess(content) {
    // Finish the successful creation of the blat custom track (bigPsl).  Called by ajax return.
    // Saves the ct name so it can be used later for rename or delete.
    var matchWord = '&table=';
    var ctBlatPos = content.indexOf(matchWord) + matchWord.length;
    if (ctBlatPos >= 0) {
        var ctBlatPosEnd = content.indexOf('"', ctBlatPos);
        ctBlat = content.slice(ctBlatPos, ctBlatPosEnd);
        if (luckyLocation === '') {
            $('input[name="' + hgBlatData.ctTableVar + '"]')[0].value = ctBlat;
            $('input[name="' + hgBlatData.ctTableVar + '"]')[1].value = ctBlat;
        }
    }
}

function buildBigPslCt(url, trackName, trackDescription) {
    // Call hgc to buildBigPsl from the blat result.
    var cgiVars = 'trackName=' + encodeURIComponent(trackName) +
                  '&trackDescription=' + encodeURIComponent(trackDescription);
    if (ctBlat !== '') {
        cgiVars += '&' + hgBlatData.ctRemoveVar + '=' + encodeURIComponent('Remove Custom Track');
        cgiVars += '&' + hgBlatData.ctTableVar + '=' + encodeURIComponent(ctBlat);
    }
    $.ajax({
        type: 'GET',
        url: url,
        data: cgiVars,
        dataType: 'html',
        trueSuccess: buildBigPslCtSuccess,
        success: catchErrorOrDispatch,
        error: errorHandler,
        cache: false,
        async: false
    });
}

$(document).ready(function() {
    buildBigPslCt(hgBlatData.url, hgBlatData.trackName, hgBlatData.trackDescription);
    if (luckyLocation !== '') {
        location.replace(luckyLocation);
        return;
    }

    // Not feeling lucky: reveal the rename/delete custom-track controls and wire their buttons.
    $('#renameFormItem')[0].style.display = 'block';
    $('#deleteCtForm')[0].style.display = 'block';

    $('#showRenameForm').click(function() {
        $('#renameForm')[0].style.display = 'block';
        $('#renameFormItem')[0].style.display = 'none';
        $('#showRenameForm')[0].style.display = 'none';
        $('input[name="trackName"]')[0].value = hgBlatData.trackName;
        $('input[name="trackDescription"]')[0].value = hgBlatData.trackDescription;
        return false;
    });

    $('#submitTrackNameDescr').click(function() {
        $('#renameForm')[0].style.display = 'none';
        $('#renameFormItem')[0].style.display = 'block';
        $('#showRenameForm')[0].style.display = 'block';
        hgBlatData.trackName = $('input[name="trackName"]')[0].value;
        hgBlatData.trackDescription = $('input[name="trackDescription"]')[0].value;
        buildBigPslCt(hgBlatData.url, hgBlatData.trackName, hgBlatData.trackDescription);
        return false;
    });
});
