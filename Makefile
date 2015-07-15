default: all

include /usr/share/phantom/module.mk

$(eval $(call MODULE,taurus_source))

include /usr/share/phantom/opts.mk

all: $(targets)

clean:; @rm -f $(targets) $(tmps) deps/*.d

.PHONY: default all clean
