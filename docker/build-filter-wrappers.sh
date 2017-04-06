#!/bin/sh

IMAGEID="$1"

# If we run in an ephemeral docker container, we can
# safely clobber the filters we find.
for filter in /usr/local/share/diamond/filters/* ; do
    cat > $filter << EOF
#!/bin/sh
exec docker run --rm -i --log-driver=none -v/dev/null:/dev/raw1394 --entrypoint=$filter $IMAGEID "\$@"
EOF
done

# Now tar things up and export it through a volume
# mounted at /artifacts.
cd /usr/local/share
tar -cvzf /artifacts/diamond-docker-filters.tgz diamond
