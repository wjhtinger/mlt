/*
 * filter_glsl_fieldorder.c -- change field dominance
 * Copyright (C) 2012 Christophe Thommeret
 * Author: Christophe Thommeret <hftom@free.fr>
 * Author: Dan Dennedy <dan@dennedy.org> (core version)
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

#include <framework/mlt.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>



static const char *filter_glsl_swap_fields_frag=
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;\n"
"void main(void) {\n"
"    vec2 coord = gl_TexCoord[0].xy;\n"
"    float y = coord.y + 1.0;\n"
"    y -= 2.0 * step( 1.0, mod( coord.y, 2.0 ) );\n"
"    gl_FragColor = texture2DRect( tex, vec2( coord.x, y ) );\n"
"}\n";



static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	glsl_env g = (glsl_env)mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 );
	if ( !g )
		return 1;

	// Get the properties from the frame
	mlt_properties properties = MLT_FRAME_PROPERTIES( frame );

	// Get the input image, width and height
	*format = mlt_image_glsl;
	int error = mlt_frame_get_image( frame, image, format, width, height, 1 );
	glsl_texture source_tex = (glsl_texture)mlt_properties_get_data( properties, "image", NULL );

	if ( !source_tex || error )
		return 1;

	glsl_texture dest = 0;
	int tff = mlt_properties_get_int( properties, "consumer_tff" );
	int swapped = 0;

	// Provides a manual override for misreported field order
	if ( mlt_properties_get( properties, "meta.top_field_first" ) )
		mlt_properties_set_int( properties, "top_field_first", mlt_properties_get_int( properties, "meta.top_field_first" ) );

	if ( ( mlt_properties_get_int( properties, "meta.swap_fields" ) || (mlt_properties_get_int( properties, "top_field_first" ) != tff) ) &&
		mlt_properties_get( properties, "progressive" ) &&
		mlt_properties_get_int( properties, "progressive" ) == 0 )
	{
		if ( mlt_properties_get_int( properties, "meta.swap_fields" ) ) {
			glsl_fbo fbo = g->get_fbo( g, *width, *height );
			if ( !fbo ) {
				return 1;
			}

			dest = g->get_texture( g, *width, *height, GL_RGBA );
			if ( !dest ) {
				g->release_fbo( fbo );
				return 1;
			}

			glsl_shader shader = g->get_shader( g, "filter_glsl_swap_fields_frag", &filter_glsl_swap_fields_frag );
			if ( !shader ) {
				g->release_fbo( fbo );
				g->release_texture( dest );
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
			swapped = 1;
		}

		// Correct field order if needed
		if ( mlt_properties_get_int( properties, "top_field_first" ) != tff ) {
			if ( swapped ) {
				source_tex = dest;
				dest = 0;
			}
			
			dest = g->get_texture( g, *width, *height, GL_RGBA );
			if ( !dest ) {
				if ( swapped )
					g->release_texture( source_tex );
				return 1;
			}

			glsl_fbo fbo = g->get_fbo( g, *width, *height );
			if ( !fbo ) {
				if ( swapped )
					g->release_texture( source_tex );
				g->release_texture( dest );
				return 1;
			}

			glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

			glsl_set_ortho_view( *width, *height );

			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );

			glsl_draw_quad( 0, 0, *width, 1 );
			glBegin( GL_QUADS );
				glTexCoord2f( 0.0, 0.0 ); 				glVertex3f( 0.0, 1.0, 0.);
				glTexCoord2f( 0.0, *height - 1.0 ); 	glVertex3f( 0.0, *height, 0.);
				glTexCoord2f( *width, *height - 1.0 ); 	glVertex3f( *width, *height, 0.);
				glTexCoord2f( *width, 0.0 ); 			glVertex3f( *width, 1.0, 0.);
			glEnd();

			glBindFramebuffer( GL_FRAMEBUFFER, 0 );

			g->release_fbo( fbo );

			if ( swapped )
				g->release_texture( source_tex );
		}

		if ( dest ) {
			fprintf(stderr,"filter_glsl_fieldorder -----------------------set_image, %u, %u [frame:%d] [%d]\n", (unsigned int)dest, dest->texture, mlt_properties_get_int(properties, "_position"), syscall(SYS_gettid));
			mlt_frame_set_image( frame, (uint8_t*)dest, sizeof(struct glsl_texture_s), g->texture_destructor );
			*image = (uint8_t*)dest;
		}
	}

	return 0;
}


static mlt_frame process( mlt_filter filter, mlt_frame frame )
{
	mlt_frame_push_get_image( frame, filter_get_image );
	return frame;
}


mlt_filter filter_glsl_fieldorder_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	if ( !mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 ) )
		return NULL;
	
	mlt_filter filter = calloc( 1, sizeof( *filter ) );
	if ( mlt_filter_init( filter, NULL ) == 0 )
	{
		filter->process = process;
	}
	return filter;
}
