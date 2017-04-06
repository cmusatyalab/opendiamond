# Various Docker images

Here we build the opendiamond helper libraries for various OSes that are
available as Docker containers. These are currently set up to be used as base
images to develop Diamond filters and not for running Diamond daemon processes
(diamondd/dataretriever/scopeserver).

The Centos6/7 images build both 32-bit and 64-bit libraries which are installed
in `/usr/lib` and `/usr/lib64` respectively.

The Debian-jessie and Ubuntu-xenial only build 64-bit libraries and install
them in `/usr/local/lib`.


## Docker Registry

These images should be available using an image-id as follows.

    registry.cmusatyalab.org/diamond/opendiamond:{jessie,xenial,centos6,centos7}


## Filter wrappers

When a new filter has been built and installed in an image under
`/usr/local/share/diamond/{predicates,codecs,filters}`, client-side wrappers
can be created with the following command.

    mkdir artifacts
    docker run --rm -v `pwd`/artifacts:/artifacts {imageid} /build-filter-wrappers.sh

This will create a `diamond-filters.tgz` archive in the artifacts directory
that contains `diamond/{predicates,codecs,filters}` but with executable filter
code replaces with wrappers to execute the docker container.
