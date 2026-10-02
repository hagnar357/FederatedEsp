CC = gcc
CFLAGS = -Wall -Wextra -g
PROGRAM_NAME = FederatedLearningTeacherClient
TARGET = $(PROGRAM_NAME)
BUILDDIR = .build
LIBS = -lwebsockets -lm

ZEPHYR_DIR = ../FederatedLearningZephyr
SERVER_DIR = ../FederatedLearningServer

# the shims in include/ must come before the node headers (espconfiguration.h is replaced)
INCLUDES = -Iinclude -I$(ZEPHYR_DIR)/include -I$(SERVER_DIR)/lib

# node training code compiled unchanged + cJSON from the server
NODE_SOURCES = $(ZEPHYR_DIR)/src/federatedlearning.c $(ZEPHYR_DIR)/src/JSONConverter.c $(ZEPHYR_DIR)/src/knowledgedistillation.c
LOCAL_SOURCES = $(wildcard src/*.c) main.c
CJSON_SOURCE = $(SERVER_DIR)/src/cJSON.c

NODE_OBJECTS = $(patsubst $(ZEPHYR_DIR)/src/%.c,$(BUILDDIR)/node_%.o,$(NODE_SOURCES))
LOCAL_OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(LOCAL_SOURCES)))
CJSON_OBJECT = $(BUILDDIR)/cJSON.o

all: build

build: $(TARGET)

$(TARGET): $(NODE_OBJECTS) $(LOCAL_OBJECTS) $(CJSON_OBJECT)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(BUILDDIR)/node_%.o: $(ZEPHYR_DIR)/src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILDDIR)/main.o: main.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CJSON_OBJECT): $(CJSON_SOURCE) | $(BUILDDIR)
	$(CC) -w $(INCLUDES) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -f $(BUILDDIR)/*.o $(TARGET)

.PHONY: all build clean
