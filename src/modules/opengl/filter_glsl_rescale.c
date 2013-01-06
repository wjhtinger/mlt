/*
 * filter_glsl_rescale.c
 * Copyright (C) 2012 Christphe Thommeret
 * Author: Christphe Thommeret <hftom@free.fr>
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
#include <framework/mlt_factory.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>



/** Do it :-).
*/

static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	// Get the filter from the stack
	mlt_filter filter = mlt_frame_pop_service( frame );

	glsl_env g = mlt_glsl_get( mlt_service_profile( MLT_FILTER_SERVICE( filter ) ) );
	if ( !g )
		return 1;
	
	// Get the frame properties
	mlt_properties properties = MLT_FRAME_PROPERTIES( frame );

	// Get the filter properties
	mlt_properties filter_properties = MLT_FILTER_PROPERTIES( filter );

	// Correct Width/height if necessary
	if ( *width == 0 || *height == 0 )
	{
		mlt_profile profile = mlt_service_profile( MLT_FILTER_SERVICE( filter ) );
		*width = profile->width;
		*height = profile->height;
	}

	// There can be problems with small images - avoid them (by hacking - gah)
	if ( *width >= 6 && *height >= 6 )
	{
		int iwidth = *width;
		int iheight = *height;
		double factor = mlt_properties_get_double( filter_properties, "factor" );
		factor = factor > 0 ? factor : 1.0;
		int owidth = *width * factor;
		int oheight = *height * factor;
		char *interps = mlt_properties_get( properties, "rescale.interp" );

		// Default from the scaler if not specifed on the frame
		if ( interps == NULL )
		{
			interps = mlt_properties_get( MLT_FILTER_PROPERTIES( filter ), "interpolation" );
			mlt_properties_set( properties, "rescale.interp", interps );
		}
	
		// If meta.media.width/height exist, we want that as minimum information
		if ( mlt_properties_get_int( properties, "meta.media.width" ) )
		{
			iwidth = mlt_properties_get_int( properties, "meta.media.width" );
			iheight = mlt_properties_get_int( properties, "meta.media.height" );
		}
	
		// Let the producer know what we are actually requested to obtain
		if ( strcmp( interps, "none" ) )
		{
			mlt_properties_set_int( properties, "rescale_width", *width );
			mlt_properties_set_int( properties, "rescale_height", *height );
		}
		else
		{
			// When no scaling is requested, revert the requested dimensions if possible
			mlt_properties_set_int( properties, "rescale_width", iwidth );
			mlt_properties_set_int( properties, "rescale_height", iheight );
		}

		// Deinterlace if height is changing to prevent fields mixing on interpolation
		// One exception: non-interpolated, integral scaling
		if ( iheight != oheight && ( strcmp( interps, "nearest" ) || ( iheight % oheight != 0 ) ) )
			mlt_properties_set_int( properties, "consumer_deinterlace", 1 );

		*format = mlt_image_glsl;
		int error = mlt_frame_get_image( frame, image, format, &iwidth, &iheight, 1 );
		glsl_texture source_tex = (glsl_texture)mlt_properties_get_data( properties, "image", NULL );

		if ( !source_tex || error )
			return 1;

		// Get rescale interpretation again, in case the producer wishes to override scaling
		interps = mlt_properties_get( properties, "rescale.interp" );
	
		if ( strcmp( interps, "none" ) && ( iwidth != owidth || iheight != oheight ) )
		{
			glsl_texture dest = NULL;
			
			if ( !strcmp( interps, "nearest" ) || !strcmp( interps, "bilinear" ) )
			{
				dest = glsl_rescale_bilinear( g, source_tex, iwidth, iheight, owidth, oheight );
			}
			else
			{
				int spline = CATMULLROM_SPLINE;
				if ( owidth < iwidth || oheight < iheight )
					spline = COS_SPLINE;
				dest = glsl_rescale_bicubic( g, source_tex, iwidth, iheight, owidth, oheight, spline );
			}

			if ( dest ) {
				fprintf(stderr,"filter_glsl_rescale -----------------------set_image, %u, %u [frame:%d] [%d] w=%d h=%d\n",
					(unsigned int)dest, dest->texture, mlt_properties_get_int(MLT_FRAME_PROPERTIES( frame ), "_position"), syscall(SYS_gettid), owidth, oheight);
				mlt_frame_set_image( frame, (uint8_t*)dest, sizeof(struct glsl_texture_s), g->texture_destructor );
				*image = (uint8_t*)dest;
			}

			*width = owidth;
			*height = oheight;
		}
		else
		{
			*width = iwidth;
			*height = iheight;
		}
	}

	return 0;
}


/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter filter, mlt_frame frame )
{
	// Push the filter
	mlt_frame_push_service( frame, filter );

	// Push the get image method
	mlt_frame_push_service( frame, filter_get_image );

	return frame;
}


/** Constructor for the filter.
*/

mlt_filter filter_glsl_rescale_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	if ( !mlt_glsl_get( profile ) )
		return NULL;
	
	// Create a new scaler
	mlt_filter this = mlt_filter_new( );

	// If successful, then initialise it
	if ( this != NULL )
	{
		// Get the properties
		mlt_properties properties = MLT_FILTER_PROPERTIES( this );

		// Set the process method
		this->process = filter_process;

		// Set the inerpolation
		mlt_properties_set( properties, "interpolation", arg == NULL ? "bilinear" : arg );
	}

	return this;
}
