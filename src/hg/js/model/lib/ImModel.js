var ImModel = (function() {
    // A base class for an app-specific subclass that manages immutable UI
    // state, communication with the CGI/server, and UI events.

    var ImModel = function(render) {
        // construct a new instance of (a subclass of) ImModel and bind its functions
        // so that "this" means this.
        _.bindAll(this);
        // render is a function that takes an Immutable.Map representing UI state
        // and our methods update, undo and redo, and uses them to reconstruct the app UI.
        this.render = function() { render(this.state, this.update, this.undo, this.redo); };

        // A single immutable data structure holds all UI state
        this.state = Immutable.Map();

        // Undo/redo stacks made easy by using immutable state
        this.undoStack = [];
        this.redoStack = [];

        // Handler functions for server and UI update paths
        // Cart is flat, so cartJsonHandlers is just an object that maps cart var names
        // to arrays of functions.
        this.cartJsonHandlers = {};
        this.cartValidateHandlers = [];
        // There can be complicated paths through uiHandlers, with handler functions
        // at any level, so uiHandlers has a tree structure:
        this.uiHandlers = { handlers: {}, children: {} };

        // Call each mixin's initialize function so it can register handlers.
        if (this.mixins) {
            _.forEach(this.mixins, function(mixin) {
                if (mixin.initialize) {
                    mixin.initialize.call(this);
                } else {
                    this.error("Mixin doesn't have initialize():", mixin);
                }
            }, this);
        }

    };

    // ImModel methods:
    _.extend(ImModel.prototype, {

        error: function() {
            // By default, log error on the console and alert user.
            console.error.apply(console, arguments);
            alert(Array.prototype.slice.call(arguments, 0, 1));
        },

        registerCartVarHandler: function(cartVars, handler) {
            // Associate handler with each name in cartVars so that when the server sends
            // a response including that name, handler will be invoked with this and 
            // arguments (cartVar, newValue).
            if (_.isString(cartVars)) {
                cartVars = [cartVars];
            }
            _.forEach(cartVars, function(cartVar) {
                if (! this.cartJsonHandlers[cartVar]) {
                    this.cartJsonHandlers[cartVar] = [];
                }
                this.cartJsonHandlers[cartVar].push(handler);
            }, this);
        },

        registerCartValidateHandler: function(handler) {
            // After all of the registered cart var handlers have been called,
            // the registered validation / cleanup function will be called.
            this.cartValidateHandlers.push(handler);
        },

        registerUiHandler: function(partialPath, handler) {
            // Associate handler with partialPath so that when the UI sends an event whose
            // path begins with partialPath, handler will be invoked with this and
            // arguments path and optional data.
            if (_.isString(partialPath) || _.isNumber(partialPath)) {
                partialPath = [partialPath];
            }
            // Follow partialPath as far as it already goes in this.uiHandlers;
            // add nodes to this.uiHandlers if part or all of partialPath isn't there yet.
            // Each node of uiHandlers is an object with an array of handlers and,
            // if not a leaf node, an array or object of child nodes.
            // The last component of path becomes an index into the handlers array
            // of the next-to-last component's node.
            var node = this.uiHandlers, done = false;
            _.forEach(partialPath, function(index, depth) {
                var nextIndex = partialPath[depth+1];
                if (! node.children[index]) {
                    // children.[index] doesn't yet exist; look at next element in partialPath,
                    // if any, to determine whether children.[index].children should be an array,
                    // object, or if handler should be added to node.handlers instead.
                    if (nextIndex) {
                        node.children[index] = { handlers: {}, children: {} };
                    } else {
                        // last index in partialPath -- assign handler:
                        node.handlers[index] = node.handlers[index] || [];
                        node.handlers[index].push(handler);
                        done = true;
                    }
                } else if (! nextIndex) {
                    // found node for last index in partialPath -- assign handler to current node:
                    node.handlers[index] = node.handlers[index] || [];
                    node.handlers[index].push(handler);
                    done = true;
                }
                // Now node.children[index] definitely exists; descend to it.
                node = node.children[index];
            }, this);
            if (! done) {
                this.error('registerUiHandler: error in tree logic since partialPath was neither '+
                           'found nor created:', path, this.uiHandlers);
            }
        },

        mergeServerResponse: function(jsonData) {
            // For each object in jsonData, if it has special handler(s) associated with it,
            // invoke the handler(s); otherwise, just put it in the top level of mutState.
            Object.keys(jsonData).forEach(function(key) {
                var newValue = jsonData[key];
                var handlers = this.cartJsonHandlers[key];
                if (handlers) {
                    handlers.forEach(function(handler) {
                        handler.call(this, key, newValue);
                    }, this);
                } else if (key === 'error') {
                    // Default, can be overridden of course
                    this.error('error message from CGI:', newValue);
                } else {
                    this.mutState.set(key, Immutable.fromJS(newValue));
                }
            }, this);
        },

        handleServerResponse: function(jsonData) {
            // The server has sent some data; update state and call any registered handlers.
            console.log('from server:', jsonData);
            // No plan to support undo/redo for server updates, so just change this.state in place:
            this.state = this.state.withMutations(function(mutState) {
                this.mutState = mutState;
                // Update state with data from server:
                this.mergeServerResponse(jsonData);
                // Call validation/cleanup handler(s) if any:
                _.forEach(this.cartValidateHandlers, function(handler) {
                    handler.call(this);
                }, this);
                this.mutState = null;
            }.bind(this));
            this.render();
        },

        bumpUiState: function(updater) {
            // Reset our redo stack and push current state onto undo stack.
            // Replace this.state with a new immutable state changed by updater (bound to this),
            // which is a function that receives a mutable copy of this.state and may make
            // changes to the mutable copy.
            this.undoStack.push(this.state);
            this.redoStack = [];
            this.state = this.state.withMutations(function(mutState) {
                mutState.set('canUndo', true);
                mutState.set('canRedo', false);
                updater.call(this, mutState);
            }.bind(this));
        },

        undo: function() {
            // User wants to undo their last action; if we can undo, push current state
            // onto redo stack and replace with popped state from undo stack (updated to have
            // correct canUndo and canRedo flags).
            if (this.undoStack.length) {
                var prevState = this.undoStack.pop();
                this.redoStack.push(this.state);
                this.state = prevState.withMutations(function(mutState) {
                    mutState.set('canUndo', this.undoStack.length > 0);
                    mutState.set('canRedo', true);
                }.bind(this));
            } else {
                // Shouldn't get here because of canUndo/canRedo flags
                alert("Sorry, can't undo any more.");
            }
            this.render();
        },

        redo: function() {
            // User wants to redo their last action; if we can redo, push current state
            // onto undo stack and replace with popped state from redo stack (updated to have
            // correct canUndo and canRedo flags).
            if (this.redoStack.length) {
                var nextState = this.redoStack.pop();
                this.undoStack.push(this.state);
                this.state = nextState.withMutations(function(mutState) {
                    mutState.set('canUndo', true);
                    mutState.set('canRedo', this.redoStack.length > 0);
                }.bind(this));
            } else {
                // Shouldn't get here because of canUndo/canRedo flags
                alert("Sorry, can't redo any more.");
            }
            this.render();
        },

        update: function(path, data) {
            // A UI event has occurred and now it's time to update UI state and possibly
            // send a request or update to the server/CGI.  path must point us to at
            // least one leaf function in uiHandlers, and may contain additional components
            // that help the handler function figure out what changed but don't point to any
            // handler.  Some data may or may not be given; pass whatever we get to the handler.
            console.log('ImModel.update:', path, data);
            this.bumpUiState(function(mutState) {
                this.mutState = mutState;
                var node = this.uiHandlers, handlers;
                _.forEach(path, function(index, depth) {
                    var nextIndex = path[depth+1];
                    var handlers = node.handlers[index];
                    if (handlers) {
                        _.forEach(handlers, function(handler) {
                            handler.call(this, path, data);
                        }, this);
                    }
                    if (node.children[index]) {
                        node = node.children[index];
                    } else if (nextIndex) {
                        // path continues past contents of uiHandlers -- we're done
                        return false; // break out of _.forEach
                    }
                }, this);
                this.mutState = null;
            });
            this.render();
        },

        cartDo: function(commandObj) {
            // Send a command to the server; this.handleServerResponse will process response.
            cart.send(commandObj, this.handleServerResponse);
        },

        cartSend: function(commandObj) {
            // Send a command to the server; no need to handle response.
            cart.send(commandObj);
        },

        cartSet: function(cartVar, newValue) {
            // Tell server about cart var's new value, no need to handle server reponse.
            var setting = {};
            setting[cartVar] = newValue;
            this.cartSend({cgiVar: setting});
        },

        changeCartVar: function(path, newValue) {
            // Change state's [path][cartVar] to newValue (if they differ), tell the server about it,
            // and re-render.
            this.mutState.setIn(path, newValue);
            var cartVar = path.pop();
            this.cartSet(cartVar, newValue);
            this.render();
        }

    });

    return ImModel;

})();

// Use Backbone.js's extend function for class inheritance:
ImModel.extend = backboneExtend;

