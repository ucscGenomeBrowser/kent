// hgVarAnnogrator functions: ajax updating plus building a query representation from page el's

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

function hgvaAjax(command)
{
    command.querySpec = hgvaBuildQuerySpec();
    $('select[name="db"] :first').each(function(i,el){ command.db = el.value; });
    $('input[name="hgsid"]').each(function(i,el){ command.hgsid = el.value; });
    $('input[name="ctfile"]').each(function(i,el){ command.ctfile = el.value; });
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

function hgvaExecuteQuery()
{
    hgvaAjax({'action': 'execute'});
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
    var message = responseJson.serverSays;
    if (message != null)
	console.log('server says: ' + JSON.stringify(message));
}
