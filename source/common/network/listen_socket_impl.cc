#include "common/network/listen_socket_impl.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <string>

#include "envoy/common/exception.h"

#include "common/api/os_sys_calls_impl.h"
#include "common/common/assert.h"
#include "common/common/fmt.h"
#include "common/network/address_impl.h"
#include "common/network/utility.h"

namespace Envoy {
namespace Network {

void ListenSocketImpl::doBind() {
  const Api::SysCallIntResult result = local_address_->bind(fd_);
  if (result.rc_ == -1) {
    close();
    throw SocketBindException(
        fmt::format("cannot bind '{}': {}", local_address_->asString(), strerror(result.errno_)),
        result.errno_);
  }
  if (local_address_->type() == Address::Type::Ip && local_address_->ip()->port() == 0) {
    // If the port we bind is zero, then the OS will pick a free port for us (assuming there are
    // any), and we need to find out the port number that the OS picked.
    local_address_ = Address::addressFromFd(fd_);
  }
}

void ListenSocketImpl::setListenSocketOptions(const Network::Socket::OptionsSharedPtr& options) {
  if (!Network::Socket::applyOptions(options, *this,
                                     envoy::api::v2::core::SocketOption::STATE_PREBIND)) {
    throw EnvoyException("ListenSocket: Setting socket options failed");
  }
}

void ListenSocketImpl::setupSocket(const Network::Socket::OptionsSharedPtr& options) {
  setListenSocketOptions(options);

  doBind();
}

template <>
void NetworkListenSocket<
    NetworkSocketTrait<Address::SocketType::Stream>>::setPrebindSocketOptions() {

  int on = 1;
  auto& os_syscalls = Api::OsSysCallsSingleton::get();
  Api::SysCallIntResult status =
      os_syscalls.setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  RELEASE_ASSERT(status.rc_ != -1, "failed to set SO_REUSEADDR socket option");
}

template <>
void NetworkListenSocket<
    NetworkSocketTrait<Address::SocketType::Datagram>>::setPrebindSocketOptions() {}

UdsListenSocket::UdsListenSocket(const Address::InstanceConstSharedPtr& address)
    : ListenSocketImpl(address->socket(Address::SocketType::Stream), address) {
  RELEASE_ASSERT(fd_ != -1, "");
  doBind();
}

UdsListenSocket::UdsListenSocket(int fd, const Address::InstanceConstSharedPtr& address)
    : ListenSocketImpl(fd, address) {}

} // namespace Network
} // namespace Envoy
