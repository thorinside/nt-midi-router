PLUGIN_NAME = nt_midi_router
SOURCES = nt_midi_router.cpp

DNT_API ?= ./distingNT_API
UNAME_S := $(shell uname -s)
TARGET ?= hardware

ifeq ($(TARGET),hardware)
    CXX = arm-none-eabi-g++
    CFLAGS = -std=c++11 \
             -mcpu=cortex-m7 \
             -mfpu=fpv5-d16 \
             -mfloat-abi=hard \
             -mthumb \
             -Os \
             -fPIC \
             -ffunction-sections \
             -fdata-sections \
             -fno-rtti \
             -fno-exceptions \
             -fno-unwind-tables \
             -fno-asynchronous-unwind-tables \
             -Wall
    INCLUDES = -I. -I$(DNT_API)/include
    LDFLAGS = -Wl,--relocatable -nostdlib
    OUTPUT_DIR = plugins
    BUILD_DIR = build
    OUTPUT = $(OUTPUT_DIR)/$(PLUGIN_NAME).o
    OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
    CHECK_CMD = arm-none-eabi-nm $(OUTPUT) | grep ' U ' || true
    SIZE_CMD = arm-none-eabi-size -A $(OUTPUT)

else ifeq ($(TARGET),test)
    ifeq ($(UNAME_S),Darwin)
        CXX = clang++
        CFLAGS = -std=c++11 -fPIC -Os -Wall -Wno-c99-designator -Wno-reorder-init-list -fno-rtti -fno-exceptions
        LDFLAGS = -dynamiclib -undefined dynamic_lookup
        EXT = dylib
    endif

    ifeq ($(UNAME_S),Linux)
        CXX = g++
        CFLAGS = -std=c++11 -fPIC -Os -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -shared
        EXT = so
    endif

    ifeq ($(OS),Windows_NT)
        CXX = g++
        CFLAGS = -std=c++11 -fPIC -Os -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -shared
        EXT = dll
    endif

    INCLUDES = -I. -I$(DNT_API)/include
    OUTPUT_DIR = plugins
    OUTPUT = $(OUTPUT_DIR)/$(PLUGIN_NAME).$(EXT)
    CHECK_CMD = nm $(OUTPUT) | grep ' U ' || true
    SIZE_CMD = ls -lh $(OUTPUT)
endif

all: $(OUTPUT)

ifeq ($(TARGET),hardware)
$(OUTPUT): $(OBJECTS)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"
	@$(SIZE_CMD)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

else ifeq ($(TARGET),test)
$(OUTPUT): $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $(SOURCES)
	@echo "Built: $@"
	@$(SIZE_CMD)
endif

hardware:
	@$(MAKE) TARGET=hardware

test:
	@$(MAKE) TARGET=test

unit:
	@mkdir -p build
	clang++ -std=c++11 -Wall -Wextra -Wno-c99-designator -Wno-reorder-init-list \
		-I. -I$(DNT_API)/include -o build/nt_midi_router_test tests/nt_midi_router_test.cpp
	@build/nt_midi_router_test

both: hardware test

verify:
	@$(MAKE) unit
	@$(MAKE) TARGET=hardware
	@$(MAKE) TARGET=test
	@$(MAKE) check TARGET=hardware

check: $(OUTPUT)
	@$(CHECK_CMD)

size: $(OUTPUT)
	@$(SIZE_CMD)

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)

.PHONY: all hardware test unit both verify check size clean
