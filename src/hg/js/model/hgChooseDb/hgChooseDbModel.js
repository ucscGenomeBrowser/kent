/* global AppComponent, ImModel, cart */

var HgChooseDbModel = ImModel.extend({

    popularSpeciesClick: function(mutState, path) {
        // User clicked on a popular species button; show db menu for that species.
        var clickedSpecies = path.pop();
        // TODO: make server request here and move the following to a handleCartVar.
        var popularSpecies = mutState.get('popularSpecies');
        var clickedSpeciesData = popularSpecies.find(function(popSpecies) {
            return (popSpecies.get('genome') === clickedSpecies);
        });
        mutState.set('dbMenuData', clickedSpeciesData);
    },

    searchDone: function(mutState, path, uiItem) {
        // Use uiItem object (from jqueryui autocomplete data) to update state & get new menu
        mutState.set('searchTerm', uiItem.value);
        this.cartDo({ getDbMenu: uiItem });
    },

    changeDb: function(mutState, path, newDb) {
        mutState.setIn(['dbMenuData', 'db'], newDb);
    },

    goToHgTracks: function(mutState) {
        // Update the invisible form with the selected db and submit it (to hgTracks).
        $('input[name="db"]').val(mutState.getIn(['dbMenuData', 'db']));
        $('#mainForm')[0].submit();
    },

    initialize: function() {
        // Register handlers for ui events (no cart responses yet):
        this.registerUiHandler(['popular'], this.popularSpeciesClick);
        this.registerUiHandler(['searchDone'], this.searchDone);
        this.registerUiHandler(['db'], this.changeDb);
        this.registerUiHandler(['go'], this.goToHgTracks);

        // Bootstrap initial state from page or by server request, then render:
        if (window.initialAppState) {
            this.state = Immutable.fromJS(window.initialAppState);
            this.render();
        } else {
            this.cartDo({ getPopularSpecies: {} });
        }
    }

}); // HgChooseDbModel

cart.setCgi('hgChooseDb');

var container = document.getElementById('appContainer');

//#*** Copied from HgAiModel -- libify in ImModel??
function render(state, update, undo, redo) {
    // ImModel calls this function after making changes to state.
    // Pass the immutable state object and callbacks to our top-level react component:
    React.render(React.createElement(AppComponent, {appState: state,
                                                    update: update, undo: undo, redo: redo }),
		 container);
}

var hgChooseDbModel = new HgChooseDbModel(render);
hgChooseDbModel.initialize();
