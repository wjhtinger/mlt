CFLAGS += -I../..

LDFLAGS += -L../../framework -lmlt -lm

include ../../../config.mak

TARGET = ../libmltopengl$(LIBSUF)

OBJS = factory.o

CPPOBJS = fbo_input.o
CPPOBJS += filter_glsl_manager.o
CPPOBJS += filter_movit_blur.o
CPPOBJS += filter_movit_convert.o
CPPOBJS += filter_movit_crop.o
CPPOBJS += filter_deconvolution_sharpen.o
CPPOBJS += filter_movit_diffusion.o
CPPOBJS += filter_movit_glow.o
CPPOBJS += filter_lift_gamma_gain.o
CPPOBJS += filter_movit_mirror.o
CPPOBJS += filter_movit_opacity.o
CPPOBJS += filter_movit_rect.o
CPPOBJS += filter_movit_resample.o
CPPOBJS += filter_movit_resize.o
CPPOBJS += filter_movit_saturation.o
CPPOBJS += filter_movit_vignette.o
CPPOBJS += filter_white_balance.o
CPPOBJS += mlt_movit_input.o
CPPOBJS += transition_movit_mix.o
CPPOBJS += transition_movit_overlay.o

ifeq ($(targetos), MinGW)
	CFLAGS  += -DGLEW_STATIC
	LDFLAGS += -lglew32 -lopengl32
else
ifeq ($(targetos), Darwin)
	CFLAGS += -FOpenGL
	LDFLAGS += -framework OpenGL -lGLEW
else
	OBJS += consumer_xgl.o
	LDFLAGS += -lpthread -lGL -lX11 -lGLEW
endif
endif

CXXFLAGS += -Wno-deprecated $(CFLAGS)
CXXFLAGS += `pkg-config --cflags movit 2> /dev/null`

SHADERDIR = `pkg-config --variable=shaderdir movit`
CXXFLAGS += -DSHADERDIR=\"$(SHADERDIR)\"

LDFLAGS += -L../../mlt++ -lmlt++
LDFLAGS += `pkg-config --libs movit 2> /dev/null`

SRCS := $(OBJS:.o=.c) $(CPPOBJS:.o=.cpp)

all: $(TARGET)

$(TARGET): $(OBJS) $(CPPOBJS)
		$(CXX) $(SHFLAGS) -o $@ $(OBJS) $(CPPOBJS) $(LDFLAGS)

depend:	$(SRCS)
		$(CXX) -MM $(CXXFLAGS) $^ 1>.depend

distclean:	clean
		rm -f .depend config.h config.mak

clean:	
		rm -f $(OBJS) $(TARGET) $(CPPOBJS)

install: all
	install -m 755 $(TARGET) "$(DESTDIR)$(libdir)/mlt"
	install -d "$(DESTDIR)$(datadir)/mlt/opengl/movit"
	install -m 644 *.yml "$(DESTDIR)$(datadir)/mlt/opengl"
	install -m 644 movit/*.frag "$(DESTDIR)$(datadir)/mlt/opengl/movit"
	install -m 644 movit/*.vert "$(DESTDIR)$(datadir)/mlt/opengl/movit"

ifneq ($(wildcard .depend),)
include .depend
endif
