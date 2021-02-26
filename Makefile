PREFIX = /usr/local
BIN = swm

install: ${BIN}
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install ${BIN} ${DESTDIR}${PREFIX}/bin/${BIN}
	chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}

uninstall: ${DESTDIR}${PREFIX}/bin/${BIN}
	rm -f ${DESTDIR}${PREFIX}/bin/${BIN}

