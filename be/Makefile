TARGETS = \
/golf/local/dmd2/bin/dmd \
/golf/local/dmd/linux/bin/dmd \
/usr/local/bin/goruby-1.9 \
/golf/local/j601 \
/usr/local/bin/xgawk \
/usr/local/bin/cyc \
/usr/local/bin/kite \
/golf/local/arc3/as2.scm \
/golf/local/gobin/8l \
/golf/local/fernando \
/golf/local/FlogScript.php \
/golf/local/wake/wake \

#/golf/local/dmd2/linux/bin/dmd \

all: $(TARGETS)
	make -C modules
	git archive HEAD srv | tar -C / -xvf -
	cd /golf && make
	chmod +s /golf/remount

#/golf/local/dmd2/linux/bin/dmd:
/golf/local/dmd2/bin/dmd:
	fetcher/d2

/golf/local/dmd/linux/bin/dmd:
	fetcher/d

/usr/local/bin/goruby-1.9:
	fetcher/grb

/golf/local/j601:
	fetcher/j

/usr/local/bin/xgawk:
	fetcher/xgawk

/usr/local/bin/cyc:
	fetcher/curry

/usr/local/bin/kite:
	fetcher/kt

/golf/local/arc3/as2.scm:
	fetcher/arc

/golf/local/gobin/8l:
	fetcher/go

/golf/local/fernando: fetcher/fernando.hs
	ghc --make -o $@ $<

/golf/local/FlogScript.php: fetcher/FlogScript.php
	cp $< $@
	chmod 755 $@

/golf/local/wake/wake:
	fetcher/wake
