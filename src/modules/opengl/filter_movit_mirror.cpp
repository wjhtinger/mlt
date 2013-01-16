/*
 * filter_movit_mirror.cpp
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
#include "movit/mirror_effect.h"

extern "C" {

mlt_filter filter_movit_mirror_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	mlt_filter filter = NULL;
	glsl_env glsl = mlt_glsl_get( profile );

	if ( glsl && ( filter = mlt_filter_new() ) )
	{
		if ( !mlt_glsl_init_movit( glsl, profile ) )
		{
			EffectChain* chain = (EffectChain*) glsl->movitChain;
			chain->add_effect( new MirrorEffect() );
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

