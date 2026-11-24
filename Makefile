.PHONY: all clean demo smoke
all: src/Makefile
	$(MAKE) -C src MODE=release

src/Makefile:
	cd src && opp_makemake -f -o vho

clean:
	$(MAKE) -C src MODE=release clean

demo: all
	./src/vho -u Qtenv -n src -c Demo

smoke: all
	./src/vho -u Cmdenv -n src -c Demo
