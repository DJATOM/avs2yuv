// Avs2YUV by Loren Merritt

// Contributors: Anton Mitrofanov (aka BugMaster)
//               Oka Motofumi (aka Chikuzen)
//               Vladimir Kontserenko (aka DJATOM)

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "avs_internal.c"

#if defined(AVS_POSIX)
#include <unistd.h>
#if defined(AVS_MACOS)
#define AVS_LIBNAME "libavisynth.dylib"
#else
#define AVS_LIBNAME "libavisynth.so"
#endif
#else
#include <io.h>
#define fileno _fileno
#define dup _dup
#define fdopen _fdopen
#if defined(_MSC_VER)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define snprintf _snprintf
typedef signed __int64 int64_t;
// MSVC specifically lacks clock_gettime.
#define OLD_TIME_BEHAVIOR
#endif
#define AVS_LIBNAME "avisynth.dll"
#endif

#if defined(AVS_MACOS)
#if defined(PPC32)
// AviSynth+ can be built/run natively on OS X 10.4 and 10.5,
//  which are the last two versions that can be used on PPC.
// clock_gettime() support was introduced for macOS in 10.12,
// so we definitely need ftime if we're on a PPC version of OSX.
// Intel users between 10.6 and 10.11 will have to force it
// by passing CFLAGS during make.
#define OLD_TIME_BEHAVIOR
#endif
#endif

#ifdef OLD_TIME_BEHAVIOR
#include <sys/timeb.h>
#endif

#if !defined(INT_MAX)
#define INT_MAX 0x7fffffff
#endif

#define MY_VERSION "Avs2YUV 0.30"
#define AUTHORS "Writen by Loren Merritt, modified by BugMaster, Chikuzen\nand currently maintained by DJATOM"

#define MAX_FH 10
#define AVS_BUFSIZE (128*1024)

static volatile int b_ctrl_c = 0;

void sigintHandler(int sig_num)
{
    exit(0);
    b_ctrl_c = 1;
}

int64_t avs2yuv_mdate(void)
{
#ifdef OLD_TIME_BEHAVIOR
    struct timeb tb;
    ftime(&tb);
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
#else
    struct timespec tb;
    clock_gettime(CLOCK_MONOTONIC, &tb);
    return ((int64_t)tb.tv_sec * 1000 + (int64_t)tb.tv_nsec / 1000000) * 1000;
#endif
}

int main(int argc, const char* argv[])
{
    const char* infile = NULL;
    const char* outfile[MAX_FH] = {NULL};
    int y4m_headers[MAX_FH] = {0};
    FILE* out_fh[10] = {NULL};
    int out_fhs = 0;
    int nostderr = 0;
    int usage = 0;
    int seek = 0;
    int end = 0;
    int slave = 0;
    int raw_output = 0;
    int interlaced = 0;
    int tff = 0;
    int input_depth = 8;
    int input_width;
    int input_height;
    unsigned fps_num = 0;
    unsigned fps_den = 0;
    unsigned par_width = 0;
    unsigned par_height = 0;
    int i_frame = 0;
    int64_t i_frame_total = 0;
    int64_t i_start = 0;
    #if defined(AVS_WINDOWS)
    SetConsoleTitle("avs2yuv: preparing frameserver");
    #endif
    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] != 0) {
            if(!strcmp(argv[i], "-h"))
                usage = 1;
            else if(!strcmp(argv[i], "-nstdr"))
                nostderr = 1;
            else if(!strcmp(argv[i], "-o")) {
                if(i > argc-2) {
                    fprintf(stderr, "Error: -o needs an argument.\n");
                    return 2;
                }
                i++;
                goto add_outfile;
            } else if(!strcmp(argv[i], "-seek")) {
                if(i > argc-2) {
                    fprintf(stderr, "Error: -seek needs an argument.\n");
                    return 2;
                }
                seek = atoi(argv[++i]);
                if(seek < 0) usage = 1;
            } else if(!strcmp(argv[i], "-frames")) {
                if(i > argc-2) {
                    fprintf(stderr, "Error: -frames needs an argument.\n");
                    return 2;
                }
                end = atoi(argv[++i]);
            } else if(!strcmp(argv[i], "-raw")) {
                raw_output = 1;
                if (!nostderr) {
                    fprintf(stderr, "Warning: output will not contain any headers!\nYou will have to point resolution, framerate and format (and duration) manually to your reading software.\n");
                }
            } else if(!strcmp(argv[i], "-slave")) {
                slave = 1;
            } else if(!strcmp(argv[i], "-depth")) {
                if(i > argc-2) {
                    fprintf(stderr, "Error: -depth needs an argument.\n");
                    return 2;
                }
                input_depth = atoi(argv[++i]);
                if(input_depth < 8 || input_depth > 16) {
                    fprintf(stderr, "Error: -depth \"%s\" is not supported.\n", argv[i]);
                    return 2;
                }
            } else if(!strcmp(argv[i], "-fps")) {
                if(i > argc-2) {
                    fprintf(stderr, "Error: -fps needs an argument.\n");
                    return 2;
                }
                const char *str = argv[++i];
                if(sscanf(str, "%u/%u", &fps_num, &fps_den) != 2) {
                    char *str_end;
                    double fps = strtod(str, &str_end);
                    if(str_end == str || *str_end != '\0' || fps <= 0 || fps > INT_MAX)
                        fps_num = 0;
                    else if(fps <= INT_MAX/1000) {
                        fps_num = (unsigned)(fps * 1000 + .5);
                        fps_den = 1000;
                    } else {
                        fps_num = (unsigned)(fps + .5);
                        fps_den = 1;
                    }
                }
                if(!fps_num || !fps_den) {
                    fprintf(stderr, "Error: -fps \"%s\" is not supported.\n", str);
                    return 2;
                }
            } else if(!strcmp(argv[i], "-par")) {
                if(i > argc-2) {
                    fprintf(stderr, "Error: -par needs an argument.\n");
                    return 2;
                }
                const char *str = argv[++i];
                if( (sscanf(str, "%u:%u", &par_width, &par_height) != 2) &&
                    (sscanf(str, "%u/%u", &par_width, &par_height) != 2) )
                {
                    fprintf(stderr, "Error: -par \"%s\" is not supported.\n", str);
                    return 2;
                }
            } else {
                fprintf(stderr, "Error: no such option \"%s\".\n", argv[i]);
                return 2;
            }
        } else if(!infile) {
            infile = argv[i];
            const char *dot = strrchr(infile, '.');
            if(!dot || strcmp(".avs", dot))
                fprintf(stderr, "Error: infile \"%s\" doesn't look like an avisynth script.\n", infile);
        } else {
add_outfile:
            if(out_fhs > MAX_FH-1) {
                fprintf(stderr, "Error: too many output files.\n");
                return 2;
            }
            outfile[out_fhs] = argv[i];
            y4m_headers[out_fhs] = !raw_output;
            out_fhs++;
        }
    }
    if(usage || !infile || (!out_fhs && !nostderr)) {
        fprintf(stderr, MY_VERSION "\n"AUTHORS "\n"
        "Usage: avs2yuv [options] in.avs [-o out.y4m] [-o out2.y4m]\n"
        "-nstdr\tdo not print info to stderr\n"
        "-seek\tseek to the given frame number\n"
        "-frames\tstop after processing this many frames\n"
        "-slave\tinit script and do nothing\n\t(useful for piping from TCPDeliver to AvsNetPipe)\n"
        "-raw\toutput raw data\n"
        "-depth\tspecify input bit depth\n\t(default 8, trying to guess from the script)\n"
        "-fps\toverwrite input framerate\n"
        "-par\tspecify pixel aspect ratio\n"
        "The outfile may be \"-\", meaning stdout.\n"
        "Output format is yuv4mpeg, as used by MPlayer, FFmpeg, Libav, x264, mjpegtools.\n"
        );
        return 2;
    }
    int retval = 1;
    avs_hnd_t avs_h = {0};
    if(internal_avs_load_library(&avs_h) < 0) {
        fprintf(stderr, "Error: failed to load %s.\n", AVS_LIBNAME);
        goto fail;
    }
    avs_h.env = avs_h.func.avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    if(avs_h.func.avs_get_error) {
        const char *error = avs_h.func.avs_get_error(avs_h.env);
        if(error) {
            fprintf(stderr, "Error: %s.\n", error);
            goto fail;
        }
    }
    AVS_Value arg = avs_new_value_string(infile);
    AVS_Value res = avs_h.func.avs_invoke(avs_h.env, "Import", arg, NULL);
    if(avs_is_error(res)) {
        fprintf(stderr, "Error: %s.\n", avs_as_string(res));
        goto fail;
    }
    if(!avs_is_clip(res)) {
        fprintf(stderr, "Error: \"%s\" didn't return a video clip.\n", infile);
        goto fail;
    }
    avs_h.clip = avs_h.func.avs_take_clip(res, avs_h.env);
    const AVS_VideoInfo *inf = avs_h.func.avs_get_video_info(avs_h.clip);
    if(!avs_has_video(inf)) {
        fprintf(stderr, "Error: \"%s\" has no video data.\n", infile);
        goto fail;
    }
    if(!avs_h.func.avs_component_size(inf)) {
        fprintf(stderr, "Error: this program only works with Avisynth+.\n");
        goto fail;
    }
    /* if the clip is made of fields instead of frames, call weave to make them frames */
    if(avs_is_field_based(inf)) {
        fprintf(stderr, "Detected fieldbased (separated) input, weaving to frames.\n");
        AVS_Value tmp = avs_h.func.avs_invoke(avs_h.env, "Weave", res, NULL);
        if(avs_is_error(tmp)) {
            fprintf(stderr, "Error: couldn't weave fields into frames.\n");
            goto fail;
        }
        res = internal_avs_update_clip(&avs_h, &inf, tmp, res);
        interlaced = 1;
        tff = avs_is_tff(inf);
    }
    if(!nostderr)
        fprintf(stderr, "%s\n", MY_VERSION);
    input_width  = inf->width;
    input_height = inf->height;
    if(input_depth > 8 && (avs_h.func.avs_is_yv12(inf) || avs_h.func.avs_is_yv16(inf) || avs_h.func.avs_is_yv24(inf))) { //ignore native hbd cs
        if(input_width & 3) {
            if(!nostderr)
                fprintf(stderr, "Error: avisynth %d-bit hack requires that width is at least mod4.\n", input_depth);
            goto fail;
        }
        if(!nostderr)
            fprintf(stderr, "Avisynth %d-bit hack enabled!\n", input_depth);
        if(avs_h.func.avs_component_size(inf) == 1)
            input_width >>= 1;
    }
    if(!fps_num || !fps_den) {
        fps_num = inf->fps_numerator;
        fps_den = inf->fps_denominator;
    }
    if(!nostderr) {
        fprintf(stderr, "Script file:\t%s\nResolution:\t%dx%d\n", infile, input_width, input_height);
        if(fps_den == 1)
            fprintf(stderr, "Frames per sec:\t%u\n", fps_num);
        else
            fprintf(stderr, "Frames per sec:\t%u/%u (%.3f)\n", fps_num, fps_den, (float) fps_num/fps_den);
        fprintf(stderr, "Total frames:\t%d\n", inf->num_frames);
    }
    signal(SIGINT, sigintHandler);
    //start processing
    i_start = avs2yuv_mdate();
    time_t tm_s = time(0);
    i_frame_total = inf->num_frames;
    if(b_ctrl_c)
        goto close_files;
    avs_h.func.avs_release_value(res);
    for(int i = 0; i < out_fhs; i++) {
        if(!strcmp(outfile[i], "-")) {
            for(int j = 0; j < i; j++)
                if(out_fh[j] == stdout) {
                    fprintf(stderr, "Error: can't write to stdout multiple times.\n");
                    goto fail;
                }
            int dupout = dup(fileno(stdout));
            fclose(stdout);
            #if defined(AVS_WINDOWS)
            _setmode(dupout, _O_BINARY);
            #endif
            out_fh[i] = fdopen(dupout, "wb");
        } else {
            out_fh[i] = fopen(outfile[i], "wb");
            if(!out_fh[i]) {
                fprintf(stderr, "Error: failed to create/open \"%s\".\n", outfile[i]);
                goto fail;
            }
        }
    }
    char *interlace_type = interlaced ? tff ? "t" : "b" : "p";
    char csp_type[200];
    int chroma_h_shift = 0;
    int chroma_v_shift = 0;
    if(avs_h.func.avs_is_y(inf)) {
        if(input_depth == 16)
            sprintf(csp_type, "Cmono16 XYSCSS=Cmono16");
        else if(avs_h.func.avs_bits_per_component(inf) == 16)
            sprintf(csp_type, "Cmono16 XYSCSS=Cmono16");
        else if((avs_h.func.avs_bits_per_component(inf) > 8 && avs_h.func.avs_bits_per_component(inf) < 16) || (input_depth > 8 && input_depth < 16)) {
            fprintf(stderr, "Warning: output bit-depth is forced to 16 (was %d)!\n", avs_h.func.avs_bits_per_component(inf));
            sprintf(csp_type, "Cmono16 XYSCSS=Cmono16");
        } else
            sprintf(csp_type, "Cmono");
    } else if(avs_h.func.avs_is_420(inf)) {
        chroma_h_shift = 1;
        chroma_v_shift = 1;
        if(input_depth > 8)
            sprintf(csp_type, "C420p%d XYSCSS=C420p%d", input_depth, input_depth);
        else if(avs_h.func.avs_bits_per_component(inf) > 8)
            sprintf(csp_type, "C420p%d XYSCSS=C420p%d", avs_h.func.avs_bits_per_component(inf), avs_h.func.avs_bits_per_component(inf));
        else
            sprintf(csp_type, "C420mpeg2");
    } else if(avs_h.func.avs_is_422(inf)) {
        chroma_h_shift = 1;
        if(input_depth > 8)
            sprintf(csp_type, "C422p%d XYSCSS=C422p%d", input_depth, input_depth);
        else if(avs_h.func.avs_bits_per_component(inf) > 8)
            sprintf(csp_type, "C422p%d XYSCSS=C422p%d", avs_h.func.avs_bits_per_component(inf), avs_h.func.avs_bits_per_component(inf));
        else
            sprintf(csp_type, "C422");
    } else if(avs_h.func.avs_is_444(inf)) {
        if(input_depth > 8)
            sprintf(csp_type, "C444p%d XYSCSS=C444p%d", input_depth, input_depth);
        else if(avs_h.func.avs_bits_per_component(inf) > 8)
            sprintf(csp_type, "C444p%d XYSCSS=C444p%d", avs_h.func.avs_bits_per_component(inf), avs_h.func.avs_bits_per_component(inf));
        else
            sprintf(csp_type, "C444");
    } else {
        if(!raw_output) {
            fprintf(stderr, "Error: unsupported colorspace.\nYou still can output any format in headerless mode. Use \"-raw\" option if you really need that.\n");
            goto fail;
        }
    }
    for(int i = 0; i < out_fhs; i++) {
        if(setvbuf(out_fh[i], NULL, _IOFBF, AVS_BUFSIZE)) {
            fprintf(stderr, "Error: failed to create buffer for \"%s\".\n", outfile[i]);
            goto fail;
        }
        if(!y4m_headers[i])
            continue;
        fprintf(out_fh[i], "YUV4MPEG2 W%d H%d F%u:%u I%s A%u:%u %s\n", input_width, input_height, fps_num, fps_den, interlace_type, par_width, par_height, csp_type);
        fflush(out_fh[i]);
    }
    int frame_size = (inf->width * avs_h.func.avs_component_size(inf)) * inf->height + (avs_h.func.avs_num_components(inf) - 1) * ((inf->width * avs_h.func.avs_component_size(inf)) >> chroma_h_shift) * (inf->height >> chroma_v_shift);
    int write_target = out_fhs * frame_size; // how many bytes per frame we expect to write
    if(slave) {
        seek = 0;
        end = INT_MAX;
        #if defined(AVS_WINDOWS)
        SetConsoleTitle("avs2yuv: slave process running");
        #endif
    } else {
        end += seek;
        if(end <= seek || end > inf->num_frames)
            end = inf->num_frames;
    }
    for(int frm = seek; frm < end; ++frm) {
        if(slave) {
            char input[80];
            frm = -1;
            do {
                if(!fgets(input, 80, stdin))
                    goto close_files;
                sscanf(input, "%d", &frm);
            } while(frm < 0);
            if(frm >= inf->num_frames)
                frm = inf->num_frames-1;
        }
        AVS_VideoFrame *f = avs_h.func.avs_get_frame(avs_h.clip, frm);
        const char *err = avs_h.func.avs_clip_get_error(avs_h.clip);
        if(err) {
            fprintf(stderr, "Error: %s occurred while reading frame %d.\n", err, frm);
            goto fail;
        }
        if(out_fhs) {
            static const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
            int wrote = 0;
            for(int i = 0; i < out_fhs; i++)
                if(y4m_headers[i])
                    fwrite("FRAME\n", 1, 6, out_fh[i]);
            for(int p = 0; p < avs_h.func.avs_num_components(inf); p++) {
                int w = (inf->width * avs_h.func.avs_component_size(inf)) >> (p ? chroma_h_shift : 0);
                int h = inf->height >> (p ? chroma_v_shift : 0);
                int pitch = avs_h.func.avs_get_pitch_p(f, planes[p]);
                const BYTE* data = avs_h.func.avs_get_read_ptr_p(f, planes[p]);
                for(int y = 0; y < h; y++) {
                    for(int i = 0; i < out_fhs; i++)
                        wrote += fwrite(data, 1, w, out_fh[i]);
                    data += pitch;
                }
            }
            if(wrote != write_target) {
                fprintf(stderr, "Error: wrote only %d of %d bytes.\n", wrote, write_target);
                goto fail;
            }
            if(slave) { // assume timing doesn't matter in other modes
                for(int i = 0; i < out_fhs; i++)
                    fflush(out_fh[i]);
            }
        }
        #if defined(AVS_WINDOWS)
        if(frm == 0) {
            SetConsoleTitle("avs2yuv: executing script");
        }
        #endif
        if(!nostderr) {
            char buf[400];
            int64_t i_time = avs2yuv_mdate();
            int64_t i_elapsed = i_time - i_start;
            double fps = i_elapsed > 0 ? (frm - seek) * 1000000. / i_elapsed : 0;
            i_frame = frm + 1;
            int secs = i_elapsed / 1000000;
            int eta = i_elapsed * (i_frame_total - i_frame + seek) / ((int64_t)(i_frame - seek) * 1000000);
            i_frame_total = end - seek;
            if(!nostderr) {
                #if defined(AVS_WINDOWS)
                sprintf(buf, "avs2yuv [%.1f%%], %d/%d frames, %.2f fps, eta %d:%02d:%02d", 100. * frm / i_frame_total, frm, (int)i_frame_total, fps, eta / 3600, (eta / 60) % 60, eta % 60);
                SetConsoleTitle(buf);
                #endif
                static int print_progress_header = 1;
                if(print_progress_header) {
                    if ( (end < inf->num_frames) || (seek > 0) ) {
                        fprintf(stderr, "%6s %12s %12s   %7s %9s %9s\n", "Progress", "Frames", "Frame#", "FPS", "Elapsed", "Remain");
                    } else {
                        fprintf(stderr, "%6s %12s   %7s %9s %9s\n", "Progress", "Frames", "FPS", "Elapsed", "Remain");
                    }
                    print_progress_header = 0;
                }
                if ( (end < inf->num_frames) || (seek > 0) ) {
                    sprintf(buf, "[%5.1f%%] %6d/%-6d      %6d  %8.2f %3d:%02d:%02d %3d:%02d:%02d", 100. * (i_frame - seek) / i_frame_total, i_frame - seek, (int)i_frame_total, frm, fps, secs / 3600, (secs / 60) % 60, secs % 60, eta / 3600, (eta / 60) % 60, eta % 60);
                } else {
                    sprintf(buf, "[%5.1f%%] %6d/%-6d %8.2f %3d:%02d:%02d %3d:%02d:%02d", 100. * frm / i_frame_total, frm, (int)i_frame_total, fps, secs / 3600, (secs / 60) % 60, secs % 60, eta / 3600, (eta / 60) % 60, eta % 60);
                }
                fprintf(stderr, "%s   \r", buf);
            }
            fflush(stderr);
        }
        avs_h.func.avs_release_video_frame(f);
    }
    for(int i = 0; i < out_fhs; i++)
        fflush(out_fh[i]);
close_files:
    retval = 0;
    if(!nostderr) {
        time_t tm2 = time(NULL);
        fprintf(stderr, "\n");
        fprintf(stderr, "Started:\t%s", ctime(&tm_s));
        fprintf(stderr, "Finished:\t%s", ctime(&tm2));
        tm2 = tm2 - tm_s;
        fprintf(stderr, "Elapsed:\t%d:%02d:%02d\n", (int)tm2 / 3600, (int)tm2 % 3600 / 60, (int)tm2 % 60);
    }
fail:
    for(int i = 0; i < out_fhs; i++)
        if(out_fh[i])
            fclose(out_fh[i]);
    if(avs_h.library)
        internal_avs_close_library(&avs_h);
    return retval;
}
