SUBDIRS= source 

all release:
	for dir in $(SUBDIRS); do ( make -C $$dir) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do (make -C $$dir clean;) ||exit 1; done

distclean:
	for dir in $(SUBDIRS); do (make -C $$dir distclean;) ||exit 1; done

install: