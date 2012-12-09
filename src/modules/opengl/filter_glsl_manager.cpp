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

#include <mlt++/MltFilter.h>
#include "mlt_glsl.h"

class GlslManager : public Mlt::Filter
{
public:
	GlslManager() : Mlt::Filter( mlt_filter_new() )
	{
		if ( get_filter() ) {
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
		filter->set( "glsl_supported", mlt_glsl_supported() );
	}

	static void onInit( mlt_properties owner, GlslManager* filter )
	{
		mlt_log_verbose( filter->get_service(), "%s: %d\n", __FUNCTION__, syscall(SYS_gettid) );
		mlt_glsl_init( filter->get_profile() );
	}

	static void onStart( mlt_properties owner, GlslManager* filter )
	{
		mlt_log_verbose( filter->get_service(), "%s: %d\n", __FUNCTION__, syscall(SYS_gettid) );
		mlt_glsl_start( mlt_glsl_get( filter->get_profile() ) );
	}
};

extern "C" {

mlt_filter filter_glsl_manager_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	GlslManager* g = new GlslManager();
	return g->get_filter();
}

}
