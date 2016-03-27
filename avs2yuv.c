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
#include <io.h>
#include <fcntl.h>
#include "avs_internal.c"
#include <signal.h>
#include <windows.h>
#include <sys/timeb.h>

#ifndef INT_MAX
#define INT_MAX 0x7fffffff
#endif

#define MY_VERSION "Avs2YUV 0.26"
#define AUTHORS "Writen by Loren Merritt, modified by BugMaster, Chikuzen\nand currently maintained by DJATOM"
#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define snprintf _snprintf
#endif

#define MAX_FH 10
#define AVS_BUFSIZE (128*1024)
#define CSP_AUTO (-1)
#define CSP_I420 1
#define CSP_I422 2
#define CSP_I444 3

typedef signed __int64 int64_t;

int64_t avs2yuv_mdate( void )
{
    struct timeb tb;
    ftime( &tb );
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
}

static int csp_to_int(const char *arg)
{
	if(!strcasecmp(arg, "auto"))
		return CSP_AUTO;
    if(!strcmp(arg, "i420"))
        return CSP_I420;
    if(!strcmp(arg, "i422"))
        return CSP_I422;
    if(!strcmp(arg, "i444"))
        return CSP_I444;
    return 0;
}

static volatile int b_ctrl_c = 0;

void sigintHandler(int sig_num)
{
    exit(0);
	b_ctrl_c = 1;
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
    int rawyuv = 0;
    int interlaced = 0;
    int tff = 0;
    int csp = CSP_I420;
	int input_depth = 8;
	int guess_depth = 1;
	int input_width;
	int input_height;
	unsigned fps_num = 0;
	unsigned fps_den = 0;
	unsigned par_width = 0;
	unsigned par_height = 0;
	
	int i_frame = 0;
    int64_t i_frame_total = 0;
	int64_t i_start = 0;
	
	SetConsoleTitle ("avs2yuv: preparing frameserver");
    
	for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] != 0) {
            if(!strcmp(argv[i], "-h"))
                usage = 1;
			else if(!strcmp(argv[i], "-nstdr"))
                nostderr = 1;
            else if(!strcmp(argv[i], "-o")) {
                if(i > argc-2) {
                    fprintf(stderr, "-o needs an argument\n");
                    return 2;
                }
                i++;
                goto add_outfile;
            } else if(!strcmp(argv[i], "-seek")) {
                if(i > argc-2) {
                    fprintf(stderr, "-seek needs an argument\n");
                    return 2;
                }
                seek = atoi(argv[++i]);
                if(seek < 0) usage = 1;
            } else if(!strcmp(argv[i], "-frames")) {
                if(i > argc-2) {
                    fprintf(stderr, "-frames needs an argument\n");
                    return 2;
                }
                end = atoi(argv[++i]);
            } else if(!strcmp(argv[i], "-raw")) {
                rawyuv = 1;
            } else if(!strcmp(argv[i], "-slave")) {
                slave = 1;
            } else if(!strcmp(argv[i], "-csp")) {
                if(i > argc-2) {
                    fprintf(stderr, "-csp needs an argument\n");
                    return 2;
                }
                csp = csp_to_int(argv[++i]);
                if(!csp) {
                    fprintf(stderr, "-csp \"%s\" is unknown\n", argv[i]);
                    return 2;
                }
            } else if(!strcmp(argv[i], "-depth")) {
                if(i > argc-2) {
                    fprintf(stderr, "-depth needs an argument\n");
                    return 2;
                }
                input_depth = atoi(argv[++i]);
                guess_depth = 0;
				if(input_depth < 8 || input_depth > 16) {
                    fprintf(stderr, "-depth \"%s\" is not supported\n", argv[i]);
                    return 2;
                }
            } else if(!strcmp(argv[i], "-fps")) {
                if(i > argc-2) {
                    fprintf(stderr, "-fps needs an argument\n");
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
                    fprintf(stderr, "-fps \"%s\" is not supported\n", str);
                    return 2;
                }
            } else if(!strcmp(argv[i], "-par")) {
                if(i > argc-2) {
                    fprintf(stderr, "-par needs an argument\n");
                    return 2;
                }
                const char *str = argv[++i];
                if( (sscanf(str, "%u:%u", &par_width, &par_height) != 2) &&
                    (sscanf(str, "%u/%u", &par_width, &par_height) != 2) )
                {
                    fprintf(stderr, "-par \"%s\" is not supported\n", str);
                    return 2;
                }
            } else {
                fprintf(stderr, "no such option: %s\n", argv[i]);
                return 2;
            }
        } else if(!infile) {
            infile = argv[i];
            const char *dot = strrchr(infile, '.');
            if(!dot || strcmp(".avs", dot))
                fprintf(stderr, "infile \"%s\" doesn't look like an avisynth script\n", infile);
        } else {
add_outfile:
            if(out_fhs > MAX_FH-1) {
                fprintf(stderr, "too many output files\n");
                return 2;
            }
            outfile[out_fhs] = argv[i];
            y4m_headers[out_fhs] = !rawyuv;
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
        "-raw\toutput raw I420/I422/I444 instead of yuv4mpeg\n"
        "-csp\tconvert to I420/I422/I444 or AUTO colorspace (default I420)\n"
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
        fprintf(stderr, "error: failed to load avisynth.dll\n");
        goto fail;
    }

    avs_h.env = avs_h.func.avs_create_script_environment(AVS_INTERFACE_25);
    if(avs_h.func.avs_get_error) {
        const char *error = avs_h.func.avs_get_error(avs_h.env);
        if(error) {
            fprintf(stderr, "error: %s\n", error);
            goto fail;
        }
    }

    AVS_Value arg = avs_new_value_string(infile);
    AVS_Value res = avs_h.func.avs_invoke(avs_h.env, "Import", arg, NULL);
    if(avs_is_error(res)) {
        fprintf(stderr, "error: %s\n", avs_as_string(res));
        goto fail;
    }
    /* check if the user is using a multi-threaded script and apply distributor if necessary.
       adapted from avisynth's vfw interface */
    AVS_Value mt_test = avs_h.func.avs_invoke(avs_h.env, "GetMTMode", avs_new_value_bool(0), NULL);
    int mt_mode = avs_is_int(mt_test) ? avs_as_int(mt_test) : 0;
    avs_h.func.avs_release_value(mt_test);
    if( mt_mode > 0 && mt_mode < 5 ) {
        AVS_Value temp = avs_h.func.avs_invoke(avs_h.env, "Distributor", res, NULL);
        avs_h.func.avs_release_value(res);
        res = temp;
    }
	/* 
		guess bit depth from avisynth script, 
		requires BitsConvert wrapper and 
		params w/o spaces, like "BitsConvert(16,1,10,2)"
	*/
	if(guess_depth) {
		int p1 = -1, p2 = -1, p3 = -1, p4 = -1;
		char guess_buf[200];
		FILE * avsfs = fopen (infile, "r");
		while (fgets(guess_buf, sizeof(guess_buf), avsfs) && (p1 == -1 || p2 == -1 || p3 == -1 || p4 == -1)) { //try to search untill we get desirable data
			sscanf (guess_buf, "BitsConvert(%d,%d,%d,%d)", &p1, &p2, &p3, &p4); //ensure that line happens only one time in the script
			if(p3 >= 9 || p3 <= 16 && p4 == 2) //react only when output is interleaved
				input_depth = p3;
		}
		fclose (avsfs);
	}
    if(!avs_is_clip(res)) {
        fprintf(stderr, "error: \"%s\" didn't return a video clip\n", infile);
        goto fail;
    }
    avs_h.clip = avs_h.func.avs_take_clip(res, avs_h.env);
    const AVS_VideoInfo *inf = avs_h.func.avs_get_video_info(avs_h.clip);
    
	if(!avs_has_video(inf)) {
        fprintf(stderr, "error: \"%s\" has no video data\n", infile);
        goto fail;
    }
    /* if the clip is made of fields instead of frames, call weave to make them frames */
    if(avs_is_field_based(inf)) {
        fprintf(stderr, "detected fieldbased (separated) input, weaving to frames\n");
        AVS_Value tmp = avs_h.func.avs_invoke(avs_h.env, "Weave", res, NULL);
        if(avs_is_error(tmp)) {
            fprintf(stderr, "error: couldn't weave fields into frames\n");
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
	if(input_depth > 8) {
		if(input_width & 3) {
			if(!nostderr)
				fprintf(stderr, "avisynth %d-bit hack requires that width is at least mod4\n", input_depth);
			goto fail;
		}
		if(!nostderr) 
			fprintf(stderr, "avisynth %d-bit hack enabled\n", input_depth);
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

    if(csp == CSP_AUTO) {
        if(avs_is_yv12(inf))
            csp = CSP_I420;
        else if(avs_is_yv16(inf) || avs_is_yuy2(inf))
            csp = CSP_I422;
        else if(avs_is_yv24(inf))
            csp = CSP_I444;
        else
            csp = CSP_I420; // not supported colorspaces (like RGB) we try convert to I420
    }

	signal(SIGINT, sigintHandler);
	
	//start processing
	i_start = avs2yuv_mdate();
	time_t tm_s = time(0);
	i_frame_total  = inf->num_frames;
	
	if(b_ctrl_c)
		goto close_files;
		
    if( (csp == CSP_I420 && !avs_is_yv12(inf)) ||
        (csp == CSP_I422 && !avs_is_yv16(inf)) ||
        (csp == CSP_I444 && !avs_is_yv24(inf)) )
    {
        if(input_depth > 8) {
            fprintf(stderr, "error: colorspace conversion is not possible with high depth input [%d-bit depth]\n", input_depth);
            goto fail;
        }
        const char *csp_name = csp == CSP_I444 ? "YV24" :
                               csp == CSP_I422 ? "YV16" :
                               "YV12";
        fprintf(stderr, "converting input clip to %s\n", csp_name);
        if(csp < CSP_I444 && (inf->width&1)) {
            fprintf(stderr, "error: input clip width not divisible by 2 (%dx%d)\n", inf->width, inf->height);
            goto fail;
        }
        if(csp == CSP_I420 && interlaced && (inf->height&3)) {
            fprintf(stderr, "error: input clip height not divisible by 4 (%dx%d)\n", inf->width, inf->height);
            goto fail;
        }
        if((csp == CSP_I420 || interlaced) && (inf->height&1)) {
            fprintf(stderr, "error: input clip height not divisible by 2 (%dx%d)\n", inf->width, inf->height);
            goto fail;
        }
        const char *arg_name[2] = {NULL, "interlaced"};
        AVS_Value arg_arr[2];
        arg_arr[0] = res;
        arg_arr[1] = avs_new_value_bool(interlaced);
        char conv_func[14] = {"ConvertTo"};
        strcat(conv_func, csp_name);
        AVS_Value tmp = avs_h.func.avs_invoke(avs_h.env, conv_func, avs_new_value_array(arg_arr, 2), arg_name);
        if(avs_is_error(tmp)) {
            fprintf(stderr, "error: couldn't convert input clip to %s\n", csp_name);
            goto fail;
        }
        res = internal_avs_update_clip(&avs_h, &inf, tmp, res);
    }
    avs_h.func.avs_release_value(res);

    for(int i = 0; i < out_fhs; i++) {
        if(!strcmp(outfile[i], "-")) {
            for(int j = 0; j < i; j++)
                if(out_fh[j] == stdout) {
                    fprintf(stderr, "error: can't write to stdout multiple times\n");
                    goto fail;
                }
            int dupout = _dup(_fileno(stdout));
            fclose(stdout);
            _setmode(dupout, _O_BINARY);
            out_fh[i] = _fdopen(dupout, "wb");
        } else {
            out_fh[i] = fopen(outfile[i], "wb");
            if(!out_fh[i]) {
                fprintf(stderr, "error: failed to create/open \"%s\"\n", outfile[i]);
                goto fail;
            }
        }
    }

    char *interlace_type = interlaced ? tff ? "t" : "b" : "p";
    char csp_type[200];
    int chroma_h_shift = 0;
    int chroma_v_shift = 0;
    switch(csp) {
        case CSP_I420:
            if(input_depth > 8)
                sprintf(csp_type, "C420p%d XYSCSS=420P%d", input_depth, input_depth);
            else
                strcpy(csp_type, "C420mpeg2 XYSCSS=420MPEG2");
            chroma_h_shift = 1;
            chroma_v_shift = 1;
            break;
        case CSP_I422:
            if(input_depth > 8)
                sprintf(csp_type, "C422p%d XYSCSS=422P%d", input_depth, input_depth);
            else
                strcpy(csp_type, "C422 XYSCSS=422");
            chroma_h_shift = 1;
            break;
        case CSP_I444:
            if(input_depth > 8)
                sprintf(csp_type, "C444p%d XYSCSS=444P%d", input_depth, input_depth);
            else
                strcpy(csp_type, "C444 XYSCSS=444");
            break;
        default:
            goto fail; //can't happen
    }
    for(int i = 0; i < out_fhs; i++) {
        if(setvbuf(out_fh[i], NULL, _IOFBF, AVS_BUFSIZE))
        {
            fprintf(stderr, "error: failed to create buffer for \"%s\"\n", outfile[i]);
            goto fail;
        }
        if(!y4m_headers[i])
            continue;
        fprintf(out_fh[i], "YUV4MPEG2 W%d H%d F%u:%u I%s A%u:%u %s\n",
            input_width, input_height, fps_num, fps_den, interlace_type, par_width, par_height, csp_type);
        fflush(out_fh[i]);
    }

    int frame_size = inf->width * inf->height + 2 * (inf->width >> chroma_h_shift) * (inf->height >> chroma_v_shift);
    int write_target = out_fhs * frame_size; // how many bytes per frame we expect to write

    if(slave) {
        seek = 0;
        end = INT_MAX;
		SetConsoleTitle ("avs2yuv: slave mode");
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
            fprintf(stderr, "error: %s occurred while reading frame %d\n", err, frm);
            goto fail;
        }

        if(out_fhs) {
            static const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
            int wrote = 0;

            for(int i = 0; i < out_fhs; i++)
                if(y4m_headers[i])
                    fwrite("FRAME\n", 1, 6, out_fh[i]);

            for(int p = 0; p < 3; p++) {
                int w = inf->width  >> (p ? chroma_h_shift : 0);
                int h = inf->height >> (p ? chroma_v_shift : 0);
                int pitch = avs_get_pitch_p(f, planes[p]);
                const BYTE* data = avs_get_read_ptr_p(f, planes[p]);
                for(int y = 0; y < h; y++) {
                    for(int i = 0; i < out_fhs; i++)
                        wrote += fwrite(data, 1, w, out_fh[i]);
                    data += pitch;
                }
            }
            if(wrote != write_target) {
                fprintf(stderr, "error: wrote only %d of %d bytes\n", wrote, write_target);
                goto fail;
            }
            if(slave) { // assume timing doesn't matter in other modes
                for(int i = 0; i < out_fhs; i++)
                    fflush(out_fh[i]);
            }
        }
		if(frm == 0) {
			SetConsoleTitle ("avs2yuv: executing script");
		}
		// an advanced verbose output, which is ported from x264
		if(!nostderr) {
			char buf[400];
			int64_t i_time = avs2yuv_mdate();
			int64_t i_elapsed = i_time - i_start;
			double fps = i_elapsed > 0 ? frm * 1000000. / i_elapsed : 0;
			i_frame = frm + 1;
			int secs = i_elapsed / 1000000;
			int eta = i_elapsed * (i_frame_total - i_frame) / ((int64_t)i_frame * 1000000);
			if(!nostderr) {
				sprintf( buf, "avs2yuv [%.1f%%], %d/%d frames, %.2f fps, eta %d:%02d:%02d", 100. * frm / i_frame_total, frm, (int) i_frame_total, fps, eta/3600, (eta/60)%60, eta%60 );
				SetConsoleTitle (buf);
				static int print_progress_header = 1;
				if( print_progress_header )
				{
					fprintf( stderr, "%6s  %12s  %7s %9s %9s\n", "Progress", "Frames   ", "FPS", "Elapsed", "Remain" );
					print_progress_header = 0;
				}
				sprintf( buf, "[%5.1f%%] %6d/%-6d %8.2f %3d:%02d:%02d %3d:%02d:%02d", 
					100. * frm / i_frame_total, frm, (int) i_frame_total, fps, secs/3600, (secs/60)%60, secs%60, eta/3600, (eta/60)%60, eta%60 );
				fprintf ( stderr, "%s   \r", buf );
			}
			fflush( stderr ); // needed in windows
		}
        avs_h.func.avs_release_video_frame(f);
    }

    for(int i = 0; i < out_fhs; i++)
        fflush(out_fh[i]);

close_files:
    retval = 0;
	if(!nostderr) {
		time_t tm2 = time(NULL);
		fprintf (stderr, "Started:\t%s", ctime(&tm_s));
		fprintf (stderr, "Finished:\t%s", ctime(&tm2));
		tm2 = tm2 - tm_s;
		fprintf ( stderr, "Elapsed:\t%d:%02d:%02d\n", (int) tm2 / 3600, (int) tm2 % 3600 / 60, (int) tm2 % 60 );	
	}
fail:
    for(int i = 0; i < out_fhs; i++)
        if(out_fh[i])
            fclose(out_fh[i]);
    if(avs_h.library)
        internal_avs_close_library(&avs_h);
    return retval;
}
