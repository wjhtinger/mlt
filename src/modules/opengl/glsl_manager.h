/*
 * glsl_manager.h
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

#ifndef GLSL_MANAGER_H
#define GLSL_MANAGER_H

#include <GL/glew.h>
#include <framework/mlt_producer.h>
#include <framework/mlt_filter.h>
#include <framework/mlt_frame.h>

class Effect;

int mlt_glsl_init_movit( mlt_producer producer );
Effect* mlt_glsl_get_effect( mlt_filter filter, mlt_frame frame );
Effect* mlt_glsl_add_effect( mlt_filter filter, mlt_frame frame, Effect* a_effect );
void mlt_glsl_render_fbo( mlt_producer producer, void *chain, GLuint fbo, int width, int height );

#endif // GLSL_MANAGER_H
