#ifndef LGIT_9X28_API_H
#define LGIT_9X28_API_H
/*
   return value : 

   RET_SUCCE_CODE_0, 0x00, Firmware update success
   RET_ERROR_CODE_1, 0x01, Error to search modem device
   RET_ERROR_CODE_2, 0x02, Error to open or close modem port
   RET_ERROR_CODE_3, 0x03, Error command returned from modem
   RET_ERROR_CODE_4, 0x04, Error to open file (hex)
   RET_ERROR_CODE_5, 0x05, Error returned During the download the binary (mbn)
   RET_ERROR_CODE_6, 0x06, Error to exist no modem binary (mbn)
   RET_ERROR_CODE_7, 0x07, Argument count Error
   RET_ERROR_CODE_8, 0x08, Timeout Range Error (Min Seconds : 300s, Max Seconds : 600s)
   RET_ERROR_CODE_9, 0x09, Timeout Error
   */

int requestUpdateModem(char *pathdir,int timeout,int force_update);

bool log_start(char *pathdir);
bool log_end();

#endif
