/**
*
*/

#include "asf.h"
#include "common/include/nm_common.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "main.h"
#include "conf_at25dfx.h"
#include <ctype.h>


#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- WINC1500 HTTP provision with TCP Server and Serial Flash Memory --"STRING_EOL	\
"-- "BOARD_NAME " --"STRING_EOL	\
"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

/** UART module for debug. */
static struct usart_module cdc_uart_module;

static char uart_buffer[MAIN_CHAT_BUFFER_SIZE];

/** Written size to UART buffer. */
static uint16_t uart_buffer_written = 0;

/** Buffer of a character from the serial. */
static uint16_t uart_ch_buffer;

long long int IPnumber;

static SOCKET tcp_server_socket = -1;

#define AT25DFX_BUFFER_SIZE  (100)

static SOCKET tcp_client_socket_external = -1;

static uint8_t tcp_connected = 0;

static uint8_t wifi_connected;

static uint8_t gau8SocketBuffer[MAIN_CHAT_BUFFER_SIZE];

static SOCKET tcp_client_socket = -1;

static uint8_t gau8SocketTestBuffer[MAIN_WIFI_M2M_BUFFER_SIZE];

char* CommandArray[4];

char *SSID_Read[AT25DFX_BUFFER_SIZE];

void eraseSerialBuffer(void);

void sendTCP(char* message);

int commandTCP(char* IPAddress, int port);

long long int calculateIP(char *IPstring);

#define AT25DFX_BUFFER_SIZE  (100)

uint8_t *IPAddress;

char *SSID;

char *Password;

uint8_t *IP_read[AT25DFX_BUFFER_SIZE];

char *SSID_read[AT25DFX_BUFFER_SIZE];

char *SSID_read2[AT25DFX_BUFFER_SIZE];

char *Password_read[AT25DFX_BUFFER_SIZE];

struct spi_module at25dfx_spi;

struct at25dfx_chip_module at25dfx_chip;

typedef struct s_msg_wifi_product {
	uint8_t name[9];
} t_msg_wifi_product;

static t_msg_wifi_product msg_wifi_product = {
	.name = MAIN_WIFI_M2M_PRODUCT_NAME,
};


static void uart_callback(const struct usart_module *const module)
{
	static uint8_t ignore_cnt = 0;
	if (ignore_cnt > 0) {
		ignore_cnt--;
		return;
		} else if (uart_ch_buffer == 0x1B) { /* Ignore escape and following 2 characters. */
		ignore_cnt = 2;
		return;
		} else if (uart_ch_buffer == 0x8) { /* Ignore backspace. */
		return;
	}
	/* If input string is bigger than buffer size limit, ignore the excess part. */
	if (uart_buffer_written < MAIN_CHAT_BUFFER_SIZE) {
		uart_buffer[uart_buffer_written++] = uart_ch_buffer & 0xFF;
	}
}


/**
* \brief Configure UART console.
*/
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	/* Register USART callback for receiving user input. */
	usart_register_callback(&cdc_uart_module, (usart_callback_t)uart_callback, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback(&cdc_uart_module, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable(&cdc_uart_module);
}

#define HEX2ASCII(x) (((x) >= 10) ? (((x) - 10) + 'A') : ((x) + '0'))
static void set_dev_name_to_mac(uint8 *name, uint8 *mac_addr)
{
	/* Name must be in the format WINC1500_00:00 */
	uint16 len;

	len = m2m_strlen(name);
	if (len >= 5) {
		name[len - 1] = HEX2ASCII((mac_addr[5] >> 0) & 0x0f);
		name[len - 2] = HEX2ASCII((mac_addr[5] >> 4) & 0x0f);
		name[len - 4] = HEX2ASCII((mac_addr[4] >> 0) & 0x0f);
		name[len - 5] = HEX2ASCII((mac_addr[4] >> 4) & 0x0f);
	}
}

//Socket callback for the server
static void server_socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	switch (u8Msg) {
		/* Socket bind */
		case SOCKET_MSG_BIND:
		{
			tstrSocketBindMsg *pstrBind = (tstrSocketBindMsg *)pvMsg;
			if (pstrBind && pstrBind->status == 0) {
				printf("socket_cb: bind success!\r\n");
				listen(tcp_server_socket, 0);
				} else {
				printf("socket_cb: bind error!\r\n");
				close(tcp_server_socket);
				tcp_server_socket = -1;
			}
		}
		break;

		/* Socket listen */
		case SOCKET_MSG_LISTEN:
		{
			tstrSocketListenMsg *pstrListen = (tstrSocketListenMsg *)pvMsg;
			if (pstrListen && pstrListen->status == 0) {
				printf("socket_cb: listen success!\r\n");
				accept(tcp_server_socket, NULL, NULL);
				} else {
				printf("socket_cb: listen error!\r\n");
				close(tcp_server_socket);
				tcp_server_socket = -1;
			}
		}
		break;

		/* Connect accept */
		case SOCKET_MSG_ACCEPT:
		{
			tstrSocketAcceptMsg *pstrAccept = (tstrSocketAcceptMsg *)pvMsg;
			if (pstrAccept) {
				printf("socket_cb: accept success!\r\n");
				accept(tcp_server_socket, NULL, NULL);
				tcp_client_socket = pstrAccept->sock;
				tcp_connected = 1;
				recv(tcp_client_socket, gau8SocketTestBuffer, sizeof(gau8SocketTestBuffer), 0);
				} else {
				printf("socket_cb: accept error!\r\n");
				close(tcp_server_socket);
				tcp_server_socket = -1;
				tcp_connected = 0;
			}
		}
		break;
		case SOCKET_MSG_CONNECT:
		{
			tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
			if (pstrConnect && pstrConnect->s8Error >= 0) {
				printf("socket_cb: connect success.\r\n");
				tcp_connected = 1;
				recv(tcp_client_socket, gau8SocketBuffer, sizeof(gau8SocketBuffer), 0);
				
				} else {
				printf("socket_cb: connect error!\r\n");
				tcp_connected = 0;
				
			}
		}
		break;

		/* Message send */
		case SOCKET_MSG_SEND:
		{
			recv(tcp_client_socket, gau8SocketBuffer, sizeof(gau8SocketBuffer), 0);
		}
		break;

		/* Message receive */
		case SOCKET_MSG_RECV:
		{
			tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;
			if (pstrRecv && pstrRecv->s16BufferSize > 0) {
				printf("socket_cb: recv success!\r\n");
				printf("The message received is %s\r\n", pstrRecv->pu8Buffer);
				
				send(tcp_client_socket, &msg_wifi_product, sizeof(t_msg_wifi_product), 0);
				
				}else {
				printf("socket_cb: recv error!\r\n");
				close(tcp_server_socket);
				tcp_server_socket = -1;
				break;
			}
			recv(tcp_client_socket, gau8SocketBuffer, sizeof(gau8SocketBuffer), 0);
		}

		break;

		default:
		break;
	}
}

//Socket callback for the client
void client_socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg){
	switch (u8Msg) {
		/* Socket connected */
		case SOCKET_MSG_CONNECT:
		{
			tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
			if (pstrConnect && pstrConnect->s8Error >= 0) {
				printf("socket_cb: connect success!\r\n");
				send(tcp_client_socket_external, &msg_wifi_product, sizeof(t_msg_wifi_product), 0);
				} else {
				printf("socket_cb: connect error!\r\n");
				close(tcp_client_socket_external);
				tcp_client_socket_external = -1;
			}
		}
		break;

		/* Message send */
		case SOCKET_MSG_SEND:
		{
			printf("socket_cb: send success!\r\n");
			recv(tcp_client_socket_external, gau8SocketTestBuffer, sizeof(gau8SocketTestBuffer), 0);
		}
		break;

		/* Message receive */
		case SOCKET_MSG_RECV:
		{
			
			tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;
			if (pstrRecv && pstrRecv->s16BufferSize > 0) {
				printf("socket_cb: recv success!\r\n");
				} else {
				printf("socket_cb: recv error!\r\n");
				close(tcp_client_socket_external);
				tcp_client_socket_external = -1;
			}
		}

		break;

		default:
		break;
	}
}

// The socket callback handler, chooses which callback function should be executed for each socket
static void socket_cb(SOCKET sock, uint8_t msg_type, void *msg_data)
{
	if (sock == tcp_server_socket || sock == tcp_client_socket){
		server_socket_cb(sock, msg_type, msg_data);
	}
	else if (sock == tcp_client_socket_external){
		client_socket_cb(sock, msg_type, msg_data);
	}
}



static void wifi_callback(uint8 msg_type, void *msg_data)
{
	tstrM2mWifiStateChanged *msg_wifi_state;
	uint8 *msg_ip_addr;

	switch (msg_type) {
		case M2M_WIFI_RESP_CON_STATE_CHANGED:
		msg_wifi_state = (tstrM2mWifiStateChanged *)msg_data;
		if (msg_wifi_state->u8CurrState == M2M_WIFI_CONNECTED) {
			printf("Wi-Fi connected\r\n");
			//m2m_wifi_request_dhcp_client();
			} else if (msg_wifi_state->u8CurrState == M2M_WIFI_DISCONNECTED) {
			/* If Wi-Fi is disconnected. */
			printf("Wi-Fi disconnected\r\n");
		}

		break;

		case M2M_WIFI_REQ_DHCP_CONF:
		msg_ip_addr = (uint8 *)msg_data;
		printf("Wi-Fi IP is %u.%u.%u.%u\r\n",
		msg_ip_addr[0], msg_ip_addr[1], msg_ip_addr[2], msg_ip_addr[3]);
		/* Try to connect to MQTT broker when Wi-Fi was connected. */
		
		break;

		case M2M_WIFI_RESP_PROVISION_INFO:
		{
			tstrM2MProvisionInfo *pstrProvInfo = (tstrM2MProvisionInfo *)msg_data;
			printf("wifi_cb: M2M_WIFI_RESP_PROVISION_INFO.\r\n");

			if (pstrProvInfo->u8Status == M2M_SUCCESS) {
				m2m_wifi_connect((char *)pstrProvInfo->au8SSID, strlen((char *)pstrProvInfo->au8SSID), pstrProvInfo->u8SecType,
				pstrProvInfo->au8Password, M2M_WIFI_CH_ALL);
				
				SSID = pstrProvInfo->au8SSID;
				Password = pstrProvInfo->au8Password;
				printf("SSID %s\r\n", SSID);
				
				
				at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x10000, false);
				at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x20000, false);
				
				at25dfx_chip_erase_block(&at25dfx_chip, 0x10000, AT25DFX_BLOCK_SIZE_4KB);
				at25dfx_chip_erase_block(&at25dfx_chip, 0x20000, AT25DFX_BLOCK_SIZE_4KB);
				
				at25dfx_chip_write_buffer(&at25dfx_chip, 0x10000, SSID, AT25DFX_BUFFER_SIZE);
				at25dfx_chip_write_buffer(&at25dfx_chip, 0x20000, Password, AT25DFX_BUFFER_SIZE);
				
				at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
				
				at25dfx_chip_read_buffer(&at25dfx_chip, 0x10000, SSID_Read, AT25DFX_BUFFER_SIZE);
				printf("SSID read from flash:  %s\r\n", SSID_Read);
				
				at25dfx_chip_read_buffer(&at25dfx_chip, 0x20000, Password, AT25DFX_BUFFER_SIZE);
				printf("Password read from flash:  %s\r\n", Password);
				
				m2m_wifi_request_dhcp_client();
				
				wifi_connected = 1;
				
				
				} else {
				printf("wifi_cb: Provision failed.\r\n");
			}
		}
		break;

		default:
		break;
	}
}

void handle_input_message(void)
{
	int i, msg_len;

	if (uart_buffer_written == 0) {
		return;
		} else if (uart_buffer_written >= MAIN_CHAT_BUFFER_SIZE) {
		
		send(tcp_client_socket, uart_buffer, MAIN_CHAT_BUFFER_SIZE, 0);
		uart_buffer_written = 0;
		} else {
		for (i = 0; i < uart_buffer_written; i++) {
			/* Find newline character ('\n' or '\r\n') and publish the previous string . */
			if (uart_buffer[i] == '\n') {
				/* Remove LF and CR from uart buffer.*/
				if (uart_buffer[i - 1] == '\r') {
					msg_len = i - 1;
					} else {
					msg_len = i;
				}

				uart_buffer[msg_len] = 0;
				printf("The input message is %s\r\n",uart_buffer);
				
				char* pch;
				
				int count = 0;
				int arrCount = 0;
				//printf("Splitting string into individual elements\r\n");
				pch = strtok(uart_buffer, " ");
				
				
				
				while (pch != NULL){
					printf("%s\n", pch);
					
					CommandArray[count++] = pch;
					
					pch = strtok(NULL, " ");
					
				}
				
				size_t commandSize = 3;
				size_t connectSize = 7;
				size_t sendSize = 4;
				
				//printf("Done Splitting\r\n");
				//printf("The command is %s\r\n", CommandArray[0]);
				
				//To connect to a TCP connection,
				//"TCP CONNECT {IP ADRESS} {PORT}"
				if(!strncmp("TCP",CommandArray[0],commandSize)){
					
					if(!strncmp("CONNECT", CommandArray[1],connectSize)){
						commandTCP(CommandArray[2], atoi(CommandArray[3]));
					}
					if (!strncmp("SEND", CommandArray[1], sendSize))
					{
						sendTCP(CommandArray[2]);
						printf("Sent %s to the server\r\n", CommandArray[2]);
					}
				}
				
				/* Move remain data to start of the buffer. */
				if (uart_buffer_written > i + 1) {
					memmove(uart_buffer, uart_buffer + i + 1, uart_buffer_written - i - 1);
					uart_buffer_written = uart_buffer_written - i - 1;
					} else {
					uart_buffer_written = 0;
				}

				break;
			}
		}
	}
}


int commandTCP(char* IPAddress, int port){
	IPnumber = calculateIP(IPAddress);
	printf("Port number is %d\r\n", port);
	struct sockaddr_in addr2;
	int8_t ret;

	/* Initialize socket address structure. */
	addr2.sin_family = AF_INET;
	addr2.sin_port = _htons(port);
	addr2.sin_addr.s_addr = _htonl(IPnumber);
	
	if ((tcp_client_socket_external < 0) && (wifi_connected ==1)){
		if ((tcp_client_socket_external = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			printf("main: failed to create TCP client socket4 error!\r\n");
			return;
		}

		//Connect to tcp client
		ret = connect(tcp_client_socket_external, (struct sockaddr *)&addr2, sizeof(struct sockaddr_in));
		printf("connected to the external TCP server \r\n");
		if (ret < 0) {
			close(tcp_client_socket_external);
			tcp_client_socket_external = -1;
		}

	}
}

void sendTCP(char* message){
	printf("message to the server is: %s\r\n", message);
	send(tcp_client_socket_external, message, strlen(message), 0);
}

long long int calculateIP(char *IPstring){
	
	char* p;
	int count = 0;
	int i =0;
	
	char* IPArray[4];
	int IPNum[4];
	long long int IPnumber = 0;
	int testNumber = 0;
	p = strtok(IPstring, ".");
	while (p != NULL){
		//printf("%s\n", p);
		
		IPArray[count++] = p;
		
		p = strtok(NULL, ".");
		
	}
	
	for(i = 0; i < 4; i++){
		//printf("%d\r\n", i);
		IPNum[i] = atoi(IPArray[i]);
	}
	
	IPnumber = (IPNum[3]*1) + (IPNum[2]*256) + (IPNum[1]*65536) + 3221225472;
	return IPnumber;
	
	
}

/**
* \brief Main application function.
*
* \return program return value.
*/



static void at25dfx_init(void)
{
	struct at25dfx_chip_config at25dfx_chip_config;
	struct spi_config at25dfx_spi_config;

	at25dfx_spi_get_config_defaults(&at25dfx_spi_config);
	at25dfx_spi_config.mode_specific.master.baudrate = AT25DFX_CLOCK_SPEED;
	at25dfx_spi_config.mux_setting = AT25DFX_SPI_PINMUX_SETTING;
	at25dfx_spi_config.pinmux_pad0 = AT25DFX_SPI_PINMUX_PAD0;
	at25dfx_spi_config.pinmux_pad1 = AT25DFX_SPI_PINMUX_PAD1;
	at25dfx_spi_config.pinmux_pad2 = AT25DFX_SPI_PINMUX_PAD2;
	at25dfx_spi_config.pinmux_pad3 = AT25DFX_SPI_PINMUX_PAD3;

	spi_init(&at25dfx_spi, AT25DFX_SPI, &at25dfx_spi_config);
	spi_enable(&at25dfx_spi);
	

	at25dfx_chip_config.type = AT25DFX_MEM_TYPE;
	at25dfx_chip_config.cs_pin = AT25DFX_CS;

	at25dfx_chip_init(&at25dfx_chip, &at25dfx_spi, &at25dfx_chip_config);

}


void eraseSerialBuffer(void){
	at25dfx_chip_set_global_sector_protect(&at25dfx_chip, false);
	at25dfx_chip_erase_block(&at25dfx_chip, 0x10000, AT25DFX_BLOCK_SIZE_4KB);
	at25dfx_chip_erase_block(&at25dfx_chip, 0x20000, AT25DFX_BLOCK_SIZE_4KB);
	at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
}

int main(void)
{
	tstrWifiInitParam param;
	int8_t ret;

	uint8_t mac_addr[6];
	uint8_t u8IsMacAddrValid;

	/* Initialize the board. */
	system_init();

	/* Initialize the UART console. */
	at25dfx_init();
	configure_console();
	printf(STRING_HEADER);

	/* Initialize the BSP. */
	nm_bsp_init();
	
	at25dfx_chip_wake(&at25dfx_chip);
	
	//Checks if the chip is responsive
	if (at25dfx_chip_check_presence(&at25dfx_chip) != STATUS_OK) {
		// Handle missing or non-responsive device
		printf("Chip is unresponsive\r\n");
	}
	
	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_callback;
	
	
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
		while (1) {
		}
	}

	m2m_wifi_get_otp_mac_address(mac_addr, &u8IsMacAddrValid);
	if (!u8IsMacAddrValid) {
		m2m_wifi_set_mac_address(gau8MacAddr);
	}

	m2m_wifi_get_mac_address(gau8MacAddr);

	set_dev_name_to_mac((uint8_t *)gacDeviceName, gau8MacAddr);
	set_dev_name_to_mac((uint8_t *)gstrM2MAPConfig.au8SSID, gau8MacAddr);
	m2m_wifi_set_device_name((uint8_t *)gacDeviceName, (uint8_t)m2m_strlen((uint8_t *)gacDeviceName));
	gstrM2MAPConfig.au8DHCPServerIP[0] = 0xC0; /* 192 */
	gstrM2MAPConfig.au8DHCPServerIP[1] = 0xA8; /* 168 */
	gstrM2MAPConfig.au8DHCPServerIP[2] = 0x01; /* 1 */
	gstrM2MAPConfig.au8DHCPServerIP[3] = 0x01; /* 1 */

	at25dfx_chip_read_buffer(&at25dfx_chip, 0x10000, SSID_read, AT25DFX_BUFFER_SIZE);
	
	char c;
	c = SSID_read[0];
	//Basic error detection, which only checks if the first letter of the SSID is a valid char in the alphabet
	if(( c>='a' && c<='z') || (c>='A' && c<='Z')){
		printf("SSID is valid, will continue to connect to wifi\r\n");
		at25dfx_chip_read_buffer(&at25dfx_chip, 0x10000, SSID_read, AT25DFX_BUFFER_SIZE);
		at25dfx_chip_read_buffer(&at25dfx_chip, 0x20000, Password_read, AT25DFX_BUFFER_SIZE);
		
		printf("SSID  read is %s\r\n", SSID_read);
		printf("Password read is: %s\r\n", Password_read);

		m2m_wifi_connect((char *)SSID_read, strlen(SSID_read), M2M_WIFI_SEC_WPA_PSK, (char *)Password_read, M2M_WIFI_CH_ALL);
		wifi_connected = 1;
		
	}

	else{
		printf("SSID is not valid, Wifi provisioning will start");
		
		m2m_wifi_start_provision_mode((tstrM2MAPConfig *)&gstrM2MAPConfig, (char *)gacHttpProvDomainName, 1);
		printf("Provision Mode started.\r\nConnect to [%s] via AP[%s] and fill up the page.\r\n", MAIN_HTTP_PROV_SERVER_DOMAIN_NAME, gstrM2MAPConfig.au8SSID);
	}


	struct sockaddr_in addr;
	/* Initialize socket address structure. */
	addr.sin_family = AF_INET;
	addr.sin_port = _htons(MAIN_WIFI_M2M_SERVER_PORT);
	addr.sin_addr.s_addr = 0;
	
	socketInit();
	registerSocketCallback(socket_cb, NULL);
	
	printf("\r\n");
	printf("To connect to a TCP server enter command 'TCP CONNECT {IP ADDRESS} PORT'\r\n");
	printf("To send a command to the TCP server use command 'TCP SEND {COMMAND}'\r\n");
	printf("\r\n");

	while (1) {
		m2m_wifi_handle_events(NULL);
		usart_read_job(&cdc_uart_module, &uart_ch_buffer);
		handle_input_message();
		
		
		if (wifi_connected == M2M_WIFI_CONNECTED) {
			
			if (tcp_server_socket < 0) {
				/* Open TCP server socket */
				if ((tcp_server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
					printf("main: failed to create TCP server socket error!\r\n");
					continue;
				}
				/* Bind service*/
				bind(tcp_server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
			}
		}
		
		//When button is pressed, reset the system and erase the Serial flash.
		if (port_pin_get_input_level(BUTTON_0_PIN) != BUTTON_0_INACTIVE){
			port_pin_set_output_level(LED0_PIN, LED_0_ACTIVE);
			delay_s(1);
			
			m2m_wifi_disconnect();
			close(tcp_server_socket);
			tcp_server_socket = -1;
			wifi_connected = 0;
			
			eraseSerialBuffer();
			NVIC_SystemReset();
			delay_s(1);
			
			
			}else{
			port_pin_set_output_level(LED0_PIN, LED_0_INACTIVE);
		}
		
	}

	return 0;
}
