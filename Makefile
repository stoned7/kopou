all:
	cd src && $(MAKE) $@
debug:
	cd src && $(MAKE) $@

.PHONY: clean install

install:
	cd src && $(MAKE) $@
clean:
	rm -rf *.o kopou
	rm -rf *.log
	rm -rf *.kpu
