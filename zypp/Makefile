REPODIR?=	/etc/zypp

all:

clean:

install:
	sed -e 's/@@REPO@@/$(REPO)/' trx-control.repo >repo.out
	install -D -m 644 repo.out \
		${DESTDIR}${REPODIR}/trx-control.repo
	rm repo.out

repo:
	echo $(REPO)
