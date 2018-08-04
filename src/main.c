
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

#define CONFIG_PLIST_FILE_PATH "/Volumes/EFI/EFI/CLOVER/config.plist"
#define SERVER_ADDR "13.209.75.83"
#define SERVER_PORT 19871

#define CMD_GET_5CODES "5codes"

BOOL get_hardware_code_string(INT8 *p_buff, INT32 buff_len)
{
	INT32 sock_fd = 0;
	INT32 head = 0;
	INT32 recv_len = 0;
	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (ERROR == sock_fd)
	{
		DEBUG_PRINT(DEBUG_ERROR, "socket failed: %s\n", strerror(errno));
		return FALSE;
	}

	if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == ERROR)
	{
		DEBUG_PRINT(DEBUG_ERROR, "connect failed: %s\n", strerror(errno));
		SAFE_CLOSE(sock_fd);
		return FALSE;
	}
	
	/* 发送请求命令 */
	head = htonl(strlen(CMD_GET_5CODES));
	if (writen(sock_fd, &head, 4) < 0)
	{
		DEBUG_PRINT(DEBUG_ERROR, "sock_fd writen failed!\n");
		SAFE_CLOSE(sock_fd);
		return FALSE;
	}
	if (writen(sock_fd, CMD_GET_5CODES, strlen(CMD_GET_5CODES)) < 0)
	{
		DEBUG_PRINT(DEBUG_ERROR, "sock_fd writen failed!\n");
		SAFE_CLOSE(sock_fd);
		return FALSE;
	}

	/* 接收响应 */
	if (readn(sock_fd, &head, 4) != 4)
	{
		DEBUG_PRINT(DEBUG_ERROR, "readn failed!\n");
		SAFE_CLOSE(sock_fd);
		return FALSE;
	}
	recv_len = ntohl(head);
	memset(p_buff, 0, buff_len);
	if (recv_len < buff_len)
	{
		if (readn(sock_fd, p_buff, recv_len) != recv_len)
		{
			DEBUG_PRINT(DEBUG_ERROR, "readn failed!\n");
			SAFE_CLOSE(sock_fd);
			return FALSE;
		}
	}
	else
	{
		if (readn(sock_fd, p_buff, buff_len-1) != (buff_len-1))
		{
			DEBUG_PRINT(DEBUG_ERROR, "readn failed!\n");
			SAFE_CLOSE(sock_fd);
			return FALSE;
		}
	}

	SAFE_CLOSE(sock_fd);
	return TRUE;
}

/**
 * @brief 用户功能初始化
 * @param[in] argc 命令行中参数的个数
 * @param[in] argv 命令行参数数组
 * @return 无
 */
void user_fun(INT32 argc, INT8 *argv[])
{
	INT8 hardware_code_string[128] = {0};
	INT8 *p_rom = NULL;
	INT8 *p_board_serial_number = NULL;
	INT8 *p_serial_number = NULL;
	INT8 *ptr = NULL;
	UINT8 rom[8] = {0};
	INT32 len = 0;
	INT8 rom_base64[16] = {0};
	INT32 i = 0;
	INT8 *p_external_program_path = NULL;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-e") == 0)
		{
			i++;
			if (i < argc)
			{
				p_external_program_path = argv[i];
			}
			break;
		}
	}

	if (!get_hardware_code_string(hardware_code_string, sizeof(hardware_code_string)))
	{
		DEBUG_PRINT(DEBUG_ERROR, "get_hardware_code_string failed\n");
		return;
	}

	DEBUG_PRINT(DEBUG_NOTICE, "get_hardware_code_string=%s\n", hardware_code_string);
	ptr = strchr(hardware_code_string, ':');
	if (NULL == ptr)
	{
		DEBUG_PRINT(DEBUG_NOTICE, "there is no more hardware code\n");
		return;
	}
	*ptr = '\0';
	p_rom = hardware_code_string;
	p_board_serial_number = ptr + 1;
	ptr = strchr(p_board_serial_number, ':');
	if (NULL == ptr)
	{
		DEBUG_PRINT(DEBUG_WARN, "hardware_code_string format error\n");
		return;
	}
	*ptr = '\0';
	p_serial_number = ptr + 1;
	ptr = strchr(p_serial_number, ':');
	if (NULL == ptr)
	{
		DEBUG_PRINT(DEBUG_WARN, "hardware_code_string format error\n");
		return;
	}
	*ptr = '\0';
	DEBUG_PRINT(DEBUG_NOTICE, "rom=%s,board_serial_number=%s,serial_number=%s\n", 
		p_rom, p_board_serial_number, p_serial_number);

	system("diskutil mount disk0s1");
	sleep(3);

	if (!plist_init(CONFIG_PLIST_FILE_PATH))
	{
		return;
	}

	len = str2hex(p_rom, strlen(p_rom), rom, sizeof(rom));
	base64_encode(rom, rom_base64);
	plist_set_data_value("ROM", rom_base64);
	plist_set_string_value("BoardSerialNumber", p_board_serial_number);
	plist_set_string_value("SerialNumber", p_serial_number);
	if (!plist_save(CONFIG_PLIST_FILE_PATH))
	{
		plist_destroy();
		return;
	}
	plist_destroy();

	system("diskutil umount disk0s1");

	if (NULL != p_external_program_path)
	{
		sleep(2);
		system(p_external_program_path);
	}

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
			break;
		}
	}
	
	user_system_init(argc, argv);
	
	return OK;
}
