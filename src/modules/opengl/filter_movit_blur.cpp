/*
 * filter_movit_blur.cpp
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
#include "movit/blur_effect.h"

static mlt_frame process( mlt_filter filter, mlt_frame frame )
{
	mlt_properties properties = MLT_FILTER_PROPERTIES( filter );
	Effect* effect = (Effect*) mlt_properties_get_data( properties, "effect", NULL );
	bool ok = effect->set_float( "radius", mlt_properties_get_double( properties, "radius" ) );
	assert(ok);
	return frame;
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

			mlt_properties_set_data( properties, "effect", effect, 0, NULL, NULL );
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
