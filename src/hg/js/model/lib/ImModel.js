/* global backboneExtend, cart */

var ImModel = (function() {
    // A base class for an app-specific subclass that manages immutable UI
    // state, communication with the CGI/server, and UI events.

    'use strict';

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

        // cart is currently a global object but let's pretend it's not
        this.cart = cart;

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
            alert(Array.prototype.slice.call(arguments, 0, 2).join(' '));
        },

        registerCartVarHandler: function(cartVars, handler) {
            // Associate handler with each name in cartVars so that when the server sends
            // a response including that name, handler will be invoked with this and 
            // arguments mutState (mutable copy of state), cartVar, and newValue.
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
            if (! handler || ! _.isFunction(handler)) {
                this.error("registerUiHandler called without a valid handler", handler);
            }
            this.cartValidateHandlers.push(handler);
        },

        registerUiHandler: function(partialPath, handler) {
            // Associate handler with partialPath so that when the UI sends an event whose
            // path begins with partialPath, handler will be invoked with this and
            // arguments mutState (mutable copy of state), path, and optional data.
            if (! handler || ! _.isFunction(handler)) {
                this.error("registerUiHandler called without a valid handler", handler);
            }
            if (_.isString(partialPath) || _.isNumber(partialPath)) {
                partialPath = [partialPath];
            } else {
                partialPath = _.clone(partialPath);
            }
            // Follow partialPath through this.uiHandlers, adding nodes if necessary.
            // The last member of partialPath is an index into a node.handler object that
            // contains function(s) corresponding to partialPath.
            // Preceding members of partialPath are indexes into node.children objects that
            // contain child nodes.
            var node = this.uiHandlers;
            var lastIndex = partialPath.pop();
            _.forEach(partialPath, function(index) {
                // Descend to child, creating the child first if necessary.
                if (! node.children[index]) {
                    node.children[index] = { handlers: {}, children: {} };
                }
                node = node.children[index];
            });
            // Install handler in node.
            node.handlers[lastIndex] = node.handlers[lastIndex] || [];
            node.handlers[lastIndex].push(handler);
        },

        defaultServerErrorHandler: function(serverMessage) {
            this.error("error message from server:", serverMessage);
        },

        mergeServerResponse: function(mutState, jsonData, errorHandler) {
            // For each object in jsonData, if it has special handler(s) associated with it,
            // invoke the handler(s); otherwise, just put it in the top level of mutState.
            _.forEach(jsonData, function(newValue, key) {
                var handlers = this.cartJsonHandlers[key];
                if (handlers) {
                    handlers.forEach(function(handler) {
                        handler.call(this, mutState, key, newValue);
                    }, this);
                } else if (key === 'error') {
                    // Default, can be overridden of course
                    errorHandler = errorHandler || this.defaultServerErrorHandler;
                    errorHandler(newValue);
                } else {
                    mutState.set(key, Immutable.fromJS(newValue));
                }
            }, this);
        },

        handleServerResponseWithErrorHandler: function(errorHandler, jsonData) {
            // The server has sent some data; update state and call any registered handlers.
            console.log('from server:', jsonData);
            // No plan to support undo/redo for server updates, so just change this.state in place:
            this.state = this.state.withMutations(function(mutState) {
                // Update state with data from server:
                this.mergeServerResponse(mutState, jsonData, errorHandler);
                // Call validation/cleanup handler(s) if any:
                _.forEach(this.cartValidateHandlers, function(handler) {
                    handler.call(this, mutState);
                }, this);
            }.bind(this));
            this.render();
        },

        bumpUiState: function(updater) {
            // Reset our redo stack and push current state onto undo stack.
            // Replace this.state with a new immutable state changed by updater,
            // which is a function bound to this that receives a mutable copy of
            // this.state and may make changes to the mutable copy.
            var startState = this.state;
            this.state = this.state.withMutations(updater);
            if (this.state !== startState) {
                this.undoStack.push(startState);
                this.redoStack = [];
                this.state = this.state.withMutations(function(mutState) {
                    mutState.set('canUndo', true);
                    mutState.set('canRedo', false);
                });
            }
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
            // Follow path through uiHandlers and collect all handlers along the path:
            var node = this.uiHandlers;
            var handlers = [];
            _.forEach(path, function(index) {
                handlers = handlers.concat(node.handlers[index] || []);
                if (node.children[index]) {
                    node = node.children[index];
                } else {
                    // path continues past contents of uiHandlers -- we're done
                    return false; // break out of _.forEach
                }
            });
            // Advance to the next immutable state by calling all handlers with a temporarily
            // mutable copy of the current state:
            this.bumpUiState(function(mutState) {
                _.forEach(handlers, function(handler) {
                    handler.call(this, mutState, path, data);
                }, this);
            }.bind(this));
            this.cart.flush();
            this.render();
        },

        handlerForErrorHandler: function (errorHandler) {
            // If a custom errorHandler is provided, then return
            // this.handleServerResponseWithErrorHandler bound to errorHandler.
            // Otherwise return it bound to null so it wil use this.defaultServerErrorHandler.
            var handler = this.handleServerResponse;
            if (errorHandler) {
                handler = _.bind(this.handleServerResponseWithErrorHandler,
                                 this, errorHandler);
            } else if (!handler) {
                this.handleServerResponse = _.bind(this.handleServerResponseWithErrorHandler,
                                                   this, null);
                handler = this.handleServerResponse;
            }
            return handler;
        },

        cartDo: function(commandObj, errorHandler) {
            // Send a command to the server, expecting a response.
            var handler = this.handlerForErrorHandler(errorHandler);
            this.cart.send(commandObj, handler);
        },

        cartUploadFile: function(commandObj, jqFileInput, errorHandler) {
            // Send a command to the server, along with the file contents of jqFileInput.
            var handler = this.handlerForErrorHandler(errorHandler);
            this.cart.uploadFile(commandObj, jqFileInput, handler);
        },

        cartSend: function(commandObj) {
            // Send a command to the server; no need to handle response.
            this.cart.send(commandObj);
        },

        cartSet: function(cartVar, newValue) {
            // Tell server about cart var's new value, no need to handle server reponse.
            var setting = {};
            setting[cartVar] = newValue;
            this.cartSend({cgiVar: setting});
        },

        changeCartString: function(mutState, path, newValue) {
            // Change state's [path][cartVar] to newValue (if they differ), tell the server about it,
            // and re-render.
            mutState.setIn(path, newValue);
            var cartVar = path.pop();
            this.cartSet(cartVar, newValue);
            this.render();
        }

    });

    return ImModel;

})();

// Use Backbone.js's extend function for class inheritance:
ImModel.extend = backboneExtend;

