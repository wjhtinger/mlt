/*
 * qgl_wrapper.h -- a Qt OpenGL based consumer for MLT
 * Copyright (C) 2011 Christophe Thommeret
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

#ifndef MLT_QGL_WRAPPER
#define MLT_QGL_WRAPPER

#ifdef __cplusplus
extern "C" {
#endif

#include <framework/mlt_consumer.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_deque.h>
#include <framework/mlt_factory.h>
#include <framework/mlt_filter.h>
#include <framework/mlt_log.h>
#include <framework/mlt.h>

#include <pthread.h>

#include "config.h"



typedef struct consumer_qgl_s *consumer_qgl;

struct consumer_qgl_s
{
	struct mlt_consumer_s parent;
	mlt_properties properties;
	mlt_deque queue;
	pthread_t thread;
	int joined;
	int running;
	int playing;
	int qgl_started;
};


extern int XInitThreads();
extern void start_qgl( consumer_qgl );

#ifdef __cplusplus
}
#endif

#endif
