override CFLAGS += -std=c18 -Wall
override LDFLAGS +=

BUILDDIR := build
TARGET := rpi0w-led

SOURCES := main.c
OBJECTS := $(addprefix $(BUILDDIR)/, $(SOURCES:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	@echo "Linking $@..."
	@$(CC) $^ -o $(TARGET) $(LDFLAGS)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILDDIR) $(TARGET)

.PHONY: clean all
