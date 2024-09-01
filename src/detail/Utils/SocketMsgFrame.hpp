#ifndef MSG_SEND_RECV_HPP
#define MSG_SEND_RECV_HPP

#include <string>
#include <boost/asio.hpp>

using boost::asio::io_context;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;

inline std::string recvMsgFromSocket(tcp::socket &sock) {
    // 接收消息头
    char msgHeaderBE[4]{};
    boost::asio::read(sock, boost::asio::buffer(msgHeaderBE, 4));

    // 网络字节序转为主机字节序
    std::uint32_t msgBodyLength = 0;
    std::memcpy(&msgBodyLength, msgHeaderBE, 4);
    msgBodyLength = ntohl(msgBodyLength);

    if (msgBodyLength == 0) return {};
    std::vector<char> msgBody(msgBodyLength);
    boost::asio::read(sock, boost::asio::buffer(msgBody.data(), msgBodyLength));
    return {msgBody.begin(), msgBody.end()};
}

inline void sendMsgToSocket(tcp::socket &sock, const std::string &msg) {
    if (msg.empty()) return;
    // 动态分配消息帧内存，包括 4 bytes 的消息长度和消息体
    std::vector<char> msgFrame(4 + msg.size());
    const auto msgLengthBE = htonl(msg.size());
    std::memcpy(msgFrame.data(), &msgLengthBE, 4);
    std::memcpy(msgFrame.data() + 4, msg.data(), msg.size());

    boost::asio::write(sock, boost::asio::buffer(msgFrame, 4 + msg.size()));
}

inline awaitable<std::string> asyncRecvMsgFromSocket(tcp::socket &sock) {
    // 接收消息头
    char msgHeaderBE[4]{};
    co_await async_read(sock, boost::asio::buffer(msgHeaderBE, 4), use_awaitable);

    // 网络字节序转为主机字节序
    std::uint32_t msgBodyLength = 0;
    std::memcpy(&msgBodyLength, msgHeaderBE, 4);
    msgBodyLength = ntohl(msgBodyLength);
    if (msgBodyLength == 0) co_return std::string();

    std::vector<char> msgBody(msgBodyLength);
    co_await async_read(sock, boost::asio::buffer(msgBody.data(), msgBodyLength), use_awaitable);

    co_return std::string(msgBody.begin(), msgBody.end());
}

inline awaitable<void> asyncSendMsgToSocket(tcp::socket &sock, const std::string &msg) {
    if (msg.empty()) co_return;
    // 动态分配消息帧内存，包括 4 bytes 的消息长度和消息体
    std::vector<char> msgFrame(4 + msg.size());
    const auto msgLengthBE = htonl(msg.size());
    std::memcpy(msgFrame.data(), &msgLengthBE, 4);
    std::memcpy(msgFrame.data() + 4, msg.data(), msg.size());

    // 异步发送消息帧
    co_await async_write(sock, boost::asio::buffer(msgFrame));
}

#endif //MSG_SEND_RECV_HPP
