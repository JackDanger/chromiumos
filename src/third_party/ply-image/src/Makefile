C = ply-image.c \
    ply-frame-buffer.c \
    ply-list.c \


H = ply-frame-buffer.h \
    ply-image.h \
    ply-list.h \
    ply-utils.h \

CFLAGS ?= -O2

ply-image: $(C) $(H) Makefile
	$(CC) $(CFLAGS) $(C) $(LIBS) -Wall -I. -o ply-image -o ply-image -lpng
	strip ply-image

clean:
	rm -f ply-image *~
