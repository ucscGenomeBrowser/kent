/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, Icon, Modal, TextInput */
var pt = React.PropTypes;

var PositionPopup = null;  // subcomponent, defined below

var PositionSearch = React.createClass({
    // Text input for position or search term, optionally with autocomplete if
    // props includes geneSuggestTrack, and a PositionPopup (defined below)
    // for multiple position/search matches.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'position', newValue) called when user changes position
    // update(path + 'hidePosPopup') called when user clicks to hide popup
    // update(path + 'positionMatch', matches): user clicks position link in popup
    //                                          (matches obj is from hgFind)

    propTypes: { positionInfo: pt.object.isRequired,  // Immutable.Map {
                 //   position: initial value of position input
                 //   loading (bool): display spinner next to position input
                 //   positionMatches (Immutable.Vector of Maps): multiple search results for popup
                 //   geneSuggestTrack: optional track to use for autocomplete
                 // }

                 // Conditionally required
                 db: pt.string,         // must be given if positionInfo includes geneSuggestTrack

                 // Optional
                 className: pt.string   // class(es) to pass to wrapper div
               },

    autoCompleteSourceFactory: function() {
	// This returns a 'source' callback function for jqueryui.autocomplete.
	// We get a lot of duplicate requests (especially the first letters of
	// words), so we keep a cache of the suggestions lists we've retreived.
	var cache = {};

        function makeUrl(db, key) {
	    return '../cgi-bin/hgSuggest?db=' + db + '&prefix=' + encodeURIComponent(key);
        }

	return function (request, acCallback) {
	    // This is a callback for jqueryui.autocomplete: when the user types
	    // a character, this is called with the input value as request.term and an acCallback
	    // for this to return the result to autocomplete.
	    // See http://api.jqueryui.com/autocomplete/#option-source
            if (this.props.positionInfo.get('geneSuggestTrack')) {
                var key = request.term;
                var db = this.props.db;
                if (cache[db] && cache[db][key]) {
                    acCallback(cache[db][key]);
                } else {
		    var url = makeUrl(db, key);
		    $.getJSON(url)
		    .done(function(result) {
                            cache[db] = cache[db] || {};
                            cache[db][key] = result;
                            acCallback(result);
                    });
		    // ignore errors to avoid spamming people on flaky network connections
		    // with tons of error messages (#8816).
                }
            }
	}.bind(this);
    },

    autoCompleteMenuOpen: function() {
	// This is an 'open' event callback for autocomplete to let us know when the
	// menu showing completions is opened.
	// See http://api.jqueryui.com/autocomplete/#event-open
	// Determine whether the menu will need a scrollbar:
        var $jq = $(this.refs.input.getDOMNode());
        var pos = $jq.offset().top + $jq.height();
        if (!isNaN(pos)) {
	    // take off a little more because IE needs it:
            var maxHeight = $(window).height() - pos - 30;
            var auto = $('.ui-autocomplete');
            var curHeight = $(auto).children().length * 21;
            if (curHeight > maxHeight) {
                $(auto).css({maxHeight: maxHeight+'px', overflow:'scroll', zIndex: 12});
            } else {
                $(auto).css({maxHeight: 'none', overflow:'hidden', zIndex: 12});
            }
        }
    },

    autoCompleteSelect: function(event, ui) {
	// This is a callback for autocomplete to let us know that the user selected
	// a gene from the list.
	// See http://api.jqueryui.com/autocomplete/#event-select
        var geneTrack = this.props.positionInfo.get('geneSuggestTrack');
        this.setState({position: ui.item.id,

                       //#*** TODO: This currently does nothing.  Hook it up to the model.
	               // highlight genes choosen from suggest list (#6330)
	               hgFindParams: { 'track': geneTrack,
				       'vis': 'pack',
				       'extraSel': '',
				       'matches': ui.item.internalId }
                       });
        this.props.update(this.props.path.concat('position'), ui.item.id);
        // Don't let autocomplete whack the input's value:
        event.preventDefault();
    },

    componentDidMount: function() {
        // If we have a geneSuggest track, set up autocomplete.
        var inputNode, $input;
        inputNode = this.refs.input.getDOMNode();
        $input = $(inputNode);
        $input.autocomplete({
            delay: 500,
            minLength: 2,
            source: this.autoCompleteSourceFactory(),
            open: this.autoCompleteMenuOpen,
            select: this.autoCompleteSelect
	});
        // IE8 voodoo... I tried logging all events on input node and disabling
        // ones that looked weird until I found something that prevented buggy behavior.
        // Without the following, if you click on an autocomplete item in IE8, then
        // IE8 only acts on the blur on the position field (causing an hgFind request)
        // and totally loses the select action (jquery-ui's autocomplete never gets
        // the select event).  This prevents the premature blur action when we click
        // on an autocomplete item, as long as jquery-ui className doesn't change...
        if (inputNode.onbeforedeactivate !== undefined) {
            console.log('IE8 onbeforedeactivate hack');
            $input.on('beforedeactivate', function(ev) {
                if (ev.originalEvent &&
                    ev.originalEvent.toElement &&
                    /ui-state-focus/.test(ev.originalEvent.toElement.className)) {
                    ev.preventDefault();
                }
            });
        }
    },

    render: function() {
        var posInfo = this.props.positionInfo;
        var spinner = null, posPopup = null;
        var loading = posInfo.get('loading');
        if (loading) {
            spinner = <Icon type="spinner" className="floatRight" />;
        }
        var matches = posInfo.get('positionMatches');
        if (matches) {
            posPopup = <PositionPopup positionMatches={matches}
                                      update={this.props.update} path={this.props.path}/>;
        }
        if (loading || matches) {
            // If the autocomplete menu is displayed when position search begins or when
            // position search results arrive, make the menu go away:
            $(this.refs.input.getDOMNode()).blur();
        }
        return (
            <div className={this.props.className}>
              <TextInput value={posInfo.get('position')}
                         path={this.props.path.concat('position')} update={this.props.update}
                         placeholder=""
                         size={45} ref='input' />
              {spinner}
              {posPopup}
            </div>
        );
    }

}); // PositionSearch

PositionPopup = React.createClass({
    // Helper component: when there are multiple matches from position/search,
    // display them in a popup box with links for the user to choose a position.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'hidePosPopup') called when user clicks to hide popup
    // update(path + 'positionMatch', matches): user clicks position link in popup
    //                                          (matches obj is from hgFind)

    propTypes: { positionMatches: pt.object.isRequired,  // Immutable.Map: multiple search results
               },

    makePosMatchLink: function(matches, i) {
        // Display a search result; if for a specific position, make a URL to choose that pos.
        var position = matches.get('position');
        var description = matches.get('description');
        description = description ? ' - ' + description : null;
        var posName = matches.get('posName');
        var posLabel = posName;
        if (position) {
            // Wrap posName with a link to select this position
            var update = this.props.update;
            var path = this.props.path.concat('positionMatch');
            var onClick = function(e) {
                update(path, matches);
                e.preventDefault();
                e.stopPropagation();
            };
            posLabel = <span>
              <a href="#" onClick={onClick} className="posLink">{posName}</a>{' at ' + position}
            </span>;
        }
        return (
            <div key={i}>
              {posLabel} {description}
            </div>
        );
    },

    makePosPopupSection: function(trackMatches, i) {
        // Make a section for results from one track.
        return (
            <div key={i}>
              <h3>{trackMatches.get('description')}</h3>
              {trackMatches.get('matches').map(this.makePosMatchLink).toJS()}
              <br />
            </div>
        );
    },

    render: function() {
        // Display position matches if necessary.
        return (
            <Modal title='Your search turned up multiple results; please choose one.'
                   path={this.props.path.concat('hidePosPopup')} update={this.props.update}>
              {this.props.positionMatches.map(this.makePosPopupSection).toJS()}
            </Modal>
        );
    }

}); // PositionPopup

// Without this, jshint complains that PositionSearch is not used.  Module system would help.
PositionSearch = PositionSearch;
