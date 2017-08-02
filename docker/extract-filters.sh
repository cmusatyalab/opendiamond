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
for fxml in `find /usr/local/share/diamond/predicates -type f -name *.xml -print`
do
    echo "Bundling $filter" 1>&2
    diamond-bundle-predicate $fxml
    rm -f $fxml
done

# Export native filters if no docket image is specified
if [ -n "$1" ] ; then

    UNIQUE_ID="$1"

    # tar up 'native' filters
    docker run --rm $UNIQUE_ID \
        tar -C /usr/local/share -cz diamond > diamond-native-filters.tgz

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

