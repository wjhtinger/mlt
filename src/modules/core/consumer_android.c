/*
 * consumer_sdl.c -- A Simple DirectMedia Layer consumer
 * Copyright (C) 2003-2019 Meltytech, LLC
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <framework/mlt_consumer.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_deque.h>
#include <framework/mlt_factory.h>
#include <framework/mlt_filter.h>
#include <framework/mlt_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdatomic.h>



/** This classes definition.
*/

typedef struct consumer_android_s *consumer_android;

struct consumer_android_s
{
	struct mlt_consumer_s parent;
	mlt_properties properties;
	mlt_deque queue;
	pthread_t thread;
	int joined;
	atomic_int running;
	uint8_t audio_buffer[ 4096 * 10 ];
	int audio_avail;
	pthread_mutex_t audio_mutex;
	pthread_cond_t audio_cond;
	pthread_mutex_t video_mutex;
	pthread_cond_t video_cond;
	int window_width;
	int window_height;
	int previous_width;
	int previous_height;
	int width;
	int height;
	atomic_int playing;
	int sdl_flags;
	uint8_t *buffer;
	int bpp;
	int is_purge;
};

/** Forward references to static functions.
*/

static int consumer_start( mlt_consumer parent );
static int consumer_stop( mlt_consumer parent );
static int consumer_is_stopped( mlt_consumer parent );
static void consumer_purge( mlt_consumer parent );
static void consumer_close( mlt_consumer parent );
static void *consumer_thread( void * );

/** This is what will be called by the factory - anything can be passed in
	via the argument, but keep it simple.
*/
mlt_consumer consumer_android_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	// Create the consumer object
	consumer_android self = calloc( 1, sizeof( struct consumer_android_s ) );

	// If no malloc'd and consumer init ok
	if ( self != NULL && mlt_consumer_init( &self->parent, self, profile ) == 0 )
	{
		// Create the queue
		self->queue = mlt_deque_init( );

		// Get the parent consumer object
		mlt_consumer parent = &self->parent;

		// We have stuff to clean up, so override the close method
		parent->close = consumer_close;

		// get a handle on properties
		mlt_service service = MLT_CONSUMER_SERVICE( parent );
		self->properties = MLT_SERVICE_PROPERTIES( service );

		// Set the default volume
		mlt_properties_set_double( self->properties, "volume", 1.0 );

		// This is the initialisation of the consumer
		pthread_mutex_init( &self->audio_mutex, NULL );
		pthread_cond_init( &self->audio_cond, NULL);
		pthread_mutex_init( &self->video_mutex, NULL );
		pthread_cond_init( &self->video_cond, NULL);
		
		// Default scaler (for now we'll use nearest)
		mlt_properties_set( self->properties, "rescale", "nearest" );
		mlt_properties_set( self->properties, "deinterlace_method", "onefield" );
		mlt_properties_set_int( self->properties, "top_field_first", -1 );

		// Default buffer for low latency
		mlt_properties_set_int( self->properties, "buffer", 1 );

		// Default audio buffer
		mlt_properties_set_int( self->properties, "audio_buffer", 2048 );

		// Default scrub audio
		mlt_properties_set_int( self->properties, "scrub_audio", 1 );

		// Ensure we don't join on a non-running object
		self->joined = 1;
		
		// process actual param
		if ( arg && sscanf( arg, "%dx%d", &self->width, &self->height ) )
		{
			mlt_properties_set_int( self->properties, "_arg_size", 1 );
		}
		else
		{
			self->width = mlt_properties_get_int( self->properties, "width" );
			self->height = mlt_properties_get_int( self->properties, "height" );
		}
	
		// Allow thread to be started/stopped
		parent->start = consumer_start;
		parent->stop = consumer_stop;
		parent->is_stopped = consumer_is_stopped;
		parent->purge = consumer_purge;

		// Register specific events
		//mlt_events_register( self->properties, "consumer-sdl-event", ( mlt_transmitter )consumer_sdl_event );

		// Return the consumer produced
		return parent;
	}

	// malloc or consumer init failed
	free( self );

	// Indicate failure
	return NULL;
}

static void consumer_android_event( mlt_listener listener, mlt_properties owner, mlt_service self, void **args )
{
	//if ( listener != NULL )
	//	listener( owner, self, ( SDL_Event * )args[ 0 ] );
}

int consumer_start( mlt_consumer parent )
{
	consumer_android self = parent->child;

	if ( !self->running )
	{
		mlt_properties properties = MLT_CONSUMER_PROPERTIES( parent );
		int video_off = mlt_properties_get_int( properties, "video_off" );
		int preview_off = mlt_properties_get_int( properties, "preview_off" );
		int display_off = video_off | preview_off;
		int audio_off = mlt_properties_get_int( properties, "audio_off" );
		int sdl_started = mlt_properties_get_int( properties, "sdl_started" );
		char *output_display = mlt_properties_get( properties, "output_display" );
		char *window_id = mlt_properties_get( properties, "window_id" );
		char *audio_driver = mlt_properties_get( properties, "audio_driver" );
		char *video_driver = mlt_properties_get( properties, "video_driver" );
		char *audio_device = mlt_properties_get( properties, "audio_device" );

		consumer_stop( parent );

		self->running = 1;
		self->joined = 0;


		if ( ! mlt_properties_get_int( self->properties, "_arg_size" ) )
		{
			if ( mlt_properties_get_int( self->properties, "width" ) > 0 )
				self->width = mlt_properties_get_int( self->properties, "width" );
			if ( mlt_properties_get_int( self->properties, "height" ) > 0 )
				self->height = mlt_properties_get_int( self->properties, "height" );
		}

		self->bpp = mlt_properties_get_int( self->properties, "bpp" );


		// Default window size
		if ( mlt_properties_get_int( self->properties, "_arg_size" ) )
		{
			self->window_width = self->width;
			self->window_height = self->height;
		}
		else
		{
			double display_ratio = mlt_properties_get_double( self->properties, "display_ratio" );
			self->window_width = ( double )self->height * display_ratio + 0.5;
			self->window_height = self->height;
		}

		pthread_create( &self->thread, NULL, consumer_thread, self );
	}

	return 0;
}

int consumer_stop( mlt_consumer parent )
{
	// Get the actual object
	consumer_android self = parent->child;

	if ( self->joined == 0 )
	{
		// Kill the thread and clean up
		self->joined = 1;
		self->running = 0;

		if ( !mlt_properties_get_int( MLT_CONSUMER_PROPERTIES( parent ), "audio_off" ) )
		{
			pthread_mutex_lock( &self->audio_mutex );
			pthread_cond_broadcast( &self->audio_cond );
			pthread_mutex_unlock( &self->audio_mutex );
		}

#ifndef _WIN32
		if ( self->thread )
#endif
			pthread_join( self->thread, NULL );

	}

	return 0;
}

int consumer_is_stopped( mlt_consumer parent )
{
	consumer_android self = parent->child;
	return !self->running;
}

void consumer_purge( mlt_consumer parent )
{
	consumer_android self = parent->child;
	if ( self->running )
	{
		pthread_mutex_lock( &self->video_mutex );
		while ( mlt_deque_count( self->queue ) )
			mlt_frame_close( mlt_deque_pop_back( self->queue ) );
		self->is_purge = 1;
		pthread_cond_broadcast( &self->video_cond );
		pthread_mutex_unlock( &self->video_mutex );
	}
}


static int consumer_play_video( consumer_android self, mlt_frame frame )
{
	// Get the properties of this consumer
	mlt_properties properties = self->properties;

	mlt_image_format vfmt = mlt_properties_get_int( properties, "mlt_image_format" );
	int width = self->width, height = self->height;
	uint8_t *image;
	int changed = 0;

	int video_off = mlt_properties_get_int( properties, "video_off" );
	int preview_off = mlt_properties_get_int( properties, "preview_off" );
	mlt_image_format preview_format = mlt_properties_get_int( properties, "preview_format" );
	int display_off = video_off | preview_off;
	
	if ( self->running )
	{	
		CONSUMER_RENDER call_back = NULL;
		mlt_image_format f;
		//int width = 1920;
		//int height = 1080;
		mlt_consumer_render_get(&call_back, &vfmt, &width, &height);
		if(call_back != NULL)
		{	
			// Get the image, width and height
			mlt_frame_get_image( frame, &image, &vfmt, &width, &height, 0 );
			if (image == NULL )
			{
				mlt_log_error( MLT_CONSUMER_SERVICE(self), "_do_render NULL!");
			}
			else
			{
				mlt_log_debug( MLT_CONSUMER_SERVICE(self), "_do_render data[%d][%d][%d]!", vfmt, width, height);
				int size = mlt_image_format_size(vfmt, width, height - 1, NULL );
				call_back(image, size, vfmt, width, height);	
			}			
		}	
		
		mlt_events_fire( properties, "consumer-frame-show", frame, NULL );
	}

	return 0;
}



static void *video_thread( void *arg )
{
	// Identify the arg
	consumer_android self = arg;

	// Obtain time of thread start
	struct timeval now;
	int64_t start = 0;
	int64_t elapsed = 0;
	struct timespec tm;
	mlt_frame next = NULL;
	mlt_properties properties = NULL;
	double speed = 0;

	// Get real time flag
	int real_time = mlt_properties_get_int( self->properties, "real_time" );

	// Get the current time
	gettimeofday( &now, NULL );

	// Determine start time
	start = ( int64_t )now.tv_sec * 1000000 + now.tv_usec;

	while ( self->running )
	{
		// Pop the next frame
		pthread_mutex_lock( &self->video_mutex );
		next = mlt_deque_pop_front( self->queue );
		while ( next == NULL && self->running )
		{
			pthread_cond_wait( &self->video_cond, &self->video_mutex );
			next = mlt_deque_pop_front( self->queue );
		}
		pthread_mutex_unlock( &self->video_mutex );

		if ( !self->running || next == NULL ) break;

		// Get the properties
		properties =  MLT_FRAME_PROPERTIES( next );

		// Get the speed of the frame
		speed = mlt_properties_get_double( properties, "_speed" );

		// Get the current time
		gettimeofday( &now, NULL );

		// Get the elapsed time
		elapsed = ( ( int64_t )now.tv_sec * 1000000 + now.tv_usec ) - start;

		// See if we have to delay the display of the current frame
		if ( mlt_properties_get_int( properties, "rendered" ) == 1 && self->running )
		{
			// Obtain the scheduled playout time
			int64_t scheduled = mlt_properties_get_int( properties, "playtime" );

			// Determine the difference between the elapsed time and the scheduled playout time
			int64_t difference = scheduled - elapsed;
			//mlt_log_error( MLT_CONSUMER_SERVICE(&self->parent), "######22 play time[%ld][%ld][%ld]\n", difference, scheduled, elapsed);
			// Smooth playback a bit
			if ( real_time && ( difference > 20000 && speed == 1.0 ) )
			{
				tm.tv_sec = difference / 1000000;
				tm.tv_nsec = ( difference % 1000000 ) * 500;
				nanosleep( &tm, NULL );
			}

			// Show current frame if not too old
			if ( !real_time || ( difference > -10000 || speed != 1.0 || mlt_deque_count( self->queue ) < 2 ) )
				consumer_play_video( self, next );

			// If the queue is empty, recalculate start to allow build up again
			if ( real_time && ( mlt_deque_count( self->queue ) == 0 && speed == 1.0 ) )
			{
				gettimeofday( &now, NULL );
				start = ( ( int64_t )now.tv_sec * 1000000 + now.tv_usec ) - scheduled + 20000;
			}
		}
		else
		{
			static int dropped = 0;
			mlt_log_info( MLT_CONSUMER_SERVICE(&self->parent), "dropped video frame %d\n", ++dropped );
		}

		// This frame can now be closed
		mlt_frame_close( next );
		next = NULL;
	}

	if ( next != NULL )
		mlt_frame_close( next );

	mlt_consumer_stopped( &self->parent );

	return NULL;
}

/** Threaded wrapper for pipe.
*/

static void *consumer_thread( void *arg )
{
	// Identify the arg
	consumer_android self = arg;

	// Get the consumer
	mlt_consumer consumer = &self->parent;

	// Convenience functionality
	int terminate_on_pause = mlt_properties_get_int( MLT_CONSUMER_PROPERTIES( consumer ), "terminate_on_pause" );
	int terminated = 0;

	// Video thread
	pthread_t thread;

	// internal initialization
	int init_audio = 1;
	int init_video = 1;
	mlt_frame frame = NULL;
	double duration = 0;
	int64_t playtime = 0;
	struct timespec tm = { 0, 100000 };
	self->playing = 1;  //应该放到音频播放里面启动
	// Loop until told not to
	while( self->running )
	{
		// Get a frame from the attached producer
		frame = !terminated? mlt_consumer_rt_frame( consumer ) : NULL;

		// Check for termination
		if ( terminate_on_pause && frame )
			terminated = mlt_properties_get_double( MLT_FRAME_PROPERTIES( frame ), "_speed" ) == 0.0;

		// Ensure that we have a frame
		if ( frame )
		{
			// Play audio
			//init_audio = consumer_play_audio( self, frame, init_audio, &duration );

			// Determine the start time now
			if ( self->playing && init_video )
			{
				// Create the video thread
				pthread_create( &thread, NULL, video_thread, self );

				// Video doesn't need to be initialised any more
				init_video = 0;
			}

			// Set playtime for this frame
			mlt_properties_set_int( MLT_FRAME_PROPERTIES( frame ), "playtime", playtime );

			while ( self->running && mlt_deque_count( self->queue ) > 15 )
				nanosleep( &tm, NULL );

			// Push this frame to the back of the queue
			pthread_mutex_lock( &self->video_mutex );
			if ( self->is_purge )
			{
				mlt_frame_close( frame );
				frame = NULL;
				self->is_purge = 0;
			}
			else
			{
				mlt_deque_push_back( self->queue, frame );
				pthread_cond_broadcast( &self->video_cond );
			}
			pthread_mutex_unlock( &self->video_mutex );

			// Calculate the next playtime
			//playtime += ( duration * 1000 );
			duration = 1000000.0 / mlt_properties_get_double( self->properties, "fps" );
			playtime += duration;
			//mlt_log_error( MLT_CONSUMER_SERVICE(&self->parent), "###### play time[%d][%ld]\n", (int)duration, playtime);
		}
		else if ( terminated )
		{
			if ( init_video || mlt_deque_count( self->queue ) == 0 )
				break;
			else
				nanosleep( &tm, NULL );
		}
	}

	self->running = 0;
	
	// Unblock sdl_preview
	if ( mlt_properties_get_int( MLT_CONSUMER_PROPERTIES( consumer ), "put_mode" ) &&
	     mlt_properties_get_int( MLT_CONSUMER_PROPERTIES( consumer ), "put_pending" ) )
	{
		frame = mlt_consumer_get_frame( consumer );
		if ( frame )
			mlt_frame_close( frame );
		frame = NULL;
	}

	// Kill the video thread
	if ( init_video == 0 )
	{
		pthread_mutex_lock( &self->video_mutex );
		pthread_cond_broadcast( &self->video_cond );
		pthread_mutex_unlock( &self->video_mutex );
		pthread_join( thread, NULL );
	}

	while( mlt_deque_count( self->queue ) )
		mlt_frame_close( mlt_deque_pop_back( self->queue ) );

	pthread_mutex_lock( &self->audio_mutex );
	self->audio_avail = 0;
	pthread_mutex_unlock( &self->audio_mutex );

	return NULL;
}

/** Callback to allow override of the close method.
*/

static void consumer_close( mlt_consumer parent )
{
	// Get the actual object
	consumer_android self = parent->child;

	// Stop the consumer
	///mlt_consumer_stop( parent );

	// Now clean up the rest
	mlt_consumer_close( parent );

	// Close the queue
	mlt_deque_close( self->queue );

	// Destroy mutexes
	pthread_mutex_destroy( &self->audio_mutex );
	pthread_cond_destroy( &self->audio_cond );
		
	// Finally clean up this
	free( self );
}
