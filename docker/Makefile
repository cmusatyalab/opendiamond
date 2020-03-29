IMAGEID ?= registry.cmusatyalab.org/diamond/opendiamond/dev
SRC_TAR ?= opendiamond-HEAD.tar.gz
DISTRIBUTIONS = buster xenial centos6 centos7

all:
	( cd .. && git archive --output=docker/$(SRC_TAR) HEAD )
	for DIST in $(DISTRIBUTIONS); do \
	    docker build --pull -t $(IMAGEID):$$DIST -f Dockerfile.$$DIST . ; \
	done
	#for DIST in $(DISTRIBUTIONS); do \
	#    docker push $(IMAGEID):$$DIST ; \
	#done
	$(RM) $(SRC_TAR)
