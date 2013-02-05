// hgVarAnnogrator functions: ajax updating plus building a query representation from page el's

// jslint settings and list of globals for jslint to ignore:
/*jslint
  browse: true,  devel: true,
  onevar: false,  plusplus: false,  regexp: false,  white: false */
/*global
  $: false,
  activeFilterList: false, availableFilterList: false,
  loadingImage: false,
  setCartVar: false, setCartVars: false, warn: false, catchErrorOrDispatch: false */

// Tell jslint and browser to use strict mode for this file:
"use strict";

var hgvaRequestData = "";

function hgvaChangeRegion()
{
    var newVal = $('select[name="hgva_regionType"]').val();
    if (newVal === 'range') {
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

function hgvaAddFilter(parentId)
{
    var availableFilters = availableFilterList[parentId];
    var filterDiv = $("#" + parentId + " [name='filter']");
    var newHtml, i, name;
    if (availableFilters && availableFilters.length > 0) {
	newHtml = "<select name='newFilterSel' class='newFilterSel'>\n";
	newHtml += "<option value='none' selected />\n";
	for (i = 0;  i < availableFilters.length;  i++) {
	    name = availableFilters[i].label;
	    newHtml += "<option value='" + name + "'>" + name + "</option>\n";
	}
	newHtml += "</select>\n";
	filterDiv.append(newHtml);
    } else {
	$("[name='addFilter']").hide();
    }
}

function hgvaOptionHtml(value, label, selected)
{
    var newHtml = "<option value='" + value + "'";
    if (selected === value) {
	newHtml += " selected";
    }
    newHtml += ">" + label + "</option>\n";
    return newHtml;
}

var hgvaUpdateCartOnChange =
    "onchange='setCartVar(\"querySpec\", JSON.stringify(hgvaBuildQuerySpec()));'";

function hgvaStringFilterInputs(filterSpec)
{
    var newHtml = "<select name='opSel'>\n";
    var sel = filterSpec.op;
    if (!sel || sel === 'afNoFilter') {
	sel = 'afMatch';
    }
    newHtml += hgvaOptionHtml('afMatch', 'does match', sel);
    newHtml += hgvaOptionHtml('afNotMatch', 'does not match', sel);
    newHtml += "</select>\n";
    newHtml += "<input type='text' name='matchPattern' size=30 " + hgvaUpdateCartOnChange;
    if (typeof filterSpec.values === 'string') {
	newHtml += " value='" + filterSpec.values + "'";
    }
    newHtml += "/>\n";
    return newHtml;
}

function hgvaNumericFilterInputs(filterSpec)
{
    var newHtml = "<select name='opSel'>\n";
    var sel = filterSpec.op;
    if (!sel || sel === 'afNoFilter') {
	sel = 'afEqual';
    }
    newHtml += hgvaOptionHtml('afInRange', 'in range', sel);
    newHtml += hgvaOptionHtml('afLT', '&lt;', sel);
    newHtml += hgvaOptionHtml('afLTE', '&le;', sel);
    newHtml += hgvaOptionHtml('afEqual', '=', sel);
    newHtml += hgvaOptionHtml('afNotEqual', '!=', sel);
    newHtml += hgvaOptionHtml('afGTE', '&ge;', sel);
    newHtml += hgvaOptionHtml('afGT', '&gt;', sel);
    newHtml += "</select>\n";
    newHtml += "<input type='text' name='num1' size=20 " + hgvaUpdateCartOnChange;
    if (typeof filterSpec.values === 'list') {
	newHtml += " value='" + filterSpec.values[0] + "'";
    } else if (typeof filterSpec.values === 'number') {
	newHtml += " value='" + filterSpec.values + "'";
    }
    newHtml += "/>\n";
    newHtml += "<input type='text' name='num2' size=20 style='display: none;' ";
    newHtml += hgvaUpdateCartOnChange;
    if (typeof filterSpec.values === 'list' && typeof filterSpec.values[1] === 'number') {
	newHtml += " value='" + filterSpec.values[1] + "'";
    }
    newHtml +="/>\n";
    return newHtml;
}

function hgvaFilterFromSpec(filterSpec)
{
    var newHtml = "<div name='" + filterSpec.label + "'>";
    newHtml += filterSpec.label + "&nbsp;&nbsp;";
    //#*** It's awful to use numeric values here... should have C translate enum <--> string
    if (filterSpec.type === 2 || filterSpec.type === 10 || filterSpec.type === 11) {
	newHtml += hgvaStringFilterInputs(filterSpec);
    } else {
	newHtml += hgvaNumericFilterInputs(filterSpec);
    }
    newHtml += "<input type='hidden' name='asType' value='" + filterSpec.type + "' />\n";
    newHtml += "</div>\n";
    return newHtml;
}

function hgvaCompleteFilter(filterSelEl, parentId)
{
    var availableFilters = availableFilterList[parentId];
    var filterSel = $(filterSelEl);
    var selectedVal = filterSel.children(":selected").val();
    var newHtml;
    var i, filterSpec;
    if (availableFilters) {
	for (i = 0;  i < availableFilters.length;  i++) {
	    if (availableFilters[i].label === selectedVal) {
		filterSpec = availableFilters[i];
		break;
	    }
	}
	if (! filterSpec) {
	    console.log("Can't find filterSpec for " + selectedVal);
	    return;
	}
	newHtml = hgvaFilterFromSpec(filterSpec);
	filterSel.replaceWith(newHtml);
	availableFilters.splice(i, 1);
	if (availableFilters.length === 0) {
	    $("[name='addFilter']").hide();
	}
    }
}

function hgvaFilterOpChange(singleFilterDiv)
{
    var selectedOp = singleFilterDiv.find("select[name='opSel']").children(":selected").val();
    if (selectedOp === "afInRange") {
	singleFilterDiv.children("input[name='num2']").show();
    } else {
	singleFilterDiv.children("input[name='num2']").hide();
    }
}

function hgvaMakeActiveFilters()
{
    var filtersForSource =
	function(eachIx, el) {
	    var i, newHtml = "";
	    var filterList = activeFilterList[el.id + "Contents"];
	    if (! filterList) {
		return;
	    }
	    for (i = 0;  i < filterList.length;  i++) {
		newHtml += hgvaFilterFromSpec(filterList[i]);
	    }
	    $(el).find("div.sourceFilter").append(newHtml);
	};
    $('#sourceContainer').children().not(':hidden').each(filtersForSource);
}

function makeHashAddFx(hashObject)
{
    return function(eachIx, el) { hashObject[el.name] = el.value; };
}

function hgvaDescribeFilterDivs(filterDivs, settings)
{
    var filterList = [];
    var i, filterDiv, name, addToFilter;
    for (i = 0;  i < filterDivs.length;  i++) {
	filterDiv = $(filterDivs[i]);
	filterList[i] = { "name": filterDiv.attr("name") };
	addToFilter = makeHashAddFx(filterList[i]);
	filterDiv.children("input,select").each(addToFilter);
    }
    return filterList;
}

function hgvaDescribeSource(source)
{
    var settings = {};
    settings.id = source.id + "Contents";
    var addToSettings = makeHashAddFx(settings);
    $('#'+source.id+' select').each(addToSettings);
    $('#'+source.id+' :hidden').each(addToSettings);
    var filterDivs = $('#'+source.id+' div[name="filter"] > div');
    settings.filters = hgvaDescribeFilterDivs(filterDivs, settings);
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
    var pushFunc = function(eachIx, source) { sources.push(hgvaDescribeSource(source)); };
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

function hgvaUpdateCart()
{
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

// This will be set in the first .each call to hgvaUpdateOrder from hgvaSourceSortUpdates,
// and used by subsequent calls.
var hgvaPrimaryTrackLabel;

function hgvaUpdateOptionText(eachIx, optionEl)
{
    var matches = optionEl.text.match(/(Keep( all)?)( .*)(items.*items)/);
    if (!matches) {
	console.log("No match for option text: '" + optionEl.text + "'");
    } else if (matches[3] !== hgvaPrimaryTrackLabel) {
	optionEl.text = matches[1] + ' ' + hgvaPrimaryTrackLabel + ' ' + matches[4];
    }
}

function hgvaUpdateOrder(eachIx, sourceEl)
{
    var source = $(sourceEl);
    var isPrimary = source.find('[name="isPrimary"]');
    var intersectSel = source.find('[name="intersectSel"]');
    var trackSel = source.find('[name="trackSel"]');
    if (eachIx === 0) {
	isPrimary.val('1');
	intersectSel.hide();
	hgvaPrimaryTrackLabel = trackSel.find(":selected").text();
    } else {
	isPrimary.val('0');
	var options = intersectSel.find('option');
	options.each(hgvaUpdateOptionText);
	// IE8 superimposes intersectSel and filter div unless I invoke .height() here. why???:
	intersectSel.height();
	intersectSel.show();
    }
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
	    // If the first active source's group/track/table just changed, then we need to
	    // call hgvaUpdateOrder to update intersectSel option labels.
	    var grandparent = $(update.id).parents().first();
	    var firstActiveSource = $('#sourceContainer').children().not(':hidden').first();
	    if (firstActiveSource[0].id === grandparent[0].id) {
		$('#sourceContainer').children().not(':hidden').each(hgvaUpdateOrder);
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
    hgvaUpdateCart();
}

function hgvaErrorHandler(request, textStatus)
{
    var str;
    var tryAgain = true;
    if(textStatus && textStatus.length && textStatus !== "error") {
        str = "Encountered network error : '" + textStatus + "'.";
    } else {
        if(request.responseText) {
            tryAgain = false;
	    if (request.status === 500) {
		// Prevent entire Error 500 page from appearing inside warn box:
		str = "Our software aborted due to an error. " +
		      "We apologize for the inconvenience. " +
		      "Please copy and paste this into an email to " +
		      "genome-www@soe.ucsc.edu so we can debug:<BR>\n" +
		      hgvaRequestData;
	    } else {
		str = "Encountered error: '" + request.responseText + "'";
	    }
        } else {
            str = "Encountered a network error.";
        }
    }
    if(tryAgain) {
        str += " Please try again. If the problem persists, please check your network connection.";
    }
    warn(str);
    loadingImage.abort();
}

function hgvaAjax(command, async)
{
    if (async === null) {
	async = true;
    }
    hgvaExpandCommand(command);
    hgvaRequestData = 'updatePage=' + JSON.stringify(command);
    $.ajax({
        type: "POST",
	async: async,
        dataType: "JSON",
        url: "hgVarAnnogrator",
	data: hgvaRequestData,
        trueSuccess: hgvaUpdatePage,
        success: catchErrorOrDispatch,
        error: hgvaErrorHandler,
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
    if (regionType === "range") {
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
			  JSON.stringify(command) + "'>");
    $('#mainForm').submit();
}

function hgvaSourceSortUpdate(event, ui)
{
    $('#sourceContainer').children().not(':hidden').each(hgvaUpdateOrder);
    // do ajax to update cart's querySpec and refresh output options
    hgvaAjax({'action': 'reorderSources'});
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
    if (parent.attr("name") === "filter") {
	parent = parent.parents().first();
    }
    var parentId = parent[0].id;
    if (target.value === "Add Filter") {
	hgvaAddFilter(parentId);
    } else if (target.name === "newFilterSel") {
	hgvaCompleteFilter(target, parentId);
    } else if (target.name === "opSel") {
	hgvaFilterOpChange($(target).parent());
    } else {
	hgvaEventAjax(target, parentId);
    }
}

function hgvaHideSection(event)
{
    var target = (event.target) ? event.target : event.srcElement;
    var section = $(target).parents().filter(".hideableSection").first();
    section.hide();
    if (section.filter("[id^=source]").length > 0) {
	$("div#addData").show();
    }
    hgvaEventAjax(target, $(section)[0].id + "Contents");
    $('#sourceContainer').children().not(':hidden').each(hgvaUpdateOrder);
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
	    handle: '[name="sortHandle"]',
	    update: hgvaSourceSortUpdate
	    });
    // Initialize filters already configured by the user:
    hgvaMakeActiveFilters();
    // Hide 'Select More Data' button if we have run out of extra sources to offer.
    var hiddenSources = $("div.hideableSection").filter("[id^=source]").filter(':hidden');
    if (hiddenSources.length === 0) {
	$("div#addData").hide();
    }
});
