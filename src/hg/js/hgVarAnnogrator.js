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
    var firstHiddenSource = $('div[id^=source]').filter(':hidden').filter(':first');
    if (firstHiddenSource.length === 0) {
	alert('Sorry, maximum number of sources reached.');
    }
    else {
	// Move source to the bottom and display it.
	var fhs = firstHiddenSource.detach();
	$('#sourceContainer').append(fhs);
	firstHiddenSource.show();
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
    var i, update, value;
    var message = responseJson.serverSays;
    if (message !== null) {
	console.log('server says: ' + JSON.stringify(message));
    }
    if (responseJson.resubmit) {
	$(responseJson.resubmit).submit();
	return true;
    }
    var updateList = responseJson.updates;
    if (updateList !== null) {
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
    if (valueList !== null) {
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

function hgvaEventBubble(event)
{
    if (event.type == 'click' && event.target.type != 'submit') {
	return true;
    }
    // Most events come here at the section-contents level, so event.currentTarget.id
    // identifies the section whose contents need updating.
    var ancestor = event.currentTarget.id;
    // However, Remove buttons are above that level (section not section contents),
    // and they don't bubble, I guess because the section is hidden.
    // So Remove buttons' onclick adds the sectionId to the event and manually calls
    // this function, so we look for that sectionId"
    if (! ancestor && event.sectionId) {
	ancestor = event.sectionId + 'Contents';
    }
    hgvaAjax({'action': 'event',
		     'ancestor': ancestor,
		     'id': event.target.id,
		     'name': event.target.name});
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
});
