
PARENT_MODULE=	linux

SUBDIR=		log select socket stat

all:
	make -C log
	make -C select
	make -C socket
	make -C stat

install:
	make -C log install
	make -C select install
	make -C socket install
	make -C stat install

clean:
	make -C log clean
	make -C select clean
	make -C socket clean
	make -C stat clean
