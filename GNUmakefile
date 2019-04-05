PROFILE ?= release
SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard src/*.h)
INCLUDE := $(wildcard src/inc/*.inc) $(wildcard src/inc/ops/*.c)
CCFLAGS := -std=c99 -Wall -Wno-unused -Wno-multichar -D ten_LIBM
LINK    := -lm
CC      ?= gcc

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
    CCFLAGS += -g -O0 -D ten_DEBUG
    POSTFIX := -debug
else
    ifeq ($(PROFILE),release)
        CCFLAGS += -O3 -D NDEBUG
        POSTFIX := 
    else
        $(error "Invalid build profile")
    endif
endif

libten$(DLL): $(HEADERS) $(INCLUDE) $(SOURCES)
	$(CC) $(CCFLAGS) -shared -fpic $(SOURCES) $(LINK) -o libten$(POSTFIX)$(DLL)
	strip -w -K "ten_*" libten$(POSTFIX)$(DLL)

libten$(LIB): $(HEADERS) $(INCLUDE) $(SOURCES)
	$(CC) $(CCFLAGS) -c $(SOURCES) $(LINK)
	ld -r *.o -o libten.o
    ifneq ($(PROFILE),debug)
		strip -w -K "ten_*" libten.o
    endif
	ar rcs libten$(POSTFIX)$(LIB) libten.o
	rm *.o

ten.h: src/ten_api.h
	cp src/ten_api.h ten.h

.PHONY: test
test:
	$(CC) $(CCFLAGS) -D ten_TEST -D TEST_PATH='"test/"' $(SOURCES) $(LINK) test/test.c -o tester$(EXE)
	./tester$(EXE)
	rm tester$(EXE)


.PHONY: test
clean:
	- rm libten$(DLL)
	- rm libten$(LIB)
	- rm ten.h
	- rm *.o
