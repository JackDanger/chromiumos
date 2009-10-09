C = ply-image.c \
    ply-frame-buffer.c \
    ply-list.c \


H = ply-frame-buffer.h \
    ply-image.h \
    ply-list.h \
    ply-utils.h \


ply-image: $(C) $(H) Makefile
	cc $(C) $(LIBS) -Wall -O2 -I. -o ply-image -lpng
	strip ply-image

clean:
	rm -f ply-image *~
