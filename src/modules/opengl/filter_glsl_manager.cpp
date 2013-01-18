/*
 * filter_glsl_manager.cpp
 * Copyright (C) 2012 Dan Dennedy <dan@dennedy.org>
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

#include <string>
#include <mlt++/MltFilter.h>
#include "mlt_glsl.h"
#include "movit/init.h"
#include "movit/effect_chain.h"
#include "mlt_movit_input.h"


class GlslManager : public Mlt::Filter
{
public:
	GlslManager() : Mlt::Filter( mlt_filter_new() )
	{
		mlt_filter filter = get_filter();
		if ( filter ) {
			// Mlt::Filter() adds a reference that we do not need.
			mlt_filter_close( filter );
			// Set the mlt_filter child in case we choose to override virtual functions.
			filter->child = this;

			mlt_events_register( get_properties(), "test glsl", NULL );
			listen( "test glsl", this, (mlt_listener) onTest );
		}
	}

private:
	static void onTest( mlt_properties owner, GlslManager* filter )
	{
		mlt_log_verbose( filter->get_service(), "%s: %d\n", __FUNCTION__, syscall(SYS_gettid) );
		init_movit( std::string(mlt_environment( "MLT_DATA" )).append("/opengl/movit"),
			mlt_log_get_level() == MLT_LOG_DEBUG? MOVIT_DEBUG_ON : MOVIT_DEBUG_OFF );
		filter->set( "glsl_supported", movit_initialized );
	}
};

namespace Mlt
{
class VerticalFlip : public Effect {
public:
	VerticalFlip() {}
	virtual std::string effect_type_id() const { return "MltVerticalFlip"; }
	std::string output_fragment_shader() {
		return "vec4 FUNCNAME(vec2 tc) { tc.y = 1.0 - tc.y; return INPUT(tc); }\n";
	}
	virtual bool needs_linear_light() const { return false; }
	virtual bool needs_srgb_primaries() const { return false; }
	AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
};
}

extern "C" {

glsl_env mlt_glsl_init( mlt_profile profile );

mlt_filter filter_glsl_manager_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	GlslManager* g = new GlslManager();
	mlt_glsl_init( profile );
	return g->get_filter();
}

} // extern "C"

static void deleteChain( EffectChain* chain )
{
	delete chain;
}

int mlt_glsl_init_movit( mlt_producer producer )
{
	int error = 1;
	mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer );
	EffectChain* chain = (EffectChain*) mlt_properties_get_data( properties, "movit chain", NULL );
	if ( !chain ) {
		mlt_profile profile = mlt_service_profile( MLT_PRODUCER_SERVICE( producer ) );
		MltInput* input = new MltInput( profile->width, profile->height );
		chain = new EffectChain( mlt_profile_dar( profile ), 1.0f );
		chain->add_input( input );
		mlt_properties_set_data( properties, "movit chain", chain, 0, (mlt_destructor) deleteChain, NULL );
		mlt_properties_set_data( properties, "movit input", input, 0, NULL, NULL );
		mlt_properties_set_int( properties, "_movit finalized", 0 );
		error = 0;
	}
	return error;
}

Effect* mlt_glsl_get_effect( mlt_filter filter, mlt_frame frame )
{
	mlt_producer producer = mlt_producer_cut_parent( mlt_frame_get_original_producer( frame ) );
	mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer );
	char *unique_id = mlt_properties_get( MLT_FILTER_PROPERTIES(filter), "_unique_id" );
	return (Effect*) mlt_properties_get_data( properties, unique_id, NULL );
}

Effect* mlt_glsl_add_effect( mlt_filter filter, mlt_frame frame, Effect* effect )
{
	if ( !mlt_glsl_get_effect( filter, frame ) ) {
		mlt_producer producer = mlt_producer_cut_parent( mlt_frame_get_original_producer( frame ) );
		mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer );
		char *unique_id = mlt_properties_get( MLT_FILTER_PROPERTIES(filter), "_unique_id" );
		EffectChain* chain = (EffectChain*) mlt_properties_get_data( properties, "movit chain", NULL );
		chain->add_effect( effect );
		mlt_properties_set_data( properties, unique_id, effect, 0, NULL, NULL );
	}
	return effect;
}

void mlt_glsl_render_fbo( mlt_producer producer, void* chain, GLuint fbo, int width, int height )
{
	EffectChain* effect_chain = (EffectChain*) chain;
	mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer );
	if ( !mlt_properties_get_int( properties, "_movit finalized" ) ) {
		mlt_properties_set_int( properties, "_movit finalized", 1 );
		effect_chain->add_effect( new Mlt::VerticalFlip() );
		effect_chain->finalize();
	}
	effect_chain->render_to_fbo( fbo, width, height );
}
