# MIT License
#
# Copyright (c) 2020 Aleksei Dynda
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


default: all

AUDIO_PLAYER ?= n
CPPFLAGS += -I./include -I./src

OBJS=src/chips/ay-3-8910.o \
     src/chips/nes_apu.o \
     src/chips/nes_cpu.o \
     src/chips/nsf_cartridge.o \
     main.o \
     src/formats/vgm_decoder.o \
     src/formats/nsf_decoder.o \
     src/vgm_file.o \

ifneq ($(AUDIO_PLAYER),n)
    LDFLAGS = -lSDL2
    CPPFLAGS += -DAUDIO_PLAYER=1
endif

all: $(OBJS)
	$(CXX) -o vgm2wav $(CCFLAGS) $(OBJS) $(LDFLAGS)

clean:
	@rm -rf $(OBJS) vgm2wav
