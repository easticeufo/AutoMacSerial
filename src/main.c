
/**@file 
 * @note tiansixinxi. All Right Reserved.
 * @brief  
 * 
 * @author  madongfang
 * @date     2016-5-26
 * @version  V1.0.0
 * 
 * @note ///Description here 
 * @note History:        
 * @note     <author>   <time>    <version >   <desc>
 * @note  
 * @warning  
 */

#include "base_fun.h"
#include "config_plist_opt.h"

#define BOARD_SERIAL_NUMBER "C0732950A19F1HCAF"
#define SERIAL_NUMBER "C07L40J4H884"
#define CONFIG_PLIST_FILE_PATH "/Volumes/EFI/EFI/CLOVER/config.plist"
#define ROM_ID "44FB4292D612"

/**
 * @brief 用户功能初始化
 * @param[in] argc 命令行中参数的个数
 * @param[in] argv 命令行参数数组
 * @return 无
 */
void user_fun(INT32 argc, INT8 *argv[])
{
	UINT8 rom[8] = {0};
	INT32 len = 0;
	INT8 rom_base64[16] = {0};
	system("diskutil mount disk0s1");
	sleep(3);

	if (!plist_init(CONFIG_PLIST_FILE_PATH))
	{
		return;
	}

	len = str2hex(ROM_ID, strlen(ROM_ID), rom, sizeof(rom));
	base64_encode(rom, rom_base64);
	plist_set_data_value("ROM", rom_base64);
	plist_set_string_value("BoardSerialNumber", BOARD_SERIAL_NUMBER);
	plist_set_string_value("SerialNumber", SERIAL_NUMBER);
	if (!plist_save(CONFIG_PLIST_FILE_PATH))
	{
		plist_destroy();
		return;
	}
	plist_destroy();

	system("diskutil umount disk0s1");
	sync();

	sleep(2);

	if (reboot(RB_AUTOBOOT) == ERROR) // 成功重启此函数不会返回
	{
		DEBUG_PRINT(DEBUG_ERROR, "reboot failed: %s\n", strerror(errno));
	}

	return;
}

/**
 * @brief 用户系统初始化
 * @param[in] argc 命令行中参数的个数
 * @param[in] argv 命令行参数数组
 * @return 无
 */
void user_system_init(INT32 argc, INT8 *argv[])
{
	user_fun(argc, argv);
	return;
}

void print_usage(INT8 *name)
{
	printf("\nUsage:\n\n"
		"  %s [option] [parameter]\n\n"
		"  Options:\n"
		"    -debug level #set debug print level: 0-DEBUG_NONE 1-DEBUG_ERROR 2-DEBUG_WARN 3-DEBUG_NOTICE 4-DEBUG_INFO"
		"\n\n"
		, name);
	
	return;
}

/**		  
 * @brief		主函数入口
 * @param[in] argc 命令行中参数的个数
 * @param[in] argv 命令行参数数组
 * @return OK 
 */
INT32 main(INT32 argc, INT8 *argv[])
{
	INT32 i = 0;

	/* 忽略某些信号，防止进程被这些信号终止 */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-debug") == 0)
		{
			i++;
			if (i >= argc)
			{
				print_usage(argv[0]);
				return ERROR;
			}
			set_debug_level(atoi(argv[i]));
		}
	}
	
	user_system_init(argc, argv);
	
	return OK;
}
