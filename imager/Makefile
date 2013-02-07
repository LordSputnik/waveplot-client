CC = g++
CFLAGS = -std=c++11 -c
ODIR = obj
_OBJS = audio.o main.o
OBJS = $(patsubst %,$(ODIR)/%,$(_OBJS))
LINK_OPTS = -L/usr/local/lib -lavformat -lavcodec -lavutil -lpng
OUT = WavePlotImager
INCLUDE= -I/usr/local/include

all : $(OBJS)
	$(CC) $(OBJS) -o $(OUT) $(LINK_OPTS)

$(ODIR)/%.o : %.cpp $(ODIR)
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(ODIR):
	mkdir $(ODIR)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(OUT)
