
/*=======================================================================*/
//  ! INCLUDES
/*=======================================================================*/
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
//#include "app_common_config.h"
#include "rsi_common_apis.h"
#include "rsi_debug.h"
#include "freertos.h"
//! FatFS
#include "fatfs.h"
#include "ff.h"
#include "dummyfile.h"
#include "sdcard.h"

#define MAX_READ_BUF_LEN 4096
#define FASTCLKTIME(a) ((a) * 32 / 180)
/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/

const osThreadAttr_t sdcard_thread_attributes = {
  .name       = "sdcard_thread",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 8000,
  .priority   = osPriorityNormal,
  .tz_module  = 0,
  .reserved   = 0,
};


osThreadId_t SDinfoHandle;
osThreadId_t SDManagerHandle;
osMutexId_t SDCardMutexHandle;

extern char USERPath[4];   /* USER logical drive path */

FATFS FatFs;   // FATFS handle
FRESULT fres;  // Common result code
FILINFO fno;    // Structure holds information
FATFS *getFreeFs;     // Read information
static FIL fil;
DIR dir;        // Directory object structure
DWORD free_clusters;  // Free Clusters
DWORD free_sectors;   // Free Sectors
DWORD total_sectors;  // Total Sectors

static unsigned int read;
static unsigned int bytesWrote;

#if 0
static uint32_t sdcard_app_event_map;
/*==============================================*/
/**
 * @fn         sdcard_app_set_event
 * @brief      sets the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to set/raise the specific event.
 */
void sdcard_app_set_event(uint32_t event_num)
{
  sdcard_app_event_map |= event_num;

  osMutexRelease(SDCardMutexHandle);

  return;
}
#endif

void dmesg(FRESULT fres) {

  switch (fres) {
  case FR_OK:
    printf("Succeeded \r\n");
    break;
  case FR_DISK_ERR:
    printf("A hard error occurred in the low level disk I/O layer \r\n");
    break;
  case FR_INT_ERR:
    printf("Assertion failed \r\n");
    break;
  case FR_NOT_READY:
    printf("The physical drive cannot work\r\n");
    break;
  case FR_NO_FILE:
    printf("Could not find the file \r\n");
    break;
  case FR_NO_PATH:
    printf("Could not find the path \r\n");
    break;
  case FR_INVALID_NAME:
    printf("The path name format is invalid \r\n");
    break;
  case FR_DENIED:
    printf("Access denied due to prohibited access or directory full \r\n");
    break;
  case FR_EXIST:
    printf("Exist or access denied due to prohibited access \r\n");
    break;
  case FR_INVALID_OBJECT:
    printf("The file/directory object is invalid \r\n");
    break;
  case FR_WRITE_PROTECTED:
    printf("The physical drive is write protected \r\n");
    break;
  case FR_INVALID_DRIVE:
    printf("The logical drive number is invalid \r\n");
    break;
  case FR_NOT_ENABLED:
    printf("The volume has no work area \r\n");
    break;
  case FR_NO_FILESYSTEM:
    printf("There is no valid FAT volume \r\n");
    break;
  case FR_MKFS_ABORTED:
    printf("The f_mkfs() aborted due to any parameter error \r\n");
    break;
  case FR_TIMEOUT:
    printf(
        "Could not get a grant to access the volume within defined period \r\n");
    break;
  case FR_LOCKED:
    printf(
        "The operation is rejected according to the file sharing policy \r\n");
    break;
  case FR_NOT_ENOUGH_CORE:
    printf("LFN working buffer could not be allocated \r\n");
    break;
  case FR_TOO_MANY_OPEN_FILES:
    printf("Number of open files > _FS_SHARE \r\n");
    break;
  case FR_INVALID_PARAMETER:
    printf("Given parameter is invalid \r\n");
    break;
  default:
    printf("An error occured. (%d)\r\n", fres);
  }

}


void ls(char *path) {

  printf("\r\nFiles/Folder List:\r\n");
  fres = f_opendir(&dir, path);

  if (fres == FR_OK) {
    while (1) {

      fres = f_readdir(&dir, &fno);

      if ((fres != FR_OK) || (fno.fname[0] == 0)) {
        break;
      }
      printf(" %c%c%c%c%c %u-%02u-%02u, %02u:%02u %10d %s/%s\r\n",
          ((fno.fattrib & AM_DIR) ? 'D' : '-'),
          ((fno.fattrib & AM_RDO) ? 'R' : '-'),
          ((fno.fattrib & AM_SYS) ? 'S' : '-'),
          ((fno.fattrib & AM_HID) ? 'H' : '-'),
          ((fno.fattrib & AM_ARC) ? 'A' : '-'),
          ((fno.fdate >> 9) + 1980), (fno.fdate >> 5 & 15),
          (fno.fdate & 31), (fno.ftime >> 11), (fno.ftime >> 5 & 63),
          (int) fno.fsize, path, fno.fname);
    }
  }
  printf("\r\n");
}

void StartSDinfo(void const *argument)
{

  UNUSED_PARAMETER(argument);
  /* USER CODE BEGIN StartSDinfo */
  uint16_t cnt=0;

  printf("\nStartSDinfoTask!\n");

  /* Infinite loop */
  for (;;) {

      osMutexAcquire(SDCardMutexHandle, portMAX_DELAY);

      printf("\n[SDinfo%03d]: Mount SDCard", cnt++);
      fres = f_mount(&FatFs, (const TCHAR*) USERPath, 1); // 1 -> Mount now
      if (fres == FR_OK)
      {
        // Get some statistics from the SD card
        fres = f_getfree("", &free_clusters, &getFreeFs);
        // Formula comes from ChaN's documentation
        total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
        free_sectors = free_clusters * getFreeFs->csize;
        printf("\n[SDinfo]: SD Card Status:\r\n\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n",
          total_sectors / 2, free_sectors / 2);
        osDelay(100);
        // List files and folder in root
        printf("\n[SDinfo111]: ");
        ls("");
      }
      else
      {
        printf("\n[SDinfo222]: ");
        dmesg(fres);
      }
      f_mount(NULL, "", 0);
      printf("\n[SDinfo]: Unmount SDCard\r\n");
      osMutexRelease(SDCardMutexHandle);
      osDelay(portMAX_DELAY);
  }
  /* USER CODE END StartSDinfo */
}


void StartSDManager(void const *argument) {

  UNUSED_PARAMETER(argument);
  //int32_t event_id = 0;


  uint8_t* dummy_text = init_dummy_text();
  //const uint8_t max_rbuf_len = 128;

  uint8_t readBuf[MAX_READ_BUF_LEN] = {0};
  //uint8_t writeBuf[30] = {0};

  /* Infinite loop */
  printf("\r\nStartSDManager!\r\n");

  for (;;) {

    osMutexAcquire(SDCardMutexHandle, portMAX_DELAY);

    printf("Mounting SDCard\r\n");
    printf("USERPath: %s\r\n", USERPath);

    fres = f_mount(&FatFs, (const TCHAR*) USERPath, 1); // 1 -> Mount now
    if (fres == FR_OK)
    {
        printf("Mount success\r\n");
        ls("");

#if 0
        // Create Folder
        fres = f_mkdir("DEMO");
        if (fres == FR_OK) {
            printf("\n[SDManager]: Create Folder");
        } else {
            printf("\n[SDManager][Create_Folder]: ");
          dmesg(fres);
        }
#endif


#if 0
        // Read File
        fres = f_open(&fil, "WRITE3.TXT", FA_OPEN_ALWAYS |FA_READ );
        if (fres == FR_OK)
        {
          //int res = f_gets((TCHAR*)readBuf, 30, &fil);
            uint32_t start_time = osKernelGetTickCount();
            uint32_t total_byteread = 0;
            int res;

            do{
                res = f_read(&fil, readBuf, MAX_READ_BUF_LEN, &read);
                if (res == FR_OK) {
                    //printf("\n[SDManager]: Read File res:%d recvLen:%d \r\n", res, read);
                    //printf("\n[SDManager]: 'test.txt' contents: %s\r\n", readBuf);
                    total_byteread += read;
                }
                else
                {
                    printf("\n[SDManager] Read File failure:%d", fres);
                    dmesg(res);
                }
            }while(!f_eof(&fil));

            uint32_t duration   = osKernelGetTickCount() - start_time;


          printf("\r\n Read File Duration: (%lu ms), total read: %lu byte\r\n", FASTCLKTIME(duration), total_byteread);
          printf("read buffer size: %lu\r\n",MAX_READ_BUF_LEN);
          printf("Tick Freq: (%lu hz)\r\n",osKernelGetTickFreq());
          printf("Sys Timer Freq: (%lu hz)\r\n",osKernelGetSysTimerFreq());
        }
        else
        {
          printf("\n[SDManager][Read_File]: ");
          dmesg(fres);
        }
        f_close(&fil);
        osDelay(1000);
#endif

#if 1
      fres = f_unlink("write3.txt");
      if (fres != FR_OK)
      { dmesg(fres); }
      else
      { printf("unlink write3.txt\r\n");}

      // Write File
      fres = f_open(&fil, "write3.txt", FA_WRITE | FA_OPEN_ALWAYS );
      if (fres == FR_OK) {
        //Copy in a string
        //strncpy((char*) writeBuf, "I hate Java!", 13);
        //uint8_t bytesWrote;
        uint32_t start_time = osKernelGetTickCount();

        uint32_t total_bytes = 0;
#if 0
        fres = f_write(&fil, dummy_file, dummy_file_len, &bytesWrote);
        total_bytes = bytesWrote;
#else
        for (uint32_t i = 0; i < DUMMY_ITERATION; i++){
            fres = f_write(&fil, dummy_text, DUMMY_TEXT_LEN, &bytesWrote);

            if (fres != FR_OK)
            { dmesg(fres); }
        }

        total_bytes = DUMMY_WRITE_LEN;
#endif
        uint32_t duration   = osKernelGetTickCount() - start_time;
        if (fres == FR_OK) {
            printf("\n[SDManager]: Write File");
            printf("\n[SDManager]: Wrote %lu bytes to 'write3.txt'!\r\n", total_bytes);
        } else {
            printf("\n[SDManager][Write_File]: ");
          dmesg(fres);
        }
        printf("\r\nWrite duration: (%lu ms)\r\n", FASTCLKTIME(duration));
        printf("write buffer size: %lu\r\n", (uint32_t)DUMMY_TEXT_LEN);
        printf("Tick Freq: (%lu hz)\r\n",osKernelGetTickFreq());
        printf("Sys Timer Freq: (%lu hz)\r\n",osKernelGetSysTimerFreq());
      } else {
          printf("\n[SDManager][Create_File]: ");
        dmesg(fres);
      }

      fres=f_close(&fil);
      dmesg(fres);

      printf("USERPath: %s\r\n", USERPath);
#endif

    } else {
      dmesg(fres);
    }

    ls("");

    fres=f_unmount((const TCHAR*) USERPath);
    dmesg(fres);
    printf("\n[SDManager]: Unmount SDCard");


    osMutexRelease(SDCardMutexHandle);
    osDelay(portMAX_DELAY);
  }

  /* USER CODE END StartSDManager */
}


FRESULT sdcard_write(const void* data, const unsigned int len)
{
  return f_write(&fil, data, len, &bytesWrote);
}


FIL* sdcard_get_wfile(){
  return &fil;
}

void fatfs_sdcard_init(void)
{

  MX_FATFS_Init();  /*retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);*/

  printf("\r\nStartSDManager!\r\n");

  printf("Mounting SDCard\r\n");
  printf("USERPath: %s\r\n", USERPath);

  fres = f_mount(&FatFs, (const TCHAR*) USERPath , 1); // 1 -> Mount now
  if (fres == FR_OK)
  {
      printf("Mount success\r\n");
      ls("");
      printf("\r\n");
  }
  dmesg(fres);

  char ofile_name[] = "write2.txt";
  fres = f_unlink(ofile_name);
  if (fres == FR_OK) {
        printf("\r\nUnlink File \"%s\" ", ofile_name);
        bytesWrote = 0; //RESET
    }
    dmesg(fres);

  // Write File
  fres = f_open(&fil, ofile_name, FA_WRITE | FA_OPEN_ALWAYS );
  if (fres == FR_OK) {
      printf("\r\nFile \"%s\" open ", ofile_name);
      bytesWrote = 0; //RESET
  }
  dmesg(fres);

  printf("USERPath: %s\r\n", USERPath);
}

void sdcard_ends(){
  FRESULT fres = f_close(&fil);
  if (fres == FR_OK) {
      printf("\r\nFile closed ");
  }
  dmesg(fres);

  ls("");

  printf("USERPath: %s\r\n", USERPath);
  fres = f_unmount((const TCHAR*) USERPath);
  if (fres == FR_OK) {
      printf("\n[SDManager]: Unmount SDCard ");
  }
  dmesg(fres);

  printf("USERPath: %s\r\n", USERPath);
  if(FATFS_UnLinkDriver(USERPath) == 0)
  { printf("FATFS_UnLinkDriver ok\r\n"); }
  else
  { printf("FATFS_UnLinkDriver failed\r\n"); }
  printf("USERPath: %s\r\n", USERPath);

}

