/*
 * filter_glsl_resize.c -- resizing filter
 * Copyright (C) 2012 Christophe Thommeret
 * Author: Christophe Thommeret <hftom@free.fr>
 * Author: Charles Yates <charles.yates@pandora.be> (core version)
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
#include <framework/mlt_profile.h>
#include <framework/mlt_factory.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>



/** Do it :-).
*/

static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	glsl_env g = (glsl_env)mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 );
	if ( !g || (g && !g->user_data) )
		return 1;
	
	// Get the properties from the frame
	mlt_properties properties = MLT_FRAME_PROPERTIES( frame );

	// Pop the top of stack now
	mlt_filter filter = mlt_frame_pop_service( frame );
	mlt_profile profile = mlt_service_profile( MLT_FILTER_SERVICE( filter ) );

	// Retrieve the aspect ratio
	double aspect_ratio = mlt_deque_pop_back_double( MLT_FRAME_IMAGE_STACK( frame ) );
	double consumer_aspect = mlt_profile_sar( mlt_service_profile( MLT_FILTER_SERVICE( filter ) ) );

	// Correct Width/height if necessary
	if ( *width == 0 || *height == 0 )
	{
		*width = profile->width;
		*height = profile->height;
	}

	// Assign requested width/height from our subordinate
	int owidth = *width;
	int oheight = *height;

	// Check for the special case - no aspect ratio means no problem :-)
	if ( aspect_ratio == 0.0 )
		aspect_ratio = consumer_aspect;

	// Reset the aspect ratio
	mlt_properties_set_double( properties, "aspect_ratio", aspect_ratio );

	// Hmmm...
	char *rescale = mlt_properties_get( properties, "rescale.interp" );
	if ( rescale != NULL && !strcmp( rescale, "none" ) )
		return mlt_frame_get_image( frame, image, format, width, height, writable );

	if ( mlt_properties_get_int( properties, "distort" ) == 0 )
	{
		// Normalise the input and out display aspect
		int normalised_width = profile->width;
		int normalised_height = profile->height;
		int real_width = mlt_properties_get_int( properties, "meta.media.width" );
		int real_height = mlt_properties_get_int( properties, "meta.media.height" );
		if ( real_width == 0 )
			real_width = mlt_properties_get_int( properties, "width" );
		if ( real_height == 0 )
			real_height = mlt_properties_get_int( properties, "height" );
		double input_ar = aspect_ratio * real_width / real_height;
		double output_ar = consumer_aspect * owidth / oheight;
		
// 		fprintf( stderr, "real %dx%d normalised %dx%d output %dx%d sar %f in-dar %f out-dar %f\n",
// 		real_width, real_height, normalised_width, normalised_height, owidth, oheight, aspect_ratio, input_ar, output_ar);

		// Optimised for the input_ar > output_ar case (e.g. widescreen on standard)
		int scaled_width = rint( ( input_ar * normalised_width ) / output_ar );
		int scaled_height = normalised_height;

		// Now ensure that our images fit in the output frame
		if ( scaled_width > normalised_width )
		{
			scaled_width = normalised_width;
			scaled_height = rint( ( output_ar * normalised_height ) / input_ar );
		}

		// Now calculate the actual image size that we want
		owidth = rint( scaled_width * owidth / normalised_width );
		oheight = rint( scaled_height * oheight / normalised_height );

		// Tell frame we have conformed the aspect to the consumer
		mlt_frame_set_aspect_ratio( frame, consumer_aspect );
	}

	mlt_properties_set_int( properties, "distort", 0 );

	// Now pass on the calculations down the line
	mlt_properties_set_int( properties, "resize_width", *width );
	mlt_properties_set_int( properties, "resize_height", *height );

	*format = mlt_image_glsl;
	if ( mlt_frame_get_image( frame, image, format, &owidth, &oheight, writable ) )
		return 1;

	glsl_texture source_tex = (glsl_texture)mlt_properties_get_data( properties, "image", NULL );
	if ( !source_tex )
		return 1;

	int iwidth = mlt_properties_get_int( properties, "width" );
	int iheight = mlt_properties_get_int( properties, "height" );
	int left = (*width - iwidth) / 2;
	int top = (*height - iheight) / 2;

	if ( *width <= iwidth && *height <= iheight ) {
		*image = (uint8_t*)source_tex;
		return 0;
	}

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

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );
	glClear( GL_COLOR_BUFFER_BIT );

	glsl_set_ortho_view( *width, *height );

	glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );
	
	glBegin( GL_QUADS );
		glTexCoord2f( 0.0, 0.0 );			glVertex3f( left, top, 0.);
		glTexCoord2f( 0.0, iheight );		glVertex3f( left, top + iheight, 0.);
		glTexCoord2f( iwidth, iheight );	glVertex3f( left + iwidth, top + iheight, 0.);
		glTexCoord2f( iwidth, 0.0 );		glVertex3f( left + iwidth, top, 0.);
	glEnd();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	g->release_fbo( fbo );
	/* unlock gl */
	g->context_unlock( g );

	if ( dest ) {
		fprintf(stderr,"filter_glsl_resize -----------------------set_image, %u, %u [frame:%d] [%d] w=%d h=%d\n", (unsigned int)dest, dest->texture, mlt_properties_get_int(MLT_FRAME_PROPERTIES( frame ), "_position"), syscall(SYS_gettid), *width, *height);
		mlt_frame_set_image( frame, (uint8_t*)dest, sizeof(struct glsl_texture_s), g->texture_destructor );
		*image = (uint8_t*)dest;
	}

	return 0;
}


/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter filter, mlt_frame frame )
{
	// Store the aspect ratio reported by the source
	mlt_deque_push_back_double( MLT_FRAME_IMAGE_STACK( frame ), mlt_frame_get_aspect_ratio( frame ) );

	// Push this on to the service stack
	mlt_frame_push_service( frame, filter );

	// Push the get_image method on to the stack
	mlt_frame_push_get_image( frame, filter_get_image );

	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_glsl_resize_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	if ( !mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 ) )
		return NULL;
	
	mlt_filter this = calloc( sizeof( struct mlt_filter_s ), 1 );
	if ( mlt_filter_init( this, this ) == 0 )
	{
		this->process = filter_process;
	}
	return this;
}
