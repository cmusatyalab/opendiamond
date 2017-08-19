#!/bin/sh
#
# Extract wrappers to run Diamond filters from docker container
#
#   UNIQUE_ID=$(docker inspect --format='{{ (index .RepoDigests 0) }}' $IMAGEID)
#   docker run --rm $UNIQUE_ID /extract-filters.sh $UNIQUE_ID > diamond-docker-filters.tgz
#
# To extract binary Diamond filters:
#
#   docker run --rm $UNIQUE_ID /extract-filters.sh > diamond-native-filters.tgz

set -e

# Bundle plain XML files into Diamond predicates
if [ -d /usr/local/share/diamond/predicates ] ; then
    for fxml in `find /usr/local/share/diamond/predicates -name *.xml -print`
    do
        echo "Bundling $fxml" 1>&2
        ( cd /usr/local/share/diamond/predicates ; diamond-bundle-predicate $fxml )
        rm -f $fxml
    done
fi

# Bundle plain XML files into Diamond codecs
if [ -d /usr/local/share/diamond/codecs ] ; then
    for fxml in `find /usr/local/share/diamond/codecs -name *.xml -print`
    do
        echo "Bundling $fxml" 1>&2
        ( cd /usr/local/share/diamond/codecs ; diamond-bundle-predicate $fxml )
        rm -f $fxml
    done
fi

# Wrap native filters if docker image is specified
if [ -n "$1" -a -d /usr/local/share/diamond/filters ] ; then
    UNIQUE_ID="$1"
    for filter in `find /usr/local/share/diamond/filters -type f -perm /100 -print`
    do
        echo "Wrapping $filter" 1>&2
        cat > $filter << EOF
# diamond-docker-filter
docker_image: ${UNIQUE_ID}
filter_command: ${filter}
EOF
    done
fi

tar -C /usr/local/share -cz diamond

