#ifndef __FILE_WRITER_H__  
 #define __FILE_WRITER_H__  
   
 #include "MediaBuffer.h"  
   
 extern "C" {  
 #include "libavformat/avformat.h"  
 };  
   
 class FileWriterOpenParam {  
 public:  
      enum AVCodecID     video_codec_id;  
      int                    width;  
      int                    height;  
      AVRational          frame_rate;  
      unsigned int     gop_size;  
      enum AVCodecID     audio_codec_id;  
      enum AVSampleFormat sample_fmt;  
      int                    channels;  
      int                    sample_rate;  
      unsigned char*     video_extradata;  
      int                    video_extradata_size;  
      unsigned char*     audio_extradata;  
      int                    audio_extradata_size;  
      int                    filepath_format;  
   
      int                    buffering_time;  
   
      FileWriterOpenParam();  
      FileWriterOpenParam(FileWriterOpenParam *param);  
      virtual ~FileWriterOpenParam();  
 };  
   
 enum FILE_SPLIT { SPLIT_DURATION, SPLIT_LOCALTIME };  
   
 class FileWriter  
 {  
 protected:  
      FileWriter();       
 public:  
      virtual ~FileWriter();  
   
 public:  
      virtual int open(char *filepath, FileWriterOpenParam &param);  
      virtual int write(MediaBuffer *pBuffer);  
      virtual void close();  
   
 protected:  
      int64_t     m_nStartPTS;  
      int64_t     m_nStartDTS;  
   
      int          m_nFormat;  
   
      unsigned char*     m_pVideoExtraData;  
      int                m_nVideoExtraDataSize;  

      unsigned char*     m_pAudioExtraData;
      int                m_nAudioExtraDataSize;
 };  
   
   
 #endif  
