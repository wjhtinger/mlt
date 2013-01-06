/*
 * factory.c -- the factory method interfaces
 * Copyright (C) 2006 Visual Media
 * Author: Charles Yates <charles.yates@gmail.com>
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

#include <string.h>
#include <limits.h>
#include <framework/mlt.h>



extern mlt_consumer consumer_xgl_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );

extern mlt_filter filter_glsl_manager_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_greyscale_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_csc_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_crop_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_saturation_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_resize_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_fieldorder_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_rescale_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_deinterlace_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_brightness_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_contrast_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_glsl_gamma_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_filter filter_movit_blur_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );

extern mlt_transition transition_glsl_luma_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );

static mlt_properties metadata( mlt_service_type type, const char *id, void *data )
{
	char file[ PATH_MAX ];
	snprintf( file, PATH_MAX, "%s/opengl/%s", mlt_environment( "MLT_DATA" ), (char*) data );
	return mlt_properties_parse_yaml( file );
}

MLT_REPOSITORY
{
#if !defined(__DARWIN__) && !defined(WIN32)
	MLT_REGISTER( consumer_type, "xgl", consumer_xgl_init );
#endif
	MLT_REGISTER( filter_type, "glsl.manager", filter_glsl_manager_init );
	MLT_REGISTER( filter_type, "glsl.greyscale", filter_glsl_greyscale_init );
	MLT_REGISTER( filter_type, "glsl.csc", filter_glsl_csc_init );
	MLT_REGISTER( filter_type, "glsl.crop", filter_glsl_crop_init );
	MLT_REGISTER( filter_type, "glsl.saturation", filter_glsl_saturation_init );
	MLT_REGISTER( filter_type, "glsl.resize", filter_glsl_resize_init );
	MLT_REGISTER( filter_type, "glsl.fieldorder", filter_glsl_fieldorder_init );
	MLT_REGISTER( filter_type, "glsl.rescale", filter_glsl_rescale_init );
	MLT_REGISTER( filter_type, "glsl.deinterlace", filter_glsl_deinterlace_init );
	MLT_REGISTER( filter_type, "glsl.brightness", filter_glsl_brightness_init );
	MLT_REGISTER( filter_type, "glsl.contrast", filter_glsl_contrast_init );
	MLT_REGISTER( filter_type, "glsl.gamma", filter_glsl_gamma_init );
	MLT_REGISTER( filter_type, "movit.blur", filter_movit_blur_init );
	MLT_REGISTER( transition_type, "glsl.luma", transition_glsl_luma_init );
	
	MLT_REGISTER_METADATA( filter_type, "glsl.greyscale", metadata, "filter_glsl_greyscale.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.csc", metadata, "filter_glsl_csc.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.crop", metadata, "filter_glsl_crop.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.saturation", metadata, "filter_glsl_saturation.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.resize", metadata, "filter_glsl_resize.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.fieldorder", metadata, "filter_glsl_fieldorder.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.rescale", metadata, "filter_glsl_rescale.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.deinterlace", metadata, "filter_glsl_deinterlace.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.brightness", metadata, "filter_glsl_brightness.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.contrast", metadata, "filter_glsl_contrast.yml" );
	MLT_REGISTER_METADATA( filter_type, "glsl.gamma", metadata, "filter_glsl_gamma.yml" );
	MLT_REGISTER_METADATA( transition_type, "glsl.luma", metadata, "transition_glsl_luma.yml" );
}
