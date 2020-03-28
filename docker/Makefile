IMAGEID ?= registry.cmusatyalab.org/diamond/opendiamond/dev
SRC_TAR ?= opendiamond-HEAD.tar.gz
TINI_VERSION ?= v0.16.1
MINICONDA_VERSION ?= 4.7.12.1
DISTRIBUTIONS = buster xenial centos6 centos7


all: tini miniconda.sh
	( cd .. && git archive --output=docker/$(SRC_TAR) HEAD )
	for DIST in $(DISTRIBUTIONS); do \
	    docker build --pull -t $(IMAGEID):$$DIST -f Dockerfile.$$DIST . ; \
	    docker push $(IMAGEID):$$DIST ; \
	done
	$(RM) $(SRC_TAR)

tini:
	wget -O tini https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini
	chmod +x tini

miniconda.sh:
	wget -O miniconda.sh https://repo.anaconda.com/miniconda/Miniconda3-${MINICONDA_VERSION}-Linux-x86_64.sh
