CC = gcc
CFLAGS = -Wall -Wextra -Iinclude `pkg-config --cflags libzip libxml-2.0 json-c libcurl` -g
LDFLAGS = `pkg-config --libs libzip libxml-2.0 json-c libcurl`

SRC_DIR = src
OBJ_DIR = build
BIN_DIR = .

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/epubtrans

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/*.o $(TARGET)

install:
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/epubtrans
	install -d $(DESTDIR)/usr/local/etc/ebook-translator
	install -m 644 conf/config.json $(DESTDIR)/usr/local/etc/ebook-translator/config.json

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/epubtrans
	rm -rf $(DESTDIR)/usr/local/etc/ebook-translator/

.PHONY: all clean install uninstall
