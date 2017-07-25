#!/usr/bin/env bash

cd /usr/local/share/diamond/predicates
for fxml in *.xml; do
    diamond-bundle-predicate $fxml
    rm -f $fxml
done