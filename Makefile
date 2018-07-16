
ROOT_DIR = .

include $(ROOT_DIR)/compile_config

CC := $(TOOL_PREFIX)gcc

LIBS_DIR := $(ROOT_DIR)/libs

INCLUDES := 
INCLUDES += -I$(ROOT_DIR)/include

LIBS := 
#链接静态库

#链接动态库
LIBS += -lpthread
LIBS += -ldl
LIBS += -lm

SRCS := 
SRCS += src/main.c
SRCS += src/base_fun.c
SRCS += src/config_plist_opt.c

OBJS = $(SRCS:%.c=build/%.o)

CFLAGS += -pipe -Wall -O2 $(INCLUDES)

TARGET = build/AutoMacSerial

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CC) -L$(LIBS_DIR) -o $@ $^ $(LIBS)

$(OBJS):build/%.o:%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf build
