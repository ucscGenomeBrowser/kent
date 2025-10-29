/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, Icon, Modal, TextInput */
var pt = React.PropTypes;

var SpeciesSearch = React.createClass({
    // Text input for position or search term, optionally with autocomplete if
    // props includes geneSuggestTrack, and a PositionPopup (defined below)
    // for multiple position/search matches.

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'position', newValue) called when user changes position
    // update(path + 'hidePosPopup') called when user clicks to hide popup
    // update(path + 'positionMatch', matches): user clicks position link in popup
    //                                          (matches obj is from hgFind)

    propTypes: {   // Immutable.Map {
        //   position: initial value of position input
        //   loading (bool): display spinner next to position input
        //   positionMatches (Immutable.Vector of Maps): multiple search results for popup

        // Optional
        className: pt.string   // class(es) to pass to wrapper div
    },

    /*
    autoCompleteSourceFactory: function() {
        // This returns a 'source' callback function for jqueryui.autocomplete.
        // We get a lot of duplicate requests (especially the first letters of
        // words), so we keep a cache of the suggestions lists we've retreived.
        var cache = {};

        function makeUrl(key) {
            return 'hubApi/findGenome?browser=mustExist&q=' encodeURIComponent(key);
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
                    var url = makeUrl(key);
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
        // a species from the list.
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
    */

    componentDidMount: function() {
        // If we have a geneSuggest track, set up autocomplete.
        var inputNode, $input;
        inputNode = this.refs.input.getDOMNode();
        $input = $(inputNode);
        initSpeciesAutoCompleteDropdown($input.id, genomeLabel);
    },

    render: function() {
        return (
            <div className={this.props.className}>
              <label className="genomeSearchLabelDefault" for="genomeSearch">
                Change selected genome:
              </label>
              <div className="searchBarAndButton">
                  <TextInput 
                             update={this.props.update}
                             size={45} ref='input' />
                  <div className="searchCell">
                    Current Genome:
                    <span id="genomeLabel">temp</span>
                  </div>
              </div>
            </div>
        );
    }

}); // SpeciesSearch

// Without this, jshint complains that SpeciesSearch is not used.  Module system would help.
SpeciesSearch = SpeciesSearch;
