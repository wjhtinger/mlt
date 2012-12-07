/*
 * filter_glsl_deinterlace.c
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
#include <framework/mlt_log.h>
#include <framework/mlt_producer.h>
#include <framework/mlt_events.h>
#include <framework/mlt_factory.h>
#include <framework/mlt_frame.h>

#include <string.h>
#include <stdlib.h>



static const char *filter_glsl_linearblend_frag=
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;\n"
"void main() {\n"
"    vec2 coord = gl_TexCoord[0].xy;\n"
"    vec4 blend = 2.0 * texture2DRect( tex, coord );\n"
"    blend += texture2DRect( tex, vec2( coord.x, coord.y - 1.0 ) );\n"
"    blend += texture2DRect( tex, vec2( coord.x, coord.y + 1.0 ) );\n"
"    gl_FragColor = blend / 4.0;\n"
"}\n";



static glsl_texture deinterlace_linearblend( glsl_env g, glsl_texture source_tex, int width, int height )
{
	glsl_fbo fbo = g->get_fbo( g, width, height );
	if ( !fbo )
		return NULL;

	glsl_texture dest = g->get_texture( g, width, height, GL_RGBA );
	if ( !dest ) {
		g->release_fbo( fbo );
		return NULL;
	}

	glsl_shader shader = g->get_shader( g, "filter_glsl_linearblend_frag", &filter_glsl_linearblend_frag );
	if ( !shader ) {
		g->release_fbo( fbo );
		g->release_texture( dest );
		return NULL;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

	glsl_set_ortho_view( width, height );

	glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );
	glUseProgram( shader->program );
	glUniform1i( glGetUniformLocationARB( shader->program, "tex" ), 0 );

	glsl_draw_quad( 0, 0, width, height );

	glUseProgram( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g->release_fbo( fbo );

	return dest;
}



static glsl_texture deinterlace_onefield( glsl_env g, glsl_texture source_tex, int width, int height )
{
	glsl_fbo fbo = g->get_fbo( g, width, height/2 );
	if ( !fbo )
		return NULL;

	glsl_texture dest = g->get_texture( g, width, height/2, GL_RGBA );
	if ( !dest ) {
		g->release_fbo( fbo );
		return NULL;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

	glsl_set_ortho_view( width, height/2 );

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );

	glBegin( GL_QUADS );
		glTexCoord2f( 0.0, 0.0 );			glVertex3f( 0.0, 0.0, 0.);
		glTexCoord2f( 0.0, height );		glVertex3f( 0.0, height/2, 0.);
		glTexCoord2f( width, height );		glVertex3f( width, height/2, 0.);
		glTexCoord2f( width, 0.0 );			glVertex3f( width, 0.0, 0.);
	glEnd();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	g->release_fbo( fbo );

	return dest;
}



/** Do it :-).
*/

static int filter_get_image( mlt_frame this, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	// Pop the service off the stack
	mlt_filter filter = mlt_frame_pop_service( this );

	glsl_env g = mlt_glsl_get( mlt_service_profile( MLT_FILTER_SERVICE( filter ) ) );
	if ( !g )
		return 1;

	mlt_properties properties = MLT_FRAME_PROPERTIES( this );

	// Get the input image
	*format = mlt_image_glsl;
	int error = mlt_frame_get_image( this, image, format, width, height, writable );

	int deinterlace = mlt_properties_get_int( properties, "consumer_deinterlace" );
	int progressive = mlt_properties_get_int( properties, "progressive" );

	if ( deinterlace && !progressive )
	{
		// Determine deinterlace method
		char *method_str = mlt_properties_get( MLT_FILTER_PROPERTIES( filter ), "method" );
		char *frame_method_str = mlt_properties_get( properties, "deinterlace_method" );

		if ( frame_method_str )
			method_str = frame_method_str;

		glsl_texture source_tex = (glsl_texture)mlt_properties_get_data( MLT_FRAME_PROPERTIES( this ), "image", NULL );

		if ( !source_tex || error )
			return 1;

		glsl_texture dest = NULL;

		if ( !method_str || !strcmp( method_str, "onefield" ) )
		{
			glsl_texture onefield = deinterlace_onefield( g, source_tex, *width, *height );
			if ( !onefield )
				return 1;
			dest = glsl_rescale_bilinear( g, onefield, *width, *height/2, *width, *height );
			//dest = glsl_rescale_bicubic( g, onefield, *width, *height/2, *width, *height, CATMULLROM_SPLINE );
			g->release_texture( onefield );
		}
		else
		{
			dest = deinterlace_linearblend( g, source_tex, *width, *height );
		}

		if ( dest ) {
			fprintf(stderr,"filter_glsl_deinterlace -----------------------set_image, %u, %u [frame:%d] [%d]\n", (unsigned int)dest, dest->texture, mlt_properties_get_int(MLT_FRAME_PROPERTIES( this ), "_position"), syscall(SYS_gettid));
			mlt_frame_set_image( this, (uint8_t*)dest, sizeof(struct glsl_texture_s), g->texture_destructor );
			*image = (uint8_t*)dest;
			// Make sure that others know the frame is deinterlaced
			mlt_properties_set_int( properties, "progressive", 1 );
		}
	}

	/*if ( !deinterlace || progressive )
	{
		// Signal that we no longer need previous and next frames
		mlt_service service = mlt_properties_get_data( MLT_FILTER_PROPERTIES(filter), "service", NULL );
		if ( service )
			mlt_properties_set_int( MLT_SERVICE_PROPERTIES(service), "_need_previous_next", 0 );
	}*/

	return error;
}


/** Deinterlace filter processing - this should be lazy evaluation here...
*/

static mlt_frame deinterlace_process( mlt_filter this, mlt_frame frame )
{
	// Push this on to the service stack
	mlt_frame_push_service( frame, this );

	// Push the get_image method on to the stack
	mlt_frame_push_get_image( frame, filter_get_image );
	
	return frame;
}

static void on_service_changed( mlt_service owner, mlt_service filter )
{
	//mlt_service service = mlt_properties_get_data( MLT_SERVICE_PROPERTIES(filter), "service", NULL );
	//mlt_properties_set_int( MLT_SERVICE_PROPERTIES(service), "_need_previous_next", 1 );
}

/** Constructor for the filter.
*/

mlt_filter filter_glsl_deinterlace_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	if ( !mlt_glsl_get( profile ) )
		return NULL;
	
	mlt_filter this = mlt_filter_new( );
	if ( this != NULL )
	{
		this->process = deinterlace_process;
		mlt_properties_set( MLT_FILTER_PROPERTIES( this ), "method", arg );
		//mlt_events_listen( MLT_FILTER_PROPERTIES( this ), this, "service-changed", (mlt_listener) on_service_changed ); 
	}
	return this;
}

