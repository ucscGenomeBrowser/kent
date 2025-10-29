kentSrc = ../../../..
include ../../../../inc/common.mk

JS_DIR=../..

# The original JSX source files
JSX_FILES= CheckboxLabel.jsx CladeOrgDb.jsx Icon.jsx LabeledSelect.jsx LoadingImage.jsx \
		Modal.jsx Section.jsx TextInput.jsx PositionSearch.jsx \
		SetClearButtons.jsx Sortable.jsx SpeciesSearch.jsx TextArea.jsx UserRegions.jsx

# The final product after compiling and bundling
BUNDLE_FILE=reactLibBundle.js

include ${JS_DIR}/install.mk
include ../react.mk

JSX=/cluster/software/src/node-v0.10.24-linux-x64/bin/jsx
UGLIFYJS=/cluster/software/src/node-v0.10.24-linux-x64/bin/uglifyjs

