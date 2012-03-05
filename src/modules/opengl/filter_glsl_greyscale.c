/*
 * filter_glsl_greyscale.c
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
#include <framework/mlt_factory.h>

#include <stdio.h>
#include <stdlib.h>



static const char *filter_glsl_greyscale_frag=
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;\n"
"void main() {\n"
"    vec4 col = texture2DRect( tex, gl_TexCoord[0].xy );\n"
"    // rec.709\n"
"    float luma = dot(col.rgb, vec3(0.212671, 0.71516, 0.072169));\n"
"    gl_FragColor = vec4( vec3( luma ), col.a );\n"
"}\n";



/** Do it :-).
*/

static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	glsl_env g = (glsl_env)mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 );
	if ( !g || (g && !g->user_data) )
		return 1;

	*format = mlt_image_glsl;
	int error = mlt_frame_get_image( frame, image, format, width, height, 1 );
	glsl_texture source_tex = (glsl_texture)(*image);// (glsl_texture)mlt_properties_get_data( MLT_FRAME_PROPERTIES( frame ), "image", NULL );

	if ( !source_tex || error )
		return 1;

	/* lock gl */
	g->context_lock( g );

	glsl_fbo fbo = g->get_fbo( g, *width, *height );
	if ( !fbo ) {
		/* unlock gl */
		g->context_unlock( g );
		return 1;
	}

	glsl_texture dest = g->get_texture( g, *width, *height, GL_RGBA );
	if ( !dest ) {
		g->release_fbo( fbo );
		/* unlock gl */
		g->context_unlock( g );
		return 1;
	}
	
	glsl_shader shader = g->get_shader( g, "filter_glsl_greyscale_frag", &filter_glsl_greyscale_frag );
	if ( !shader ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		/* unlock gl */
		g->context_unlock( g );
		return 1;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

	glsl_set_ortho_view( *width, *height );

	glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );
	glUseProgram( shader->program );
	glUniform1i( glGetUniformLocationARB( shader->program, "tex" ), 0 );
	
	glsl_draw_quad( 0, 0, *width, *height );

	glUseProgram( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	
	g->release_fbo( fbo );
	/* unlock gl */
	g->context_unlock( g );

	if ( dest ) {
		fprintf(stderr,"filter_glsl_greyscale -----------------------set_image, %u, %u [frame:%d] [%d]\n", (unsigned int)dest, dest->texture, mlt_properties_get_int(MLT_FRAME_PROPERTIES( frame ), "_position"), syscall(SYS_gettid));
		mlt_frame_set_image( frame, (uint8_t*)dest, sizeof(struct glsl_texture_s), g->texture_destructor );
		*image = (uint8_t*)dest;
	}

	return error;
}

/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter filter, mlt_frame frame )
{
	mlt_frame_push_get_image( frame, filter_get_image );
	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_glsl_greyscale_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	if ( !mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 ) )
		return NULL;
		
	mlt_filter this = mlt_filter_new( );
	if ( this != NULL )
		this->process = filter_process;
	return this;
}

