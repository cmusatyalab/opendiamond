
IMAGEID ?= registry.cmusatyalab.org/diamond/opendiamond
SRC_TAR ?= opendiamond-HEAD.tar.gz


all:
	( cd .. && git archive --output=docker/$(SRC_TAR) HEAD )
	docker build -t $(IMAGEID):centos6 -f Dockerfile.centos6 .
	docker build -t $(IMAGEID):centos7 -f Dockerfile.centos7 .
	docker build -t $(IMAGEID):jessie -f Dockerfile.jessie .
	docker build -t $(IMAGEID):stretch -f Dockerfile.stretch .
	docker build -t $(IMAGEID):xenial -f Dockerfile.xenial .
	$(RM) -f $(SRC_TAR)

publish:
	docker push $(IMAGEID):centos6
	docker push $(IMAGEID):centos7
	docker push $(IMAGEID):jessie
	docker push $(IMAGEID):stretch
	docker push $(IMAGEID):xenial

