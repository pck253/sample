#ifndef PCH_H
#define PCH_H

#define NETWORK_MODULE 1

#include "common.h"

#include <asio.hpp>

#include "public/connection.h"
#include "public/network_accessor.h"

using AcceptorIndex = int16_t;
#define NOT_FROM_ACCEPTOR -1

#include "connection/send_buffer.h"
#include "connection/receive_buffer.h"
#include "connection/socket_connection_impl.h"
#include "connection/imn_connection_impl.h"
#include "connection_manager.h"
#include "connecter/connecter.h"
#include "listener/listener.h"
#include "internal_module_network/internal_module_network.h"
#include "network_accessor_impl.h"
#include "network.h"

#endif //PCH_H
