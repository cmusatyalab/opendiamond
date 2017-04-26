
IMAGEID ?= registry.cmusatyalab.org/diamond/opendiamond

all:
	docker build -t $(IMAGEID):jessie -f Dockerfile.jessie .
	docker build -t $(IMAGEID):xenial -f Dockerfile.xenial .
	docker build -t $(IMAGEID):centos6 -f Dockerfile.centos6 .
	docker build -t $(IMAGEID):centos7 -f Dockerfile.centos7 .

publish:
	docker push $(IMAGEID):jessie
	docker push $(IMAGEID):xenial
	docker push $(IMAGEID):centos6
	docker push $(IMAGEID):centos7
