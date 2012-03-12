/*
 * consumer_qgl.c -- a Qt OpenGL based consumer for MLT
 * Copyright (C) 2012 Christophe Thommeret
 * Author: Christophe Thommeret <hftom@free.fr>
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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "qgl_wrapper.h"
#include "mlt_glsl.h"



/** Forward references to static functions.
*/

static int consumer_start( mlt_consumer parent );
static int consumer_stop( mlt_consumer parent );
static int consumer_is_stopped( mlt_consumer parent );
static void consumer_close( mlt_consumer parent );
static void *consumer_thread( void * );



/** This is what will be called by the factory - anything can be passed in
	via the argument, but keep it simple.
*/

mlt_consumer consumer_qgl_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	fprintf(stderr, "consumer_qgl_init\n");
	// Create the consumer object
	consumer_qgl this = calloc( sizeof( struct consumer_qgl_s ), 1 );

	// If no malloc'd and consumer init ok
	if ( this != NULL && mlt_consumer_init( &this->parent, this, profile ) == 0 )
	{
		// Create the queue
		this->queue = mlt_deque_init( );

		// Get the parent consumer object
		mlt_consumer parent = &this->parent;

		// We have stuff to clean up, so override the close method
		parent->close = consumer_close;

		// get a handle on properties
		mlt_service service = MLT_CONSUMER_SERVICE( parent );
		this->properties = MLT_SERVICE_PROPERTIES( service );

		// Default scaler
		mlt_properties_set( this->properties, "rescale", "bilinear" );
		mlt_properties_set( this->properties, "deinterlace_method", "onefield" );

		// default image format
		mlt_properties_set( this->properties, "mlt_image_format", "none" );

		// not more than 1
		mlt_properties_set_int( this->properties, "real_time", 0 );

		// Default buffer for low latency
		mlt_properties_set_int( this->properties, "buffer", 1 );

		// Ensure we don't join on a non-running object
		this->joined = 1;
		this->qgl_started = 0;

		// Allow thread to be started/stopped
		parent->start = consumer_start;
		parent->stop = consumer_stop;
		parent->is_stopped = consumer_is_stopped;

		glsl_env g = glsl_env_create();
		if ( g )
		{
			mlt_properties prop = mlt_global_properties();
			if ( prop )
			{
				mlt_properties_set_data( prop, "glsl_env", (void*)g, 0, NULL, NULL );
				//mlt_properties_set_data( prop, "glsl_make_current", (void*)g->context_lock, 0, NULL, NULL );
				//mlt_properties_set_data( prop, "glsl_done_current", (void*)g->context_unlock, 0, NULL, NULL );
			}
		}

		// Return the consumer produced
		return parent;
	}

	// malloc or consumer init failed
	free( this );

	// Indicate failure
	return NULL;
}



int consumer_start( mlt_consumer parent )
{
	consumer_qgl this = parent->child;

	if ( !this->running )
	{
		consumer_stop( parent );

		this->running = 1;
		this->joined = 0;

		pthread_create( &this->thread, NULL, consumer_thread, this );
	}

	return 0;
}



int consumer_stop( mlt_consumer parent )
{
	// Get the actual object
	consumer_qgl this = parent->child;
	
	if ( this->running && this->joined == 0 )
	{
		// Kill the thread and clean up
		this->joined = 1;
		this->running = 0;

		if ( this->thread )
			pthread_join( this->thread, NULL );
	}

	return 0;
}



int consumer_is_stopped( mlt_consumer parent )
{
	consumer_qgl this = parent->child;
	return !this->running;
}



static void *consumer_thread( void *arg )
{
	// Identify the arg
	consumer_qgl this = arg;

	XInitThreads();
	start_qgl( this );

	return NULL;
}



/** Callback to allow override of the close method.
*/

static void consumer_close( mlt_consumer parent )
{
	// Get the actual object
	consumer_qgl this = parent->child;

	// Stop the consumer
	///mlt_consumer_stop( parent );

	// Now clean up the rest
	mlt_consumer_close( parent );

	// Close the queue
	mlt_deque_close( this->queue );

	// Finally clean up this
	free( this );
}
