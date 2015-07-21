default: all

PHANTOM?=/usr/share/phantom

include $(PHANTOM)/phantom/module.mk

$(eval $(call MODULE,taurus_source))

include $(PHANTOM)/opts.mk

FIXINC:=$(FIXINC) -isystem $(PHANTOM)

all: $(targets)

clean:; @rm -f $(targets) $(tmps) deps/*.d

.PHONY: default all clean
