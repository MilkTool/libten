CCFLAGS := -std=c99 -Wall -D ten_LIBM
PROFILE := release
TYPE    := dynamic

ifeq ($(OS),Windows_NT)
    D := /D
    G := /g
    O := /O
else
    D := -D
    G := -g
    O := -O
endif

ifeq ($(PROFILE),debug)
    CCFLAGS += $(G) $(O)0 $(D) ten_DEBUG
else ifeq($PROFILE),release)
    CCFLAGS += $(O)3 $(D) NDEBUG
else ifeq($(PROFILE),test)
    CCFLAGS += $(G) $(O)0 $(D) ten_TEST $(D) ten_DEBUG
else
    $(error "Invalid build profile")
endif

ifeq ($(OS),Windows_NT)
    CC ?= cl
else
    CC ?= gcc
endif

ifeq ($(CC),cl)
    COMPILE   := $(CC) $(CCFLAGS) /c
    SHARED    := link /DLL /out:libten.dll *.obj
    STATIC    := lib /out:libten.lib *.obj
else
    ifeq ($(TYPE),dynamic)
        CCFLAGS += -fpic
    endif
    
    COMPILE   := $(CC) $(CCFLAGS) -c
    STRIP     := ld -o ten.o -r *.o &&  objcpy --strip-all --keep-symbols api.txt ten.o ten-s.o
    SHARED    := $(STRIP) && $(CC) --shared -o libten.so ten-s.o
    STATIC    := $(STRIP) && ar rcs libten.a ten-s.o
endif



