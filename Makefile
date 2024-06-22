ALL_MOD = libkernel unit_test microbench install
ALL_MOD_CLEAN = $(addsuffix .clean, $(ALL_MOD))

all: microbench

microbench: libkernel
	make -C $(basename $@)

detection: libkernel install
	cp /lib/libkernel.so /lib/libkernel_d.so 

prevention: libkernel install
	cp /lib/libkernel.so /lib/libkernel_p.so

libkernel:
	@cd libkernel; ./apply_patch.sh; cmake .;
	make -C $(basename $@)

unit_test:
	@cd unit_test; cmake .;
	make -C $(basename $@) -j
	@cd unit_test; sudo ctest

install:
	sudo make -C libkernel install_lib

$(ALL_MOD_CLEAN):
	@if [ -e $(basename $@)/Makefile ] ; then \
		make -C $(basename $@) clean ;\
	fi

clean: $(ALL_MOD_CLEAN)
	@echo "cleaning complete" 

.PHONY: $(ALL_MOD_CLEAN) $(ALL_MOD) default all clean
