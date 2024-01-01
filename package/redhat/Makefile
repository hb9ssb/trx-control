all:
	mkdir -p ~/rpmbuild/SOURCES
	make -C ../.. clean
	tar czf ~/rpmbuild/SOURCES/trx-control-1.0.0.tar.gz -C ../.. \
		--transform=s/./trx-control-1.0.0/ --exclude-vcs .
	rpmbuild -bb --define "_topdir $$HOME/rpmbuild" \
		${RPM_OPTS} trx-control.spec
