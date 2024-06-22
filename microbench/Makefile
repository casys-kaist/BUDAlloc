 BUILDDIR=build
 SOURCEDIR=src/
 CFLAGS=-Wall -g
 LDFLAGS=-lm
 CSRCS=$(wildcard $(SOURCEDIR)*.c)
 CS :=$(CSRCS:src/%.c=$(BUILDDIR)/%)

 LIBS = lib/time_stat.c
 CC=gcc -Wno-unused-function

all: $(BUILDDIR) $(CS)
	@echo $(CS)

$(BUILDDIR):
	@mkdir $(BUILDDIR)

$(BUILDDIR)/% : $(SOURCEDIR)%.c
	$(CC) $(CFLAGS) $< $(LIBS) $(LDFLAGS) -o $@

memory/memory_perf.c:
	@echo $@

install:
	@export LD_PRELOAD=/lib/libkernel.so

clean:
	rm -rf $(BUILDDIR) hugepagefile

.PHONY: all clean 

