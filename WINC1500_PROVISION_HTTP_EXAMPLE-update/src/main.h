/**
*
*/

#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
	#endif

	#define MAIN_M2M_AP_SEC                  M2M_WIFI_SEC_OPEN

	#define MAIN_M2M_AP_WEP_KEY              "1234567890"

	#define MAIN_M2M_AP_SSID_MODE            SSID_MODE_VISIBLE

	#define MAIN_HTTP_PROV_SERVER_DOMAIN_NAME    "arrowconfig.com"

	#define MAIN_M2M_DEVICE_NAME                 "WINC1500_00:00"

	#define MAIN_MAC_ADDRESS                     {0xf8, 0xf0, 0x05, 0x45, 0xD4, 0x84}
	
	#define MAIN_WIFI_M2M_BUFFER_SIZE          1460

	#define MAIN_CHAT_BUFFER_SIZE       1460

	#define MAIN_WIFI_M2M_PRODUCT_NAME        "ACK-WINC\r\n"

	#define MAIN_WIFI_M2M_SERVER_IP           0xFFFFFFFF /* 255.255.255.255 */

	#define MAIN_WIFI_M2M_SERVER_PORT         (2323)

	#define MAIN_WIFI_M2M_EXTERNAL_SERVER_PORT			  (2323)

	static tstrM2MAPConfig gstrM2MAPConfig = {
		MAIN_M2M_DEVICE_NAME, 1, 0, WEP_40_KEY_STRING_SIZE, MAIN_M2M_AP_WEP_KEY, (uint8)MAIN_M2M_AP_SEC, MAIN_M2M_AP_SSID_MODE
	};

	static CONST char gacHttpProvDomainName[] = MAIN_HTTP_PROV_SERVER_DOMAIN_NAME;

	static uint8 gau8MacAddr[] = MAIN_MAC_ADDRESS;
	static sint8 gacDeviceName[] = MAIN_M2M_DEVICE_NAME;

	#ifdef __cplusplus
}
#endif

#endif /* MAIN_H_INCLUDED */
