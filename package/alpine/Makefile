ARCH?=		$(shell uname -m)

REPOPATH=	/var/apks/$(ARCH)

SUBDIR +=	trx-control

all:
	@for d in ${SUBDIR}; \
	do \
		(cd $$d; make) \
	done

	ls -l $(REPOPATH)

	@-(cd ${REPOPATH}; apk index *.apk -o APKINDEX.tar.gz)
	@abuild-sign -q ${REPOPATH}/APKINDEX.tar.gz
	@cp /root/.abuild/signkey.rsa.pub ${REPOPATH}/signkey.rsa.pub
