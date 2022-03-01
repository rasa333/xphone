objs1  = funcs.o broker.o
objs2  = funcs.o xphone.o
objs3  = funcs.o fritz.o
CC     = gcc
LIBS   = -lX11 -lforms -lm -ldl -lnotify -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0
CFLAGS = -O2 -Wunused -Wno-deprecated-declarations -pthread -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libpng16 -I/usr/include/x86_64-linux-gnu -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
server = xphone_broker
client = xphone
fritz  = xphone_fritz
%.o : %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $<

all	: $(server) $(client) $(fritz)


$(server)  : $(objs1)
	        $(CC) -s $(CFLAGS) -o $(server) $(objs1) -lm

$(client)  : $(objs2)
	        $(CC) -s $(CFLAGS) -o $(client) $(objs2) $(LIBS)

$(fritz)  : $(objs3)
	        $(CC) -s $(CFLAGS) -o $(fritz) $(objs3) -llinphone -lortp -lm

$(objects) : $(wildcard *.h)

clean	:
		rm -f *.o xphone_broker xphone xphone_fritz
