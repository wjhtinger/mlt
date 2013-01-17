CFLAGS += -I../.. -I../../eigen3

LDFLAGS += -L../../framework -lmlt -lm

include ../../../config.mak

TARGET = ../libmltopengl$(LIBSUF)

OBJS = factory.o mlt_glsl.o

CPPOBJS = filter_glsl_manager.o

# Movit Core
CPPOBJS += movit/util.o movit/effect.o movit/effect_chain.o movit/init.o

# Movit Inputs
CPPOBJS += movit/flat_input.o
CPPOBJS += movit/ycbcr_input.o

# Movit Effects
CPPOBJS += movit/lift_gamma_gain_effect.o
CPPOBJS += movit/white_balance_effect.o
CPPOBJS += movit/gamma_expansion_effect.o
CPPOBJS += movit/gamma_compression_effect.o
CPPOBJS += movit/colorspace_conversion_effect.o
CPPOBJS += movit/saturation_effect.o
CPPOBJS += movit/vignette_effect.o
CPPOBJS += movit/mirror_effect.o
CPPOBJS += movit/blur_effect.o
CPPOBJS += movit/diffusion_effect.o
CPPOBJS += movit/glow_effect.o
CPPOBJS += movit/unsharp_mask_effect.o
CPPOBJS += movit/mix_effect.o
CPPOBJS += movit/resize_effect.o
CPPOBJS += movit/resample_effect.o
CPPOBJS += movit/dither_effect.o
CPPOBJS += movit/deconvolution_sharpen_effect.o
CPPOBJS += movit/sandbox_effect.o
CPPOBJS += movit/overlay_effect.o
CPPOBJS += movit/alpha_division_effect.o
CPPOBJS += movit/alpha_multiplication_effect.o
CPPOBJS += movit/padding_effect.o

# Movit MLT services
CPPOBJS += filter_movit_blur.o
CPPOBJS += filter_movit_convert.o
CPPOBJS += filter_movit_mirror.o
CPPOBJS += filter_movit_resample.o
CPPOBJS += filter_movit_resize.o
CPPOBJS += mlt_movit_input.o

ifeq ($(targetos), MinGW)
	LDFLAGS += -lopengl32
else
ifeq ($(targetos), Darwin)
	CFLAGS += -FOpenGL
	LDFLAGS += -framework OpenGL
else
	OBJS += consumer_xgl.o
	LDFLAGS += -lpthread -lGL -lX11
endif
endif

CXXFLAGS += $(CFLAGS) -Wno-deprecated

LDFLAGS += -L../../mlt++ -lmlt++ -lGLEW

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
	install -m 644 movit/*.{frag,vert} "$(DESTDIR)$(datadir)/mlt/opengl/movit"

ifneq ($(wildcard .depend),)
include .depend
endif
