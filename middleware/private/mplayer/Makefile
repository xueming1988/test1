SUBDIRS= mplayer

all release install:
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

