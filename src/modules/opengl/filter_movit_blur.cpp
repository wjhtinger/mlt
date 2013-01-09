/*
 * filter_movit_blur.cpp
 * Copyright (C) 2013 Dan Dennedy <dan@dennedy.org>
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

#include <framework/mlt.h>
#include <string.h>
#include <assert.h>

#include "mlt_glsl.h"
#include "movit/init.h"
#include "movit/effect_chain.h"
#include "movit/flat_input.h"
#include "movit/blur_effect.h"

// Flip upside-down to compensate for different origin.
template<class T>
static void vertical_flip(T *data, unsigned width, unsigned height)
{
	for (unsigned y = 0; y < height / 2; ++y) {
		unsigned flip_y = height - y - 1;
		for (unsigned x = 0; x < width; ++x) {
			std::swap(data[y * width + x], data[flip_y * width + x]);
		}
	}
}

// TODO: move this into a colorspace conversion mlt_filter
static int get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	*format = mlt_image_rgb24a;
	int error = mlt_frame_get_image( frame, image, format, width, height, writable );
	if ( !error )
	{
		glsl_env glsl = mlt_glsl_get( mlt_service_profile( MLT_PRODUCER_SERVICE( mlt_frame_get_original_producer( frame ) ) ) );
		if ( glsl )
		{
			FlatInput* input = (FlatInput*) glsl->movitInput;
			input->set_pixel_data( *image );
			int rgba_size = mlt_image_format_size( *format, *width, *height, NULL );
			glsl_texture tex = glsl->get_texture( glsl, *width, *height, GL_RGBA );
			glsl_fbo fbo = glsl->get_fbo( glsl, *width, *height );
			// Use a PBO to hold the data we read back with glReadPixels()
			// (Intel/DRI goes into a slow path if we don't read to PBO)
			glsl_pbo pbo = glsl->get_pbo( glsl, rgba_size );

			if ( tex && fbo && pbo )
			{
				EffectChain* chain = (EffectChain*) glsl->movitChain;

				// Set the FBO
				glBindFramebuffer( GL_FRAMEBUFFER, fbo->fbo );
				check_error();
				glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, tex->texture, 0 );
				check_error();
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );
				check_error();

				mlt_glsl_render_fbo( glsl, fbo->fbo, *width, *height );

				// Read FBO into PBO
				glBindBuffer( GL_PIXEL_PACK_BUFFER_ARB, pbo->pbo );
				check_error();
				glBufferData( GL_PIXEL_PACK_BUFFER_ARB, rgba_size, NULL, GL_STREAM_READ );
				check_error();
				glReadPixels( 0, 0, *width, *height, GL_RGBA, GL_UNSIGNED_BYTE, BUFFER_OFFSET(0) );
				check_error();

				// Copy from PBO
				uint8_t* buf = (uint8_t*) glMapBuffer( GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY );
				check_error();
				*image = (uint8_t*) mlt_pool_alloc( rgba_size );
				mlt_frame_set_image( frame, *image, rgba_size, mlt_pool_release );
				memcpy( *image, buf, rgba_size );
				// TODO: add and use a GLSL vertical flip effect
				vertical_flip( (uint32_t*) *image, *width, *height );

				// Release PBO and FBO
				glUnmapBuffer( GL_PIXEL_PACK_BUFFER_ARB );
				check_error();
				glBindBuffer( GL_PIXEL_PACK_BUFFER_ARB, 0 );
				check_error();
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );
				check_error();
			}
			if ( tex ) glsl->release_texture( tex );
			if ( fbo ) glsl->release_fbo( fbo );
		}
	}
	return error;
}

static mlt_frame process( mlt_filter filter, mlt_frame frame )
{
	mlt_frame_push_get_image( frame, get_image );
	mlt_properties properties = MLT_FILTER_PROPERTIES( filter );
	Effect* effect = (Effect*) mlt_properties_get_data( properties, "effect", NULL );
	effect->set_float( "radius", mlt_properties_get_double( properties, "radius" ) );
	return frame;
}

static void deleteInput( void *o )
{
	delete (Input*) o;
}

static void deleteEffect( void *o )
{
	delete (Effect*) o;
}

// TODO: move this into movit/util.cpp and invoke only from colorspace mlt_filter
static bool setupMovit( mlt_properties properties, glsl_env glsl, mlt_profile profile )
{
	if ( !glsl->movitChain )
	{
		ImageFormat inout_format;
		inout_format.color_space = COLORSPACE_sRGB;
		inout_format.gamma_curve = GAMMA_sRGB;
		FlatInput* input = new FlatInput( inout_format, FORMAT_RGBA, GL_UNSIGNED_BYTE, profile->width, profile->height );
		EffectChain *chain = new EffectChain( mlt_profile_dar( profile ), 1.0f );
		glsl->movitChain = chain;
		glsl->movitInput = input;
		chain->add_input( input );
		chain->add_output( inout_format );
		mlt_properties_set_data( properties, "input", input, 0, deleteInput, NULL );
	}
	return true;
}

extern "C" {

mlt_filter filter_movit_blur_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	mlt_filter filter = NULL;
	glsl_env glsl = mlt_glsl_get( profile );

	if ( glsl && ( filter = mlt_filter_new() ) )
	{
		mlt_properties properties = MLT_FILTER_PROPERTIES( filter );
		if ( setupMovit( properties, glsl, profile ) )
		{
			EffectChain* chain = (EffectChain*) glsl->movitChain;
			Effect* effect = chain->add_effect( new BlurEffect() );

			mlt_properties_set_data( properties, "effect", effect, 0, deleteEffect, NULL );
			effect->set_int( "width", profile->width );
			effect->set_int( "height", profile->height );
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
