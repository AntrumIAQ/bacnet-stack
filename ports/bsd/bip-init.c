/**
 * @file
 * @brief Initializes BACnet/IP interface (BSD/MAC OS X)
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2005
 * @copyright SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0
 */
#include <stdint.h> /* for standard integer types uint8_t etc. */
#include <stdbool.h> /* for the standard bool type. */
#include <ifaddrs.h>
#include "bacnet/bacdcode.h"
#include "bacnet/bacint.h"
#include "bacnet/datalink/bip.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/bbmd/h_bbmd.h"
#include "bacport.h"

/* unix sockets */
static int BIP_Socket = -1;
static int BIP_Broadcast_Socket = -1;

/* NOTE: we store address and port in network byte order
   since BACnet/IP uses network byte order for all address byte arrays
*/
/* port to use - stored here in network byte order */
/* Initialize to 0 - this will force initialization in demo apps */
static uint16_t BIP_Port;
/* IP address - stored here in network byte order */
static struct in_addr BIP_Address;
/* IP broadcast address - stored here in network byte order */
static struct in_addr BIP_Broadcast_Addr;
/* broadcast binding mechanism */
static bool BIP_Broadcast_Binding_Address_Override;
static struct in_addr BIP_Broadcast_Binding_Address;
/* enable debugging */
static bool BIP_Debug = false;
/* interface name */
static char BIP_Interface_Name[IF_NAMESIZE] = { 0 };

/**
 * @brief Print the IPv4 address with debug info
 * @param str - debug info string
 * @param addr - IPv4 address
 */
static void debug_print_ipv4(
    const char *str,
    const struct in_addr *addr,
    const unsigned int port,
    const unsigned int count)
{
    if (BIP_Debug) {
        fprintf(
            stderr, "BIP: %s %s:%hu (%u bytes)\n", str, inet_ntoa(*addr),
            ntohs(port), count);
        fflush(stderr);
    }
}

/**
 * @brief Return the active BIP socket.
 * @return The active BIP socket, or -1 if uninitialized.
 * @see bip_get_broadcast_socket
 */
int bip_get_socket(void)
{
    return BIP_Socket;
}

/**
 * @brief Return the active BIP Broadcast socket.
 * @return The active BIP Broadcast socket, or -1 if uninitialized.
 * @see bip_get_socket
 */
int bip_get_broadcast_socket(void)
{
    return BIP_Broadcast_Socket;
}

/**
 * @brief Enabled debug printing of BACnet/IPv4
 */
void bip_debug_enable(void)
{
    BIP_Debug = true;
}

/**
 * @brief Disalbe debug printing of BACnet/IPv4
 */
void bip_debug_disable(void)
{
    BIP_Debug = false;
}

/**
 * @brief Set the BACnet IPv4 UDP port number
 * @param port - IPv4 UDP port number - in host byte order
 */
void bip_set_port(uint16_t port)
{
    BIP_Port = htons(port);
}

/**
 * @brief Get the BACnet IPv4 UDP port number
 * @return IPv4 UDP port number - in host byte order
 */
uint16_t bip_get_port(void)
{
    return ntohs(BIP_Port);
}

/**
 * @brief Get the IPv4 address for my interface. Used for sending src address.
 * @param addr - BACnet datalink address
 */
void bip_get_my_address(BACNET_ADDRESS *addr)
{
    unsigned int i = 0;

    if (addr) {
        addr->mac_len = 6;
        memcpy(&addr->mac[0], &BIP_Address.s_addr, 4);
        memcpy(&addr->mac[4], &BIP_Port, 2);
        /* local only, no routing */
        addr->net = 0;
        /* no SLEN */
        addr->len = 0;
        for (i = 0; i < MAX_MAC_LEN; i++) {
            /* no SADR */
            addr->adr[i] = 0;
        }
    }
}

/**
 * Get the IPv4 broadcast address for my interface.
 *
 * @param addr - BACnet datalink address
 */
void bip_get_broadcast_address(BACNET_ADDRESS *dest)
{
    int i = 0; /* counter */

    if (dest) {
        dest->mac_len = 6;
        memcpy(&dest->mac[0], &BIP_Broadcast_Addr.s_addr, 4);
        memcpy(&dest->mac[4], &BIP_Port, 2);
        dest->net = BACNET_BROADCAST_NETWORK;
        dest->len = 0; /* no SLEN */
        for (i = 0; i < MAX_MAC_LEN; i++) {
            /* no SADR */
            dest->adr[i] = 0;
        }
    }

    return;
}

/**
 * Set the BACnet/IP address
 *
 * @param addr - network IPv4 address
 */
bool bip_set_addr(const BACNET_IP_ADDRESS *addr)
{
    /* not something we do within this driver */
    (void)addr;
    return false;
}

/**
 * @brief Get the BACnet/IP address
 * @param addr - network IPv4 address
 * @return true if the address was retrieved
 */
bool bip_get_addr(BACNET_IP_ADDRESS *addr)
{
    if (addr) {
        memcpy(&addr->address[0], &BIP_Address.s_addr, 4);
        addr->port = ntohs(BIP_Port);
    }

    return true;
}

/**
 * @brief Set the BACnet/IP address
 * @param addr - network IPv4 address
 * @return true if the address was set
 */
bool bip_set_broadcast_addr(const BACNET_IP_ADDRESS *addr)
{
    /* not something we do within this driver */
    (void)addr;
    return false;
}

/**
 * Get the BACnet/IP address
 *
 * @return BACnet/IP address
 */
bool bip_get_broadcast_addr(BACNET_IP_ADDRESS *addr)
{
    if (addr) {
        memcpy(&addr->address[0], &BIP_Broadcast_Addr.s_addr, 4);
        addr->port = ntohs(BIP_Port);
    }

    return true;
}

/**
 * @brief Set the BACnet/IP subnet mask CIDR prefix
 * @return true if the subnet mask CIDR prefix is set
 */
bool bip_set_subnet_prefix(uint8_t prefix)
{
    /* not something we do within this driver */
    (void)prefix;
    return false;
}

/**
 * @brief Get the BACnet/IP subnet mask CIDR prefix
 * @return subnet mask CIDR prefix 1..32
 */
uint8_t bip_get_subnet_prefix(void)
{
    uint32_t address = 0;
    uint32_t broadcast = 0;
    uint32_t mask = 0xFFFFFFFE;
    uint8_t prefix = 0;

    address = BIP_Address.s_addr;
    broadcast = BIP_Broadcast_Addr.s_addr;
    /* calculate the subnet prefix from the broadcast address */
    for (prefix = 1; prefix <= 32; prefix++) {
        if ((address | mask) == broadcast) {
            break;
        }
        mask = mask << 1;
    }

    return prefix;
}

/**
 * The send function for BACnet/IP driver layer
 *
 * @param dest - Points to a BACNET_IP_ADDRESS structure containing the
 *  destination address.
 * @param mtu - the bytes of data to send
 * @param mtu_len - the number of bytes of data to send
 *
 * @return Upon successful completion, returns the number of bytes sent.
 *  Otherwise, -1 shall be returned to indicate the error.
 */
int bip_send_mpdu(
    const BACNET_IP_ADDRESS *dest, const uint8_t *mtu, uint16_t mtu_len)
{
    struct sockaddr_in bip_dest = { 0 };

    /* assumes that the driver has already been initialized */
    if (BIP_Socket < 0) {
        if (BIP_Debug) {
            fprintf(stderr, "BIP: driver not initialized!\n");
            fflush(stderr);
        }
        return BIP_Socket;
    }
    /* load destination IP address */
    bip_dest.sin_family = AF_INET;
    memcpy(&bip_dest.sin_addr.s_addr, &dest->address[0], 4);
    bip_dest.sin_port = htons(dest->port);
    /* Send the packet */
    debug_print_ipv4(
        "Sending MPDU->", &bip_dest.sin_addr, bip_dest.sin_port, mtu_len);
    return sendto(
        BIP_Socket, (const char *)mtu, mtu_len, 0, (struct sockaddr *)&bip_dest,
        sizeof(struct sockaddr));
}

/**
 * BACnet/IP Datalink Receive handler.
 *
 * @param src - returns the source address
 * @param npdu - returns the NPDU buffer
 * @param max_npdu -maximum size of the NPDU buffer
 * @param timeout - number of milliseconds to wait for a packet
 *
 * @return Number of bytes received, or 0 if none or timeout.
 */
uint16_t bip_receive(
    BACNET_ADDRESS *src, uint8_t *npdu, uint16_t max_npdu, unsigned timeout)
{
    uint16_t npdu_len = 0; /* return value */
    fd_set read_fds;
    int max = 0;
    struct timeval select_timeout;
    struct sockaddr_in sin = { 0 };
    BACNET_IP_ADDRESS addr = { 0 };
    socklen_t sin_len = sizeof(sin);
    int received_bytes = 0;
    int offset = 0;
    uint16_t i = 0;
    int socket;

    /* Make sure the socket is open */
    if (BIP_Socket < 0) {
        return 0;
    }
    /* we could just use a non-blocking socket, but that consumes all
       the CPU time.  We can use a timeout; it is only supported as
       a select. */
    if (timeout >= 1000) {
        select_timeout.tv_sec = timeout / 1000;
        select_timeout.tv_usec =
            1000 * (timeout - select_timeout.tv_sec * 1000);
    } else {
        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 1000 * timeout;
    }
    FD_ZERO(&read_fds);
    FD_SET(BIP_Socket, &read_fds);
    FD_SET(BIP_Broadcast_Socket, &read_fds);

    max = BIP_Socket > BIP_Broadcast_Socket ? BIP_Socket : BIP_Broadcast_Socket;

    /* see if there is a packet for us */
    if (select(max + 1, &read_fds, NULL, NULL, &select_timeout) > 0) {
        socket =
            FD_ISSET(BIP_Socket, &read_fds) ? BIP_Socket : BIP_Broadcast_Socket;
        received_bytes = recvfrom(
            socket, (char *)&npdu[0], max_npdu, 0, (struct sockaddr *)&sin,
            &sin_len);
    } else {
        return 0;
    }
    /* See if there is a problem */
    if (received_bytes < 0) {
        return 0;
    }
    /* no problem, just no bytes */
    if (received_bytes == 0) {
        return 0;
    }
    /* the signature of a BACnet/IPv packet */
    if (npdu[0] != BVLL_TYPE_BACNET_IP) {
        return 0;
    }
    /* Erase up to 16 bytes after the received bytes as safety margin to
     * ensure that the decoding functions will run into a 'safe field'
     * of zero, if for any reason they would overrun, when parsing the
     * message. */
    max = (int)max_npdu - received_bytes;
    if (max > 0) {
        if (max > 16) {
            max = 16;
        }
        memset(&npdu[received_bytes], 0, max);
    }
    /* Data link layer addressing between B/IPv4 nodes consists of a 32-bit
       IPv4 address followed by a two-octet UDP port number (both of which
       shall be transmitted with the most significant octet first). This
       address shall be referred to as a B/IPv4 address.
    */
    memcpy(&addr.address[0], &sin.sin_addr.s_addr, 4);
    addr.port = ntohs(sin.sin_port);
    debug_print_ipv4(
        "Received MPDU->", &sin.sin_addr, sin.sin_port, received_bytes);
    /* pass the packet into the BBMD handler */
    if (socket == BIP_Socket) {
        offset = bvlc_handler(&addr, src, npdu, received_bytes);
    } else {
        offset = bvlc_broadcast_handler(&addr, src, npdu, received_bytes);
    }
    if (offset > 0) {
        npdu_len = received_bytes - offset;
        debug_print_ipv4(
            "Received NPDU->", &sin.sin_addr, sin.sin_port, npdu_len);
        if (npdu_len <= max_npdu) {
            /* shift the buffer to return a valid NPDU */
            for (i = 0; i < npdu_len; i++) {
                npdu[i] = npdu[offset + i];
            }
        } else {
            if (BIP_Debug) {
                fprintf(stderr, "BIP: NPDU dropped!\n");
                fflush(stderr);
            }
            npdu_len = 0;
        }
    }

    return npdu_len;
}

/**
 * The common send function for BACnet/IP application layer
 *
 * @param dest - Points to a #BACNET_ADDRESS structure containing the
 *  destination address.
 * @param npdu_data - Points to a BACNET_NPDU_DATA structure containing the
 *  destination network layer control flags and data.
 * @param mtu - the bytes of data to send
 * @param mtu_len - the number of bytes of data to send
 * @return Upon successful completion, returns the number of bytes sent.
 *  Otherwise, -1 shall be returned to indicate the error.
 */
int bip_send_pdu(
    BACNET_ADDRESS *dest,
    BACNET_NPDU_DATA *npdu_data,
    uint8_t *pdu,
    unsigned pdu_len)
{
    return bvlc_send_pdu(dest, npdu_data, pdu, pdu_len);
}

/**
 * @brief gets an IP address by hostname (or string of numbers)
 *
 * gets an IP address by name, where name can be a string that is an
 * IP address in dotted form, or a name that is a domain name
 *
 * @param host_name - the host name
 * @return true if the address was retrieved
 */
bool bip_get_addr_by_name(const char *host_name, BACNET_IP_ADDRESS *addr)
{
    struct hostent *host_ent;

    if ((host_ent = gethostbyname(host_name)) == NULL) {
        return false;
    }

    if (addr) {
        /* Host addresses in a struct hostent structure are always
           given in network byte order */
        /* h_addr: This is a synonym for h_addr_list[0];
           in other words, it is the first host address.*/
        memcpy(&addr->address[0], host_ent->h_addr, 4);
    }

    return true;
}

static void *get_addr_ptr(struct sockaddr *sockaddr_ptr)
{
    void *addr_ptr = NULL;
    if (sockaddr_ptr->sa_family == AF_INET) {
        addr_ptr = &((struct sockaddr_in *)sockaddr_ptr)->sin_addr;
    } else if (sockaddr_ptr->sa_family == AF_INET6) {
        addr_ptr = &((struct sockaddr_in6 *)sockaddr_ptr)->sin6_addr;
    }
    return addr_ptr;
}

/**
 * @brief Get the default interface name using routing info
 * @return interface name, or NULL if not found or none
 */
static char *ifname_default(void)
{
    if (BIP_Interface_Name[0] != 0) {
        return BIP_Interface_Name;
    }
    snprintf(BIP_Interface_Name, sizeof(BIP_Interface_Name), "%s", "en0");

    return BIP_Interface_Name;
}

/** Gets the local IP address and local broadcast address from the system,
 *  and saves it into the BACnet/IP data structures.
 *
 * @param ifname [in] The named interface to use for the network layer.
 *        Eg, for MAC OS X, ifname is en0, en1, and others.
 * @param addr [out] The netmask addr, broadcast addr, ip addr.
 * @param request [in] addr broadaddr netmask
 */
/**
 * @brief Issue a specific request foor an interface via an ioctl() call.
 * @param ifname - the interface name
 * @param addr [out] the address in host order.
 * @param request - the ioctl() request
 * @return 0 on success, else the error from the ioctl() call.
 */
int bip_get_local_address_ioctl(
    const char *ifname, struct in_addr *addr, uint32_t request)
{
    char rv = '\0'; /* return value */

    struct ifaddrs *ifaddrs_ptr;
    int status;
    status = getifaddrs(&ifaddrs_ptr);
    if (status == -1) {
        fprintf(
            stderr, "Error in 'getifaddrs': %d (%s)\n", errno, strerror(errno));
    }
    while (ifaddrs_ptr) {
        if ((ifaddrs_ptr->ifa_addr->sa_family == AF_INET) &&
            (strcmp(ifaddrs_ptr->ifa_name, ifname) == 0)) {
            void *addr_ptr = NULL;
            if (!ifaddrs_ptr->ifa_addr) {
                return rv;
            }
            switch (request) {
                case SIOCGIFADDR:
                    addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_addr);
                    break;
                case SIOCGIFBRDADDR:
                    addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_broadaddr);
                    break;
                case SIOCGIFNETMASK:
                    addr_ptr = get_addr_ptr(ifaddrs_ptr->ifa_netmask);
                    break;
                default:
                    break;
            }
            if (addr_ptr) {
                memcpy(addr, addr_ptr, sizeof(struct in_addr));
            }
        }
        ifaddrs_ptr = ifaddrs_ptr->ifa_next;
    }
    freeifaddrs(ifaddrs_ptr);
    return rv;
}

/**
 * @brief Get the netmask of the BACnet/IP's interface via an getifaddrs() call.
 * @param netmask [out] The netmask, in host order.
 * @return 0 on success, else the error from the getifaddrs() call.
 */
int bip_get_local_netmask(struct in_addr *netmask)
{
    int rv;
    char *ifname = getenv("BACNET_IFACE");

    if (ifname == NULL) {
        ifname = "en0";
    }
    rv = bip_get_local_address_ioctl(ifname, netmask, SIOCGIFNETMASK);

    return rv;
}

/**
 * @brief Set the broadcast socket binding address
 * @param baddr The broadcast socket binding address, in host order.
 * @return 0 on success
 */
int bip_set_broadcast_binding(const char *ip4_broadcast)
{
    BIP_Broadcast_Binding_Address.s_addr = inet_addr(ip4_broadcast);
    BIP_Broadcast_Binding_Address_Override = true;

    return 0;
}

/** Gets the local IP address and local broadcast address from the system,
 *  and saves it into the BACnet/IP data structures.
 *
 * @param ifname [in] The named interface to use for the network layer.
 *        Eg, for MAC OS X, ifname is en0, en1, and others.
 */
void bip_set_interface(const char *ifname)
{
    struct in_addr local_address;
    struct in_addr broadcast_address;
    int rv = 0;

    /* setup local address */
    rv = bip_get_local_address_ioctl(ifname, &local_address, SIOCGIFADDR);
    if (rv < 0) {
        local_address.s_addr = 0;
    }
    BIP_Address.s_addr = local_address.s_addr;
    if (BIP_Debug) {
        fprintf(stderr, "BIP: Interface: %s\n", ifname);
        fprintf(stderr, "BIP: Address: %s\n", inet_ntoa(local_address));
        fflush(stderr);
    }
    /* setup local broadcast address */
#ifdef BACNET_IP_BROADCAST_USE_CLASSADDR
    long broadcast_address;
    long net_address;

    broadcast_address = 0;
    net_address = local_address.s_addr;
    if (IN_CLASSA(ntohl(net_address))) {
        broadcast_address =
            (ntohl(net_address) & ~IN_CLASSA_HOST) | IN_CLASSA_HOST;
    } else if (IN_CLASSB(ntohl(net_address))) {
        broadcast_address =
            (ntohl(net_address) & ~IN_CLASSB_HOST) | IN_CLASSB_HOST;
    } else if (IN_CLASSC(ntohl(net_address))) {
        broadcast_address =
            (ntohl(net_address) & ~IN_CLASSC_HOST) | IN_CLASSC_HOST;
    } else if (IN_CLASSD(ntohl(net_address))) {
        broadcast_address =
            (ntohl(net_address) & ~IN_CLASSD_HOST) | IN_CLASSD_HOST;
    } else {
        broadcast_address = INADDR_BROADCAST;
    }
    BIP_Broadcast_Addr.s_addr = htonl(broadcast_address);
#else
    rv =
        bip_get_local_address_ioctl(ifname, &broadcast_address, SIOCGIFBRDADDR);
    if (rv < 0) {
        BIP_Broadcast_Addr.s_addr = ~0;
    } else {
        BIP_Broadcast_Addr = local_address;
        BIP_Broadcast_Addr.s_addr = broadcast_address.s_addr;
    }
#endif
    if (BIP_Debug) {
        fprintf(
            stderr, "BIP: Broadcast Address: %s\n",
            inet_ntoa(BIP_Broadcast_Addr));
        fprintf(
            stderr, "BIP: UDP Port: 0x%04X [%hu]\n", ntohs(BIP_Port),
            ntohs(BIP_Port));
        fflush(stderr);
    }
}

static int createSocket(const struct sockaddr_in *sin)
{
    int status = 0; /* return from socket lib calls */
    int sockopt = 0;
    int sock_fd = -1;

    /* assumes that the driver has already been initialized */
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        return sock_fd;
    }
    /* Allow us to use the same socket for sending and receiving */
    /* This makes sure that the src port is correct when sending */
    sockopt = 1;
    status = setsockopt(
        sock_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    if (status < 0) {
        close(sock_fd);
        return status;
    }
    /* allow us to send a broadcast */
    status = setsockopt(
        sock_fd, SOL_SOCKET, SO_BROADCAST, &sockopt, sizeof(sockopt));
    if (status < 0) {
        close(sock_fd);
        return status;
    }
    /* Bind to the proper interface to send without default gateway */
    /* status = setsockopt(
        sock_fd, SOL_SOCKET, SO_BINDTODEVICE, BIP_Interface_Name,
        strlen(BIP_Interface_Name));
    if (status < 0) {
        if (BIP_Debug) {
            perror("SO_BINDTODEVICE: ");
        }
    } */
    /* bind the socket to the local port number and IP address */
    status =
        bind(sock_fd, (const struct sockaddr *)sin, sizeof(struct sockaddr));
    if (status < 0) {
        close(sock_fd);
        return status;
    }

    return sock_fd;
}

/** Initialize the BACnet/IP services at the given interface.
 * @ingroup DLBIP
 * -# Gets the local IP address and local broadcast address from the system,
 *  and saves it into the BACnet/IP data structures.
 * -# Opens a UDP socket
 * -# Configures the socket for sending and receiving
 * -# Configures the socket so it can send broadcasts
 * -# Binds the socket to the local IP address at the specified port for
 *    BACnet/IP (by default, 0xBAC0 = 47808).
 *
 * @note For MAC OS X, ifname is en0, en1, and others.
 *
 * @param ifname [in] The named interface to use for the network layer.
 *        If NULL, the "en0" interface is assigned.
 * @return True if the socket is successfully opened for BACnet/IP,
 *         else False if the socket functions fail.
 */
bool bip_init(char *ifname)
{
    struct sockaddr_in sin;
    int sock_fd = -1;
    struct sockaddr_in broadcast_sin_config;
    int broadcast_sock_fd;

    if (ifname) {
        snprintf(BIP_Interface_Name, sizeof(BIP_Interface_Name), "%s", ifname);
        bip_set_interface(ifname);
    } else {
        bip_set_interface(ifname_default());
    }
    if (BIP_Address.s_addr == 0) {
        fprintf(
            stderr, "BIP: Failed to get an IP address from %s!\n",
            BIP_Interface_Name);
        fflush(stderr);
        return false;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = BIP_Port;
    memset(&(sin.sin_zero), '\0', sizeof(sin.sin_zero));

    sin.sin_addr.s_addr = BIP_Address.s_addr;
    sock_fd = createSocket(&sin);
    BIP_Socket = sock_fd;
    if (sock_fd < 0) {
        return false;
    }

    broadcast_sin_config.sin_family = AF_INET;
    broadcast_sin_config.sin_port = BIP_Port;
    memset(
        &(broadcast_sin_config.sin_zero), '\0',
        sizeof(broadcast_sin_config.sin_zero));
    if (BIP_Broadcast_Binding_Address_Override) {
        broadcast_sin_config.sin_addr.s_addr =
            BIP_Broadcast_Binding_Address.s_addr;
    } else {
#if defined(BACNET_IP_BROADCAST_USE_INADDR_ANY)
        broadcast_sin_config.sin_addr.s_addr = htonl(INADDR_ANY);
#elif defined(BACNET_IP_BROADCAST_USE_INADDR_BROADCAST)
        broadcast_sin_config.sin_addr.s_addr = htonl(INADDR_BROADCAST);
#else
        broadcast_sin_config.sin_addr.s_addr = BIP_Broadcast_Addr.s_addr;
#endif
    }
    if (broadcast_sin_config.sin_addr.s_addr == BIP_Address.s_addr) {
        /* handle the case when a network interface on the system
           reports the interface's unicast IP address as being
           the same as its broadcast IP address */
        BIP_Broadcast_Socket = BIP_Socket;
    } else {
        broadcast_sock_fd = createSocket(&broadcast_sin_config);
        BIP_Broadcast_Socket = broadcast_sock_fd;
        if (broadcast_sock_fd < 0) {
            bip_cleanup();
            return false;
        }
    }

    bvlc_init();

    return true;
}

/**
 * @brief Determine if this BACnet/IP datalink is valid
 * @return true if the BACnet/IP datalink is valid
 */
bool bip_valid(void)
{
    return (BIP_Socket != -1);
}

/** Cleanup and close out the BACnet/IP services by closing the socket.
 * @ingroup DLBIP
 */
void bip_cleanup(void)
{
    if (BIP_Socket != -1) {
        close(BIP_Socket);
    }
    BIP_Socket = -1;

    if (BIP_Broadcast_Socket != -1) {
        close(BIP_Broadcast_Socket);
    }
    BIP_Broadcast_Socket = -1;
    /* these were set non-zero during interface configuration */
    BIP_Address.s_addr = 0;
    BIP_Broadcast_Addr.s_addr = 0;

    return;
}
