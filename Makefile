.PHONY: all clean demo smoke makefiles
all: makefiles
	$(MAKE) -C src MODE=release

makefiles:
	cd src && opp_makemake -f -o vho

clean: makefiles
	$(MAKE) -C src MODE=release clean

demo: all
	./src/vho -u Qtenv -n src -c Demo

smoke: all
	./src/vho -u Cmdenv -n src -c Demo
