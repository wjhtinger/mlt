/*
 * filter_glsl_csc.c
 * Copyright (C) 2012 Christophe Thommeret
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

#include "mlt_glsl.h"

#include <framework/mlt_filter.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_log.h>
#include <framework/mlt_profile.h>
#include <framework/mlt_factory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static const char *yuv420p_to_glsl_frag=
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect texY, texU, texV;\n"
"uniform vec4 r_coefs, g_coefs, b_coefs;\n"
"void main(void) {\n"
"    vec3 rgb;\n"
"    vec4 yuv;\n"
"    vec2 coord = gl_TexCoord[0].xy;\n"
"    yuv.r = texture2DRect(texY, vec2(coord.x, coord.y)).r;\n"
"    yuv.g = texture2DRect(texU, vec2(coord.x/2.0, coord.y/2.0)).r;\n"
"    yuv.b = texture2DRect(texV, vec2(coord.x/2.0, coord.y/2.0)).r;\n"
"    yuv.a = 1.0;\n"
"    rgb.r = dot( yuv, r_coefs );\n"
"    rgb.g = dot( yuv, g_coefs );\n"
"    rgb.b = dot( yuv, b_coefs );\n"
"    gl_FragColor = vec4(rgb, 1.0);\n"
"}\n";



static const char *yuv422_to_glsl_frag=
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect texYUV;\n"
"uniform vec4 r_coefs, g_coefs, b_coefs;\n"
"void main(void) {\n"
"    vec3 rgb;\n"
"    vec4 yuv;\n"
"    vec4 coord = gl_TexCoord[0].xyxx;\n"
"    coord.z -= step(1.0, mod(coord.x, 2.0));\n"
"    coord.w = coord.z + 1.0;\n"
"    yuv.r = texture2DRect(texYUV, coord.xy).r;\n"
"    yuv.g = texture2DRect(texYUV, coord.zy).a;\n"
"    yuv.b = texture2DRect(texYUV, coord.wy).a;\n"
"    yuv.a = 1.0;\n"
"    rgb.r = dot( yuv, r_coefs );\n"
"    rgb.g = dot( yuv, g_coefs );\n"
"    rgb.b = dot( yuv, b_coefs );\n"
"    gl_FragColor = vec4(rgb, 1.0);\n"
"}\n";



static const char *glsl_to_yuv422_frag=
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect texRGB;\n"
"uniform vec4 y_coefs, u_coefs, v_coefs;\n"
"void main(void) {\n"
"    vec4 rgb1, rgb2;\n"
"    float y, u, v, y2, u2, v2;\n"
"    vec3 coord = gl_TexCoord[0].xyx;\n"
"    coord.z += 1.0;\n"
"    rgb1.rgb = texture2DRect(texRGB, coord.xy).rgb;\n"
"    rgb2.rgb = texture2DRect(texRGB, coord.zy).rgb;\n"
"    rgb1.a = rgb2.a = 1.0;\n"
"    y = dot( rgb1, y_coefs );\n"
"    u = dot( rgb1, u_coefs );\n"
"    v = dot( rgb1, v_coefs );\n"
"    y2 = dot( rgb2, y_coefs );\n"
"    u2 = dot( rgb2, u_coefs );\n"
"    v2 = dot( rgb2, v_coefs );\n"
"    gl_FragColor = vec4( y, (u + u2) / 2.0, y2, (v + v2) / 2.0 );\n"
"}\n";



float from_yuv_601[] = {
	1.16438,  0.00000,  1.59603, -0.87420,
	1.16438, -0.39176, -0.81297,  0.53167,
	1.16438,  2.01723,  0.00000, -1.08563
};

float from_yuv_709[] = {
	1.16438,  0.00000,  1.79274, -0.97295,
	1.16438, -0.21325, -0.53291,  0.30148,
	1.16438,  2.11240,  0.00000, -1.13340
};

float from_yuv_240[] = {
	1.16438,  0.00000,  1.79411, -0.97363,
	1.16438, -0.25798, -0.54258,  0.32879,
	1.16438,  2.07871,  0.00000, -1.11649
};

static void load_yuv_to_glsl( GLuint prog, float *cf )
{
	glUniform4f( glGetUniformLocationARB( prog, "r_coefs" ), cf[0], cf[1], cf[2], cf[3] );
	glUniform4f( glGetUniformLocationARB( prog, "g_coefs" ), cf[4], cf[5], cf[6], cf[7] );
	glUniform4f( glGetUniformLocationARB( prog, "b_coefs" ), cf[8], cf[9], cf[10], cf[11] );
}

float to_yuv_601[] = {
	 0.25679,  0.50413,  0.09791, 0.06275,
	-0.14822, -0.29099,  0.43922, 0.50196,
	 0.43922, -0.36779, -0.07143, 0.50196
};

float to_yuv_709[] = {
	 0.18259,  0.61423,  0.06201, 0.06275,
	-0.10064, -0.33857,  0.43922, 0.50196,
	 0.43922, -0.39894, -0.04027, 0.50196
};

float to_yuv_240[] = {
	 0.18207,  0.60204,  0.07472, 0.06275,
	-0.10199, -0.33723,  0.43922, 0.50196,
	 0.43922, -0.39072, -0.04849, 0.50196
};

static void load_glsl_to_yuv( GLuint prog, float *cf )
{
	glUniform4f( glGetUniformLocationARB( prog, "y_coefs" ), cf[0], cf[1], cf[2], cf[3] );
	glUniform4f( glGetUniformLocationARB( prog, "u_coefs" ), cf[4], cf[5], cf[6], cf[7] );
	glUniform4f( glGetUniformLocationARB( prog, "v_coefs" ), cf[8], cf[9], cf[10], cf[11] );
}



static glsl_texture rgb_to_glsl( glsl_env g, uint8_t *image, int width, int height, int rgba )
{
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------%s to glsl\n", ((rgba) ? "rgba" : "rgb"));

	glsl_pbo pbo = g->get_pbo( g, width * height * ((rgba) ? 4 : 3) );
	if ( !pbo )
		return NULL;
	
	glsl_texture dest = g->get_texture( g, width, height, ((rgba) ? GL_RGBA : GL_RGB) );
	if ( !dest )
		return NULL;

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, dest->texture );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, pbo->pbo );
	void *mem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
	memcpy( mem, image, width * height * ((rgba) ? 4 : 3) );
	glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );
	glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width, height, ((rgba) ? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, 0 );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 );

	return dest;
}



static glsl_texture yuv420p_to_glsl( glsl_env g, uint8_t *image, int width, int height, int colorspace )
{
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------yuv420p to glsl\n");

	glsl_pbo pbo = g->get_pbo( g, width * height * 3 / 2 );
	if ( !pbo )
		return NULL;
	
	glsl_fbo fbo = g->get_fbo( g, width, height );
	if ( !fbo )
		return NULL;

	glsl_texture dest = g->get_texture( g, width, height, GL_RGBA );
	if ( !dest ) {
		g->release_fbo( fbo );
		return NULL;
	}

	glsl_shader shader = g->get_shader( g, "yuv420p_to_glsl_frag", &yuv420p_to_glsl_frag );
	if ( !shader ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		return NULL;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

	glsl_texture y = g->get_texture( g, width, height, GL_LUMINANCE );
	if ( !y ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		return NULL;
	}
	glsl_texture u = g->get_texture( g, width, height, GL_LUMINANCE );
	if ( !u ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		g->release_texture( y );
		return NULL;
	}
	glsl_texture v = g->get_texture( g, width, height, GL_LUMINANCE );
	if ( !v ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		g->release_texture( y );
		g->release_texture( u );
		return NULL;
	}

#define BUFFER_OFFSET(i) ((char *)NULL + (i))
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, y->texture );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, pbo->pbo );
	void *mem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
	memcpy( mem, image, width * height * 3 / 2 );
	glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );
	glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, u->texture );
	glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, BUFFER_OFFSET(width * height) );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, v->texture );
	glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, BUFFER_OFFSET((width * height) + (width * height / 4)) );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

	glsl_set_ortho_view( width, height );

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, y->texture );
	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, u->texture );
	glActiveTexture( GL_TEXTURE2 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, v->texture );

	glUseProgram( shader->program );
	glUniform1i( glGetUniformLocationARB( shader->program, "texY" ), 0 );
	glUniform1i( glGetUniformLocationARB( shader->program, "texU" ), 1 );
	glUniform1i( glGetUniformLocationARB( shader->program, "texV" ), 2 );
	switch ( colorspace ) {
		case 240:
			load_yuv_to_glsl( shader->program, from_yuv_240 );
			break;
		case 709:
			load_yuv_to_glsl( shader->program, from_yuv_709 );
			break;
		default:
			load_yuv_to_glsl( shader->program, from_yuv_601 );
			break;
	}

	glsl_draw_quad( 0, 0, width, height );

	glUseProgram( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g->release_texture( y );
	g->release_texture( u );
	g->release_texture( v );
	g->release_fbo( fbo );

	return dest;
}



static glsl_texture yuv422_to_glsl( glsl_env g, uint8_t *image, int width, int height, int colorspace )
{
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------yuv422 to glsl\n");

	glsl_pbo pbo = g->get_pbo( g, width * height * 2 );
	if ( !pbo )
		return NULL;

	glsl_texture dest = g->get_texture( g, width, height, GL_RGBA );
	if ( !dest )
		return NULL;


	glsl_fbo fbo = g->get_fbo( g, width, height );
	if ( !fbo ) {
		g->release_texture( dest );
		return NULL;
	}

	glsl_shader shader = g->get_shader( g, "yuv422_to_glsl_frag", &yuv422_to_glsl_frag );
	if ( !shader ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		return NULL;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

	glsl_texture yuv = g->get_texture( g, width, height, GL_LUMINANCE_ALPHA );
	if ( !yuv ) {
		g->release_texture( dest );
		g->release_fbo( fbo );
		return NULL;
	}
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, yuv->texture );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, pbo->pbo );
	void *mem = glMapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY );
	memcpy( mem, image, width * height * 2 );
	glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );
	glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width, height, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 0 );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, 0 );

	glsl_set_ortho_view( width, height );

	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, yuv->texture );
	glUseProgram( shader->program );
	glUniform1i( glGetUniformLocationARB( shader->program, "texYUV" ), 0 );
	switch ( colorspace ) {
		case 240:
			load_yuv_to_glsl( shader->program, from_yuv_240 );
			break;
		case 709:
			load_yuv_to_glsl( shader->program, from_yuv_709 );
			break;
		default:
			load_yuv_to_glsl( shader->program, from_yuv_601 );
			break;
	}

	glsl_draw_quad( 0, 0, width, height );

	glUseProgram( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g->release_texture( yuv );
	g->release_fbo( fbo );

	return dest;
}



static uint8_t* glsl_to_yuv422( glsl_env g, glsl_texture source_tex, int *size, int width, int height, int colorspace )
{
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------glsl to yuv422, source_tex : %u\n",source_tex->texture );

	glsl_texture dest = g->get_texture( g, width/2, height, GL_RGBA );
	if ( !dest )
		return NULL;

	glsl_fbo fbo = g->get_fbo( g, width/2, height );
	if ( !fbo ) {
		g->release_texture( dest );
		return NULL;
	}

	glsl_shader shader = g->get_shader( g, "glsl_to_yuv422_frag", &glsl_to_yuv422_frag );
	if ( !shader ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		return NULL;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

	glsl_set_ortho_view( width/2, height );

	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );
	glUseProgram( shader->program );
	glUniform1i( glGetUniformLocationARB( shader->program, "texRGB" ), 0 );
	switch ( colorspace ) {
		case 240:
			load_glsl_to_yuv( shader->program, to_yuv_240 );
			break;
		case 709:
			load_glsl_to_yuv( shader->program, to_yuv_709 );
			break;
		default:
			load_glsl_to_yuv( shader->program, to_yuv_601 );
			break;
	}

	glBegin( GL_QUADS );
		glTexCoord2f( 0.0, 0.0 );			glVertex3f( 0.0, 0.0, 0.);
		glTexCoord2f( 0.0, height );		glVertex3f( 0.0, height, 0.);
		glTexCoord2f( width, height );		glVertex3f( width/2.0, height, 0.);
		glTexCoord2f( width, 0.0 );			glVertex3f( width/2.0, 0.0, 0.);
	glEnd();

	glUseProgram( 0 );

	*size = width * height * 2;
	uint8_t *image = mlt_pool_alloc( *size );

	glReadPixels( 0, 0, width/2, height, GL_RGBA, GL_UNSIGNED_BYTE, image );
	
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g->release_fbo( fbo );
	g->release_texture( dest );

	return image;
}



static uint8_t* glsl_to_yuv420p( glsl_env g, glsl_texture source_tex, int *size, int width, int height, int colorspace )
{
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------glsl to yuv420p, source_tex : %u\n",source_tex->texture );

	uint8_t *yuv422 = glsl_to_yuv422( g, source_tex, size, width, height, colorspace );
	if ( !yuv422 )
		return NULL;

	*size = width * height * 3 / 2;
	uint8_t *image = mlt_pool_alloc( *size );

	uint8_t *start = yuv422;
	uint8_t *y_plane = image;
	uint8_t *u_plane = y_plane + (width * height);
	uint8_t *v_plane = u_plane + (width * height / 4);
	int yuv422_stride = width * 2;
	int y_stride = width;

	int w, h = height / 2;
	while ( h-- ) {
		w = width / 2;
		while ( w-- ) {
			y_plane[0] = start[0];
			y_plane[1] = start[2];
			y_plane[y_stride] = start[yuv422_stride];
			y_plane[y_stride + 1] = start[yuv422_stride + 2];
			*u_plane++ = (start[1] + start[yuv422_stride + 1]) / 2;
			*v_plane++ = (start[3] + start[yuv422_stride + 3]) / 2;
			start += 4;
			y_plane += 2;
		}
		start += yuv422_stride;
		y_plane += y_stride;
	}

	mlt_pool_release( yuv422 );

	return image;
}



static uint8_t* glsl_to_rgb( glsl_env g, glsl_texture source_tex, int *size, int width, int height, int rgba )
{
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------glsl to %s, source_tex : %u\n", ((rgba) ? "rgba" : "rgb"), source_tex->texture );

	glsl_fbo fbo = g->get_fbo( g, width, height );
	if ( !fbo )
		return NULL;

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, source_tex->texture, 0 );

	*size = width * height * ((rgba) ? 4 : 3);
	uint8_t *image = mlt_pool_alloc( *size );

	glReadPixels( 0, 0, width, height, ((rgba) ? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, image );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g->release_fbo( fbo );

	return image;
}



/** Do it :-).
*/

static int convert_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, mlt_image_format output_format )
{
	mlt_properties properties = MLT_FRAME_PROPERTIES( frame );
	int width = mlt_properties_get_int( properties, "width" );
	int height = mlt_properties_get_int( properties, "height" );
	int colorspace = mlt_properties_get_int( properties, "colorspace" );
	
	fprintf(stderr,"filter_glsl_csc convert_image -----------------------in:%s out:%s\n", mlt_image_format_name( *format ), mlt_image_format_name( output_format ) );

	if ( *format == output_format )
		return 1;

	glsl_env g = (glsl_env)mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 );
	if ( !g || (g && !g->user_data) )
		return 1;

	glsl_texture texture = 0;
	int release = 1;

	if ( *format != mlt_image_glsl ) {
		/* lock gl */
		g->context_lock( g );
	
		if ( *format == mlt_image_rgb24a || *format == mlt_image_opengl ) { 
			texture = rgb_to_glsl( g, *image, width, height, 1 );
		}
		else if ( *format == mlt_image_rgb24 ) {
			texture = rgb_to_glsl( g, *image, width, height, 0 );
		}
		else if ( *format == mlt_image_yuv420p ) {
			texture = yuv420p_to_glsl( g, *image, width, height, colorspace );
		}
		else if ( *format == mlt_image_yuv422 ) {
			texture = yuv422_to_glsl( g, *image, width, height, colorspace );
		}

		/* unlock gl */
		g->context_unlock( g );

		if ( !texture )
			return 1;
	}
	else {
		release = 0;
		texture = (glsl_texture)mlt_properties_get_data( properties, "image", NULL );
	}
	
	if ( output_format != mlt_image_glsl ) {
		/* lock gl */
		g->context_lock( g );

		uint8_t *dest = NULL;
		int size = 0;

		if ( output_format == mlt_image_yuv422 ) {
			dest = glsl_to_yuv422( g, texture, &size, width, height, colorspace );
		}
		else if ( output_format == mlt_image_yuv420p ) {
			dest = glsl_to_yuv420p( g, texture, &size, width, height, colorspace );
		}
		else if ( output_format == mlt_image_rgb24 ) {
			dest = glsl_to_rgb( g, texture, &size, width, height, 0 );
		}
		else if ( output_format == mlt_image_rgb24a || output_format == mlt_image_opengl ) {
			dest = glsl_to_rgb( g, texture, &size, width, height, 1 );
		}

		if ( release )
			g->release_texture( texture );

		/* unlock gl */
		g->context_unlock( g );

		if ( dest && size != 0 ) {
			fprintf(stderr,"filter_glsl_csc convert_image -----------------------set_image uint8_t [frame:%d] [%d]\n", mlt_properties_get_int(MLT_FRAME_PROPERTIES( frame ), "_position"), syscall(SYS_gettid));
			mlt_frame_set_image( frame, dest, size, mlt_pool_release );
			*image = dest;
			mlt_properties_set_int( properties, "format", output_format );
			*format = output_format;
		}
		else
			return 1;
	}
	else {
		if ( texture ) {
			fprintf(stderr,"filter_glsl_csc convert_image -----------------------set_image, %u, %u [frame:%d] [%d]\n", (unsigned int)texture, texture->texture, mlt_properties_get_int(MLT_FRAME_PROPERTIES( frame ), "_position"), syscall(SYS_gettid));
			mlt_frame_set_image( frame, (uint8_t*)texture, sizeof(struct glsl_texture_s), g->texture_destructor );
			*image = (uint8_t*)texture;
			mlt_properties_set_int( properties, "format", output_format );
			*format = output_format;
		}
		else
			return 1;
	}

	return 0;
}



/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter filter, mlt_frame frame )
{
	// Set a default colorspace on the frame if not yet set by the producer.
	// The producer may still change it during get_image.
	// This way we do not have to modify each producer to set a valid colorspace.
	mlt_properties properties = MLT_FRAME_PROPERTIES(frame);
	if ( mlt_properties_get_int( properties, "colorspace" ) <= 0 )
		mlt_properties_set_int( properties, "colorspace", mlt_service_profile( MLT_FILTER_SERVICE(filter) )->colorspace );

	frame->convert_image = convert_image;

	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_glsl_csc_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	fprintf(stderr,"filter_glsl_csc_init -----------------------\n");
	if ( !mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 ) )
		return NULL;
	
	mlt_filter filter = mlt_filter_new( );
	if ( filter != NULL )
		filter->process = filter_process;
	return filter;
}
