all:
	mkdir -p ~/rpmbuild/SOURCES
	make -C ../.. clean
	tar czf ~/rpmbuild/SOURCES/trx-control-${VERSION}.tar.gz -C ../.. \
		--transform=s/./trx-control-${VERSION}/ --exclude-vcs .
	rpmbuild -bb --define "_topdir $$HOME/rpmbuild" \
		--define "version ${VERSION}" \
		--define "release ${RELEASE}" \
		--define "pg_version ${PG_VERSION}" \
		${RPM_OPTS} trx-control.spec
