#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdcard_bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/sdmmc_host.h"

static const char *TAG = "_sdcard";

#define SDMMC_CMD_PIN   (gpio_num_t)3
#define SDMMC_D0_PIN    (gpio_num_t)5
#define SDMMC_D1_PIN    (gpio_num_t)6
#define SDMMC_D2_PIN    (gpio_num_t)42
#define SDMMC_D3_PIN    (gpio_num_t)2
#define SDMMC_CLK_PIN   (gpio_num_t)4

#define SDlist "/sdcard" //目录,类似于一个标准

sdmmc_card_t *card_host = NULL;


static float sdcard_get_value(void)
{
  if(card_host != NULL)
  {
    return (float)(card_host->csd.capacity)/2048/1024; //G
  }
  else
  return 0;
}

void _sdcard_init(void)
{
  esp_vfs_fat_sdmmc_mount_config_t mount_config = 
  {
    .format_if_mount_failed = false,       //如果挂靠失败，创建分区表并格式化SD卡
    .max_files = 5,                        //打开文件最大数
    .allocation_unit_size = 16 * 1024 *3,  //类似扇区大小
  };

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;//高速

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 4;           //4线
  slot_config.clk = SDMMC_CLK_PIN;
  slot_config.cmd = SDMMC_CMD_PIN;
  slot_config.d0 = SDMMC_D0_PIN;
  slot_config.d1 = SDMMC_D1_PIN;
  slot_config.d2 = SDMMC_D2_PIN;
  slot_config.d3 = SDMMC_D3_PIN;

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_fat_sdmmc_mount(SDlist, &host, &slot_config, &mount_config, &card_host));

  if(card_host != NULL)
  {
    sdmmc_card_print_info(stdout, card_host); //把卡的信息打印出来
    printf("practical_size:%.2fG\n",sdcard_get_value());//g
  }
}
/*写数据
path:路径
data:数据
*/
esp_err_t s_example_write_file(const char *path, char *data)
{
  esp_err_t err;
  if(card_host == NULL)
  {
    return ESP_ERR_NOT_FOUND;
  }
  err = sdmmc_get_status(card_host); //先检查是否有SD卡
  if(err != ESP_OK)
  {
    return err;
  }
  FILE *f = fopen(path, "w"); //获取路径地址
  if(f == NULL)
  {
    printf("path:Write Wrong path\n");
    return ESP_ERR_NOT_FOUND;
  }
  fprintf(f, data); //写入
  fclose(f);
  return ESP_OK;
}
/*
读数据
path:路径
*/
esp_err_t s_example_read_file(const char *path,uint8_t *pxbuf,uint32_t *outLen)
{
  esp_err_t err;
  if(card_host == NULL)
  {
    printf("path:card == NULL\n");
    return ESP_ERR_NOT_FOUND;
  }
  err = sdmmc_get_status(card_host); //先检查是否有SD卡
  if(err != ESP_OK)
  {
    printf("path:card == NO\n");
    return err;
  }
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    printf("path:Read Wrong path\n");
    return ESP_ERR_NOT_FOUND;
  }
  fseek(f, 0, SEEK_END);     //把指针移到最后面
  uint32_t unlen = ftell(f);
  //fgets(pxbuf, unlen, f); //读取 文本
  fseek(f, 0, SEEK_SET); //把指针移到最前面
  uint32_t poutLen = fread((void *)pxbuf,1,unlen,f);
  //printf("pxlen: %ld,outLen: %ld\n",unlen,poutLen);
  if(outLen != NULL)
  *outLen = poutLen;
  fclose(f);
  return ESP_OK;
}
/*
struct stat st;
stat(file_foo, &st);//获取文件信息 成功返回0  file_foo是字符串，文件名字需要后缀 可以表示是不是有该文件
unlink(file_foo);//删除文件 成功返回0
rename(char*,char*);//重命名文件
esp_vfs_fat_sdcard_format();//格式化
esp_vfs_fat_sdcard_unmount(mount_point, card);//卸载vfs
FILE *fopen(const char *filename, const char *mode);
"w": 以写入模式打开文件，如果文件存在，则清空文件内容；如果文件不存在，则创建新文件。
"a": 以追加模式打开文件，如果文件不存在，则创建新文件。
mkdir(dirname, mode);创建文件夹

读取其他不是文本类型数据"rb" 模式用于以只读和二进制的方式打开文件，适用于图像等二进制文件。
如果你只使用 "r"，则会以文本模式打开文件，这可能导致读取二进制文件时出现数据损坏或错误。
因此，对于图像文件（如 JPEG、PNG 等），你应该使用 "rb" 模式来确保正确读取文件内容。
b转换成二进制
通过下面两个函数可以进行文件大小返回
fseek(file, 0, SEEK_END)：将文件指针移动到文件的末尾。
ftell(file)；返回当前文件的指针位置，就是文件大小，按字节算
*/