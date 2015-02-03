/** @jsx React.DOM */
/* global ImmutableUpdate, PathUpdate, LabeledSelect, Section, TextInput */
var pt = React.PropTypes;

// Graphical / auto-complete interface for choosing an organism and assembly.

var AssemblySearch = React.createClass({
    // Auto-complete search input for typing database names or keywords found in hgcentral.dbDb

    mixins: [PathUpdate, ImmutableUpdate],
    // update(path + 'searchDone', autoCompleteObject) called when user changes search term

    propTypes: { searchTerm: pt.string  // Contents of search input
               },

    autoCompleteSourceFactory: function() {
	// This returns a 'source' callback function for jqueryui.autocomplete.
        // The function closure allows us to keep a private cache of past searches.
	var baseUrl = '../cgi-bin/hgChooseDb?hgcd_term=';
	// We get a lot of duplicate requests (especially the first letters of
	// words), so we keep a cache of the suggestions lists we've retreived.
	var cache = {};

        //#*** everything below is copied from hgAi -- libify!

	return function (request, acCallback) {
	    // This is a callback for jqueryui.autocomplete: when the user types
	    // a character, this is called with the input value as request.term and an acCallback
	    // for this to return the result to autocomplete.
	    // See http://api.jqueryui.com/autocomplete/#option-source
            var key = request.term;
            if (cache[key]) {
		acCallback(cache[key]);
            } else {
		var url = baseUrl + encodeURIComponent(key);
		$.getJSON(url)
		    .done(function(result) {
			cache[key] = result;
			acCallback(result);
		    });
		// ignore errors to avoid spamming people on flaky network connections
		// with tons of error messages (#8816).
            }
	};
    },

    autoCompleteMenuOpen: function() {

        //#*** everything below is copied from hgAi -- libify!

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
	// a term from the list.  See http://api.jqueryui.com/autocomplete/#event-select
        this.props.update(this.props.path.concat('searchDone'), ui.item);
        // Don't let autocomplete whack the input's value:
        event.preventDefault();
    },

    componentDidMount: function() {
        // Now that we have a DOM node, setup jquery-ui autocomplete on it.
        var inputNode = this.refs.input.getDOMNode();
        var $input = $(inputNode);
        $input.autocomplete({
            delay: 500,
            minLength: 2,
            source: this.autoCompleteSourceFactory(),
            open: this.autoCompleteMenuOpen,
            select: this.autoCompleteSelect
	});
    },

    render: function() {
        var path = this.props.path || [];
        var searchTerm = this.props.searchTerm;
        return (
            <div>
              <span className='sectionItem'>
                Search for an organism's common name, scientific name or database prefix:
              </span>
              <TextInput value={searchTerm}
                         path={path.concat('searchTerm')} update={this.props.update}
                         size={45} ref='input' />
            </div>
        );
    }

}); // AssemblySearch

var noImg = '../images/DOT.gif';

var AppComponent = React.createClass({
    // hgChooseDb interface

    mixins: [ImmutableUpdate],

    onGo: function() {
        var path = this.props.path || [];
        this.props.update(path.concat('go'));
    },

    renderSpeciesButton: function(species) {
        // Show a labeled image button for a species such as Human or Mouse
        var name = species.get('genome');
        var displayName = name;
        if (name === 'D. melanogaster') {
            displayName = 'Fruitfly';
        } else if (name === 'C. elegans') {
            displayName = 'Worm';
        }
        var imgPath = species.get('img') || noImg;
        var onClick = function() {
            this.props.update(['popular', name]);
        }.bind(this);
        return (
            <div key={name} className='speciesButton' onClick={onClick}>
              {displayName} <br />
              <img src={imgPath} className='speciesIcon' />
            </div>
        );
    },

    renderDbSelect: function(menuData) {
        // If our state includes db menu options, display a menu and Go button,
        // otherwise nothing.
        if (! menuData) {
            return null;
        }
        var path = this.props.path || [];
        var menuLabel = 'Choose ' + menuData.get('genome') + ' assembly version' +
                                                 ', or choose a different species above:';
        var imgPath = menuData.get('img');
        var img = imgPath ? <img src={imgPath} className='speciesIcon' /> : null;
        return (
            <div className='sectionRow'>
              {img}
              <LabeledSelect label={menuLabel} selected={menuData.get('db')}
                             options={menuData.get('dbOptions')}
                             update={this.props.update} path={path.concat('db')} />
              <br />
              <br />
              <input type='button' value='Go to Genome Browser' onClick={this.onGo} />
            </div>
        );
    },

    render: function() {
        var path = [];
        var appState = this.props.appState;
        var appStateJS = appState.toJS();
        console.log('top-level render:', appStateJS);

        var popularSpecies = appState.get('popularSpecies');
        var dbMenuData = appState.get('dbMenuData');
        var searchTerm = appState.get('searchTerm') || "";

        return (
              <Section title='Select Species and Assembly Version'>
                {popularSpecies.map(this.renderSpeciesButton).toJS()}
                <AssemblySearch searchTerm={searchTerm} path={path} update={this.props.update} />
                {this.renderDbSelect(dbMenuData)}
              </Section>
        );
    }

});

// Without this, jshint complains that AppComponent is not used.  Module system would help.
AppComponent = AppComponent;
