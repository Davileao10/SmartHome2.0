#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Configuração mínima para lwIP
#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_TCP 1
#define LWIP_UDP 1
#define MEM_ALIGNMENT 4
#define MEM_SIZE 16000 // Aumentado para suportar Wi-Fi e HTTP
#define MEMP_NUM_PBUF 16
#define PBUF_POOL_SIZE 16
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 4
#define MEMP_NUM_TCP_SEG 24 // Aumentado para suportar mais segmentos
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_DHCP 1
#define LWIP_AUTOIP 1
#define LWIP_DNS 1
#define LWIP_HTTPD 1
#define LWIP_HTTPD_SSI 1
#define LWIP_HTTPD_SUPPORT_POST 1
#define LWIP_HTTPD_DYNAMIC_HEADERS 1
#define HTTPD_USE_CUSTOM_FSDATA 0
#define LWIP_HTTPD_CGI 0
#define LWIP_NETIF_HOSTNAME 1
#define TCP_MSS 536 // Tamanho máximo do segmento TCP
#define TCP_SND_BUF 2048 // Buffer de envio TCP
#define TCP_WND 2048 // Janela TCP

#endif /* LWIPOPTS_H */
