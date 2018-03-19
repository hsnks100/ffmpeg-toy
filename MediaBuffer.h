 typedef enum {  
      VideoMedia          = 0x01,  
      AudioMedia          = 0x02,  
      SubtitleMedia       = 0x04,  
 } MediaType;  
   
 class MediaBuffer {  
 public:  
      MediaType          mediaType;  
      unsigned char*     data;  
      int                size;  
      int                keyFrame;  
      int64_t            pts;  
      int64_t            dts;  
      
 public:  
      MediaBuffer() {  
           data = NULL;  
           size = keyFrame = 0;   
           pts = dts = 0;  
      }  
      virtual ~MediaBuffer() {
          if (data) {
              delete[] data;
              data = NULL;
          }
     }
 };    
