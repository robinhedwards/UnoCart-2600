SOURCE = bios.asm
BINARY = supercharger_bios.bin
HEADER = supercharger_bios.h
GARBAGE = $(BINAEY) $(HEADER)

DASM = dasm
DASM_OPTS = -f3

XXD = xxd

all: header

bin: $(BINARY)

header: $(HEADER)

$(HEADER): $(BINARY)
	$(XXD) -i $(BINARY) > $(HEADER)

$(BINARY): $(SOURCE)
	$(DASM) $< -o$@ $(DASM_OPTS)

clean:
	-rm -fr $(GARBAGE)

.PHONY: all clean
