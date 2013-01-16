/*
 * filter_movit_resize.cpp
 * Copyright (C) 2013 Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <framework/mlt.h>
#include <string.h>
#include <assert.h>

#include "mlt_glsl.h"
#include "movit/init.h"
#include "movit/effect_chain.h"
#include "movit/resize_effect.h"

static int get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	int error = 0;
	mlt_properties properties = MLT_FRAME_PROPERTIES( frame );
	mlt_filter filter = (mlt_filter) mlt_frame_pop_service( frame );
	mlt_properties filter_properties = MLT_FILTER_PROPERTIES( filter );
	mlt_profile profile = mlt_service_profile( MLT_FILTER_SERVICE( filter ) );

	// Retrieve the aspect ratio
	double aspect_ratio = mlt_deque_pop_back_double( MLT_FRAME_IMAGE_STACK( frame ) );
	double consumer_aspect = mlt_profile_sar( profile );

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

	// XXX: This is a hack, but it forces the force_full_luma to apply by doing a RGB
	// conversion because range scaling only occurs on YUV->RGB. And we do it here,
	// after the deinterlace filter, which only operates in YUV to avoid a YUV->RGB->YUV->?.
	// Instead, it will go YUV->RGB->?.
	if ( mlt_properties_get_int( properties, "force_full_luma" ) )
		*format = mlt_image_rgb24a;

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

 		mlt_log_debug( MLT_FILTER_SERVICE(filter),
			"real %dx%d normalised %dx%d output %dx%d sar %f in-dar %f out-dar %f\n",
			real_width, real_height, normalised_width, normalised_height, owidth, oheight, aspect_ratio, input_ar, output_ar);

		// Tell frame we have conformed the aspect to the consumer
		mlt_frame_set_aspect_ratio( frame, consumer_aspect );
	}

	mlt_properties_set_int( properties, "distort", 0 );

	// Now get the image
	if ( *format == mlt_image_yuv422 )
		owidth -= owidth % 2;
	error = mlt_frame_get_image( frame, image, format, &owidth, &oheight, writable );

	if ( !error )
	{
		Effect* effect = (Effect*) mlt_properties_get_data( filter_properties, "effect", NULL );
		bool ok = effect->set_int( "width", *width );
		ok |= effect->set_int( "height", *height );
		assert(ok);
	}

	return error;
}

static mlt_frame process( mlt_filter filter, mlt_frame frame )
{
	mlt_deque_push_back_double( MLT_FRAME_IMAGE_STACK( frame ), mlt_frame_get_aspect_ratio( frame ) );
	mlt_frame_push_service( frame, filter );
	mlt_frame_push_get_image( frame, get_image );
	return frame;
}

extern "C" {

mlt_filter filter_movit_resize_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	mlt_filter filter = NULL;
	glsl_env glsl = mlt_glsl_get( profile );

	if ( glsl && ( filter = mlt_filter_new() ) )
	{
		if ( !mlt_glsl_init_movit( glsl, profile ) )
		{
			EffectChain* chain = (EffectChain*) glsl->movitChain;
			Effect* effect = chain->add_effect( new ResizeEffect() );
			mlt_properties_set_data( MLT_FILTER_PROPERTIES(filter), "effect", effect, 0, NULL, NULL );
			filter->process = process;
		}
		else
		{
			mlt_filter_close( filter );
			filter = NULL;
		}
	}
	return filter;
}

}
