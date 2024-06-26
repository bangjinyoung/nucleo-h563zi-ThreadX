/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
#include "nxd_dhcp_client.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEFAULT_PORT 7

#define SERVER_PACKET_SIZE         (1024 * 2)
#define SERVER_POOL_SIZE           (SERVER_PACKET_SIZE * 8)
#define HTTP_STACK_SIZE            2048
#define IPERF_STACK_SIZE           2048

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
TX_SEMAPHORE   DHCPSemaphore;
NX_DHCP        DHCPClient;
/* USER CODE BEGIN PV */
ULONG ip_address;
ULONG network_mask;
NX_TCP_SOCKET TCPSocket;

NX_PACKET_POOL WebServerPool;
UCHAR *http_stack;
UCHAR *iperf_stack;
static UCHAR nx_server_pool[SERVER_POOL_SIZE];

TX_THREAD dhcp_link_trhead;
TX_THREAD udp_echo_server_thread;
TX_THREAD tcp_echo_server_thread;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry (ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);
/* USER CODE BEGIN PFP */
static VOID dhcp_link_thread_entry (ULONG thread_input);
static VOID udp_echo_server_thread_entry (ULONG thread_input);
static VOID tcp_echo_server_thread_entry (ULONG thread_input);
extern void nx_iperf_entry(NX_PACKET_POOL *pool_ptr, NX_IP *ip_ptr, UCHAR *http_stack, ULONG http_stack_size, UCHAR *iperf_stack, ULONG iperf_stack_size);
/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

   /* USER CODE BEGIN App_NetXDuo_MEM_POOL */
  (void)byte_pool;
  /* USER CODE END App_NetXDuo_MEM_POOL */
  /* USER CODE BEGIN 0 */

  /* USER CODE END 0 */

  /* Initialize the NetXDuo system. */
  CHAR *pointer;
  nx_system_initialize();

    /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the Packet pool to be used for packet allocation,
   * If extra NX_PACKET are to be used the NX_APP_PACKET_POOL_SIZE should be increased
   */
  ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE, pointer, NX_APP_PACKET_POOL_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

    /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

   /* Create the main NX_IP instance */
  ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance", NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK, &NxAppPool, nx_stm32_eth_driver,
                     pointer, Nx_IP_INSTANCE_THREAD_SIZE, NX_APP_INSTANCE_PRIORITY);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

    /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */

  /* USER CODE BEGIN ARP_Protocol_Initialization */

  /* USER CODE END ARP_Protocol_Initialization */

  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the ICMP */

  /* USER CODE BEGIN ICMP_Protocol_Initialization */

  /* USER CODE END ICMP_Protocol_Initialization */

  ret = nx_icmp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable TCP Protocol */

  /* USER CODE BEGIN TCP_Protocol_Initialization */

  /* USER CODE END TCP_Protocol_Initialization */

  ret = nx_tcp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the UDP protocol required for  DHCP communication */

  /* USER CODE BEGIN UDP_Protocol_Initialization */

  /* USER CODE END UDP_Protocol_Initialization */

  ret = nx_udp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

   /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry , 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* Create the DHCP client */

  /* USER CODE BEGIN DHCP_Protocol_Initialization */

  /* USER CODE END DHCP_Protocol_Initialization */

  ret = nx_dhcp_create(&DHCPClient, &NetXDuoEthIpInstance, "DHCP Client");

  if (ret != NX_SUCCESS)
  {
    return NX_DHCP_ERROR;
  }

  /* set DHCP notification callback  */
  tx_semaphore_create(&DHCPSemaphore, "DHCP Semaphore", 0);

  /* USER CODE BEGIN MX_NetXDuo_Init */

  /* Allocate the memory for Link thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  // /* create the Link thread */
  ret = tx_thread_create(&dhcp_link_trhead, "dhcp link thread", dhcp_link_thread_entry, 0, pointer, 
                        NX_APP_THREAD_STACK_SIZE, NX_APP_THREAD_PRIORITY + 1, NX_APP_THREAD_PRIORITY + 1 , 
                        TX_NO_TIME_SLICE, TX_AUTO_START);

  // /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&udp_echo_server_thread, "udp echo server thread", udp_echo_server_thread_entry , 0, pointer, 
                        NX_APP_THREAD_STACK_SIZE, NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE,
                        TX_AUTO_START);

  /* Allocate the memory for Link thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE * 2, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the Link thread */
  ret = tx_thread_create(&tcp_echo_server_thread, "tcp echo server thread", tcp_echo_server_thread_entry, 0, pointer, 
                        NX_APP_THREAD_STACK_SIZE * 2, NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, 
                        TX_NO_TIME_SLICE, TX_DONT_START);

  /* Create the server packet pool. */
  ret = nx_packet_pool_create(&WebServerPool, "HTTP Server Packet Pool", SERVER_PACKET_SIZE, nx_server_pool, SERVER_POOL_SIZE);

  /* Check for server pool creation status. */
  if (ret != NX_SUCCESS)
  {
    printf("Server pool creation failed : 0x%02x\n", ret);
    Error_Handler();
  }

  /* Set the server stack and IPerf stack.  */
  /* Allocate the server stack. */
  ret = tx_byte_allocate(byte_pool, (VOID **) &pointer, HTTP_STACK_SIZE, TX_NO_WAIT);

  /* Check server stack memory allocation. */
  if (ret != NX_SUCCESS)
  {
    printf("Server stack memory allocation failed : 0x%02x\n", ret);
    Error_Handler();
  }
  http_stack = (UCHAR *)pointer;


  /* Allocate the IPERF stack. */
  ret = tx_byte_allocate(byte_pool, (VOID **) &pointer, IPERF_STACK_SIZE, TX_NO_WAIT);

  /* Check IPERF stack memory allocation. */
  if (ret != NX_SUCCESS)
  {
    printf("IPERF stack memory allocation failed : 0x%02x\n", ret);
    Error_Handler();
  }
  iperf_stack = (UCHAR *)pointer;

  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/**
* @brief  ip address change callback.
* @param ip_instance: NX_IP instance
* @param ptr: user data
* @retval none
*/
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* USER CODE BEGIN ip_address_change_notify_callback */
  tx_semaphore_put(&DHCPSemaphore);
  /* USER CODE END ip_address_change_notify_callback */

  /* release the semaphore as soon as an IP address is available */
  tx_semaphore_put(&DHCPSemaphore);
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID nx_app_thread_entry (ULONG thread_input)
{
  /* USER CODE BEGIN Nx_App_Thread_Entry 0 */

  /* USER CODE END Nx_App_Thread_Entry 0 */

  UINT ret = NX_SUCCESS;

  /* USER CODE BEGIN Nx_App_Thread_Entry 1 */

  /* USER CODE END Nx_App_Thread_Entry 1 */

  /* register the IP address change callback */
  ret = nx_ip_address_change_notify(&NetXDuoEthIpInstance, ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    /* USER CODE BEGIN IP address change callback error */

    /* USER CODE END IP address change callback error */
  }

  /* start the DHCP client */
  ret = nx_dhcp_start(&DHCPClient);
  if (ret != NX_SUCCESS)
  {
    /* USER CODE BEGIN DHCP client start error */

    /* USER CODE END DHCP client start error */
  }

  /* wait until an IP address is ready */
  if(tx_semaphore_get(&DHCPSemaphore, NX_APP_DEFAULT_TIMEOUT) != TX_SUCCESS)
  {
    /* USER CODE BEGIN DHCPSemaphore get error */

    /* USER CODE END DHCPSemaphore get error */
  }

  /* USER CODE BEGIN Nx_App_Thread_Entry 2 */
  nx_ip_address_get(&NetXDuoEthIpInstance, &ip_address, &network_mask);

  nx_iperf_entry(&WebServerPool, &NetXDuoEthIpInstance, http_stack, HTTP_STACK_SIZE, iperf_stack, IPERF_STACK_SIZE);

  /* the network is correctly initialized, start the TCP server thread */
  tx_thread_resume(&tcp_echo_server_thread);

  /* this thread is not needed any more, we relinquish it */
  tx_thread_relinquish();

  return;
  /* USER CODE END Nx_App_Thread_Entry 2 */

}
/* USER CODE BEGIN 1 */

static VOID dhcp_link_thread_entry (ULONG thread_input)
{
  ULONG actual_status;
  UINT linkdown = 0, status;

  while(1)
  {
    /* Get Physical Link status. */
    status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_LINK_ENABLED,
                                      &actual_status, 10);

    if(status == NX_SUCCESS)
    {
      if(linkdown == 1)
      {
        linkdown = 0;
        status = nx_ip_interface_status_check(&NetXDuoEthIpInstance, 0, NX_IP_ADDRESS_RESOLVED,
                                      &actual_status, 10);
        if(status == NX_SUCCESS)
        {
          /* The network cable is connected again. */
          printf("The network cable is connected again.\n");
          /* Print UDP Echo Server is available again. */
          printf("UDP Echo Server is available again.\n");
        }
        else
        {
          /* The network cable is connected. */
          printf("The network cable is connected.\n");
          /* Send command to Enable Nx driver. */
          nx_ip_driver_direct_command(&NetXDuoEthIpInstance, NX_LINK_ENABLE,
                                      &actual_status);
          /* Restart DHCP Client. */
          nx_dhcp_stop(&DHCPClient);
          nx_dhcp_start(&DHCPClient);
        }
      }
    }
    else
    {
      if(0 == linkdown)
      {
        linkdown = 1;
        /* The network cable is not connected. */
        printf("The network cable is not connected.\n");
      }
    }

    tx_thread_sleep(600);
  }
}

static VOID udp_echo_server_thread_entry (ULONG thread_input)
{
  UINT status;
  NX_UDP_SOCKET udp_socket;
  UINT port = 7;

  ULONG source_ip_address;
  UINT source_port;

  UCHAR data_buffer[512];
  NX_PACKET *data_packet;
  ULONG bytes_read;

  /* wait until an IP address is ready */
  if(tx_semaphore_get(&DHCPSemaphore, NX_WAIT_FOREVER) != TX_SUCCESS)
  {
    ;
  }

  /* Create the UDP socket.  */
  status = nx_udp_socket_create(&NetXDuoEthIpInstance, &udp_socket, "UDP Echo Server Socket",
                                NX_IP_NORMAL, NX_DONT_FRAGMENT, NX_IP_TIME_TO_LIVE, 512);
                                

  if (status != NX_SUCCESS)
  {
    return;
  }

  /* Bind the UDP socket to the port 7.  */
  status = nx_udp_socket_bind(&udp_socket, port, NX_WAIT_FOREVER);

  if (status != NX_SUCCESS)
  {
    return;
  }

  while (1)
  {
    /* Receive a UDP packet.  */
    status = nx_udp_socket_receive(&udp_socket, &data_packet, NX_WAIT_FOREVER);

    if (status == NX_SUCCESS)
    {
      /* data is available, read it into the data buffer */
      nx_packet_data_retrieve(data_packet, data_buffer, &bytes_read);

      /* get info about the client address and port */
      nx_udp_source_extract(data_packet, &source_ip_address, &source_port);

      /* Send the packet back to the sender.  */
      status = nx_udp_socket_send(&udp_socket, data_packet, source_ip_address, source_port);
    }
  }
}

static VOID tcp_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port)
{
  /* as soon as the IP address is ready, the semaphore is released */
  tx_semaphore_put(&DHCPSemaphore);
}

// Make tcp echo server with NetxDuo API
static VOID tcp_echo_server_thread_entry (ULONG thread_input)
{
  UINT ret;
  UCHAR data_buffer[512];

  ULONG source_ip_address;
  NX_PACKET *data_packet;

  UINT source_port;
  ULONG bytes_read;

  /* create the TCP socket */
  ret = nx_tcp_socket_create(&NetXDuoEthIpInstance, &TCPSocket, "TCP Server Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                             NX_IP_TIME_TO_LIVE, 512, NX_NULL, NX_NULL);
  if (ret)
  {
    Error_Handler();
  }

  /*
  * listen to new client connections.
  * the TCP_listen_callback will release the 'Semaphore' when a new connection is available
  */
  ret = nx_tcp_server_socket_listen(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket, 1, tcp_listen_callback);

  if (ret)
  {
    Error_Handler();
  }
  else
  {
    printf("TCP Server listening on PORT %d ..\n", DEFAULT_PORT);
  }

  if(tx_semaphore_get(&DHCPSemaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    Error_Handler();
  }
  else
  {
    ;
  }

  while(1)
  {
    ULONG socket_state;

    TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));

    /* get the socket state */
    nx_tcp_socket_info_get(&TCPSocket, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &socket_state, NULL, NULL, NULL);

    /* if the connections is not established then accept new ones, otherwise start receiving data */
    if(socket_state != NX_TCP_ESTABLISHED)
    {
      ret = nx_tcp_server_socket_accept(&TCPSocket, NX_WAIT_FOREVER);
    }

    if(ret == NX_SUCCESS)
    {
      /* receive the TCP packet send by the client */
      ret = nx_tcp_socket_receive(&TCPSocket, &data_packet, NX_WAIT_FOREVER);

      if (ret == NX_SUCCESS)
      {

        /* get the client IP address and  port */
        nx_udp_source_extract(data_packet, &source_ip_address, &source_port);

        /* retrieve the data sent by the client */
        nx_packet_data_retrieve(data_packet, data_buffer, &bytes_read);

        // /* immediately resend the same packet */
        ret =  nx_tcp_socket_send(&TCPSocket, data_packet, NX_WAIT_FOREVER);
      }
      else
      {
        nx_tcp_socket_disconnect(&TCPSocket, NX_WAIT_FOREVER);
        nx_tcp_server_socket_unaccept(&TCPSocket);
        nx_tcp_server_socket_relisten(&NetXDuoEthIpInstance, DEFAULT_PORT, &TCPSocket);
      }

    }
    else
    {
      /*toggle the green led to indicate the idle state */
    }
  }
}

/* USER CODE END 1 */
