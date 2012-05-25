CFLAGS:=-I/opt/vc/include/ -O2
LDFLAGS:=-L/opt/vc/lib/ -lEGL -lGLESv2 -lbcm_host -lX11

all: eglonx

eglonx: eglonx.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f eglonx

.PHONY: all clean

