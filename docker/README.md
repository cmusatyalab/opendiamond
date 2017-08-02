# Various Docker images

Here we build the opendiamond helper libraries for various OSes that are
available as Docker containers.

The Centos6/7 images build both 32-bit and 64-bit libraries which are installed
in `/usr/lib` and `/usr/lib64` respectively.

The Debian-jessie and Ubuntu-xenial only build 64-bit libraries and install
them in `/usr/local/lib`.

## Purpose

These images are currently set up to be used as base
images to develop Diamond filters --- mainly for speaking the Diamond filter
protocol and generating Dockerized filter wrappers.
They are **not** intended for running Diamond daemon processes
(e.g., diamondd/dataretriever/scopeserver).
In particular, not all dependencies to run these daemons are installed
in order to reduce build time and Docker image size.


## Docker Registry

These images should be available using an image-id as follows.

    registry.cmusatyalab.org/diamond/opendiamond:{jessie,xenial,centos6,centos7}


## Filter wrappers

When a new filter has been built and installed in an image under
`/usr/local/share/diamond/{predicates,codecs,filters}`, client-side wrappers
can be created with the following commands.

    docker push $IMAGEID
    UNIQUE_ID=$(docker inspect --format='{{ (index .RepoDigests 0) }}' $IMAGEID)
    docker run --rm  {imageid} /extract-filters.sh $UNIQUE_ID > diamond-filters.tgz

This will create a `diamond-filters.tgz` archive that contains
`diamond/{predicates,codecs,filters}` but with executable filter code
replaced with wrappers to execute the docker container.

