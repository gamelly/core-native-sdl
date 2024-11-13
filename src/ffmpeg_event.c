#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "zeebo.h"

// sdl
extern SDL_Renderer *renderer;
static int swidth, sheight;
static SDL_Texture *texture;
static SDL_Rect rect;

// ffmpeg
static AVFormatContext *pFormatCtx;
static int vidId = -1, audId = -1;
static double fpsrendering = 0.0;
static AVCodecContext *vidCtx;
static AVCodec *vidCodec;
static AVCodecParameters *vidpar, *audpar;
static AVFrame *vframe;
static AVPacket *packet;
static bool ffmpeg_started = false;

bool ffmpeg_avaliable() {
    bool success = true;
    do {
        if (ffmpeg_started) {
            break;
        }

        success = false;
        ffmpeg_started = true;

        av_log_set_level(AV_LOG_QUIET);
        avformat_network_init();

        pFormatCtx = avformat_alloc_context();

        if (!pFormatCtx) {
            kernel_add_error("avformat alloc ctx");
            break;
        }

        success = true;
    } while (0);

    return success;
}

void ffmpeg_load(char *file_name) {
    do {
        if (!ffmpeg_avaliable()) {
            break;
        }

        if (avformat_open_input(&pFormatCtx, file_name, NULL, NULL) < 0) {
            kernel_add_error("Cannot open");
            kernel_add_error(file_name);
            break;
        }

        if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
            kernel_add_error("Cannot find stream info.");
            break;
        }

        bool foundVideo = false;
        for (int i = 0; i < pFormatCtx->nb_streams; i++) {
            AVCodecParameters *localparam = pFormatCtx->streams[i]->codecpar;
            AVCodec *localcodec = (AVCodec *)avcodec_find_decoder(localparam->codec_id);
            if (localparam->codec_type == AVMEDIA_TYPE_VIDEO && !foundVideo) {
                vidCodec = localcodec;
                vidpar = localparam;
                vidId = i;
                AVRational rational = pFormatCtx->streams[i]->avg_frame_rate;
                fpsrendering = 1.0 / ((double)rational.num / (double)(rational.den));
                foundVideo = true;
            }
        }

        if (!vidCodec) {
            kernel_add_error("No video codec found");
            break;
        }

        vidCtx = avcodec_alloc_context3(vidCodec);
        if (!vidCtx) {
            kernel_add_error("Failed to allocate video codec context");
            break;
        }

        if (avcodec_parameters_to_context(vidCtx, vidpar) < 0) {
            kernel_add_error("Error copying video codec parameters to context");
            break;
        }
        if (avcodec_open2(vidCtx, vidCodec, NULL) < 0) {
            kernel_add_error("Error opening video codec");
            break;
        }

        vframe = av_frame_alloc();
        packet = av_packet_alloc();
        swidth = vidpar->width;
        sheight = vidpar->height;
        rect.x = 0;
        rect.y = 0;
        rect.w = swidth;
        rect.h = sheight;

        if (!vframe || !packet) {
            kernel_add_error("Error allocating frame or packet");
            break;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING | SDL_TEXTUREACCESS_TARGET, swidth, sheight);

        if (!texture) {
            kernel_add_error("texture");
            break;
        }

    } while (0);
}

void ffmpeg_init() {
    if (kernel_option.media) {
        ffmpeg_load(kernel_option.media);
    }
}

static void ffmpeg_exit() {
    if (packet) {
        av_packet_free(&packet);
    }
    if (vframe) {
        av_frame_free(&vframe);
    }
    if (vidCtx) {
        avcodec_free_context(&vidCtx);
    }
    if (pFormatCtx) {
        avformat_close_input(&pFormatCtx);
        avformat_free_context(pFormatCtx);
    }
}

void display() {
    do {
        time_t start = time(NULL);
        if (avcodec_send_packet(vidCtx, packet) < 0) {
            break;
        }
        if (avcodec_receive_frame(vidCtx, vframe) < 0) {
            break;
        }

        SDL_UpdateYUVTexture(texture, &rect, vframe->data[0], vframe->linesize[0], vframe->data[1], vframe->linesize[1], vframe->data[2],
                             vframe->linesize[2]);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        time_t end = time(NULL);
        double diffms = difftime(end, start) / 1000.0;
    } while (0);
}

static void ffmpeg_draw() {
    if (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == vidId) {
            display();
        }
        av_packet_unref(packet);
    }
}

void ffmpeg_install() {
    kernel_event_install(KERNEL_EVENT_POST_INIT, ffmpeg_init);
    kernel_event_install(KERNEL_EVENT_DRAW, ffmpeg_draw);
    kernel_event_install(KERNEL_EVENT_EXIT, ffmpeg_exit);
}
