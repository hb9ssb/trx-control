SUBDIR+=	bin/rigctl \
		sbin/rigd \
		rigs

MANDIR?=	/usr/share/man

.PHONY:	build
build:		$(SUBDIR)
build:		subdir

.PHONY: subdir $(SUBDIR)
subdir: $(SUBDIR)

$(SUBDIR):
	$(MAKE) -C $@ $(TARGET)

# Recursive targets
.PHONY: clean
clean:		TARGET=clean
clean: 		subdir

.PHONY: cleandir
cleandir:	TARGET=cleandir
cleandir: 	subdir

.PHONY: install rig-control.7
install:	TARGET=install
install: 	subdir rig-control.7
	@echo all installed

rig-control.7:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man7/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man7/$@

# Dependencies
