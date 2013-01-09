#ifndef _OPENGL_H
#define _OPENGL_H 1

// A common place to find OpenGL includes, if your system should have them in weird places.

#define GL_GLEXT_PROTOTYPES 1

#if defined(__DARWIN__)
#   include <OpenGL/gl.h>
#   include <OpenGL/glext.h>
#elif defined(WIN32)
#   include "GLee/GLee.h"
#else
#   include <GL/gl.h>
#   include <GL/glext.h>
#endif

#endif  // !defined(_OPENGL_H)
