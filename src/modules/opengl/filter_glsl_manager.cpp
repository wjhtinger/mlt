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
			mlt_events_register( get_properties(), "init glsl", NULL );
			mlt_events_register( get_properties(), "start glsl", NULL );
			listen( "test glsl", this, (mlt_listener) onTest );
			listen( "init glsl", this, (mlt_listener) onInit );
			listen( "start glsl", this, (mlt_listener) onStart );
		}
	}

private:
	static void onTest( mlt_properties owner, GlslManager* filter )
	{
		mlt_log_verbose( filter->get_service(), "%s: %d\n", __FUNCTION__, syscall(SYS_gettid) );
		init_movit( std::string(mlt_environment( "MLT_DATA" )).append("/opengl/movit"),
			mlt_log_get_level() == MLT_LOG_DEBUG? MOVIT_DEBUG_ON : MOVIT_DEBUG_OFF );
		filter->set( "glsl_supported", movit_initialized && mlt_glsl_supported() );
	}

	static void onInit( mlt_properties owner, GlslManager* filter )
	{
		mlt_log_verbose( filter->get_service(), "%s: %d\n", __FUNCTION__, syscall(SYS_gettid) );
		mlt_glsl_init( filter->get_profile() );
	}

	static void onStart( mlt_properties owner, GlslManager* filter )
	{
		mlt_log_verbose( filter->get_service(), "%s: %d\n", __FUNCTION__, syscall(SYS_gettid) );
		init_movit( std::string(mlt_environment( "MLT_DATA" )).append("/opengl/movit"),
			mlt_log_get_level() == MLT_LOG_DEBUG? MOVIT_DEBUG_ON : MOVIT_DEBUG_OFF );
		mlt_glsl_start( mlt_glsl_get( filter->get_profile() ) );
	}
};

namespace Mlt
{
class VerticalFlip : public Effect {
public:
	VerticalFlip() {}
	virtual std::string effect_type_id() const { return "Mlt::VerticalFlip"; }
	std::string output_fragment_shader() {
		return "vec4 FUNCNAME(vec2 tc) { tc.y = 1.0 - tc.y; return INPUT(tc); }\n";
	}
	virtual bool needs_linear_light() const { return false; }
	virtual bool needs_srgb_primaries() const { return false; }
};
}

extern "C" {

mlt_filter filter_glsl_manager_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	GlslManager* g = new GlslManager();
	return g->get_filter();
}

int mlt_glsl_init_movit( glsl_env glsl, mlt_profile profile )
{
	int error = 0;
	if ( !glsl->movitChain )
	{
		MltInput* input = new MltInput( profile->width, profile->height );
		EffectChain *chain = new EffectChain( mlt_profile_dar( profile ), 1.0f );
		glsl->movitChain = chain;
		glsl->movitInput = input;
		chain->add_input( input );
	}
	return error;
}

void mlt_glsl_set_image( glsl_env glsl, const uint8_t* image )
{
	MltInput* input = (MltInput*) glsl->movitInput;
	EffectChain* chain = (EffectChain*) glsl->movitChain;
	input->useFlatInput( chain, FORMAT_RGBA );
	input->set_pixel_data( image );
}

void mlt_glsl_render_fbo( glsl_env glsl, void* chain, GLuint fbo, int width, int height )
{
	EffectChain* effect_chain = (EffectChain*) chain;
	if ( !glsl->movitFinalized ) {
		glsl->movitFinalized = 1;
		effect_chain->add_effect( new Mlt::VerticalFlip() );
		effect_chain->finalize();
	}
	effect_chain->render_to_fbo( fbo, width, height );
}

void mlt_glsl_close( glsl_env glsl )
{
	delete (EffectChain*) glsl->movitChain;
	// TODO: free list members of glsl
	free( glsl );
}

}
