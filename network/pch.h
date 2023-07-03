#ifndef PCH_H
#define PCH_H

#include "common.h"

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
using namespace boost::asio;

#include <boost/system/system_category.hpp>
using boost::system::error_code;

#include "public/connection.h"
#include "public/network_accessor.h"

using AcceptorIndex = int16_t;
#define NOT_FROM_ACCEPTOR -1

#include "connection/send_buffer.h"
#include "connection/send_buffer.h"
#include "connection/receive_buffer.h"
#include "connection/connection_impl.h"
#include "connection_manager.h"
#include "connecter/connecter.h"
#include "listener/listener.h"
#include "network_accessor_impl.h"
#include "network.h"

#endif //PCH_H
