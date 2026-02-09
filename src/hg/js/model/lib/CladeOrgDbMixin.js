var CladeOrgDbMixin = function(myPath) {
    // This is a model mixin that manages UI state and server interaction for
    // the clade, org and db (aka group, genome, assembly) menus.

    'use strict';
    /*jshint validthis: true */

    var myCartVar = 'cladeOrgDb';

    function findNodeByValue(nodeList, selValue) {
        // Return the member of nodeList whose value field matches selValue, if any.
        return nodeList.find(function(node) {
            return node.get('value') === selValue;
        });
    }

    function findNodeByPath(nodeList, nodePath) {
        // Return the descendant of Immutable nodeList addressed by plain array nodePath,
        // or null if not found.
        var selNode = null;
        if (nodeList && nodePath.length > 0) {
            selNode = findNodeByValue(nodeList, nodePath[0]);
            if (selNode && nodePath.length > 1) {
                // Not at end of path -- recurse on children:
                selNode = findNodeByPath(selNode.get('children'), nodePath.slice(1));
            }
        }
        return selNode;
    }

    function myGetIn(mutState, varName) {
        // getIn myPath+varName
        return mutState.getIn(myPath.concat(varName));
    }

    function mySetIn(mutState, varName, newValue) {
        // setIn myPath+varName
        mutState.setIn(myPath.concat(varName), newValue);
    }

    function valLabelsFromNodeList(nodeList) {
        // Given an Immutable.List of cladeOrgDb tree nodes, return an Immutable value+label list
        // for <LabeledSelect> menu options.
        return nodeList.map(function(node) {
            return Immutable.Map({ value: node.get('value'),
                                   label: node.get('label') });
        });
    }

    function generateMenuOptions(mutState) {
        // Use mutState[myPath] to make clade, organism and db menu data structures.
        // If clade/org/db are inconsistent with cladeOrgDb tree, reset them.
        var cladeOrgDb = myGetIn(mutState, 'cladeOrgDb');
        if (! cladeOrgDb) {
            // Can't make menus if we're still waiting for cladeOrgDb tree.
            return;
        }
        // If we don't already have a selected clade, default to first clade:
        var selClade = myGetIn(mutState, 'clade');
        var selCladeNode = findNodeByValue(cladeOrgDb, selClade);
        if (! selCladeNode) {
            selCladeNode = cladeOrgDb.first();
            mySetIn(mutState, 'clade', selCladeNode.get('value'));
        }
        // If we don't already have a selected org or db, use the parent node's default:
        var orgList = selCladeNode.get('children');
        var selOrg = myGetIn(mutState, 'org');
        var selOrgNode = findNodeByValue(orgList, selOrg);
        if (! selOrgNode) {
            selOrgNode = findNodeByValue(orgList, selCladeNode.get('default'));
            mySetIn(mutState, 'org', selOrgNode.get('value'));
        }
        var dbList = selOrgNode.get('children');
        var selDb = myGetIn(mutState, 'db');
        var selDbNode = findNodeByValue(dbList, selDb);
        if (! selDbNode) {
            selDbNode = findNodeByValue(dbList, selOrgNode.get('default'));
            mySetIn(mutState, 'db', selDbNode.get('value'));
        }
        mySetIn(mutState, 'cladeOptions', valLabelsFromNodeList(cladeOrgDb));
        mySetIn(mutState, 'orgOptions', valLabelsFromNodeList(orgList));
        mySetIn(mutState, 'dbOptions', valLabelsFromNodeList(dbList));
    }

    // Server event handler
    function codMergeServerResponse(mutState, cartVar, newValue) {
        // Update internal state and regenerate menu options.
        if (cartVar === myCartVar) {
            _.forEach(newValue, function(value, key) {
                mySetIn(mutState, key, Immutable.fromJS(value));
            });
            generateMenuOptions(mutState);
        } else {
            this.error('CladeOrgDbMixin: expected cart var "' + myCartVar + '" but got "' +
                       cartVar + '"');
        }
    }

    // UI event handler
    function changeCladeOrgDb(mutState, uiPath, newValue) {
        // uiPath is myPath + either 'clade', 'org' or 'db'.
        // User changed clade, org or db; if clade or org, figure out the new lower-level
        // selections.  Update state, tell the server, and call this.onChangeDb if present.
        var oldDb = myGetIn(mutState, 'db');
        // Update the changed item in mutState, then handle possible side-effects on lower levels:
        mutState.setIn(uiPath, newValue);
        var db = myGetIn(mutState, 'db');
        if (db !== oldDb) {
            this.cartSend({cgiVar: {db: db}});
            if (this.onChangeDb) {
                this.onChangeDb(mutState);
            }
        }
    }

    function getDbNode(mutState) {
        // Return the currently selected db's node.
        var cladeOrgDb = myGetIn(mutState, 'cladeOrgDb');
        var clade = myGetIn(mutState, 'clade');
        var org = myGetIn(mutState, 'org');
        var db = myGetIn(mutState, 'db');
        return findNodeByPath(cladeOrgDb, [clade, org, db]);
    }

    // Convenience methods for use outside this mixin:

    function getDb(mutState) {
        // Return the currently selected db
        return myGetIn(mutState, 'db');
    }

    function getDefaultPos(mutState) {
        // Get the default position of the currently selected db.
        var dbNode = getDbNode(mutState);
        return dbNode ? dbNode.get('defaultPos') : null;
    }

    function initialize() {
        this.registerCartVarHandler(myCartVar, codMergeServerResponse);
        this.registerUiHandler(myPath, changeCladeOrgDb);
        // Install convenience methods for use outside this mixin:
        this.getDb = getDb;
        this.getDefaultPos = getDefaultPos;
    }

    // Mixin object with initialize
    return { initialize: initialize };
};

// Without this, jshint complains that CladeOrgDbMixin is not used.  Module system would help.
CladeOrgDbMixin = CladeOrgDbMixin;
