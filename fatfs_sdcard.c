
/*=======================================================================*/
//  ! INCLUDES
/*=======================================================================*/
#include "cmsis_os2.h"

#include <stdio.h>
#include <string.h>

#include "app_common_config.h"
#include "rsi_common_apis.h"



#include "rsi_debug.h"

#include "freertos.h"

//! FatFS
#include "fatfs.h"
#include "ff.h"





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



FATFS FatFs;   // FATFS handle
FRESULT fres;  // Common result code
FILINFO fno;    // Structure holds information
FATFS *getFreeFs;     // Read information
FIL fil;
DIR dir;        // Directory object structure
DWORD free_clusters;  // Free Clusters
DWORD free_sectors;   // Free Sectors
DWORD total_sectors;  // Total Sectors





void dmesg(FRESULT fres) {

  switch (fres) {
  case FR_OK:
    printf("Succeeded \n");
    break;
  case FR_DISK_ERR:
    printf("A hard error occurred in the low level disk I/O layer \n");
    break;
  case FR_INT_ERR:
    printf("Assertion failed \n");
    break;
  case FR_NOT_READY:
    printf("The physical drive cannot work");
    break;
  case FR_NO_FILE:
    printf("Could not find the file \n");
    break;
  case FR_NO_PATH:
    printf("Could not find the path \n");
    break;
  case FR_INVALID_NAME:
    printf("The path name format is invalid \n");
    break;
  case FR_DENIED:
    printf("Access denied due to prohibited access or directory full \n");
    break;
  case FR_EXIST:
    printf("Exist or access denied due to prohibited access \n");
    break;
  case FR_INVALID_OBJECT:
    printf("The file/directory object is invalid \n");
    break;
  case FR_WRITE_PROTECTED:
    printf("The physical drive is write protected \n");
    break;
  case FR_INVALID_DRIVE:
    printf("The logical drive number is invalid \n");
    break;
  case FR_NOT_ENABLED:
    printf("The volume has no work area");
    break;
  case FR_NO_FILESYSTEM:
    printf("There is no valid FAT volume");
    break;
  case FR_MKFS_ABORTED:
    printf("The f_mkfs() aborted due to any parameter error \n");
    break;
  case FR_TIMEOUT:
    printf(
        "Could not get a grant to access the volume within defined period \n");
    break;
  case FR_LOCKED:
    printf(
        "The operation is rejected according to the file sharing policy \n");
    break;
  case FR_NOT_ENOUGH_CORE:
    printf("LFN working buffer could not be allocated \n");
    break;
  case FR_TOO_MANY_OPEN_FILES:
    printf("Number of open files > _FS_SHARE \n");
    break;
  case FR_INVALID_PARAMETER:
    printf("Given parameter is invalid \n");
    break;
  default:
    printf("An error occured. (%d)\n", fres);
  }

}


void ls(char *path) {

  printf("Files/Folder List:\n");
  fres = f_opendir(&dir, path);

  if (fres == FR_OK) {
    while (1) {

      fres = f_readdir(&dir, &fno);

      if ((fres != FR_OK) || (fno.fname[0] == 0)) {
        break;
      }
      printf("\n %c%c%c%c%c %u-%02u-%02u, %02u:%02u %10d %s/%s",
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
      fres = f_mount(&FatFs, "", 1); // 1 -> Mount now
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
      printf("\n[SDinfo]: Unmount SDCard");
      osMutexRelease(SDCardMutexHandle);
      osDelay(portMAX_DELAY);
  }
  /* USER CODE END StartSDinfo */
}


void StartSDManager(void const *argument) {

  UNUSED_PARAMETER(argument);
  unsigned int read;
  /* Infinite loop */
  printf("\nStartSDManager!\n");

  for (;;) {

    osMutexAcquire(SDCardMutexHandle, portMAX_DELAY);

    printf("\n[SDManager]: Mount SDCard\r\n");

    fres = f_mount(&FatFs, "", 1); // 1 -> Mount now
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
      uint8_t readBuf[30];

#if 1
      // Read File
      fres = f_open(&fil, "TEST.TXT", FA_OPEN_ALWAYS |FA_WRITE );
      if (fres == FR_OK)
      {
        //int res = f_gets((TCHAR*)readBuf, 30, &fil);
          int res = f_read(&fil, readBuf, 30, &read);
          printf("\r\nres:%d recvLen:%d", res, read);
        if (res != 0) {
            printf("\n[SDManager]: Read File");
            printf("\n[SDManager]: 'test.txt' contents: %s\r\n",
              readBuf);
        }
        else
        {
            printf("\n[SDManager] f_open failure:%d", fres);
        }
      }
      else
      {
        printf("\n[SDManager][Read_File]: ");
        dmesg(fres);
      }
      f_close(&fil);
      osDelay(100);
#endif

#if 0
      // Write File
      fres = f_open(&fil, "write.txt",
      FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
      if (fres == FR_OK) {
        //Copy in a string
        strncpy((char*) readBuf, "I hate Java!", 12);
        uint8_t bytesWrote;
        fres = f_write(&fil, readBuf, 12, &bytesWrote);
        if (fres == FR_OK) {
            printf("\n[SDManager]: Write File");
            printf("\n[SDManager]: Wrote %i bytes to 'write.txt'!\r\n",
              bytesWrote);
        } else {
            printf("\n[SDManager][Write_File]: ");
          dmesg(fres);
        }
      } else {
          printf("\n[SDManager][Create_File]: ");
        dmesg(fres);
      }
      f_close(&fil);
#endif

    } else {
        printf("\n[SDManager]: ");
      dmesg(fres);
    }

    f_mount(NULL, "", 0);
    printf("\n[SDManager]: Unmount SDCard");
    osMutexRelease(SDCardMutexHandle);
    osDelay(portMAX_DELAY);
  }

  /* USER CODE END StartSDManager */
}


void fatfs_sdcard_init(void)
{

  MX_FATFS_Init();

  SDCardMutexHandle = osMutexNew(NULL);
  //osMutexRelease(SDCardMutexHandle);

#if 0
  /* definition and creation of SDinfo */
  //osThreadDef(SDinfo, StartSDinfo, osPriorityHigh, 0, 140);
  SDinfoHandle = osThreadNew((osThreadFunc_t)StartSDinfo, NULL, &sdcard_thread_attributes);
#else
  SDManagerHandle = osThreadNew((osThreadFunc_t)StartSDManager, NULL, &sdcard_thread_attributes);
#endif
  /* definition and creation of SDManager */
  //osThreadDef(SDManager, StartSDManager, osPriorityRealtime, 0, 300);


}
