.DEFAULT_GOAL := all
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
include $(DEVKITARM)/3ds_rules

TARGET := WarThunder3DS
APP_TITLE := WarThunder3DS
APP_DESCRIPTION := Offline arcade tank and air combat
APP_AUTHOR := Homebrew
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := $(ARCH) -O2 -g -std=gnu11 -Wall -Wextra -Wshadow -ffunction-sections -fdata-sections -I$(CTRULIB)/include -Ibuild -D__3DS__
LDFLAGS := $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -Wl,-Map,build/$(TARGET).map
LIBS := -L$(CTRULIB)/lib -lcitro3d -lctru -lm

.PHONY: all clean
all: $(TARGET).3dsx

build:
	mkdir -p build

build/vshader.shbin: source/vshader.v.pica | build
	picasso -o $@ $<

build/vshader.s: build/vshader.shbin
	cd build && bin2s -a 4 -H vshader_shbin.h vshader.shbin > vshader.s && printf '\n' >> vshader.s

build/vshader.o: build/vshader.s
	$(CC) $(ARCH) -c $< -o $@

build/main.o: source/main.c build/vshader.s
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: build/main.o build/vshader.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(TARGET).smdh:
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" "$(CTRULIB)/default_icon.png" $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh

clean:
	rm -f build/*.o build/*.d build/*.s build/*.h build/*.shbin build/*.map $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh

-include build/main.d

