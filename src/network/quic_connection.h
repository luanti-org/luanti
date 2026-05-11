// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <ngtcp2/ngtcp2.h>

#include <sys/socket.h> // sockaddr_storage, socklen_t

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "network/address.h"
#include "network/connection.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/peerhandler.h"

struct ngtcp2_crypto_conn_ref;
struct ngtcp2_crypto_ossl_ctx;
struct ssl_st;
typedef struct ssl_st SSL;

namespace con
{

class TlsClientContext;
class TlsServerContext;

/*
 * The Luanti NetworkPackets are put into the QUIC stream like this:
 * [u8 channel] [u8 reliable_flag] [u32 length] [u8[length] payload]
 */
constexpr size_t QUIC_FRAME_HEADER_LEN = 1 + 1 + 4;
constexpr u32 QUIC_FRAME_MAX_LEN = 16 * 1024 * 1024;

class QuicPeer final : public IPeer
{
public:
	QuicPeer(session_t peer_id, const Address &address)
		: IPeer(peer_id), m_address(address) {}
	const Address &getAddress() const override { return m_address; }
	void setAddress(const Address &a) { m_address = a; }
private:
	Address m_address;
};

struct QuicSession {
	ngtcp2_conn *conn = nullptr;
	SSL *ssl = nullptr;
	ngtcp2_crypto_ossl_ctx *ossl_ctx = nullptr;
	ngtcp2_crypto_conn_ref *conn_ref = nullptr;

	int64_t app_stream_id = -1;
	bool stream_open_sent = false;
	bool handshake_completed = false;
	bool peer_added_emitted = false;
	bool draining = false;
	bool timed_out = false;

	// Inbound stream bytes
	std::vector<uint8_t> rx_buffer;
	size_t rx_head = 0;
	// Outbound stream bytes
	std::vector<uint8_t> tx_buffer;
	size_t tx_head = 0;
	std::deque<std::pair<u8, std::vector<uint8_t>>> rx_frames;

	Address remote;
	session_t peer_id = 0;

	std::vector<uint8_t> scid;
	bool destroyed = false;

	~QuicSession();
	void send_app_frame(u8 channel, bool reliable, const uint8_t *data, size_t len);

	// Pointer to the unconsumed portion of tx_buffer
	const uint8_t *tx_unread() const { return tx_buffer.data() + tx_head; }
	size_t tx_unread_size() const { return tx_buffer.size() - tx_head; }
	// Mark n bytes as consumed
	void tx_advance(size_t n);
};

class QuicConnection final : public IConnection
{
public:
	QuicConnection(float timeout, bool ipv6, PeerHandler *handler);
	~QuicConnection() override;

	void Serve(Address bind_addr) override;
	void Connect(Address address) override;
	bool Connected() override;
	void Disconnect() override;
	void DisconnectPeer(session_t peer_id) override;

	bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms) override;
	void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt,
		bool reliable) override;

	session_t GetPeerID() const override { return m_local_peer_id; }
	Address GetPeerAddress(session_t peer_id) override;
	float getPeerStat(session_t, rtt_stat_type) override { return -1.0f; }
	float getLocalStat(rate_stat_type) override { return -1.0f; }

	void setServerCertificates(const std::string &cert_pem_path,
		const std::string &key_pem_path);

	// ngtcp2 callbacks must be reachable from extern "C" trampolines.
	static int cb_recv_stream_data(ngtcp2_conn *conn, uint32_t flags,
		int64_t stream_id, uint64_t offset, const uint8_t *data,
		size_t datalen, void *user_data, void *stream_user_data);
	static int cb_handshake_completed(ngtcp2_conn *conn, void *user_data);
	static int cb_stream_open(ngtcp2_conn *conn, int64_t stream_id,
		void *user_data);
	static int cb_stream_close(ngtcp2_conn *conn, uint32_t flags,
		int64_t stream_id, uint64_t app_error_code, void *user_data,
		void *stream_user_data);
	static int cb_acked_stream_data_offset(ngtcp2_conn *conn,
		int64_t stream_id, uint64_t offset, uint64_t datalen,
		void *user_data, void *stream_user_data);
	static int cb_get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid,
		uint8_t *token, size_t cidlen, void *user_data);
	static void cb_rand(uint8_t *dest, size_t destlen,
		const ngtcp2_rand_ctx *rand_ctx);

	static ngtcp2_conn *get_conn_from_ref(ngtcp2_crypto_conn_ref *ref);

private:
	struct PendingDelete {
		session_t pid = 0;
		Address remote;
		bool timed_out = false;
		bool emit_callback = false;
		std::unique_ptr<QuicSession> session;
	};

	bool stepIO(int timeout_ms);
	void drainSocket();
	void flushAllSessions();
	void handleAllExpiries();
	std::vector<PendingDelete> reapDeadSessions();

	QuicSession *getOrCreateServerSession(const Address &remote,
		const uint8_t *pkt, size_t pktlen);
	std::unique_ptr<QuicSession> makeServerSession(const Address &remote,
		const uint8_t *initial_pkt, size_t initial_pktlen);
	std::unique_ptr<QuicSession> makeClientSession(const Address &remote);

	int handleIncomingPacket(QuicSession &s, const uint8_t *pkt, size_t pktlen);
	void writeAndSendPackets(QuicSession &s);
	void extractFrames(QuicSession &s);
	void writeConnectionClose(QuicSession &s);
	void cacheLocalSockaddr();

	PeerHandler *m_handler;
	float m_timeout;
	bool m_ipv6;

	int m_socket_fd = -1;
	bool m_serving = false;
	bool m_connecting = false;
	Address m_bind_addr;
	Address m_local_addr_observed;
	// Cached form of the local address for ngtcp2_path and sendto
	struct sockaddr_storage m_local_ss{};
	socklen_t m_local_slen = 0;
	session_t m_local_peer_id = 0;

	std::unique_ptr<TlsClientContext> m_client_tls;
	std::unique_ptr<TlsServerContext> m_server_tls;
	std::string m_server_cert_pem;
	std::string m_server_key_pem;

	std::map<session_t, std::unique_ptr<QuicSession>> m_sessions;
	session_t m_next_server_peer_id = 2;
	std::mutex m_mutex;
};

IConnection *createQUIC(float timeout, bool ipv6, PeerHandler *handler);

} // namespace con
