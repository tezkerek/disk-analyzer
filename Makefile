SHELL=/bin/bash
CC = gcc
CFLAGS = -I $(SRC_DIR)

SRC_DIR = src
BUILD_DIR = build

COMMON_DEPS = $(wildcard $(SRC_DIR)/common/*.h)
CLIENT_DEPS = $(wildcard $(SRC_DIR)/client/*.h) $(COMMON_DEPS)
DAEMON_DEPS = $(wildcard $(SRC_DIR)/daemon/*.h) $(COMMON_DEPS)

COMMON_SRCS = $(wildcard $(SRC_DIR)/common/*.c)
CLIENT_SRCS = $(wildcard $(SRC_DIR)/client/*.c)
DAEMON_SRCS = $(wildcard $(SRC_DIR)/daemon/*.c)

COMMON_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

_CLIENT_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
CLIENT_OBJS = $(_CLIENT_OBJS) $(COMMON_OBJS)

_DAEMON_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(DAEMON_SRCS))
DAEMON_OBJS = $(_DAEMON_OBJS)  $(COMMON_OBJS)

ALL_OBJS = $(COMMON_OBJS) $(_CLIENT_OBJS) $(_DAEMON_OBJS)

.PHONY: pre-build all daemon client

all: pre-build daemon client

pre-build:
	mkdir -p $(BUILD_DIR)/{common,client,daemon}

$(ALL_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

daemon: $(DAEMON_OBJS) $(DAEMON_DEPS) pre-build
	$(CC) -pthread -o $(BUILD_DIR)/dad $(DAEMON_OBJS) $(CFLAGS)

client: $(CLIENT_OBJS) $(CLIENT_DEPS) pre-build
	$(CC) -o $(BUILD_DIR)/da $(CLIENT_OBJS) $(CFLAGS)
