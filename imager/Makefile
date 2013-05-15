CC = g++
CFLAGS = -std=c++11 -c -O2
ODIR = obj
_OBJS = audio.o main.o error.o
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))
LINK_OPTS = -L/usr/local/lib -lavformat -lavcodec -lavutil -lpng
OUT = WavePlotImager
INCLUDE= -I/usr/local/include

all : $(OBJS)
	$(CC) $(OBJS) -o $(OUT) $(LINK_OPTS)

$(ODIR)/%.o : %.cpp $(ODIR)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $<

$(ODIR):
	mkdir $(ODIR)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(OUT)
