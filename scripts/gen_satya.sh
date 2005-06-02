#!/bin/sh

RELEASE_NAME=diamond-satya-v2
#
# Make sure we have cleaned the source tree
#
pushd ../src
export DIAMOND_ROOT=`pwd`
make distclean
popd

#
# Remove the old version in /tmp if it exists
#
rm -rf /tmp/"$RELEASE_NAME"

#
# copy over the whole tree
#
cp -r ../src /tmp/"$RELEASE_NAME"/


# remove diretories we don't want to ship
rm -rf /tmp/"$RELEASE_NAME"/tools/filter_sim 
rm -rf /tmp/"$RELEASE_NAME"/tools/fiord 
rm -rf /tmp/"$RELEASE_NAME"/test


# generate the tar file
pushd /tmp
tar -czf /tmp/"$RELEASE_NAME".tgz "$RELEASE_NAME"

popd
cp /tmp/"$RELEASE_NAME".tgz .

