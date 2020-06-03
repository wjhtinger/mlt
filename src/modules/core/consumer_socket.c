/*
 * consumer_null.c -- a null consumer
 * Copyright (C) 2003-2014 Meltytech, LLC
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

// mlt Header files
#include <framework/mlt_consumer.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_log.h>

// System header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <errno.h>




// Forward references.
static int consumer_start( mlt_consumer consumer );
static int consumer_stop( mlt_consumer consumer );
static int consumer_is_stopped( mlt_consumer consumer );
static void *consumer_thread( void *arg );
static void consumer_close( mlt_consumer consumer );
static void _socket_init(mlt_consumer consumer);
static void _socket_term(mlt_consumer consumer);
static void _socket_send(mlt_consumer consumer, mlt_frame frame);




/** Initialise the dv consumer.
*/

mlt_consumer consumer_socket_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	// Allocate the consumer
	mlt_consumer consumer = mlt_consumer_new( profile );

	// If memory allocated and initialises without error
	if ( consumer != NULL )
	{
		// Assign close callback
		consumer->close = consumer_close;

		// Set up start/stop/terminated callbacks
		consumer->start = consumer_start;
		consumer->stop = consumer_stop;
		consumer->is_stopped = consumer_is_stopped;
	}

	// Return consumer
	return consumer;
}

/** Start the consumer.
*/

static int consumer_start( mlt_consumer consumer )
{
	// Get the properties
	mlt_properties properties = MLT_CONSUMER_PROPERTIES( consumer );

	// Check that we're not already running
	if ( !mlt_properties_get_int( properties, "running" ) )
	{
		// Allocate a thread
		pthread_t *thread = calloc( 1, sizeof( pthread_t ) );

		// Assign the thread to properties
		mlt_properties_set_data( properties, "thread", thread, sizeof( pthread_t ), free, NULL );

		// Set the running state
		mlt_properties_set_int( properties, "running", 1 );
		mlt_properties_set_int( properties, "joined", 0 );

		// Create the thread
		pthread_create( thread, NULL, consumer_thread, consumer );

		_socket_init(consumer);
	}
	return 0;
}

/** Stop the consumer.
*/

static int consumer_stop( mlt_consumer consumer )
{
	// Get the properties
	mlt_properties properties = MLT_CONSUMER_PROPERTIES( consumer );

	// Check that we're running
	if ( !mlt_properties_get_int( properties, "joined" ) )
	{
		// Get the thread
		pthread_t *thread = mlt_properties_get_data( properties, "thread", NULL );

		// Stop the thread
		mlt_properties_set_int( properties, "running", 0 );
		mlt_properties_set_int( properties, "joined", 1 );

		// Wait for termination
		if ( thread )
			pthread_join( *thread, NULL );

		_socket_term(consumer);
	}

	return 0;
}

/** Determine if the consumer is stopped.
*/

static int consumer_is_stopped( mlt_consumer consumer )
{
	// Get the properties
	mlt_properties properties = MLT_CONSUMER_PROPERTIES( consumer );
	return !mlt_properties_get_int( properties, "running" );
}

/** The main thread - the argument is simply the consumer.
*/

static void *consumer_thread( void *arg )
{
	// Map the argument to the object
	mlt_consumer consumer = arg;

	// Get the properties
	mlt_properties properties = MLT_CONSUMER_PROPERTIES( consumer );

	// Convenience functionality
	int terminate_on_pause = mlt_properties_get_int( properties, "terminate_on_pause" );
	int terminated = 0;

	// Frame and size
	mlt_frame frame = NULL;

	// Loop while running
	while( !terminated && mlt_properties_get_int( properties, "running" ) )
	{
		// Get the frame
		frame = mlt_consumer_rt_frame( consumer );

		// Check for termination
		if ( terminate_on_pause && frame != NULL )
			terminated = mlt_properties_get_double( MLT_FRAME_PROPERTIES( frame ), "_speed" ) == 0.0;

		// Check that we have a frame to work with
		if ( frame != NULL )
		{
			_socket_send(consumer, frame);
			// Close the frame
			mlt_events_fire( properties, "consumer-frame-show", frame, NULL );
			mlt_frame_close( frame );
		}
	}

	// Indicate that the consumer is stopped
	mlt_properties_set_int( properties, "running", 0 );
	mlt_consumer_stopped( consumer );

	return NULL;
}

/** Close the consumer.
*/

static void consumer_close( mlt_consumer consumer )
{
	// Stop the consumer
	mlt_consumer_stop( consumer );

	// Close the parent
	mlt_consumer_close( consumer );

	// Free the memory
	free( consumer );
}



#define CLI_PATH "/var/tmp/"
int fd;


#if 1
static void _socket_init(mlt_consumer consumer)
{
	int len, err, rval;
	struct sockaddr_un un;
	char *name = "mlt.socket";
		
	
	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{		
		mlt_log_error( MLT_CONSUMER_SERVICE(consumer), "_socket_init socket err!");
		return;
	}

	/*
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	sprintf(un.sun_path, "%s%05d", CLI_PATH, getpid());
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	unlink(un.sun_path); 
	if (bind(fd, (struct sockaddr *)&un, len) < 0) {
		mlt_log_error( MLT_CONSUMER_SERVICE(consumer), "_socket_init band err!");
		close(fd);
		return;
	}
	*/

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	un.sun_path[0] = '\0';
	strcpy(un.sun_path + 1, name);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(name) + 1;
	//len = sizeof(un);
	rval = connect(fd, (struct sockaddr *)&un, len);
	if (rval < 0) 
	{
		mlt_log_error( MLT_CONSUMER_SERVICE(consumer), "_socket_init connect err[%d][%s]!", errno, strerror(errno));
		close(fd);
		return;
	}
}

static void _socket_term(mlt_consumer consumer)
{
	close(fd);
}

static void _socket_send(mlt_consumer consumer, mlt_frame frame)
{
	mlt_image_format vfmt = mlt_image_rgb24a;
	int width = 1920;
	int	height = 1080;
	uint8_t *image;
	

	mlt_frame_get_image(frame, &image, &vfmt, &width, &height, 0);

	if (image != NULL )
	{
		int size = mlt_image_format_size(vfmt, width, height - 1, NULL );
		int send = write(fd, image, size);		
		mlt_log_error( MLT_CONSUMER_SERVICE(consumer), "_socket_send data[%d][%d]!", send, size);
	}	
}
#else if



static void _socket_init(mlt_consumer consumer)
{
	struct sockaddr_in server;
	int ns;
	char *name = "mlt.socket";
		
	
    fd = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_port = htons(4700);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        mlt_log_error( MLT_CONSUMER_SERVICE(consumer), "_socket_init band err!");
        return;
    }

}

static void _socket_term(mlt_consumer consumer)
{
	close(fd);
}

static void _socket_send(mlt_consumer consumer, mlt_frame frame)
{
	mlt_image_format vfmt = mlt_image_rgb24;
	int width = 1920;
	int	height = 1080;
	uint8_t *image;
	

	mlt_frame_get_image(frame, &image, &vfmt, &width, &height, 0);

	if (image != NULL )
	{
		int size = mlt_image_format_size(vfmt, width, height - 1, NULL );
		send(fd, image, size, 0);
	}	
}

#endif
