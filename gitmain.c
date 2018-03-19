/*
 * @file main.c
 * @author Akagi201
 * @date 2014/10/10
 *
 * Mux a H.264 bitstream (in MPEG2TS) and
 * a AAC bitstream file together into MP4 format file.
 */

#include <stdio.h>

#include <libavformat/avformat.h>

/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need 
"h264_mp4toannexb" bitstream filter (BSF)
  *Add SPS,PPS in front of IDR frame
  *Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter 
#define USE_H264BSF 0

/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need 
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter 
#define USE_AACBSF 0



static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *dec_ctx = NULL;
static int vst_idx = -1;
int main(int argc, char* argv[])
{
    //////////////////////////////
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex_v=-1,videoindex_out=-1;
	int audioindex_a=-1,audioindex_out=-1;
	int frame_index=0;
	int64_t cur_pts_v=0,cur_pts_a=0;

	//const char *in_filename_v = "cuc_ieschool.ts";//Input file URL
	const char *in_filename_v = "ksoo.h264";
	//const char *in_filename_a = "cuc_ieschool.mp3";
	//const char *in_filename_a = "gowest.m4a";
	//const char *in_filename_a = "gowest.aac";
    const char *in_filename_a = "t2.mp3";
	/*const char *in_filename_a = "audio_chn0.pcm";*/

	const char *out_filename = "cuc_ieschool.mp4";//Output file URL
	av_register_all();
	//Input
    avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0);
    avformat_find_stream_info(ifmt_ctx_v, 0);
    avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0);
    avformat_find_stream_info(ifmt_ctx_a, 0);

    //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    AVCodec* dec;
    vst_idx = av_find_best_stream(ifmt_ctx_v, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    dec_ctx = ifmt_ctx_v->streams[vst_idx]->codec;
    avcodec_open2(dec_ctx, dec, NULL);
    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	printf("===========Input Information==========\n");
	av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
	av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
	printf("======================================\n");
	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	AVOutputFormat *ofmt = NULL;
	ofmt = ofmt_ctx->oformat;

    //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    AVCodec *enc;
    AVCodecContext *enc_ctx = NULL;

    /* find the h.264 video encoder */
    enc = avcodec_find_encoder(AV_CODEC_ID_H264);

    enc_ctx = avcodec_alloc_context3(enc);

    /* put sample parameters */
    enc_ctx->bit_rate = 400000;
    /* resolution must be a multiple of two */
    enc_ctx->width = 352;
    enc_ctx->height = 288;
    /* frames per second */
    enc_ctx->time_base= (AVRational){1,25};
    enc_ctx->gop_size = 10; /* emit one intra frame every ten frames */
    enc_ctx->max_b_frames=1;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
 
    /* init the video encoder */
    avcodec_open2(enc_ctx, enc, NULL);
    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
		//Create output AVStream according to input AVStream
        if(ifmt_ctx_v->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            AVStream *in_stream = ifmt_ctx_v->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            videoindex_v=i;
            if (!out_stream) {
                printf( "Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            videoindex_out=out_stream->index;
            //Copy the settings of AVCodecContext
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                printf( "Failed to copy context from input to output stream codec context\n");
                goto end;
            }
            out_stream->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            break;
        }
	}

	for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if(ifmt_ctx_a->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			AVStream *in_stream = ifmt_ctx_a->streams[i];
			AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
			audioindex_a=i;
			if (!out_stream) {
				printf( "Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				goto end;
			}
			audioindex_out=out_stream->index;
			//Copy the settings of AVCodecContext
			if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
				printf( "Failed to copy context from input to output stream codec context\n");
				goto end;
			}
			out_stream->codec->codec_tag = 0;
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

			break;
		}
	}

	printf("==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	printf("======================================\n");
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
			printf( "Could not open output file '%s'", out_filename);
			goto end;
		}
	}
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf( "Error occurred when opening output file\n");
		goto end;
	}


	//FIX
#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb"); 
#endif
#if USE_AACBSF
	AVBitStreamFilterContext* aacbsfc =  av_bitstream_filter_init("aac_adtstoasc"); 
#endif

	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index=0;
		AVStream *in_stream, *out_stream;

		//Get an AVPacket
        if(av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){
            ifmt_ctx=ifmt_ctx_v;
            stream_index=videoindex_out;

            if(av_read_frame(ifmt_ctx, &pkt) >= 0){
                do{
                    in_stream  = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if(pkt.stream_index==videoindex_v){
                        //FIX��No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if(pkt.pts==AV_NOPTS_VALUE){
                            //Write PTS
                            AVRational time_base1=in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            pkt.dts=pkt.pts;
                            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v=pkt.pts;
                        break;
                    }
                }while(av_read_frame(ifmt_ctx, &pkt) >= 0);
            }else{
                break;
            }
        } 
        else {
			ifmt_ctx=ifmt_ctx_a;
			stream_index=audioindex_out;
			if(av_read_frame(ifmt_ctx, &pkt) >= 0){
				do{
					in_stream  = ifmt_ctx->streams[pkt.stream_index];
					out_stream = ofmt_ctx->streams[stream_index];

					if(pkt.stream_index==audioindex_a){

						//FIX��No PTS
						//Simple Write PTS
						if(pkt.pts==AV_NOPTS_VALUE){
							//Write PTS
							AVRational time_base1=in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts=pkt.pts;
							pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}
						cur_pts_a=pkt.pts;

						break;
                    }
                }while(av_read_frame(ifmt_ctx, &pkt) >= 0);
            }else{
                break;
            } 
        }

		//FIX:Bitstream Filter
#if USE_H264BSF
		av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
		av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif


		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=stream_index;

		printf("Write 1 Packet. size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf( "Error muxing packet\n");
			break;
		}
		av_free_packet(&pkt);

	}
	//Write file trailer
	av_write_trailer(ofmt_ctx);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

end:
	avformat_close_input(&ifmt_ctx_v);
	avformat_close_input(&ifmt_ctx_a);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		printf( "Error occurred.\n");
		return -1;
	}
	return 0;
}
///////////////////////////
