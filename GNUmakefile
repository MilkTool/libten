PROFILE ?= release
SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard src/*.h)
INCLUDE := $(wildcard src/inc/*.inc) $(wildcard src/inc/ops/*.inc)
CCFLAGS := -std=c99 -Wall -Wno-unused -Wno-multichar -D ten_LIBM
LINK    := 
CC      ?= gcc
LD      ?= ld
STRIP   ?= strip

ifdef TARGET
    COMPILER := $(TARGET)-$(CC)
    LINKER   := $(TARGET)-$(LD)
    STRIPPER := $(TARGET)-$(STRIP)
else
    COMPILER := $(CC)
    LINKER   := $(LD)
    STRIPPER := $(STRIP)
endif

CPATH := $(shell readlink -f $(shell which $(COMPILER)))
LPATH := $(shell readlink -f $(shell which $(LINKER)))
SPATH := $(shell readlink -f $(shell which $(STRIPPER)))

TRIM1 := sed -E 's/-[0-9]+(\.[0-9]+(\.[0-9]+)?)?$$//'
TRIM2 := sed -E 's/^.*-//'
CNAME := $(shell basename $(CPATH) | $(TRIM1) | $(TRIM2))
LNAME := $(shell basename $(LPATH) | $(TRIM1) | $(TRIM2))
SNAME := $(shell basename $(SPATH) | $(TRIM1) | $(TRIM2))

PREFIX ?= /usr/local/
LIBDIR ?= $(shell if [ -d $(PREFIX)/lib64 ]; then echo $(PREFIX)/lib64; else echo $(PREFIX)/lib; fi )
INCDIR ?= $(PREFIX)/include

# Compiler specific options.
ifeq ($(CNAME),gcc)
    CCFLAGS += -Wno-bool-compare
    LINK    += -l m
endif
ifeq ($(CNAME),clang)
    LINK    += -l m
endif

ifeq ($(OS),Windows_NT)
    EXE := .exe
    DLL := .dll
    OBJ := .o
    LIB := .a
else
    EXE := 
    DLL := .so
    OBJ := .o
    LIB := .a
endif

ifeq ($(PROFILE),debug)
    CCFLAGS += -g -O0 -D ten_DEBUG -D ten_NO_NAN_TAGS -D ten_NO_POINTER_TAGS
    POSTFIX := -debug
else
    ifeq ($(PROFILE),release)
        CCFLAGS += -O3 -D NDEBUG
        POSTFIX := 
    else
        $(error "Invalid build profile")
    endif
endif


.PHONY: build
build: libten$(POSTFIX)$(DLL) libten$(POSTFIX)$(LIB)

libten$(POSTFIX)$(DLL): $(HEADERS) $(INCLUDE) $(SOURCES)
	$(COMPILER) $(CCFLAGS) -shared -fpic $(SOURCES) $(LINK) -o libten$(POSTFIX)$(DLL)
    ifeq ($(PROFILE),release)
	    $(STRIPPER) -w -K "ten_*" libten$(POSTFIX)$(DLL)
    endif

libten$(POSTFIX)$(LIB): $(HEADERS) $(INCLUDE) $(SOURCES)
	$(COMPILER) $(CCFLAGS) -c $(SOURCES)
	$(LINKER) -r *.o -o libten.o
    ifeq ($(PROFILE),release)
	    $(STRIPPER) -w -K "ten_*" libten.o
    endif
	ar rcs libten$(POSTFIX)$(LIB) libten.o
	rm *.o

test/tester$(EXE): $(HEADERS) $(INCLUDE) $(SOURCES) test/tester.c
	$(COMPILER) $(CCFLAGS) -D ten_TEST -D TEST_PATH='"test/"' $(SOURCES) $(LINK) test/tester.c -o test/tester$(EXE)

.PHONY: install
install:
	mkdir -p $(LIBDIR)
	cp libten$(POSTFIX)$(DLL) libten$(POSTFIX)$(LIB) $(LIBDIR)
	mkdir -p $(INCDIR)
	cp src/ten.h $(INCDIR)


.PHONY: test
clean:
	- rm *$(DLL)
	- rm *$(LIB)
	- rm ten.h
	- rm *.o
	- rm tester
