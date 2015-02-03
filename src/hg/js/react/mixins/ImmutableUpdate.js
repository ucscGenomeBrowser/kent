var ImmutableUpdate = {

    // This is actually not a React component but rather a mixin for React components that
    // receive immutable props.  Immutable objects, even deeply nested structures, can be
    // compared using a top-level === to detect changes.

    shouldComponentUpdate: function(nextProps, nextState) {
        // If we can see a change in props or state, then yes we should re-render.
        var thisProps = this.props;
        var thisState = this.state || {};
        nextState = nextState || {};
        var propKeys = Object.keys(thisProps);
        var stateKeys = Object.keys(thisState);
        if (propKeys.length !== Object.keys(nextProps).length ||
            stateKeys.length !== Object.keys(nextState).length) {
            return true;
        }
        // When props are immutable, we can use a single === per prop to detect changes.
        var foundChange = false;
        _.forEach(propKeys, function(key) {
            var thisVal = thisProps[key];
            var nextVal = nextProps[key];
            var thisIsImmutable = thisVal instanceof Immutable.Iterable;
            var nextIsImmutable = nextVal instanceof Immutable.Iterable;
            if (thisIsImmutable !== nextIsImmutable &&
                !_.isNull(thisVal) && !_.isUndefined(thisVal) &&
                !_.isNull(nextVal) && !_.isUndefined(nextVal)) {
                console.warn('ImmutableUpdate.shouldComponentUpdate: inconsistent use of ' +
                             'Immutable vs mutable for prop', key, ':', thisVal, nextVal);
            }
            if (thisIsImmutable || nextIsImmutable) {
                if (thisVal !== nextVal) {
                    if (this.debug) {
                        var thisPrintable = thisIsImmutable ? thisVal.toJS() : thisVal;
                        var nextPrintable = nextIsImmutable ? nextVal.toJS() : nextVal;
                        console.log('ImmutableUpdate: immutable prop', key, 'changed from',
                                    thisPrintable, 'to', nextPrintable);
                    }
                    foundChange = true;
                    return false; // to break out of _.forEach
                }
            } else if (! _.isEqual(thisVal, nextVal)) {
                if (this.debug) {
                    console.log('ImmutableUpdate: mutable prop', key, 'changed from',
                                thisVal, 'to', nextVal);
                }
                foundChange = true;
                return false; // to break out of _.forEach
            }
        });
        if (foundChange) {
            return true;
        }
        // If this app is using state, then a deeper comparison is required for nested objects.
        stateKeys.forEach(function(key) {
            if (! _.isEqual(thisState[key], nextState[key])) {
                if (this.debug) {
                    console.log('ImmutableUpdate: state', key, 'changed from', thisState[key],
                                'to', nextState[key]);
                }
                foundChange = true;
                return false; // to break out of _.forEach
            }            
        });
        return foundChange;
    }

};

// Without this, jshint complains that ImmutableUpdate is not used.  Module system would help.
ImmutableUpdate = ImmutableUpdate;
