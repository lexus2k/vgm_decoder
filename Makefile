default: all

CPPFLAGS += -I./include

OBJS=src/ay-3-8910.o src/nes_apu.o main.o src/vgm_file.o

all: $(OBJS)
	$(CXX) -o vgm2pcm $(CCFLAGS) $(OBJS) $(LDFLAGS)

clean:
	@rm -rf $(OBJS) vgm2pcm
