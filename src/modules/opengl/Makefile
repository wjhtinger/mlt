CFLAGS += -I../..

LDFLAGS += -L../../framework -lmlt -lpthread -lm -lGL

include ../../../config.mak
include config.mak

TARGET = ../libmltopengl$(LIBSUF)

OBJS = factory.o \
	mlt_glsl.o \
	consumer_qgl.o \
	filter_glsl_greyscale.o \
	filter_glsl_csc.o \
	filter_glsl_crop.o \
	filter_glsl_saturation.o \
	filter_glsl_resize.o \
	filter_glsl_fieldorder.o \
	filter_glsl_rescale.o \
	filter_glsl_deinterlace.o \
	filter_glsl_brightness.o \
	filter_glsl_contrast.o \
	filter_glsl_gamma.o \
	transition_glsl_luma.o

CPPOBJS = qgl_wrapper.o

qgl_wrapper.o: qgl_wrapper.moc
qgl_wrapper.moc: qgl_wrapper.cpp
	moc-qt4 $(DEFINES) $(INCPATH) -i $< -o $@

CXXFLAGS += $(CFLAGS) $(QTCXXFLAGS) -Wno-deprecated

LDFLAGS += $(QTLIBS)

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
		rm -f mqgl_wrapper.cpp

install: all
	install -m 755 $(TARGET) "$(DESTDIR)$(libdir)/mlt"
	install -d "$(DESTDIR)$(datadir)/mlt/opengl"
	install -m 644 *.yml "$(DESTDIR)$(datadir)/mlt/opengl"

ifneq ($(wildcard .depend),)
include .depend
endif
