var pt = React.PropTypes;

var PathUpdateOptional = {
    // This is a mixin that specifies optional propTypes path and update for a system in which
    // there is one model with one update function for handling any UI event.
    // The path is a list of successive indices into the model's state and/or keywords
    // to trigger specific actions.  The path has enough info for the model's single
    // update function to tell what the user did and take appropriate action.

    // Usually you'll want to use PathUpdate, which requires these props, unless making
    // a component that doesn't respond to any events, like a spinner icon.

    propTypes: { path: pt.array,  // path to this component's data in app model

                 update: pt.func
                 // update(path, optionalData) called with a distinct path for each UI event.
                 // A component may add to path if the component has more than one UI event.
               }
};

// Without this, jshint complains that PathUpdateOptional is not used.  Module system would help.
PathUpdateOptional = PathUpdateOptional;
