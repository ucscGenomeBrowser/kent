// hgVarAnnogrator functions: ajax updating plus building a query representation from page el's

function hgvaChangeRegion()
{
    var newVal = $('select[name="hgva_regionType"]').val();
    if (newVal == 'range') {
	$('#positionContainer').show().focus();
    } else {
	$('#positionContainer').hide();
    }
    setCartVar("hgva_regionType", newVal);
}

function hgvaShowNextHiddenSource()
{
    var hiddenSources = $("div.hideableSection").filter("[id^=source]").filter(':hidden');
    var firstHiddenSource = hiddenSources.first();
    if (firstHiddenSource.length === 0) {
	alert('Sorry, maximum number of sources reached.');
    }
    else {
	// Move source to the bottom and display it.
	var fhs = firstHiddenSource.detach();
	$('#sourceContainer').append(fhs);
	firstHiddenSource.show();
	if (hiddenSources.length <= 1) {
	    $("div#addData").hide();
	}
    }
}

function makeHashAddFx(hashObject)
{
    return function(i, el) { hashObject[el.name] = el.value; };
}

function hgvaDescribeSource(source)
{
    var settings = {};
    settings.id = source.id + "Contents";
    var addToSettings = makeHashAddFx(settings);
    $('#'+source.id+' select').each(addToSettings);
    $('#'+source.id+' :hidden').each(addToSettings);
    $('#'+source.id+' div[id="filter"] *').not(':submit').each(addToSettings);
    return settings;
}

function hgvaDescribeOutput()
{
    var settings = {};
    var addToSettings = makeHashAddFx(settings);
    $('#outFormat select, #outFormat input').not(':submit').each(addToSettings);
    return settings;
}

function hgvaBuildQuerySpec()
{
    var sources = [];
    var pushFunc = function(i, source) { sources.push(hgvaDescribeSource(source)); };
    $('#sourceContainer').children().not(':hidden').each(pushFunc);
    var output = hgvaDescribeOutput();
    var topLevel = { "sources": sources, "output": output };
    return topLevel;
}

function hgvaExpandCommand(command)
{
    command.querySpec = hgvaBuildQuerySpec();
    var addToCommand = makeHashAddFx(command);
    $('#mainForm input').not(':button').each(addToCommand);
    $('#mainForm select').each(addToCommand);
}

function hgvaUpdatePage(responseJson)
{
    if (!responseJson) {
	return;
    }
    var i, update, value;
    var message = responseJson.serverSays;
    if (message) {
	console.log('server says: ' + JSON.stringify(message));
    }
    if (responseJson.resubmit) {
	$(responseJson.resubmit).submit();
	return true;
    }
    var updateList = responseJson.updates;
    if (updateList) {
	for (i = 0;  i < updateList.length;  i++) {
	    update = updateList[i];
	    if (update.append) {
		$(update.id).append(update.contents);
	    } else {
		$(update.id).html(update.contents);
	    }
	}
    }
    var valueList = responseJson.values;
    if (valueList) {
	for (i=0;  i < valueList.length;  i++) {
	    value = valueList[i];
	    $(value.id).val(value.value);
	}
    }
    var names = [ 'querySpec' ];
    var values = [ JSON.stringify(hgvaBuildQuerySpec()) ];
    var position = $('#mainForm input[name="position"]').val();
    if (position.match(/^[\w_]+:[\d,]+-[\d,]+$/)) {
	names.push('position');
	values.push(position);
    }
    // setCartVars returns error when this is called by synchronous ajax...
    var ignoreError = function(){};
    setCartVars(names, values, ignoreError, false);
}

function hgvaAjax(command, async)
{
    if (async === null) {
	async = true;
    }
    hgvaExpandCommand(command);
    $.ajax({
        type: "POST",
	async: async,
        dataType: "JSON",
        url: "hgVarAnnogrator",
	data: 'updatePage=' + JSON.stringify(command),
        trueSuccess: hgvaUpdatePage,
        success: catchErrorOrDispatch,
        error: errorHandler,
        cache: false
	    });
}

function hgvaLookupPosition(async)
{
    hgvaAjax({'action': 'lookupPosition'}, async);
}

function hgvaLookupPositionIfNecessary()
{
    var regionType = $('#mainForm #hgva_regionType').val();
    if (regionType == "range") {
	var position = $('#mainForm input[name="position"]').val();
	if (!position.match(/^[\w_]+:[\d,]+-[\d,]+$/)) {
	    hgvaLookupPosition(false);
	}
    }
}

function hgvaExecuteQuery()
{
    loadingImage.run();
    hgvaLookupPositionIfNecessary();
    var command = {'action': 'execute'};
    hgvaExpandCommand(command);
    $('#mainForm').append("<INPUT TYPE=HIDDEN NAME='executeQuery' VALUE='" +
			  JSON.stringify(command) + "'>);");
    $('#mainForm').submit();
}

function hgvaEventAjax(target, parentId)
{
    hgvaAjax({'action': 'event',
	      'id': target.id,
	      'name': target.name,
	      'parentId': parentId});
}

function hgvaEventHandler(event)
{
    var target = (event.target) ? event.target : event.srcElement;
    var parent = $(target).parents().first();
    hgvaEventAjax(target, $(parent)[0].id);
}

function hgvaHideSection(event)
{
    var target = (event.target) ? event.target : event.srcElement;
    var section = $(target).parents().filter(".hideableSection").first();
    section.hide();
    if (section.filter("[id^=source]").length > 0) {
	$("div#addData").show();
    }
    hgvaEventAjax(target, $(section)[0].id + "Container");
}

function hgvaSourceSortUpdate(event, ui)
{
    hgvaAjax({'action': 'reorderSources'});
}

$(document).ready(function()
{
    // initialize ajax.js's loadingImage, to warn user that query might take a while.
    loadingImage.init($("#loadingImg"), $("#loadingMsg"),
		      "<p style='color: red; font-style: italic;'>" +
		      "Executing your query may take some time. " +
		      "Please leave this window open during processing.</p>");
    // Set up event handlers:
    $("div.sourceSection").delegate(":input", "change", hgvaEventHandler);
    $("div.sourceSection").delegate(":submit", "click", hgvaEventHandler);
    $("div.outputSection").delegate(":input", "change", hgvaEventHandler);
    $("div.outputSection").delegate(":submit", "click", hgvaEventHandler);
    $("div.hideableSection").delegate(":submit[name='removeMe']", "click", hgvaHideSection);
    // Set up sorting of source sections:
    $('#sourceContainer' ).sortable({
	    containment: '#sourceContainerPlus',
	    handle: '#sortHandle',
	    update: hgvaSourceSortUpdate
	    });
    // Hide 'Select More Data' button if we have run out of extra sources to offer.
    var hiddenSources = $("div.hideableSection").filter("[id^=source]").filter(':hidden');
    if (hiddenSources.length == 0) {
	$("div#addData").hide();
    }
});
