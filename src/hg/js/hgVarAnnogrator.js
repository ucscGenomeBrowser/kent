// hgVarAnnogrator functions: ajax updating plus building a query representation from page el's

function hgvaChangeRegion()
{
    var newVal = $('select[name="hgva_regionType"]').val();
    if (newVal == 'range') {
	$('#positionContainer').show().focus();
    } else {
	$('#positionContainer').hide();
    }
}

function hgvaShowNextHiddenSource()
{
    var firstHiddenSource = $('div[id^=source]').filter(':hidden').filter(':first');
    if (firstHiddenSource.length == 0) {
	alert('Sorry, maximum number of sources reached.');
    }
    else {
	// Move source to the bottom and display it.
	var fhs = firstHiddenSource.detach();
	$('#sourceContainer').append(fhs);
	firstHiddenSource.show();
    }
}

function addJQToSettings(jq, settings)
{
    jq.each(function(i,el) { settings[el.name] = el.value; });
}

function hgvaDescribeSource(source)
{
    var settings = {};
    settings.id = source.id + "Contents";
    addJQToSettings($('#'+source.id+' select'), settings);
    addJQToSettings($('#'+source.id+' :hidden'), settings);
    addJQToSettings($('#'+source.id+' div[id="filter"] *').not(':submit'), settings);
    return settings;
}

function hgvaDescribeOutput()
{
    var settings = {};
    addJQToSettings($('#outFormat select, #outFormat input').not(':submit'), settings);
    return settings;
}

function hgvaBuildQuerySpec()
{
    var sources = [];
    $('#sourceContainer').children().not(':hidden').
	each( function(i,source){ sources.push(hgvaDescribeSource(source)); });
    var output = hgvaDescribeOutput();
    var topLevel = { "sources": sources, "output": output };
    return topLevel;
}

function hgvaExpandCommand(command)
{
    command.querySpec = hgvaBuildQuerySpec();
    var addToCommand = function(i,el){ command[el.name] = el.value; };
    $('#mainForm input').each(addToCommand);
    $('#mainForm select').each(addToCommand);
}

function hgvaAjax(command)
{
    hgvaExpandCommand(command);
    $.ajax({
        type: "POST",
        dataType: "JSON",
        url: "hgVarAnnogrator",
	data: 'updatePage=' + JSON.stringify(command),
        trueSuccess: hgvaUpdatePage,
        success: catchErrorOrDispatch,
        cache: false,
    });
}

function hgvaLookupPosition()
{
    hgvaAjax({'action': 'lookupPosition'});
}

function hgvaLookupPositionIfNecessary()
{
    var regionType = $('#mainForm #hgva_regionType').val();
    if (regionType == "range") {
	var position = $('#mainForm input[name="position"]').val();
	if (!position.match(/^[\w_]+:[\d,]+-[\d,]+$/)) {
	    hgvaLookupPosition();
	}
    }
}

function hgvaExecuteQuery()
{
    hgvaLookupPositionIfNecessary();
    showLoadingMessage("Executing your query may take some time. " +
		       "Please leave this window open during processing.");
    var command = {'action': 'execute'};
    hgvaExpandCommand(command);
    $('#executeForm input[name="executeQuery"]').val(JSON.stringify(command));
    $('#executeForm').submit();
}

function hgvaEventBubble(event)
{
    if (event.type == 'click' && event.target.type != 'submit')
	return true;
    // Most events come here at the section-contents level, so event.currentTarget.id
    // identifies the section whose contents need updating.
    var ancestor = event.currentTarget.id;
    // However, Remove buttons are above that level (section not section contents),
    // and they don't bubble, I guess because the section is hidden.
    // So Remove buttons' onclick adds the sectionId to the event and manually calls
    // this function, so we look for that sectionId"
    if (! ancestor && event.sectionId)
	ancestor = event.sectionId + 'Contents';
    hgvaAjax({'action': 'event',
		     'ancestor': ancestor,
		     'id': event.target.id,
		     'name': event.target.name});
}

function hgvaSourceSortUpdate(event, ui)
{
    hgvaAjax({'action': 'reorderSources'});
}

function hgvaUpdatePage(responseJson)
{
    var message = responseJson.serverSays;
    if (message != null)
	console.log('server says: ' + JSON.stringify(message));
    if (responseJson.resubmit) {
	$(responseJson.resubmit).submit();
	return true;
    }
    var updateList = responseJson.updates;
    if (updateList != null) {
	for (var i=0;  i < updateList.length;  i++) {
	    var update = updateList[i];
	    if (update.append) {
		$(update.id).append(update.contents);
	    } else {
		$(update.id).html(update.contents);
	    }
	}
    }
    var valueList = responseJson.values;
    if (valueList != null) {
	for (var i=0;  i < valueList.length;  i++) {
	    var value = valueList[i];
	    $(value.id).val(value.value);
	}
    }
}

//-------------------------- adapted from hgCustom.js: -----------------------------
//#*** move to utils.js
function refreshLoadingImg ()
{
    // hack to make sure animation continues in IE after form submission
    // See: http://stackoverflow.com/questions/774515/keep-an-animated-gif-going-after-form-submits
    // and http://stackoverflow.com/questions/780560/animated-gif-in-ie-stopping
    $("#loadingImg").attr('src', $("#loadingImg").attr('src'));
}

function showLoadingMessage(message)
{ // Tell the user we are processing the upload when the user clicks on the submit button.
    $("#loadingMsg").append("<p style='color: red; font-style: italic;'>" + message  + "</p>");
    if(navigator.userAgent.indexOf("Chrome") != -1) {
        // In Chrome, gif animation and setTimeout's are stopped when the browser receives
	// the first blank line/comment of the next page (basically, the current page is
	// unloaded). I have found no way around this problem, so we just show a simple
	// "Processing..." message (we can't make that blink, b/c Chrome doesn't support
	// blinking text). (Surprisingly, this is NOT true for Safari, so this is apparently
	// not a WebKit issue).

        $("#loadingImg").replaceWith("<span id='loadingBlinker'>&nbsp;&nbsp;" +
				     "<b>Processing...</b></span>");
    } else {
        $("#loadingImg").show();
        setTimeout(refreshLoadingImg, 1000);
    }
    return true;
}

$(document).ready(function()
{
    // To make the loadingImg visible on FF, we have to make sure it's visible during
    // page load (otherwise it doesn't get shown by the submitClick code).
    $("#loadingImg").hide();
});
//------------------------- end adapted from hgCustom.js -----------------------------
