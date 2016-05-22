#
# Makefile
#
# See the README file for copyright information and how to reach the author.
#
#

EPGD_SRC ?= ../..

include $(EPGD_SRC)/Make.config

PLUGIN = tvsp

SOFILE = libepgd-tvsp.so
OBJS = tvsp.o

CFLAGS += -I$(EPGD_SRC) -Wno-long-long

all: $(SOFILE)

$(SOFILE): $(OBJS)
	$(CC) $(CFLAGS) -shared $(OBJS) $(LIBS) -o $@

install:  $(SOFILE) install-config
	install -D $(SOFILE) $(PLGDEST)/

clean:
	@-rm -f $(OBJS) core* *~ *.so
	rm -f ./configs/*~
	rm -f tvsp-*.tgz

install-config:
	if ! test -d $(CONFDEST); then \
	   mkdir -p $(CONFDEST); \
	   chmod a+rx $(CONFDEST); \
	fi
	for i in `ls ./configs/tvsp*.xsl`; do\
	   if ! test -f "$(CONFDEST)/$$i"; then\
	      install --mode=644 -D "$$i" $(CONFDEST)/; \
	   fi;\
	done;
	for i in `ls ./configs/tvsp*.xml`; do\
	   if ! test -f "$(CONFDEST)/$$i"; then\
	      install --mode=644 -D "$$i" $(CONFDEST)/; \
	   fi;\
	done;

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(ARCHIVE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(ARCHIVE).tgz

#***************************************************************************
# dependencies
#***************************************************************************

tvsp.o : tvsp.c  tvsp.h
