#!/bin/sh

IMAGEID="$1"

cd /usr/local/share

# tar up 'native' filters and export through a volume
# mounted at /artifacts.
tar -cvzf /artifacts/diamond-native-filters.tgz diamond

# As long as we run in an ephemeral docker container, we
# can safely clobber any files we find.
for filter in /usr/local/share/diamond/filters/* ; do
    cat > ${filter} << EOF
#!/usr/bin/env diamond-docker-helper
docker_image: ${IMAGEID}
docker_command:  "socat TCP4-LISTEN:5555,fork,nodelay EXEC:\"${filter} --filter\" "
docker_port: 5555
EOF
done

# Now tar up the docker wrapped filters and export.
tar -cvzf /artifacts/diamond-docker-filters.tgz diamond