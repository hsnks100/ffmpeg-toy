extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/error.h>
}

#if __cplusplus 

#undef av_err2str 
#define av_err2str(errnum)  av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum) 

#endif

#include <map>
#include <string>
#include <vector>
#include <unistd.h>


// ./toy-0.1.out ksoo.h264 audio_chn0.pcm zzzz.mp4

static AVFormatContext *ifmt_ctx_v; // raw h264
static AVFormatContext *ifmt_ctx_a;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;

typedef struct StreamContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;
} StreamContext;


struct CommonContext {
    FilteringContext *filter_ctx;
    StreamContext *stream_ctx;
};

std::vector<CommonContext> contexts;
int videoindex_v=-1,videoindex_out=-1;
int audioindex_a=-1,audioindex_out=-1;


#define KLOG printf("---------------%s::%d------------\n", __FUNCTION__, __LINE__)
const int VIDEO_FILE = 0;
const int AUDIO_FILE = 1;
static int open_input_file(const char *videoFile, const char *audioFile)
{
    int ret;
    unsigned int i;

    ifmt_ctx_v = NULL;
    avformat_open_input(&ifmt_ctx_v, videoFile, NULL, NULL);
    avformat_find_stream_info(ifmt_ctx_v, NULL);

    ifmt_ctx_a = NULL;
    AVInputFormat* fmt = NULL;
    fmt = av_find_input_format("s16le");

    avformat_open_input(&ifmt_ctx_a, audioFile, fmt, NULL);
    avformat_find_stream_info(ifmt_ctx_a, NULL);

    printf("v stream count = %d\n", ifmt_ctx_v->nb_streams);
    contexts[0].stream_ctx = (StreamContext*)av_mallocz_array(ifmt_ctx_v->nb_streams, sizeof(*contexts[0].stream_ctx));
    contexts[AUDIO_FILE].stream_ctx = (StreamContext*)av_mallocz_array(ifmt_ctx_a->nb_streams, sizeof(*contexts[AUDIO_FILE].stream_ctx));

    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        AVStream *stream = ifmt_ctx_v->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        codec_ctx = avcodec_alloc_context3(dec);
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx_v, stream, NULL);
            ret = avcodec_open2(codec_ctx, dec, NULL);
            printf("v file video index = %d\n", i); 
        }
        else if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            ret = avcodec_open2(codec_ctx, dec, NULL);
            printf("v file audio index = %d\n", i);
        }
        contexts[0].stream_ctx[i].dec_ctx = codec_ctx;
    }

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        AVStream *stream = ifmt_ctx_a->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        codec_ctx = avcodec_alloc_context3(dec);
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx_a, stream, NULL);
            ret = avcodec_open2(codec_ctx, dec, NULL);
            printf("a file video index = %d\n", i);

        }
        else if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            ret = avcodec_open2(codec_ctx, dec, NULL);
            printf("a file audio index = %d\n", i);
        }
        contexts[AUDIO_FILE].stream_ctx[i].dec_ctx = codec_ctx;
    }

    av_dump_format(ifmt_ctx_v, 0, videoFile, 0);
    av_dump_format(ifmt_ctx_a, 0, audioFile, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {

        in_stream = ifmt_ctx_a->streams[i];
        dec_ctx = contexts[AUDIO_FILE].stream_ctx[i].dec_ctx;

        if(dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            out_stream->codec->codec_tag = 0;

            audioindex_out = out_stream->index;
            audioindex_a = i;
            encoder = avcodec_find_encoder( AV_CODEC_ID_AAC );
            if (!encoder) {
                av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            enc_ctx = avcodec_alloc_context3(encoder);

            enc_ctx->channels       = 2;
            enc_ctx->channel_layout = av_get_default_channel_layout(2);
            enc_ctx->sample_rate    = dec_ctx->sample_rate;
            enc_ctx->sample_fmt     = encoder->sample_fmts[0];
            enc_ctx->bit_rate       = 96000;
            /** Allow the use of the experimental AAC encoder */
            enc_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

            //if ((*ofmt_ctx)->oformat->flags & AVFMT_GLOBALHEADER)
                //enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            //enc_ctx->sample_rate = dec_ctx->sample_rate;
            //enc_ctx->channel_layout = dec_ctx->channel_layout;
            //enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
            //[> take first format from list of supported formats <]
            //enc_ctx->sample_fmt = encoder->sample_fmts[0];
            enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            //out_stream->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            out_stream->time_base = enc_ctx->time_base;
            contexts[AUDIO_FILE].stream_ctx[i].enc_ctx = enc_ctx;
        } 
    }
    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        dec_ctx = contexts[0].stream_ctx[i].dec_ctx; 
        if(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            in_stream = ifmt_ctx_v->streams[i];

            videoindex_out = out_stream->index;
            videoindex_v = i;
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                printf( "Failed to copy context from input to output stream codec context\n");
                return -1;
            }
            out_stream->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

            //contexts[0].stream_ctx[i].enc_ctx = NULL;
        }
    }


    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
        AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    KLOG;
    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        KLOG;
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        KLOG;

        snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->time_base.num, dec_ctx->time_base.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        KLOG;
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink"); 

        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout =
                av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof(args),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
                dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                av_get_sample_fmt_name(dec_ctx->sample_fmt),
                dec_ctx->channel_layout);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        KLOG;
        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                (uint8_t*)&enc_ctx->channel_layout,
                sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    KLOG;
    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    KLOG;
    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;



    av_buffersink_set_frame_size(buffersink_ctx, dec_ctx->frame_size);

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    KLOG;
    return ret;
}

static int init_filters(void)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    contexts[0].filter_ctx = (FilteringContext*)av_malloc_array(ifmt_ctx_v->nb_streams, sizeof(*contexts[0].filter_ctx));
    contexts[AUDIO_FILE].filter_ctx = (FilteringContext*)av_malloc_array(ifmt_ctx_a->nb_streams, sizeof(*contexts[AUDIO_FILE].filter_ctx));
    if (!contexts[0].filter_ctx)
        return AVERROR(ENOMEM);
    if (!contexts[AUDIO_FILE].filter_ctx)
        return AVERROR(ENOMEM);

    KLOG;

    if(0) { 
        for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
            printf("video file stream index = %d\n", i);
            contexts[0].filter_ctx[i].buffersrc_ctx  = NULL;
            contexts[0].filter_ctx[i].buffersink_ctx = NULL;
            contexts[0].filter_ctx[i].filter_graph   = NULL;
            if (ifmt_ctx_v->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                filter_spec = "anull"; /* passthrough (dummy) filter for audio */
                KLOG;
                printf("---%d\n", contexts[0].stream_ctx[i].dec_ctx->codec_type);
                ret = init_filter(&contexts[0].filter_ctx[i], 
                                  contexts[0].stream_ctx[i].dec_ctx, 
                                  contexts[0].stream_ctx[i].enc_ctx, 
                                  filter_spec);
                if (ret)
                    return ret;
            }
            else if(ifmt_ctx_v->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                filter_spec = "null"; /* passthrough (dummy) filter for video */
            }
        }
    }

    KLOG;

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        contexts[AUDIO_FILE].filter_ctx[i].buffersrc_ctx  = NULL;
        contexts[AUDIO_FILE].filter_ctx[i].buffersink_ctx = NULL;
        contexts[AUDIO_FILE].filter_ctx[i].filter_graph   = NULL;
        if (ifmt_ctx_a->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
            ret = init_filter(&contexts[AUDIO_FILE].filter_ctx[i], contexts[AUDIO_FILE].stream_ctx[i].dec_ctx,
                              contexts[AUDIO_FILE].stream_ctx[i].enc_ctx, filter_spec);
            if (ret)
                return ret;
        }
        else if(ifmt_ctx_a->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            filter_spec = "null"; /* passthrough (dummy) filter for video */
        }
    }
    KLOG;
    return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame, int fileIndex) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
    //int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
        //(ifmt_ctx_v->streams[stream_index]->codecpar->codec_type ==
         //AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *);

    if(fileIndex == AUDIO_FILE) {
        enc_func = avcodec_encode_audio2;
    }
    else {
        enc_func = avcodec_encode_video2;
    }
    if (!got_frame)
        got_frame = &got_frame_local;

    /*av_log(NULL, AV_LOG_INFO, "Encoding frame\n");*/
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    AVCodecContext* gg = contexts[fileIndex].stream_ctx[stream_index].enc_ctx;

    //printf("codec_type = %d\n" "codec_id = %d\n", (int)gg->codec_type, (int)gg->codec_id);


    ret = enc_func(contexts[fileIndex].stream_ctx[stream_index].enc_ctx, &enc_pkt,
            filt_frame, got_frame);
    av_frame_free(&filt_frame);
    if (ret < 0) {
        printf("enc_func error \n");
        av_log(NULL, AV_LOG_ERROR, "Error occurred... : %s\n", av_err2str(ret));
        return ret;
    }
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = stream_index;
    av_packet_rescale_ts(&enc_pkt,
                         contexts[fileIndex].stream_ctx[stream_index].enc_ctx->time_base,
                         ofmt_ctx->streams[stream_index]->time_base);

    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    if (ret < 0) {
        printf("av_interleaved_write_frame error %d\n", __LINE__);
        return ret;
    }

    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index, int fileIndex)
{
    int ret;
    AVFrame *filt_frame;

    /*av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");*/
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(contexts[fileIndex].filter_ctx[stream_index].buffersrc_ctx, frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1) {
        filt_frame = av_frame_alloc();
        if (!filt_frame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        /*av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");*/
        ret = av_buffersink_get_frame(contexts[fileIndex].filter_ctx[stream_index].buffersink_ctx,
                filt_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }

        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(filt_frame, stream_index, NULL, fileIndex);
        if (ret < 0) {
            printf("encode_write_frame error \n");
            break;
        }
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index, int fileIndex)
{
    int ret;
    int got_frame;

    if (!(contexts[fileIndex].stream_ctx[stream_index].enc_ctx->codec->capabilities &
                AV_CODEC_CAP_DELAY))
        return 0;

    while (1) {
        /*av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);*/
        ret = encode_write_frame(NULL, stream_index, &got_frame, fileIndex);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int main(int argc, char **argv)
{

    contexts.resize(2);
    int ret;
    AVPacket packet;
    packet.data = NULL;
    packet.size = 0;
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);

    if (argc != 4) {
        av_log(NULL, AV_LOG_ERROR, "Usage: %s <video input file> <audio input file> <output file>\n", argv[0]);
        return 1;
    }

    av_register_all();
    avfilter_register_all();

	int frame_index=0;
    int64_t cur_pts_v = 0;
    int64_t cur_pts_a = 0;
    if ((ret = open_input_file(argv[1], argv[2])) < 0) {
        goto end;
    }
    KLOG;
    if ((ret = open_output_file(argv[3])) < 0) {
        goto end;
    }
    KLOG;
    if ((ret = init_filters()) < 0) {
        goto end;
    }

    KLOG;
    /* read all packets */

    //cur_pts 처리할 차례
    av_dump_format(ofmt_ctx, 0, argv[3], 1);

    printf("videoout = %d, audioout = %d\n", videoindex_out, audioindex_out);
    while (1) {
        AVStream *in_stream, *out_stream;
        AVFormatContext* ifmt_ctx = NULL;
        if(av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, 
                         cur_pts_a, ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {

            ifmt_ctx = ifmt_ctx_v;
            //printf("video packet\n");
        }
        else {
            ifmt_ctx = ifmt_ctx_a;
            //printf("audio packet\n"); 
        }
        //ifmt_ctx = ifmt_ctx_a;

        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0) {
            break;
        }


        in_stream  = ifmt_ctx->streams[packet.stream_index];

        if(packet.pts == AV_NOPTS_VALUE) {
            //printf("nopts_value \n");
            //Write PTS
            AVRational time_base1 = in_stream->time_base;
            in_stream->r_frame_rate.den = 1;
            in_stream->r_frame_rate.num = 30;
            //Duration between 2 frames (us)
            double calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate); 
            //Parameters
            packet.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            packet.dts=packet.pts;
            packet.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE) ;
            frame_index++;
        }

        if(ifmt_ctx == ifmt_ctx_a) {
            cur_pts_a = packet.pts;
            out_stream = ofmt_ctx->streams[audioindex_out];
        }
        else {
            cur_pts_v = packet.pts;
            out_stream = ofmt_ctx->streams[videoindex_out];
        }

        //stream_index = audioindex_a; // packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
                stream_index); 
        //printf("---------------------- TYPE = %d\n", type);

        if(type == AVMEDIA_TYPE_VIDEO) {
            //Convert PTS/DTS
            packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
            packet.pos = -1;
            packet.stream_index=videoindex_out;

            //printf("Write 1 Packet. size:%5d\tpts:%lld\n",packet.size,packet.pts);
            //Write
            if (av_interleaved_write_frame(ofmt_ctx, &packet) < 0) {
                printf( "Error muxing packet\n");
                break;
            }
        }
        else if(type == AVMEDIA_TYPE_AUDIO ) { 
            //av_log(NULL, AV_LOG_ERROR, "Going to reencode&filter the frame\n");
            frame = av_frame_alloc();
            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[audioindex_a]->time_base,
                                 contexts[AUDIO_FILE].stream_ctx[audioindex_a].dec_ctx->time_base);
            dec_func = avcodec_decode_audio4;
            ret = dec_func(contexts[AUDIO_FILE].stream_ctx[audioindex_a].dec_ctx, frame, &got_frame, &packet);
            if (ret < 0) {
                sleep(3);
                av_log(NULL, AV_LOG_ERROR, "Error occurred2: %s\n", av_err2str(ret));
                av_frame_free(&frame);
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }

            if (got_frame) {
                frame->pts = frame->best_effort_timestamp;
                ret = filter_encode_write_frame(frame, audioindex_a, AUDIO_FILE);
                av_frame_free(&frame);
                if (ret < 0) {
                    printf("filter_encode_write_frame error \n");
                    goto end; 
                }
            } else {
                av_frame_free(&frame);
            }
        }
        av_packet_unref(&packet);
    }

     //flush filters and encoders 
    //for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        ////[> flush filter <]
        //if (!contexts[0].filter_ctx[i].filter_graph)
            //continue;
        //ret = filter_encode_write_frame(NULL, i, 0);
        //if (ret < 0) {
            //av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            //goto end;
        //}

        ////[> flush encoder <]
        //ret = flush_encoder(i, 0);
        //if (ret < 0) {
            //av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            //goto end;
        //}
    //}
    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        //[> flush filter <]
        if (!contexts[AUDIO_FILE].filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i, AUDIO_FILE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        //[> flush encoder <]
        ret = flush_encoder(i, AUDIO_FILE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }

    av_write_trailer(ofmt_ctx);

end:
    av_packet_unref(&packet);
    av_frame_free(&frame);
    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        avcodec_free_context(&contexts[0].stream_ctx[i].dec_ctx);
        //if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && contexts[0].stream_ctx[i].enc_ctx)
            //avcodec_free_context(&contexts[0].stream_ctx[i].enc_ctx);
        //if (contexts[0].filter_ctx && contexts[0].filter_ctx[i].filter_graph)
            //avfilter_graph_free(&contexts[0].filter_ctx[i].filter_graph);
    }
    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        avcodec_free_context(&contexts[AUDIO_FILE].stream_ctx[i].dec_ctx);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && contexts[AUDIO_FILE].stream_ctx[i].enc_ctx)
            avcodec_free_context(&contexts[AUDIO_FILE].stream_ctx[i].enc_ctx);
        if (contexts[AUDIO_FILE].filter_ctx && contexts[AUDIO_FILE].filter_ctx[i].filter_graph)
            avfilter_graph_free(&contexts[AUDIO_FILE].filter_ctx[i].filter_graph);
    }
    av_free(contexts[0].filter_ctx);
    av_free(contexts[0].stream_ctx);

    av_free(contexts[AUDIO_FILE].filter_ctx);
    av_free(contexts[AUDIO_FILE].stream_ctx);
    avformat_close_input(&ifmt_ctx_v);
    avformat_close_input(&ifmt_ctx_a);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred2: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}
