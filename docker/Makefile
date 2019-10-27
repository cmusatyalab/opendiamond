IMAGEID ?= registry.cmusatyalab.org/diamond/opendiamond/dev
SRC_TAR ?= opendiamond-HEAD.tar.gz
TINI_VERSION ?= v0.16.1
DISTRIBUTIONS = buster xenial centos6 centos7


all: tini
	( cd .. && git archive --output=docker/$(SRC_TAR) HEAD )
	for DIST in $(DISTRIBUTIONS); do \
	    docker build --pull -t $(IMAGEID):$$DIST -f Dockerfile.$$DIST . ; \
	    docker push $(IMAGEID):$$DIST ; \
	done
	$(RM) $(SRC_TAR)

tini:
	wget -O tini https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini
	chmod +x tini

