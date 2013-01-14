/*
 * filter_movit_blur.cpp
 * Copyright (C) 2013 Dan Dennedy <dan@dennedy.org>
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

#include <framework/mlt.h>
#include <string.h>
#include <assert.h>

#include "mlt_glsl.h"
#include "movit/init.h"
#include "movit/effect_chain.h"
#include "movit/blur_effect.h"

static mlt_image_format g_format = mlt_image_none;

static int get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	*format = g_format;
	int error = mlt_frame_get_image( frame, image, format, width, height, writable );
	if ( !error ) {
		if ( *format != mlt_image_glsl && frame->convert_image ) {
			// Pin the requested format to the first one returned.
			g_format = *format;
			error = frame->convert_image( frame, image, format, mlt_image_glsl );
		} else {
			error = 1;
		}
	}
	return error;
}

static mlt_frame process( mlt_filter filter, mlt_frame frame )
{
	mlt_properties properties = MLT_FILTER_PROPERTIES( filter );
	Effect* effect = (Effect*) mlt_properties_get_data( properties, "effect", NULL );
	effect->set_float( "radius", mlt_properties_get_double( properties, "radius" ) );
	mlt_frame_push_get_image( frame, get_image );
	return frame;
}

static void deleteEffect( void *o )
{
	delete (Effect*) o;
}

extern "C" {

mlt_filter filter_movit_blur_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	mlt_filter filter = NULL;
	glsl_env glsl = mlt_glsl_get( profile );

	if ( glsl && ( filter = mlt_filter_new() ) )
	{
		mlt_properties properties = MLT_FILTER_PROPERTIES( filter );
		if ( !mlt_glsl_init_movit( glsl, profile ) )
		{
			EffectChain* chain = (EffectChain*) glsl->movitChain;
			Effect* effect = chain->add_effect( new BlurEffect() );

			mlt_properties_set_data( properties, "effect", effect, 0, deleteEffect, NULL );
			effect->set_int( "width", profile->width );
			effect->set_int( "height", profile->height );
			mlt_properties_set_double( properties, "radius", 3 );
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
