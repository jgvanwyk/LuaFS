PREFIX=/opt/local
DESTDIR=/usr/local

INCLDIR=$(PREFIX)/include/lua5.1
LUALIBDIR=$(DESTDIR)/lib/lua/5.1

filesystem.so: filesystem.c
	clang -I $(INCLDIR) -bundle -undefined dynamic_lookup -o filesystem.so filesystem.c

install: filesystem.so
	mkdir -p $(LUALIBDIR)
	cp filesystem.so $(LUALIBDIR)

clean:
	rm -f filesystem.so
