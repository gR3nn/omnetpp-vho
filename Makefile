.PHONY: all clean demo smoke makefiles
all: makefiles
	$(MAKE) -C src MODE=release

makefiles:
	cd src && opp_makemake -f -o vho

clean: makefiles
	$(MAKE) -C src MODE=release clean

demo: all
	./src/vho -u Qtenv -n src -c ModularDemo

smoke: all
	./src/vho -u Cmdenv -n src -c Demo

.PHONY: legacy-demo modular-check
legacy-demo: all
	./src/vho -u Qtenv -n src -c Demo

modular-check: all
	./scripts/check-modular.sh
