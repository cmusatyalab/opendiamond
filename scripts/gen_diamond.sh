#!/bin/sh

RELEASE_NAME=diamond-l.0.0
#
# Make sure we have cleaned the source tree
#
pushd ../src
export DIAMOND_ROOT=`pwd`
make clean
popd

#
# Remove the old version in /tmp if it exists
#
rm -rf /tmp/"$RELEASE_NAME"

#
# copy over the whole tree
#
cp -r ../src /tmp/"$RELEASE_NAME"/

# remove the CVS directories
find /tmp/"$RELEASE_NAME" -name CVS -exec rm -rf {} \;

# remove diretories we don't want to ship
rm -rf /tmp/"$RELEASE_NAME"/tools/filter_sim 
rm -rf /tmp/"$RELEASE_NAME"/tools/fiord 


# generate the tar file
pushd /tmp
tar -cf /tmp/"$RELEASE_NAME".tar "$RELEASE_NAME"

popd
cp /tmp/"$RELEASE_NAME".tar .

gzip "$RELEASE_NAME".tar 
