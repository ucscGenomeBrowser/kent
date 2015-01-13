var CladeOrgDbMixin = function(myPath) {
    // This is a model mixin that manages UI state and server interaction for
    // the clade, org and db (aka group, genome, assembly) menus.

    var myCartVars = ['clade', 'org', 'db', 'cladeOptions', 'orgOptions', 'dbOptions'];

    // Server event handler
    function codMergeServerResponse(cartVar, newValue) {
        // cart vars in myCartVars all live in state[myPath] for Immutable efficiency but can arrive
        // independently from server; when we get one, update just that piece of state[myPath].
        this.mutState.setIn(myPath.concat(cartVar), Immutable.fromJS(newValue));
        // Turn off the loading spinner when we get result(s) from cart:
        if (cartVar === 'position' || cartVar === 'positionMatches') {
            this.mutState.setIn(['positionInfo', 'loading'], false);
        }
    }

    // UI event handler

    // TODO: libify this
    function capitalizeFirstLetter(string) {
    // http://stackoverflow.com/questions/1026069/capitalize-the-first-letter-of-string-in-javascript
        return string.charAt(0).toUpperCase() + string.slice(1);
    }

    function changeCladeOrgDb(path, newValue) {
        // path is myPath + either 'clade', 'org' or 'db'.
        // User changed clade, org or db; tell server, which will send a lot of db-specific info
        // and menu changes if clade or org was changed (handleServerResponse gets those).
        this.mutState.setIn(path, newValue);
        var which = path.pop();
        var command = {};
        var commandName = 'change' + capitalizeFirstLetter(which);
        command[commandName] = {newValue: newValue};
        this.cartDo(command);
    }

    function initialize() {
        this.registerCartVarHandler(myCartVars, codMergeServerResponse);
        this.registerUiHandler(myPath, changeCladeOrgDb);
    }

    // Mixin object with initialize
    return { initialize: initialize };
};
