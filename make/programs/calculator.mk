include Make.defaults

TARGET := $(BINDIR)/calculator.elf

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
