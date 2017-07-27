
IMAGEID ?= registry.cmusatyalab.org/diamond/opendiamond
SRC_TAR ?= opendiamond-HEAD.tar.gz
TINI_VERSION ?= v0.15.0


all: tini
	( cd .. && git archive --output=docker/$(SRC_TAR) HEAD )
	docker build --pull -t $(IMAGEID):centos6 -f Dockerfile.centos6 .
	docker build --pull -t $(IMAGEID):centos7 -f Dockerfile.centos7 .
	docker build --pull -t $(IMAGEID):jessie -f Dockerfile.jessie .
	docker build --pull -t $(IMAGEID):stretch -f Dockerfile.stretch .
	docker build --pull -t $(IMAGEID):xenial -f Dockerfile.xenial .
	$(RM) $(SRC_TAR)

tini:
	wget -O tini https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini
	chmod +x tini

publish:
	docker push $(IMAGEID):centos6
	docker push $(IMAGEID):centos7
	docker push $(IMAGEID):jessie
	docker push $(IMAGEID):stretch
	docker push $(IMAGEID):xenial

