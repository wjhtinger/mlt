/*
 * filter_glsl_crop.c
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

#include <framework/mlt_filter.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_log.h>
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
	
	int error = 0;
	mlt_profile profile = mlt_frame_pop_service( frame );

	// Get the properties from the frame
	mlt_properties properties = MLT_FRAME_PROPERTIES( frame );

	// Correct Width/height if necessary
	if ( *width == 0 || *height == 0 )
	{
		*width  = profile->width;
		*height = profile->height;
	}

	int left    = mlt_properties_get_int( properties, "crop.left" );
	int right   = mlt_properties_get_int( properties, "crop.right" );
	int top     = mlt_properties_get_int( properties, "crop.top" );
	int bottom  = mlt_properties_get_int( properties, "crop.bottom" );

	// Request the image at its original resolution
	if ( left || right || top || bottom )
	{
		mlt_properties_set_int( properties, "rescale_width", mlt_properties_get_int( properties, "crop.original_width" ) );
		mlt_properties_set_int( properties, "rescale_height", mlt_properties_get_int( properties, "crop.original_height" ) );
	}

	// Now get the image
	*format = mlt_image_glsl;
	error = mlt_frame_get_image( frame, image, format, width, height, writable );
	glsl_texture source_tex = (glsl_texture)mlt_properties_get_data( properties, "image", NULL );

	int owidth  = *width - left - right;
	int oheight = *height - top - bottom;
	owidth = owidth < 0 ? 0 : owidth;
	oheight = oheight < 0 ? 0 : oheight;

	if ( source_tex && ( owidth != *width || oheight != *height ) && !error && *image && owidth > 0 && oheight > 0 )
	{
		// Provides a manual override for misreported field order
		if ( mlt_properties_get( properties, "meta.top_field_first" ) )
		{
			mlt_properties_set_int( properties, "top_field_first", mlt_properties_get_int( properties, "meta.top_field_first" ) );
			mlt_properties_set_int( properties, "meta.top_field_first", 0 );
		}

		if ( top % 2 )
			mlt_properties_set_int( properties, "top_field_first", !mlt_properties_get_int( properties, "top_field_first" ) );
		
		// Create the output image

		/* lock gl */
		g->context_lock( g );

		glsl_fbo fbo = g->get_fbo( g, owidth, oheight );
		if ( !fbo ) {
			/* unlock gl */
			g->context_unlock( g );
			return 1;
		}

		glsl_texture dest = g->get_texture( g, owidth, oheight, GL_RGBA );
		if ( !dest ) {
			g->release_fbo( fbo );
			/* unlock gl */
			g->context_unlock( g );
			return 1;
		}

		glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, dest->texture, 0 );

		glsl_set_ortho_view( owidth, oheight );

		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_RECTANGLE_ARB, source_tex->texture );

		glBegin( GL_QUADS );
			glTexCoord2f( left, top );							glVertex3f( 0.0, 0.0, 0.);
			glTexCoord2f( left, *height - bottom );				glVertex3f( 0.0, oheight, 0.);
			glTexCoord2f( *width - right, *height - bottom );	glVertex3f( owidth, oheight, 0.);
			glTexCoord2f( *width - right, top );				glVertex3f( owidth, 0.0, 0.);
		glEnd();

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		g->release_fbo( fbo );

		/* unlock gl */
		g->context_unlock( g );

		fprintf(stderr,"filter_glsl_crop -----------------------set_image, %u, %u [frame:%d] [%d]\n",
				(unsigned int)dest, dest->texture, mlt_properties_get_int(properties, "_position"), syscall(SYS_gettid));
		mlt_frame_set_image( frame, (uint8_t*)dest, sizeof(struct glsl_texture_s), g->texture_destructor );
		*image = (uint8_t*)dest;
		*width = owidth;
		*height = oheight;
	}

	return error;
}

/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter filter, mlt_frame frame )
{
 if ( mlt_properties_get_int( MLT_FILTER_PROPERTIES( filter ), "active" ) )
	{
		// Push the get_image method on to the stack
		mlt_frame_push_service( frame, mlt_service_profile( MLT_FILTER_SERVICE( filter ) ) );
		mlt_frame_push_get_image( frame, filter_get_image );
	}
	else
	{
		mlt_properties filter_props = MLT_FILTER_PROPERTIES( filter );
		mlt_properties frame_props = MLT_FRAME_PROPERTIES( frame );
		int left   = mlt_properties_get_int( filter_props, "left" );
		int right  = mlt_properties_get_int( filter_props, "right" );
		int top    = mlt_properties_get_int( filter_props, "top" );
		int bottom = mlt_properties_get_int( filter_props, "bottom" );
		int width  = mlt_properties_get_int( frame_props, "meta.media.width" );
		int height = mlt_properties_get_int( frame_props, "meta.media.height" );
		int use_profile = mlt_properties_get_int( filter_props, "use_profile" );
		mlt_profile profile = mlt_service_profile( MLT_FILTER_SERVICE( filter ) );

		if ( use_profile )
		{
			top = top * height / profile->height;
			bottom = bottom * height / profile->height;
			left = left * width / profile->width;
			right = right * width / profile->width;
		}
		if ( mlt_properties_get_int( filter_props, "center" ) )
		{
			double aspect_ratio = mlt_frame_get_aspect_ratio( frame );
			if ( aspect_ratio == 0.0 )
				aspect_ratio = mlt_profile_sar( profile );
			double input_ar = aspect_ratio * width / height;
			double output_ar = mlt_profile_dar( mlt_service_profile( MLT_FILTER_SERVICE(filter) ) );
			int bias = mlt_properties_get_int( filter_props, "center_bias" );

			if ( input_ar > output_ar )
			{
				left = right = ( width - rint( output_ar * height / aspect_ratio ) ) / 2;
				if ( abs(bias) > left )
					bias = bias < 0 ? -left : left;
				else if ( use_profile )
					bias = bias * width / profile->width;
				left -= bias;
				right += bias;
			}
			else
			{
				top = bottom = ( height - rint( aspect_ratio * width / output_ar ) ) / 2;
				if ( abs(bias) > top )
					bias = bias < 0 ? -top : top;
				else if ( use_profile )
					bias = bias * height / profile->height;
				top -= bias;
				bottom += bias;
			}
		}

		// Coerce the output to an even width because subsampled YUV with odd widths is too
		// risky for downstream processing to handle correctly.
		left += ( width - left - right ) & 1;
		if ( width - left - right < 8 )
			left = right = 0;
		if ( height - top - bottom < 8 )
			top = bottom = 0;
		mlt_properties_set_int( frame_props, "crop.left", left );
		mlt_properties_set_int( frame_props, "crop.right", right );
		mlt_properties_set_int( frame_props, "crop.top", top );
		mlt_properties_set_int( frame_props, "crop.bottom", bottom );
		mlt_properties_set_int( frame_props, "crop.original_width", width );
		mlt_properties_set_int( frame_props, "crop.original_height", height );
		mlt_properties_set_int( frame_props, "meta.media.width", width - left - right );
		mlt_properties_set_int( frame_props, "meta.media.height", height - top - bottom );
	}
	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_glsl_crop_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	if ( !mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 ) )
		return NULL;

	mlt_filter this = calloc( sizeof( struct mlt_filter_s ), 1 );
	if ( mlt_filter_init( this, this ) == 0 )
	{
		this->process = filter_process;
		if ( arg )
			mlt_properties_set_int( MLT_FILTER_PROPERTIES( this ), "active", atoi( arg ) );
	}
	return this;
}
