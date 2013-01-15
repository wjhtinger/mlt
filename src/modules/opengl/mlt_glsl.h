/*
 * mlt_glsl.h
 * Copyright (C) 2011 Christophe Thommeret
 * Author: Christophe Thommeret <hftom@free.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MLT_GLSL
#define MLT_GLSL

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef WIN32
#define SYS_gettid (224)
#define syscall(X) (((X == SYS_gettid) && GetCurrentThreadId()) || 0)
#else
#include <sys/syscall.h>
#endif
#include <framework/mlt_profile.h>


#define N_SPLINES 2
#define CATMULLROM_SPLINE 	0
#define COS_SPLINE			1


#define MAXLISTCOUNT 1024
typedef struct glsl_list_s *glsl_list;
struct glsl_list_s
{
	void *items[MAXLISTCOUNT];
	int count;

	int ( *add )( glsl_list, void* );
	void* ( *at )( glsl_list, int );
	void* ( *take_at )( glsl_list, int );
	void* ( *take )( glsl_list, void* );
};


typedef struct glsl_env_s *glsl_env;

struct glsl_texture_s
{
	glsl_env parent;
	int used;
	GLuint texture;
	int width;
	int height;
	GLint internal_format;
};
typedef struct glsl_texture_s *glsl_texture;



struct glsl_fbo_s
{
	int used;
	int width;
	int height;
	GLuint fbo;
};
typedef struct glsl_fbo_s *glsl_fbo;



struct glsl_pbo_s
{
	int size;
	GLuint pbo;
};
typedef struct glsl_pbo_s *glsl_pbo;



struct glsl_shader_s
{
	int used;
	GLuint vs;
	GLuint fs;
	GLuint program;
	char name[64];
};
typedef struct glsl_shader_s *glsl_shader;

// TODO: rename this mlt_glsl
struct glsl_env_s
{
	glsl_list texture_list;
	glsl_list fbo_list;
	glsl_list shader_list;
	glsl_pbo pbo;
	
	glsl_fbo ( *get_fbo )( glsl_env, int, int );
	void ( *release_fbo )( glsl_fbo );
	glsl_texture ( *get_texture )( glsl_env, int, int, GLint );
	void ( *release_texture )( glsl_texture );
	void ( *texture_destructor )( void* );
	glsl_shader ( *get_shader )( glsl_env, const char*, const char** );
	glsl_pbo ( *get_pbo )( glsl_env, int size );

	glsl_texture bicubic_lut;
	
	int is_started;
	void *movitChain;
	void *movitInput;
	int movitFinalized;
	mlt_image_format image_format;
};

#ifdef __cplusplus
extern "C"
{
#endif

extern int mlt_glsl_supported();
extern glsl_env mlt_glsl_init( mlt_profile profile );
extern void mlt_glsl_start( glsl_env g );
extern glsl_env mlt_glsl_get( mlt_profile profile );

extern glsl_texture glsl_rescale_bilinear( glsl_env g, glsl_texture source_tex, int iwidth, int iheight, int owidth, int oheight );
extern glsl_texture glsl_rescale_bicubic( glsl_env g, glsl_texture source_tex, int iwidth, int iheight, int owidth, int oheight, int spline );
extern void glsl_set_ortho_view( int width, int height );
extern void glsl_draw_quad( float x1, float y1, float x2, float y2 );

/* These are implemented in filter_glsl_manager.cpp. */
extern int mlt_glsl_init_movit( glsl_env glsl, mlt_profile profile );
extern void mlt_glsl_render_fbo( glsl_env glsl, void *chain, GLuint fbo, int width, int height );
extern void mlt_glsl_close( glsl_env glsl );

#ifdef __cplusplus
}
#endif

#endif /*MLT_GLSL*/

