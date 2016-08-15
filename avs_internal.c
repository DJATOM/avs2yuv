/*****************************************************************************
 * avs.c: avisynth input
 *****************************************************************************
 * Copyright (C) 2009-2011 x264 project
 *
 * Authors: Steven Walters <kemuri9@gmail.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

#include <windows.h>

#define AVSC_NO_DECLSPEC
#undef EXTERN_C
#include <avisynth_c.h>
#define AVSC_DECLARE_FUNC(name) name##_func name
#define SYS_WINDOWS 1

#define LOAD_AVS_FUNC(name, continue_on_fail)\
{\
    h->func.name = (name##_func)GetProcAddress(h->library, #name);\
    if(!continue_on_fail && !h->func.name)\
        goto fail;\
}

typedef struct
{
    AVS_Clip *clip;
    AVS_ScriptEnvironment *env;
    HMODULE library;
    struct
    {
        AVSC_DECLARE_FUNC(avs_add_function);
        AVSC_DECLARE_FUNC(avs_at_exit);
        AVSC_DECLARE_FUNC(avs_bit_blt);
        AVSC_DECLARE_FUNC(avs_check_version);
        AVSC_DECLARE_FUNC(avs_clip_get_error);
        AVSC_DECLARE_FUNC(avs_copy_clip);
        AVSC_DECLARE_FUNC(avs_copy_value);
        AVSC_DECLARE_FUNC(avs_copy_video_frame);
        AVSC_DECLARE_FUNC(avs_create_script_environment);
        AVSC_DECLARE_FUNC(avs_delete_script_environment);
        AVSC_DECLARE_FUNC(avs_function_exists);
        AVSC_DECLARE_FUNC(avs_get_audio);
        AVSC_DECLARE_FUNC(avs_get_cpu_flags);
        AVSC_DECLARE_FUNC(avs_get_frame);
        AVSC_DECLARE_FUNC(avs_get_parity);
        AVSC_DECLARE_FUNC(avs_get_var);
        AVSC_DECLARE_FUNC(avs_get_version);
        AVSC_DECLARE_FUNC(avs_get_video_info);
        AVSC_DECLARE_FUNC(avs_invoke);
        AVSC_DECLARE_FUNC(avs_make_writable);
        AVSC_DECLARE_FUNC(avs_new_c_filter);
        AVSC_DECLARE_FUNC(avs_new_video_frame_a);
        AVSC_DECLARE_FUNC(avs_release_clip);
        AVSC_DECLARE_FUNC(avs_release_value);
        AVSC_DECLARE_FUNC(avs_release_video_frame);
        AVSC_DECLARE_FUNC(avs_save_string);
        AVSC_DECLARE_FUNC(avs_set_cache_hints);
        AVSC_DECLARE_FUNC(avs_set_global_var);
        AVSC_DECLARE_FUNC(avs_set_memory_max);
        AVSC_DECLARE_FUNC(avs_set_to_clip);
        AVSC_DECLARE_FUNC(avs_set_var);
        AVSC_DECLARE_FUNC(avs_set_working_dir);
        AVSC_DECLARE_FUNC(avs_sprintf);
        AVSC_DECLARE_FUNC(avs_subframe);
        AVSC_DECLARE_FUNC(avs_subframe_planar);
        AVSC_DECLARE_FUNC(avs_take_clip);
        AVSC_DECLARE_FUNC(avs_vsprintf);

        AVSC_DECLARE_FUNC(avs_get_error);
        AVSC_DECLARE_FUNC(avs_is_rgb48);
        AVSC_DECLARE_FUNC(avs_is_rgb64);
        AVSC_DECLARE_FUNC(avs_is_yv24);
        AVSC_DECLARE_FUNC(avs_is_yv16);
        AVSC_DECLARE_FUNC(avs_is_yv12);
        AVSC_DECLARE_FUNC(avs_is_yv411);
        AVSC_DECLARE_FUNC(avs_is_y8);
        AVSC_DECLARE_FUNC(avs_is_yuv444p16);
        AVSC_DECLARE_FUNC(avs_is_yuv422p16);
        AVSC_DECLARE_FUNC(avs_is_yuv420p16);
        AVSC_DECLARE_FUNC(avs_is_y16);
        AVSC_DECLARE_FUNC(avs_is_yuv444ps);
        AVSC_DECLARE_FUNC(avs_is_yuv422ps);
        AVSC_DECLARE_FUNC(avs_is_yuv420ps);
        AVSC_DECLARE_FUNC(avs_is_y32);
        AVSC_DECLARE_FUNC(avs_is_444);
        AVSC_DECLARE_FUNC(avs_is_422);
        AVSC_DECLARE_FUNC(avs_is_420);
        AVSC_DECLARE_FUNC(avs_is_y);
        AVSC_DECLARE_FUNC(avs_is_yuva);
        AVSC_DECLARE_FUNC(avs_is_planar_rgb);
        AVSC_DECLARE_FUNC(avs_is_planar_rgba);
        AVSC_DECLARE_FUNC(avs_is_color_space);

        AVSC_DECLARE_FUNC(avs_get_plane_width_subsampling);
        AVSC_DECLARE_FUNC(avs_get_plane_height_subsampling);
        AVSC_DECLARE_FUNC(avs_bits_per_pixel);
        AVSC_DECLARE_FUNC(avs_bytes_from_pixels);
        AVSC_DECLARE_FUNC(avs_row_size);
        AVSC_DECLARE_FUNC(avs_bmp_size);
        AVSC_DECLARE_FUNC(avs_get_pitch_p);
        AVSC_DECLARE_FUNC(avs_get_row_size_p);
        AVSC_DECLARE_FUNC(avs_get_height_p);
        AVSC_DECLARE_FUNC(avs_get_read_ptr_p);
        AVSC_DECLARE_FUNC(avs_is_writable);
        AVSC_DECLARE_FUNC(avs_get_write_ptr_p);

        AVSC_DECLARE_FUNC(avs_num_components);
        AVSC_DECLARE_FUNC(avs_component_size);
        AVSC_DECLARE_FUNC(avs_bits_per_component);
    } func;
} avs_hnd_t;

/* load the library and functions we require from it */
static int internal_avs_load_library(avs_hnd_t *h)
{
    h->library = LoadLibrary("avisynth");
    if(!h->library)
        return -1;
    LOAD_AVS_FUNC(avs_add_function, 0);
    LOAD_AVS_FUNC(avs_at_exit, 0);
    LOAD_AVS_FUNC(avs_bit_blt, 0);
    LOAD_AVS_FUNC(avs_check_version, 0);
    LOAD_AVS_FUNC(avs_clip_get_error, 0);
    LOAD_AVS_FUNC(avs_copy_clip, 0);
    LOAD_AVS_FUNC(avs_copy_value, 0);
    LOAD_AVS_FUNC(avs_copy_video_frame, 0);
    LOAD_AVS_FUNC(avs_create_script_environment, 0);
    LOAD_AVS_FUNC(avs_delete_script_environment, 1);
    LOAD_AVS_FUNC(avs_function_exists, 0);
    LOAD_AVS_FUNC(avs_get_audio, 0);
    LOAD_AVS_FUNC(avs_get_cpu_flags, 0);
    LOAD_AVS_FUNC(avs_get_frame, 0);
    LOAD_AVS_FUNC(avs_get_parity, 0);
    LOAD_AVS_FUNC(avs_get_var, 0);
    LOAD_AVS_FUNC(avs_get_version, 0);
    LOAD_AVS_FUNC(avs_get_video_info, 0);
    LOAD_AVS_FUNC(avs_invoke, 0);
    LOAD_AVS_FUNC(avs_make_writable, 0);
    LOAD_AVS_FUNC(avs_new_c_filter, 0);
    LOAD_AVS_FUNC(avs_new_video_frame_a, 0);
    LOAD_AVS_FUNC(avs_release_clip, 0);
    LOAD_AVS_FUNC(avs_release_value, 0);
    LOAD_AVS_FUNC(avs_release_video_frame, 0);
    LOAD_AVS_FUNC(avs_save_string, 0);
    LOAD_AVS_FUNC(avs_set_cache_hints, 0);
    LOAD_AVS_FUNC(avs_set_global_var, 0);
    LOAD_AVS_FUNC(avs_set_memory_max, 0);
    LOAD_AVS_FUNC(avs_set_to_clip, 0);
    LOAD_AVS_FUNC(avs_set_var, 0);
    LOAD_AVS_FUNC(avs_set_working_dir, 0);
    LOAD_AVS_FUNC(avs_sprintf, 0);
    LOAD_AVS_FUNC(avs_subframe, 0);
    LOAD_AVS_FUNC(avs_subframe_planar, 0);
    LOAD_AVS_FUNC(avs_take_clip, 0);
    LOAD_AVS_FUNC(avs_vsprintf, 0);

    LOAD_AVS_FUNC(avs_get_error, 1);
    LOAD_AVS_FUNC(avs_is_rgb48, 0);
    LOAD_AVS_FUNC(avs_is_rgb64, 0);
    LOAD_AVS_FUNC(avs_is_yv24, 0);
    LOAD_AVS_FUNC(avs_is_yv16, 0);
    LOAD_AVS_FUNC(avs_is_yv12, 0);
    LOAD_AVS_FUNC(avs_is_yv411, 0);
    LOAD_AVS_FUNC(avs_is_y8, 0);
    LOAD_AVS_FUNC(avs_is_yuv444p16, 0);
    LOAD_AVS_FUNC(avs_is_yuv422p16, 0);
    LOAD_AVS_FUNC(avs_is_yuv420p16, 0);
    LOAD_AVS_FUNC(avs_is_y16, 0);
    LOAD_AVS_FUNC(avs_is_yuv444ps, 0);
    LOAD_AVS_FUNC(avs_is_yuv422ps, 0);
    LOAD_AVS_FUNC(avs_is_yuv420ps, 0);
    LOAD_AVS_FUNC(avs_is_y32, 0);
    LOAD_AVS_FUNC(avs_is_444, 0);
    LOAD_AVS_FUNC(avs_is_422, 0);
    LOAD_AVS_FUNC(avs_is_420, 0);
    LOAD_AVS_FUNC(avs_is_y, 0);
    LOAD_AVS_FUNC(avs_is_yuva, 0);
    LOAD_AVS_FUNC(avs_is_planar_rgb, 0);
    LOAD_AVS_FUNC(avs_is_planar_rgba, 0);
    LOAD_AVS_FUNC(avs_is_color_space, 0);

    LOAD_AVS_FUNC(avs_get_plane_width_subsampling, 0);
    LOAD_AVS_FUNC(avs_get_plane_height_subsampling, 0);
    LOAD_AVS_FUNC(avs_bits_per_pixel, 0);
    LOAD_AVS_FUNC(avs_bytes_from_pixels, 0);
    LOAD_AVS_FUNC(avs_row_size, 0);
    LOAD_AVS_FUNC(avs_bmp_size, 0);
    LOAD_AVS_FUNC(avs_get_pitch_p, 0);
    LOAD_AVS_FUNC(avs_get_row_size_p, 0);
    LOAD_AVS_FUNC(avs_get_height_p, 0);
    LOAD_AVS_FUNC(avs_get_read_ptr_p, 0);
    LOAD_AVS_FUNC(avs_is_writable, 0);
    LOAD_AVS_FUNC(avs_get_write_ptr_p, 0);

    LOAD_AVS_FUNC(avs_num_components, 0);
    LOAD_AVS_FUNC(avs_component_size, 0);
    LOAD_AVS_FUNC(avs_bits_per_component, 0);
    return 0;
fail:
    FreeLibrary(h->library);
    return -1;
}

static AVS_Value internal_avs_update_clip(avs_hnd_t *h, const AVS_VideoInfo **vi, AVS_Value res, AVS_Value release)
{
    h->func.avs_release_clip(h->clip);
    h->clip = h->func.avs_take_clip(res, h->env);
    h->func.avs_release_value(release);
    *vi = h->func.avs_get_video_info(h->clip);
    return res;
}

static int internal_avs_close_library(avs_hnd_t *h)
{
    h->func.avs_release_clip(h->clip);
    if(h->func.avs_delete_script_environment)
       h->func.avs_delete_script_environment(h->env);
    FreeLibrary(h->library);
    return 0;
}
