#include <stdio.h>

#include "TestUtils.h"
#include "SysDefs.h"
#include "Ethernet.h"
#include "Report.h"
#include "Report.h"

#define USE_STATIC_MEM 1


/*
  If USE_STATIC_MEM is defined then all the UDP and TCP channel descriptor are statically allocated 
*/
#ifdef USE_STATIC_MEM
   #define ETH_UDP_CHNL_MAX 7                               //!< max amount of UDP channel descriptor
   static t_ethUdpChnl ListUdpChannels[ETH_UDP_CHNL_MAX];   //!< static array containing the UDP channel descriptor
   static indexUdp = 0;                                     //!< index of the currect UDP channel descriptor
#endif

#ifdef USE_STATIC_MEM
   #define ETH_TCP_CHNL_MAX 10                              //!< max amount of TCP channel descriptor
   static t_ethTcpChnl ListTcpChannels[ETH_TCP_CHNL_MAX];   //!< static array containing the TCP channel descriptor
   static indexTcp = 0;                                     //!< index of the currect TCP channel descriptor
#endif


/*********************************************************************//** 
*  \brief Initialize the UDP data structure.
*
*  @param ptr   pointer to the UDP data structure to initialize.
***************************************************************/
static void ETH_UDP_SetDefaultVal(t_ethUdpChnl* ptr)
{
    ptr->TxIsInit  = K_FALSE;
    ptr->RxIsInit  = K_FALSE;
    ptr->socket    = INVALID_SOCKET;
}

/*********************************************************************//**
*  \brief Initialize TCP data structure.
*
*  @param ptr   pointer to the TCP data structure.
*************************************************************************/
static void ETH_TCP_SetDefaultVal(t_ethTcpChnl* ptr)
{
    ptr->connect = K_FALSE;
    ptr->socket = INVALID_SOCKET;
}

/*********************************************************************//**
*  \brief   UDP channel descriptor factory  
*
*  @return  reference to an UDP channel descriptor  
*************************************************************************/
t_ethUdpChnl* ETH_UDP_GetInstance(void) 
{
    t_ethUdpChnl* ptr = NULL;   //!< pointer to the UDP channel descriptor

#ifdef USE_STATIC_MEM
    //if the current index does not exceed the array bundary
    if (indexUdp < ETH_UDP_CHNL_MAX) 
    {
        //get the reference of the current UDP channel descriptor
        ptr = &ListUdpChannels[indexUdp];

        //initialise the current UDP channel descriptor
        ETH_UDP_SetDefaultVal(ptr);

        //increase the index to point to the next UDP channel descriptor
        indexUdp++;
    }
    else
    {
        fprintf(stderr, "ERROR in %s: Requested too many UDP sockets.\n", __FUNCTION__);
        fprintf(stderr, "Increase the value of ETH_UDP_CHNL_MAX and recompile the source code.\n");
    }


#elif
    // In case USE_STATIC_MEM is not defined then the class allocate the memory - to use in case is implemented a free() mechanism.
    ptr = (t_ethUdpChnl*)malloc(sizeof(t_ethUdpChnl));
    if (!ptr) {
        return ptr;
    }
    SetDefaultVal(ptr);
#endif

    return ptr;
} 


/*********************************************************************//**
*  \brief Initialise the transmission data of the UDP channel descriptor.
*
*  @param chnl      pointer to the UDP channel descriptor.
*  @param serverAdd destination IP address.
*  @param servPort  UDP port number to use.
*  @return          1 if the socket has been initialized properly; otherwise returns 0.
*************************************************************************/
unsigned ETH_UDP_InitTx(t_ethUdpChnl* chnl, char* serverAdd, int servPort)
{
    // Reset the init flag to 0
    chnl->TxIsInit = K_FALSE;

    // Create a new socket only if the socket is invalid
    if (chnl->socket == INVALID_SOCKET)
    {
        chnl->socket = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    //check if the socket is valid
    if (chnl->socket == INVALID_SOCKET)
    {
        // if the socket is still invalid then raise the error
        fprintf(stderr, "%s: error: failed to create socket (%d)\n", __FUNCTION__, WSAGetLastError());
    }
    else
    {
        //otherwise set the socket descriptor and set the init flag to 1
        chnl->ServerAddress.sin_family       = AF_INET;
        chnl->ServerAddress.sin_addr.s_addr  = inet_addr(serverAdd);
        chnl->ServerAddress.sin_port         = htons(servPort);
        chnl->TxIsInit = K_TRUE;
    }
    
    return chnl->TxIsInit;
}

/*********************************************************************//**
*  \brief Initialise the reception data of the UDP channel descriptor.
*
*  @param chnl          pointer to the UDP channel descriptor.
*  @param clientAdd     source IP address.
*  @param clientPort    UDP port number to use.
*  @return              1 if the socket has been initialized properly; otherwise returns 0.
*************************************************************************/
unsigned ETH_UDP_InitRx(t_ethUdpChnl* chnl, char* clientAdd, int clientPort)
{
    // Reset the init flag
    chnl->RxIsInit = K_FALSE;

    // Create a new socket only if the socket is invalid
    if (chnl->socket == INVALID_SOCKET)
    {
        chnl->socket = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    //check if the socket is valid
    if (chnl->socket == INVALID_SOCKET)
    {
        // if the socket is still invalid then raise the error
        fprintf(stderr, "%s: error: failed to create socket (%d)\n", __FUNCTION__, WSAGetLastError());
    }
    else
    {
        int result;
        BOOL optVal = 1;

        // Set up to receive messages sent from the GSS
        chnl->ClientAddress.sin_family       = AF_INET;
        chnl->ClientAddress.sin_addr.s_addr  = htonl(INADDR_ANY);
        chnl->ClientAddress.sin_port         = htons(clientPort);

        // Allow the address to be re-used (allows GSS to run at the same time)
        if ((result = setsockopt(chnl->socket, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(BOOL))) == SOCKET_ERROR)
        {
            fprintf(stderr, "%s: error: setsockopt() failed (%d)\n", __FUNCTION__, WSAGetLastError());
        }
        // Bind the AS Receive socket.
        else if ((result = bind(chnl->socket, (struct sockaddr*) &chnl->ClientAddress, sizeof(chnl->ClientAddress))) == 0)
        {
            chnl->RxIsInit = K_TRUE;
        }
        else
        {
            fprintf(stderr, "%s: error: bind() failed (%d)\n", __FUNCTION__, WSAGetLastError());
        }
    }

    return chnl->RxIsInit;
}

/*********************************************************************//**
*  \brief Sends data to a target destination using the UDP channel descriptor
*
*  @param chnl  pointer to the UDP channel descriptor.
*  @param data  pointer to the data to send.
*  @param size  size of the data to send in bytes.
*  @return      total number of bytes sent; in case of error -1 is returned.
*************************************************************************/
int ETH_UDP_SendData(t_ethUdpChnl* chnl, char* data, unsigned size)
{
    int retVal = 0;

    // Transmit ID word to MPM.
    if (chnl->TxIsInit = K_TRUE)
    {
        retVal = sendto(chnl->socket, data, size, 0, (struct sockaddr*)&chnl->ServerAddress, sizeof(chnl->ServerAddress));
    }
    else
    {
        fprintf(stderr, "%s: error: Tx UDP channel is not initialize\n", __FUNCTION__);
    }

    return retVal;
}

/*********************************************************************//**
*  \brief Receives data from the UDP descriptor and then copy the data into 
*         the destination buffer.
*
*  @param chnl      pointer to the UDP channel descriptor.
*  @param buff      pointer to the destination buffer.
*  @param maxSize   maximum number of bytes copyied into the buffer.
*  @param timeout   timeout value (milliseconds).
*  @return          the number of bytes received; otherwise returns 0.
*************************************************************************/
int ETH_UDP_recvFrom(t_ethUdpChnl* chnl, char* buff, int maxSize, unsigned timeout)
{
    int result = 0;

    //check the input variables
    if (!chnl->RxIsInit)
    {
        fprintf(stderr, "%s: Error: UDP Rx channel is not initialized\n", __FUNCTION__);
    }
    else if (timeout == 0)
    {
        fprintf(stderr, "%s, Error: invalid timeout value\n", __FUNCTION__);
    }
    else
    {
        //socket data structures
        struct sockaddr fromAddr;
        int fromSize = sizeof(fromAddr);
        fd_set readFlags;

        //initialise the timer variables
        unsigned timer = 0;
        struct timeval timeVal;
        timeVal.tv_sec = timeout / 1000;
        timeVal.tv_usec = (timeout % 1000) * 1000;

        //set the timer
        UTIL_SetTimeout(&timer, timeout);

        // Keep reading until we get a mode message, or until we time out.
        do
        {
            // Set up the socket flags
            FD_ZERO(&readFlags);
            FD_SET((unsigned)chnl->socket, &readFlags);

            // Bypass the socket select if the timeout is null.
            if ((result = select(chnl->socket + 1, &readFlags, NULL, NULL, &timeVal)) == SOCKET_ERROR)
            {
                fprintf(stderr, "%s: error: select() failed (%d)\n", __FUNCTION__, WSAGetLastError());
            }
            else if (result > 0)
            {
                // Check if the socket is ready for reading
                if (FD_ISSET(chnl->socket, &readFlags))
                {
                    FD_CLR(chnl->socket, &readFlags);
                    if ((result = recvfrom(chnl->socket, (char*)chnl->buff, ETH_BUF_SIZE, 0, &fromAddr, &fromSize)) < 0)
                    {
                        // Error reading data
                        fprintf(stderr, "%s: Error in recvfrom()\n", __FUNCTION__);
                    }
                    else
                    {
                        // Data ok. Copy the data into the destination buffer.
                        memcpy(buff, chnl->buff, min(result, maxSize));
                    }
                }
            }
        } while (result == 0 && !UTIL_TimedOut(timer));
    }

    return result;
}

/*********************************************************************//**
*  \brief Receives a specific amount of data from the UDP descriptor and 
*         then copy the data into the destination buffer.
*
*  @param chnl          pointer to the UDP channel descriptor.
*  @param buff          pointer to the destination buffer.
*  @param expectedSize  expected size of the received packet.
*  @param timeout       timeout value (milliseconds).
*  @return              0 in case of error; otherwise the number of bytes received; otherwise returns 0.
*************************************************************************/
int ETH_UDP_recvData(t_ethUdpChnl* chnl, char* buff, int expectedSize, unsigned timeout)
{
    int result = 0;

    if (!chnl->RxIsInit)
    {
        fprintf(stderr, "%s: Error: UDP Rx channel is not initialized\n", __FUNCTION__);
    }
    else if (timeout == 0)
    {
        fprintf(stderr, "%s: Error: invalid timeout value\n", __FUNCTION__);
    }
    else
    {
        //socket variables
        struct sockaddr fromAddr;
        int fromSize = sizeof(fromAddr);
        fd_set readFlags;

        //initialise the timer variables
        struct timeval timeVal;
        unsigned timer = 0;
        timeVal.tv_sec = timeout / 1000;
        timeVal.tv_usec = (timeout % 1000) * 1000;

        // Setup the timer 
        UTIL_SetTimeout(&timer, timeout);

        // Keep reading until we get a mode message or until we get a time out.
        do
        {
            // Set up the socket flags
            FD_ZERO(&readFlags);
            FD_SET((unsigned)chnl->socket, &readFlags);

            // Bypass the socket select if the timeout is null
            if ((result = select(chnl->socket + 1, &readFlags, NULL, NULL, &timeVal)) < 0)
            {
                fprintf(stderr, "%s: error: select() failed (%d)\n", __FUNCTION__, WSAGetLastError());
            }
            else if (result > 0)
            {
                // Check if the socket is ready for reading
                if (FD_ISSET(chnl->socket, &readFlags))
                {
                    //clear the socket flags
                    FD_CLR(chnl->socket, &readFlags);

                    //read the data
                    if ((result = recvfrom(chnl->socket, buff , expectedSize, 0, &fromAddr, &fromSize)) == SOCKET_ERROR)
                    {
                        fprintf(stderr, "%s: error: recvfrom() failed (%d)\n", __FUNCTION__, WSAGetLastError());
                    }
                }
            }
        } while (result == 0 && ! UTIL_TimedOut(timer));
    }

    return result;
}



/*********************************************************************//**
*  \brief   Flush the UDP socket used ot receive data associated to the 
            UDP channel descriptor
*
*  @param chnl          pointer to the UDP channel descriptor.
*************************************************************************/
void ETH_UDP_Flush(t_ethUdpChnl* chnl)
{
    if (!chnl->RxIsInit)
    {
        fprintf(stderr, "%s: Error: UDP RX channel not initialized\n", __FUNCTION__);
    }
    else
    {
        // variable to check if the data are received
        int result = 0;

        //socket variables
        struct sockaddr fromAddr;
        int fromSize = sizeof(fromAddr);
        fd_set readFlags;

        // Set receive timeout to 100 ms (!)
        struct timeval timeVal;
        timeVal.tv_sec = 0;
        timeVal.tv_usec = 100000;

        // Keep reading until the buffer is empty
        do
        {
            // Set up the socket set
            FD_ZERO(&readFlags);
            FD_SET((unsigned)chnl->socket, &readFlags);

            // Wait for a message
            result = select(chnl->socket + 1,&readFlags, NULL, NULL, &timeVal);

            // Read the message
            if (result > 0)
            {
                // If the Socket is ready for reading...
                if (FD_ISSET(chnl->socket, &readFlags))
                {
                    //clear the socket flags
                    FD_CLR(chnl->socket, &readFlags);

                    //read the data
                    if ((result = recvfrom(chnl->socket, (char*)&chnl->buff[0], ETH_BUF_SIZE, 0, &fromAddr, &fromSize)) == SOCKET_ERROR)
                    {
                        fprintf(stderr, "%s: error: recvfrom() failed (%d)\n", __FUNCTION__, WSAGetLastError());
                    }
                }
            }
        } while (result > 0);
    }

    //erase the buffer associate to the UDP channel descriptor
    memset(&chnl->buff[0], 0, ETH_BUF_SIZE);
}

/*********************************************************************//**
*  \brief  TCP channel factory.

*  @return  pointer to the TCP channel descriptor
*************************************************************************/
t_ethTcpChnl* ETH_TCP_GetInstance(void) 
{
    t_ethTcpChnl* ptr = NULL;

    // In case USE_STATIC_MEM is defined the function return a pointer from a static list of structures
#ifdef USE_STATIC_MEM
    if (indexTcp < ETH_TCP_CHNL_MAX)
    {
        //get the reference to the current TCP channel descriptor and increase the index
        ptr = &ListTcpChannels[indexTcp++];
        // initialise the TCP channel descriptor
        ETH_TCP_SetDefaultVal(ptr);
    }
    else
    {
        fprintf(stderr, "%s: Error: Invalid TCP channel index (%d).\n", __FUNCTION__, indexTcp);
        fprintf(stderr, "Increase the value of ETH_TCP_CHNL_MAX and recompile the source code.\n");
    }
#elif
    // In case USE_STATIC_MEM is not defined the class allocate the memory - to use in case is implemented a free() mechanism.
    ptr = (t_ethTcpChnl*)malloc(sizeof(t_ethTcpChnl));
    if (!ptr) {
        return ptr;
    }
    SetDefaultVal(ptr);
#endif

    return ptr;
} 

/*********************************************************************//**
*  \brief Connect the TCP socket to the server.
*
*  @param chnl          pointer to the TCP channel descriptor.
*  @param serverAdd     destination IP address
*  @param servPort      TCP port number to use.
*  @return              1 if the connection is ok, 0 otherwise 
*************************************************************************/
unsigned ETH_TCP_Open(t_ethTcpChnl* chnl, char* serverAdd, int servPort)
{
    // Make sure the socket is closed.
    if (chnl->connect)
    {
        fprintf(stderr, "%s: Error: socket already open\n", __FUNCTION__);
    }
    else
    {
        // Create a TCP socket.
        if ((chnl->socket = (int)socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        {
            chnl->connect = K_FALSE;
            fprintf(stderr, "%s: Error: socket() failed (%d)\n", __FUNCTION__, WSAGetLastError());
        }
        else
        {
            //set the socket options
            chnl->ServerAddress.sin_family       = AF_INET;
            chnl->ServerAddress.sin_addr.s_addr  = inet_addr(serverAdd);
            chnl->ServerAddress.sin_port         = htons(servPort);

            //connect the socket with the destination address
            if (connect(chnl->socket, (struct sockaddr*) &chnl->ServerAddress, sizeof(chnl->ServerAddress)) == SOCKET_ERROR)
            {
                fprintf(stderr, "%s: Error: connect() failed (%d)\n", __FUNCTION__, WSAGetLastError());
            }
            else
            {
                //if the connection is oK the raise the flag
                chnl->connect = K_TRUE;
            }
        }
    }

    return chnl->connect;
}

/*********************************************************************//**
* \brief Close the TCP connection.
*
*  @param chnl          pointer to the TCP channel descriptor.
*  @return              0 if the socket has bee closed properly; -1 in case of error.
*************************************************************************/
int ETH_TCP_Close(t_ethTcpChnl* chnl)
{
    unsigned retVal = 0;  //return value

    if (chnl->connect)
    {
        // Close the socket.
        if ((retVal = shutdown(chnl->socket, 2)) == SOCKET_ERROR)
        {
            fprintf(stderr, "%s: Error: shutdown() failed\n", __FUNCTION__, WSAGetLastError());
        }
        else
        {
            // Set the value to default.
            chnl->connect = K_FALSE;
            chnl->socket = INVALID_SOCKET;
        }
    }

    return retVal;
}

/*********************************************************************//**
* \brief Send data using the TCP descriptor.
*
*  @param chnl          pointer to the TCP channel descriptor.
*  @param data          pointer to the data to transmit.
*  @param size          size of the data packet to send, in bytes.
*  @return              amount of byte sent.
*************************************************************************/
int ETH_TCP_SendData(t_ethTcpChnl* chnl, char* data, unsigned size)
{
    int wcnt = 0;   //return value

    // Make sure the socket is connected.
    if (!chnl->connect)
    {
        fprintf(stderr, "%s: error: socket not connected\n", __FUNCTION__);
    }
    else if ((wcnt = send(chnl->socket, data, size, 0)) == SOCKET_ERROR)
    {
        fprintf(stderr, "%s: send() failed (%d)\n", __FUNCTION__, WSAGetLastError());
    }
    else if (wcnt != size)
    {
        fprintf(stderr, "%s: error sending data\n", __FUNCTION__);
    }

    return wcnt;
}

