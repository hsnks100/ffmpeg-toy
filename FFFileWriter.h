#ifndef __FFFILE_WRITER_H__  
 #define __FFFILE_WRITER_H__  
   
 #include "FileWriter.h"  
   
 class FFFileWriter : public FileWriter  
 {  
 public:  
      FFFileWriter();  
      virtual ~FFFileWriter();  
   
      virtual int open(char *filepath, FileWriterOpenParam &param);  
      virtual int write(MediaBuffer *pBuffer);  
      virtual void close();  
   
 protected:  
      AVStream* addStream(enum AVMediaType mediaType, enum AVCodecID codec_id, FileWriterOpenParam &param);  
   
 protected:  
      AVFormatContext*     m_pFormatCtx;  
      AVStream*               m_pVideoStream;  
      AVStream*               m_pAudioStream;  
      AVPacket               m_avPacket;  
      FILE*                    m_pFile;  
   
      bool                    m_bWriteHeader;  
 };  
   
 #endif  
