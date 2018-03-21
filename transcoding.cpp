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

#define KLOG printf("------------------%d------------\n", __LINE__);

static AVFormatContext *ifmt_ctx_a;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

typedef struct StreamContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;
} StreamContext;
int videoindex_v=-1,videoindex_out=-1;
int audioindex_a=-1,audioindex_out=-1;


class ffStreams {
public:
    int _videoIndex;
    int _audioIndex;
    StreamContext* m_stream_ctx;
    AVFormatContext* m_ifmt_ctx;
    int openInputFile(const char* file) {
        int ret;
        unsigned int i;

        m_ifmt_ctx = NULL;
        avformat_open_input(&m_ifmt_ctx, file, NULL, NULL);
        avformat_find_stream_info(m_ifmt_ctx, NULL);

        printf("stream count = %d\n", m_ifmt_ctx->nb_streams);
        m_stream_ctx = (StreamContext*)av_mallocz_array(m_ifmt_ctx->nb_streams, sizeof(*m_stream_ctx));

        for (i = 0; i < m_ifmt_ctx->nb_streams; i++) {
            AVStream *stream = m_ifmt_ctx->streams[i];
            AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
            AVCodecContext *codec_ctx;
            codec_ctx = avcodec_alloc_context3(dec);
            ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
            /* Reencode video & audio and remux subtitles etc. */
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                codec_ctx->framerate = av_guess_frame_rate(m_ifmt_ctx, stream, NULL);
                ret = avcodec_open2(codec_ctx, dec, NULL);

            }
            else if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                ret = avcodec_open2(codec_ctx, dec, NULL);
            }
            m_stream_ctx[i].dec_ctx = codec_ctx;
        }

        av_dump_format(m_ifmt_ctx, 0, file, 0);
        return 0;
    } 
};
class ofStreams {
public:
    int openOutputFile(const char* file, ffStreams* ifstreams) {
        AVStream *out_stream;
        AVStream *in_stream;
        AVCodecContext *dec_ctx, *enc_ctx;
        AVCodec *encoder;
        int ret;
        unsigned int i;

        m_ofmt_ctx = NULL;
        KLOG;
        avformat_alloc_output_context2(&m_ofmt_ctx, NULL, NULL, file);
        for (i = 0; i < ifstreams->m_ifmt_ctx->nb_streams; i++) {
            out_stream = avformat_new_stream(m_ofmt_ctx, NULL);
            in_stream = ifstreams->m_ifmt_ctx->streams[i];
            dec_ctx = ifstreams->m_stream_ctx[i].dec_ctx;
            KLOG;

            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoindex_out = out_stream->index;
                videoindex_v = i;
                if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                    printf( "Failed to copy context from input to output stream codec context\n");
                    return -1;
                }
                out_stream->codec->codec_tag = 0;
                if (ifstreams->m_ifmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                    out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER; 
            }
            else if(dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioindex_out = out_stream->index;
                audioindex_a = i;
                encoder = avcodec_find_encoder( AV_CODEC_ID_MP3 );
                if (!encoder) {
                    av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                    return AVERROR_INVALIDDATA;
                }
                enc_ctx = avcodec_alloc_context3(encoder);
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                enc_ctx->channel_layout = dec_ctx->channel_layout;
                enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = encoder->sample_fmts[0];
                enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
                /* Third parameter can be used to pass settings to encoder */
                ret = avcodec_open2(enc_ctx, encoder, NULL);
                ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
                if (m_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                    enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

                out_stream->time_base = enc_ctx->time_base;
                ifstreams->m_stream_ctx[i].enc_ctx = enc_ctx;
                KLOG;
            }else {
                /* if this stream must be remuxed */
                ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
                    return ret;
                }
                out_stream->time_base = in_stream->time_base;
            } 
        }
        KLOG;
        av_dump_format(m_ofmt_ctx, 0, file, 1);

        KLOG;
        if (!(m_ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&m_ofmt_ctx->pb, file, AVIO_FLAG_WRITE);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", file);
                return ret;
            }
        }

        KLOG;
        /* init muxer, write output file header */
        ret = avformat_write_header(m_ofmt_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
            return ret;
        }

        return 0;
    }   
    AVFormatContext* m_ofmt_ctx;
};



ffStreams someffs;
ofStreams outffs;
#define KLOG printf("---------------%d------------\n", __LINE__)


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

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

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

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(void)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = (FilteringContext*)av_malloc_array(someffs.m_ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < someffs.m_ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph   = NULL;
        if (someffs.m_ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
            ret = init_filter(&filter_ctx[i], someffs.m_stream_ctx[i].dec_ctx,
                              someffs.m_stream_ctx[i].enc_ctx, filter_spec);
            if (ret)
                return ret;
        }
        else if(someffs.m_ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            filter_spec = "null"; /* passthrough (dummy) filter for video */
        }
    }
    /*for (i = 0; i < someffs.m_ifmt_ctx->nb_streams; i++) { */
        /*filter_ctx[i].filter_graph = NULL;*/
    /*}*/
    return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
        (someffs.m_ifmt_ctx->streams[stream_index]->codecpar->codec_type ==
         AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

    if (!got_frame)
        got_frame = &got_frame_local;

    /*av_log(NULL, AV_LOG_INFO, "Encoding frame\n");*/
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(someffs.m_stream_ctx[stream_index].enc_ctx, &enc_pkt,
            filt_frame, got_frame);
    av_frame_free(&filt_frame);
    if (ret < 0) {
        printf("enc_func error \n");
        return ret;
    }
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = stream_index;
    av_packet_rescale_ts(&enc_pkt,
                         someffs.m_stream_ctx[stream_index].enc_ctx->time_base,
                         outffs.m_ofmt_ctx->streams[stream_index]->time_base);

    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(outffs.m_ofmt_ctx, &enc_pkt);
    if (ret < 0) {
        printf("av_interleaved_write_frame error %d\n", __LINE__);
        return ret;
    }

    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
    int ret;
    AVFrame *filt_frame;

    /*av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");*/
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
            frame, 0);
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
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
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
        ret = encode_write_frame(filt_frame, stream_index, NULL);
        if (ret < 0) {
            printf("encode_write_frame error \n");
            break;
        }
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index)
{
    int ret;
    int got_frame;

    if (!(someffs.m_stream_ctx[stream_index].enc_ctx->codec->capabilities &
                AV_CODEC_CAP_DELAY))
        return 0;

    while (1) {
        /*av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);*/
        ret = encode_write_frame(NULL, stream_index, &got_frame);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int main(int argc, char **argv)
{
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

    KLOG;
	int frame_index=0;
    someffs.openInputFile(argv[1]);
    outffs.openOutputFile(argv[3], &someffs);
    KLOG;
    //if ((ret = open_input_file(argv[1], argv[2])) < 0)
        //goto end;
    KLOG;
    if ((ret = init_filters()) < 0)
        goto end;

    KLOG;

    /* read all packets */
    while (1) {
        AVStream *in_stream, *out_stream;
        if ((ret = av_read_frame(someffs.m_ifmt_ctx, &packet)) < 0)
            break;

        in_stream  = someffs.m_ifmt_ctx->streams[packet.stream_index];
        out_stream = outffs.m_ofmt_ctx->streams[videoindex_out];

        if(packet.pts == AV_NOPTS_VALUE) {
            printf("nopts_value \n");
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
        stream_index = packet.stream_index;
        type = someffs.m_ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
                stream_index);

        if(type == AVMEDIA_TYPE_VIDEO) {
            //Convert PTS/DTS
            packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
            packet.pos = -1;
            packet.stream_index=stream_index;

            printf("Write 1 Packet. size:%5d\tpts:%lld\n",packet.size,packet.pts);
            //Write
            if (av_interleaved_write_frame(outffs.m_ofmt_ctx, &packet) < 0) {
                printf( "Error muxing packet\n");
                break;
            }
        }
        else if(type == AVMEDIA_TYPE_AUDIO) {
            av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
            frame = av_frame_alloc();
            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            av_packet_rescale_ts(&packet,
                                 someffs.m_ifmt_ctx->streams[stream_index]->time_base,
                                 someffs.m_stream_ctx[stream_index].dec_ctx->time_base);
            dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
                avcodec_decode_audio4;
            ret = dec_func(someffs.m_stream_ctx[stream_index].dec_ctx, frame,
                           &got_frame, &packet);
            if (ret < 0) {
                av_frame_free(&frame);
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }

            if (got_frame) {
                frame->pts = frame->best_effort_timestamp;
                ret = filter_encode_write_frame(frame, stream_index);
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

    /* flush filters and encoders */
    for (i = 0; i < someffs.m_ifmt_ctx->nb_streams; i++) {
        //[> flush filter <]
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        //[> flush encoder <]
        ret = flush_encoder(i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }

    av_write_trailer(outffs.m_ofmt_ctx);

end:
    av_packet_unref(&packet);
    av_frame_free(&frame);
    for (i = 0; i < someffs.m_ifmt_ctx->nb_streams; i++) {
        avcodec_free_context(&someffs.m_stream_ctx[i].dec_ctx);
        if (outffs.m_ofmt_ctx && outffs.m_ofmt_ctx->nb_streams > i && outffs.m_ofmt_ctx->streams[i] && someffs.m_stream_ctx[i].enc_ctx)
            avcodec_free_context(&someffs.m_stream_ctx[i].enc_ctx);
        if (filter_ctx && filter_ctx[i].filter_graph)
            avfilter_graph_free(&filter_ctx[i].filter_graph);
    }
    av_free(filter_ctx);
    av_free(someffs.m_stream_ctx);
    avformat_close_input(&someffs.m_ifmt_ctx);
    if (outffs.m_ofmt_ctx && !(outffs.m_ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&outffs.m_ofmt_ctx->pb);
    avformat_free_context(outffs.m_ofmt_ctx);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred2: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}
