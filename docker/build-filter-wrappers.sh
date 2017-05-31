#!/bin/sh

IMAGEID="$1"

cd /usr/local/share

# tar up 'native' filters and export through a volume
# mounted at /artifacts.
tar -cvzf /artifacts/diamond-native-filters.tgz diamond

# As long as we run in an ephemeral docker container, we
# can safely clobber any files we find.
for filter in /usr/local/share/diamond/filters/* ; do
    cat > $filter << EOF
#!/bin/sh
docker pull $IMAGEID >/dev/null 2>&1
NAME=filter-\$(cat /proc/sys/kernel/random/uuid)
docker run --name \$NAME --rm --detach --publish 127.0.0.1::5555 \
    --entrypoint /diamond-init.sh $IMAGEID $filter "\$@" >/dev/null
sleep 1
exec socat - TCP4:\$(docker port \$NAME 5555)
EOF
done

# Now tar up the docker wrapped filters and export.
tar -cvzf /artifacts/diamond-docker-filters.tgz diamond
