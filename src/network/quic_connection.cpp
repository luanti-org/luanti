// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "network/quic_connection.h"

#include "network/quic_tls.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "constants.h"
#include "util/base64.h"
#include "util/serialize.h"

namespace con
{

namespace {

// ngtcp2 requires a monotonic counter at nanosecond resolution
uint64_t timestamp_ns()
{
	using namespace std::chrono;
	return static_cast<uint64_t>(duration_cast<nanoseconds>(
		steady_clock::now().time_since_epoch()).count());
}

// Convert a Luanti Address to a sockaddr_storage. Returns the actual length used.
socklen_t to_sockaddr(const Address &a, struct sockaddr_storage *ss)
{
	std::memset(ss, 0, sizeof(*ss));
	if (a.getFamily() == AF_INET) {
		auto *sin = reinterpret_cast<struct sockaddr_in *>(ss);
		sin->sin_family = AF_INET;
		sin->sin_port = htons(a.getPort());
		sin->sin_addr = a.getAddress();
		return sizeof(*sin);
	}
	auto *sin6 = reinterpret_cast<struct sockaddr_in6 *>(ss);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = htons(a.getPort());
	sin6->sin6_addr = a.getAddress6();
	return sizeof(*sin6);
}

Address from_sockaddr(const struct sockaddr_storage *ss)
{
	Address a;
	if (ss->ss_family == AF_INET) {
		const auto *sin = reinterpret_cast<const struct sockaddr_in *>(ss);
		a.setAddress(ntohl(sin->sin_addr.s_addr));
		a.setPort(ntohs(sin->sin_port));
	} else if (ss->ss_family == AF_INET6) {
		const auto *sin6 = reinterpret_cast<const struct sockaddr_in6 *>(ss);
		IPv6AddressBytes b;
		std::memcpy(b.bytes, sin6->sin6_addr.s6_addr, 16);
		a.setAddress(&b);
		a.setPort(ntohs(sin6->sin6_port));
	}
	return a;
}

// This is how we get to the QuicSession we are using when receiving a
// callback from ngtcp2 which gives a ngtcp2_crypto_conn_ref pointer
struct ConnRefImpl {
	ngtcp2_crypto_conn_ref ref;
	QuicSession *session;
};

void log_printf(void *, const char *fmt, ...)
{
	// Set LUANTI_QUIC_DEBUG=1 in the environment to get verbose ngtcp2 logs
	static const bool verbose = std::getenv("LUANTI_QUIC_DEBUG") != nullptr;
	if (!verbose)
		return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void rand_bytes(uint8_t *dest, size_t destlen)
{
	if (RAND_bytes(dest, static_cast<int>(destlen)) == 1)
		return;

	// Fallback to std::random_device (though I think it's rare for RAND_bytes to fail)
	std::random_device rd;
	for (size_t i = 0; i < destlen; ++i)
		dest[i] = static_cast<uint8_t>(rd());
}

// Build the ngtcp2 callback table once per direction, since the pointers stay the same
const ngtcp2_callbacks &client_callbacks()
{
	static const ngtcp2_callbacks cb = []() {
		ngtcp2_callbacks c{};
		c.client_initial = ngtcp2_crypto_client_initial_cb;
		c.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
		c.encrypt = ngtcp2_crypto_encrypt_cb;
		c.decrypt = ngtcp2_crypto_decrypt_cb;
		c.hp_mask = ngtcp2_crypto_hp_mask_cb;
		c.recv_retry = ngtcp2_crypto_recv_retry_cb;
		c.update_key = ngtcp2_crypto_update_key_cb;
		c.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
		c.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
		c.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
		c.version_negotiation = ngtcp2_crypto_version_negotiation_cb;
		c.rand = QuicConnection::cb_rand;
		c.get_new_connection_id = QuicConnection::cb_get_new_connection_id;
		c.handshake_completed = QuicConnection::cb_handshake_completed;
		c.recv_stream_data = QuicConnection::cb_recv_stream_data;
		c.acked_stream_data_offset = QuicConnection::cb_acked_stream_data_offset;
		c.stream_open = QuicConnection::cb_stream_open;
		c.stream_close = QuicConnection::cb_stream_close;
		return c;
	}();
	return cb;
}

const ngtcp2_callbacks &server_callbacks()
{
	static const ngtcp2_callbacks cb = []() {
		ngtcp2_callbacks c = client_callbacks();
		c.client_initial = nullptr;
		c.recv_retry = nullptr;
		c.recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
		return c;
	}();
	return cb;
}

void make_transport_params(ngtcp2_transport_params &params)
{
	ngtcp2_transport_params_default(&params);
	params.initial_max_streams_bidi = 4;
	params.initial_max_streams_uni = 0;
	params.initial_max_stream_data_bidi_local = 1024 * 1024;
	params.initial_max_stream_data_bidi_remote = 1024 * 1024;
	params.initial_max_data = 4 * 1024 * 1024;
	params.max_idle_timeout = 30 * NGTCP2_SECONDS;
}

void make_default_settings(ngtcp2_settings &settings)
{
	ngtcp2_settings_default(&settings);
	settings.initial_ts = timestamp_ns();
	settings.log_printf = log_printf;
}

} // namespace

/*
	QuicSession
*/

QuicSession::~QuicSession()
{
	if (conn) {
		ngtcp2_conn_del(conn);
		conn = nullptr;
	}
	if (ossl_ctx) {
		ngtcp2_crypto_ossl_ctx_del(ossl_ctx);
		ossl_ctx = nullptr;
	}
	if (ssl) {
		SSL_set_app_data(ssl, nullptr);
		SSL_free(ssl);
		ssl = nullptr;
	}
	if (conn_ref) {
		delete static_cast<ConnRefImpl *>(static_cast<void *>(conn_ref));
		conn_ref = nullptr;
	}
}

void QuicSession::send_app_frame(u8 channel, bool reliable, const uint8_t *data, size_t len)
{
	if (len > QUIC_FRAME_MAX_LEN)
		throw ConnectionException("Outbound NetworkPacket exceeds frame size limit");

	// Compact the buffer
	if (tx_head > 0 && tx_head >= tx_buffer.size() / 2) {
		tx_buffer.erase(tx_buffer.begin(), tx_buffer.begin() + tx_head);
		tx_head = 0;
	}

	size_t old = tx_buffer.size();
	tx_buffer.resize(old + QUIC_FRAME_HEADER_LEN + len);
	uint8_t *p = tx_buffer.data() + old;
	p[0] = channel;
	p[1] = reliable ? 1 : 0;
	writeU32(p + 2, static_cast<u32>(len));
	if (len > 0)
		std::memcpy(p + QUIC_FRAME_HEADER_LEN, data, len);
}

void QuicSession::tx_advance(size_t n)
{
	tx_head += n;
	if (tx_head >= tx_buffer.size()) {
		tx_buffer.clear();
		tx_head = 0;
	}
}

/*
	QuicConnection
*/

QuicConnection::QuicConnection(float timeout, bool ipv6, PeerHandler *handler)
	: m_handler(handler), m_timeout(timeout), m_ipv6(ipv6)
{
	// One-time init for ngtcp2_crypto_ossl which is safe to call repeatedly
	ngtcp2_crypto_ossl_init();
}

QuicConnection::~QuicConnection()
{
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		m_sessions.clear();
	}
	if (m_socket_fd >= 0) {
		::close(m_socket_fd);
		m_socket_fd = -1;
	}
}

void QuicConnection::setServerCertificates(const std::string &cert_pem_path,
	const std::string &key_pem_path)
{
	m_server_cert_pem = cert_pem_path;
	m_server_key_pem = key_pem_path;
}

ngtcp2_conn *QuicConnection::get_conn_from_ref(ngtcp2_crypto_conn_ref *ref)
{
	auto *impl = reinterpret_cast<ConnRefImpl *>(ref);
	return impl->session->conn;
}

void QuicConnection::cb_rand(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *)
{
	rand_bytes(dest, destlen);
}

int QuicConnection::cb_get_new_connection_id(ngtcp2_conn *, ngtcp2_cid *cid,
	uint8_t *token, size_t cidlen, void *)
{
	rand_bytes(cid->data, cidlen);
	cid->datalen = cidlen;
	if (token)
		rand_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN);
	return 0;
}

int QuicConnection::cb_handshake_completed(ngtcp2_conn *, void *user_data)
{
	auto *s = static_cast<QuicSession *>(user_data);
	s->handshake_completed = true;
	return 0;
}

int QuicConnection::cb_stream_open(ngtcp2_conn *, int64_t stream_id, void *user_data)
{
	auto *s = static_cast<QuicSession *>(user_data);
	if (s->app_stream_id < 0)
		s->app_stream_id = stream_id;
	return 0;
}

int QuicConnection::cb_stream_close(ngtcp2_conn *, uint32_t, int64_t, uint64_t,
	void *, void *)
{
	return 0;
}

int QuicConnection::cb_acked_stream_data_offset(ngtcp2_conn *, int64_t, uint64_t,
	uint64_t, void *, void *)
{
	// We hand bytes to ngtcp2_conn_writev_stream and don't keep a separate
	// resend buffer, so there's nothing to free here.
	return 0;
}

int QuicConnection::cb_recv_stream_data(ngtcp2_conn *, uint32_t /* flags */,
	int64_t stream_id, uint64_t /* offset */, const uint8_t *data,
	size_t datalen, void *user_data, void *)
{
	auto *s = static_cast<QuicSession *>(user_data);
	if (s->app_stream_id < 0)
		s->app_stream_id = stream_id;

	if (datalen) {
		s->rx_buffer.insert(s->rx_buffer.end(), data, data + datalen);
		ngtcp2_conn_extend_max_stream_offset(s->conn, stream_id, datalen);
		ngtcp2_conn_extend_max_offset(s->conn, datalen);
	}
	return 0;
}

void QuicConnection::extractFrames(QuicSession &s)
{
	// Parse as many complete frames as possible out of the unread region
	size_t avail = s.rx_buffer.size() - s.rx_head;
	while (avail >= QUIC_FRAME_HEADER_LEN) {
		const uint8_t *p = s.rx_buffer.data() + s.rx_head;
		u8 channel = p[0];
		// We're not currently using the reliable flag p[1]
		u32 len = readU32(p + 2);
		if (len > QUIC_FRAME_MAX_LEN)
			throw InvalidIncomingDataException("QUIC frame too large");
		if (avail < QUIC_FRAME_HEADER_LEN + len)
			// We haven't received the whole frame header yet, so wait for more bytes
			break;

		s.rx_frames.emplace_back(channel,
			std::vector<uint8_t>(p + QUIC_FRAME_HEADER_LEN, p + QUIC_FRAME_HEADER_LEN + len));
		s.rx_head += QUIC_FRAME_HEADER_LEN + len;
		avail -= QUIC_FRAME_HEADER_LEN + len;
	}
	// Compact only when more than half the buffer is already consumed
	if (s.rx_head > 0 && s.rx_head >= s.rx_buffer.size() / 2) {
		if (s.rx_head == s.rx_buffer.size()) {
			s.rx_buffer.clear();
		} else {
			std::memmove(s.rx_buffer.data(),
				s.rx_buffer.data() + s.rx_head,
				s.rx_buffer.size() - s.rx_head);
			s.rx_buffer.resize(s.rx_buffer.size() - s.rx_head);
		}
		s.rx_head = 0;
	}
}

/*
	Socket helpers
*/

namespace {

int open_udp_socket(bool ipv6)
{
	int fd = ::socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		throw ConnectionBindFailed(
			(std::string("socket() failed: ") + std::strerror(errno)).c_str());
	}
	int yes = 1;
	::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (ipv6) {
		if (::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
			::close(fd);
			throw ConnectionBindFailed(
				(std::string("setsockopt(IPV6_V6ONLY) failed: ")
				+ std::strerror(errno)).c_str());
		}
	}
	int flags = ::fcntl(fd, F_GETFL, 0);
	::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	return fd;
}

} // namespace

void QuicConnection::cacheLocalSockaddr()
{
	m_local_slen = sizeof(m_local_ss);
	std::memset(&m_local_ss, 0, sizeof(m_local_ss));
	if (::getsockname(m_socket_fd,
			reinterpret_cast<struct sockaddr *>(&m_local_ss),
			&m_local_slen) != 0) {
		// Should never happen on a successfully bound socket
		m_local_slen = 0;
		return;
	}
	m_local_addr_observed = from_sockaddr(&m_local_ss);
}

void QuicConnection::Serve(Address bind_addr)
{
	std::lock_guard<std::mutex> lk(m_mutex);
	m_bind_addr = bind_addr;
	m_serving = true;

	if (m_server_cert_pem.empty() || m_server_key_pem.empty())
		throw ConnectionException(
			"QuicConnection::Serve() requires setServerCertificates() first");

	m_server_tls = std::make_unique<TlsServerContext>();
	if (!m_server_tls->init(m_server_cert_pem, m_server_key_pem))
		throw ConnectionException("Failed to initialize server TLS context");

	m_socket_fd = open_udp_socket(bind_addr.isIPv6());
	struct sockaddr_storage ss;
	socklen_t slen = to_sockaddr(bind_addr, &ss);
	if (::bind(m_socket_fd, reinterpret_cast<struct sockaddr *>(&ss), slen) < 0) {
		::close(m_socket_fd);
		m_socket_fd = -1;
		throw ConnectionBindFailed(
			(std::string("bind() failed: ") + std::strerror(errno)).c_str());
	}

	cacheLocalSockaddr();
	if (m_local_slen == 0)
		m_local_addr_observed = bind_addr;
}

std::unique_ptr<QuicSession> QuicConnection::makeClientSession(
	const Address &remote)
{
	auto s = std::make_unique<QuicSession>();
	s->remote = remote;

	// SSL_CTX with our verification policy
	if (!m_client_tls) {
		m_client_tls = std::make_unique<TlsClientContext>();
		if (!m_client_tls->init())
			throw ConnectionException("Client TLS init failed");
	}

	s->ssl = SSL_new(m_client_tls->get());
	if (!s->ssl)
		throw ConnectionException("SSL_new failed");

	VerifyConfig vc;
	vc.hostname = remote.getHostName();
	if (vc.hostname.empty())
		vc.hostname = remote.serializeString();
	if (remote.hasPinnedCertSha256()) {
		vc.pinned_sha256_raw = ::base64_decode(remote.getPinnedCertSha256Base64());
	} else if (remote.isNumericHostName()) {
		vc.numeric_no_pin = true;
	}
	if (!m_client_tls->configureSession(s->ssl, vc))
		throw ConnectionException("Client TLS session configuration failed");

	if (ngtcp2_crypto_ossl_ctx_new(&s->ossl_ctx, s->ssl) != 0)
		throw ConnectionException("ngtcp2_crypto_ossl_ctx_new failed");

	auto *ref_impl = new ConnRefImpl;
	std::memset(ref_impl, 0, sizeof(*ref_impl));
	ref_impl->ref.get_conn = &QuicConnection::get_conn_from_ref;
	ref_impl->session = s.get();
	s->conn_ref = &ref_impl->ref;
	SSL_set_app_data(s->ssl, &ref_impl->ref);

	// Connection IDs
	ngtcp2_cid scid, dcid;
	scid.datalen = 8;
	rand_bytes(scid.data, scid.datalen);
	dcid.datalen = 18;
	rand_bytes(dcid.data, dcid.datalen);
	s->scid.assign(scid.data, scid.data + scid.datalen);

	struct sockaddr_storage remote_ss;
	socklen_t remote_slen = to_sockaddr(remote, &remote_ss);

	ngtcp2_path path;
	path.local.addr = reinterpret_cast<ngtcp2_sockaddr *>(&m_local_ss);
	path.local.addrlen = m_local_slen;
	path.remote.addr = reinterpret_cast<ngtcp2_sockaddr *>(&remote_ss);
	path.remote.addrlen = remote_slen;
	path.user_data = nullptr;

	ngtcp2_settings settings;
	make_default_settings(settings);

	ngtcp2_transport_params params;
	make_transport_params(params);

	int rv = ngtcp2_conn_client_new(&s->conn, &dcid, &scid, &path,
		NGTCP2_PROTO_VER_V1, &client_callbacks(), &settings, &params,
		nullptr, s.get());
	if (rv != 0) {
		throw ConnectionException(
			(std::string("ngtcp2_conn_client_new: ") + ngtcp2_strerror(rv)).c_str());
	}
	ngtcp2_conn_set_tls_native_handle(s->conn, s->ossl_ctx);
	return s;
}

void QuicConnection::Connect(Address address)
{
	// We currently don't allow connecting to an IP address without a pinned certificate.
	// If we didn't catch the case here, it would throw a somewhat ambiguous ERR_CRYPTO
	if (address.isNumericHostName() && !address.hasPinnedCertSha256()) {
		throw ConnectionException(
			"A numeric IP target requires a certificate fingerprint pin. Use "
			"'host:port+<base64-sha256>' (paste the line printed by the server on "
			"startup), or connect by hostname for validation against the system CA store.");
	}

	std::lock_guard<std::mutex> lk(m_mutex);
	m_connecting = true;

	m_socket_fd = open_udp_socket(address.isIPv6());

	// Bind to ephemeral local port
	struct sockaddr_storage local_ss;
	std::memset(&local_ss, 0, sizeof(local_ss));
	socklen_t local_slen;
	if (address.isIPv6()) {
		auto *sin6 = reinterpret_cast<struct sockaddr_in6 *>(&local_ss);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = 0;
		local_slen = sizeof(*sin6);
	} else {
		auto *sin = reinterpret_cast<struct sockaddr_in *>(&local_ss);
		sin->sin_family = AF_INET;
		sin->sin_port = 0;
		local_slen = sizeof(*sin);
	}
	if (::bind(m_socket_fd, reinterpret_cast<struct sockaddr *>(&local_ss),
			local_slen) < 0) {
		::close(m_socket_fd);
		m_socket_fd = -1;
		throw ConnectionBindFailed("bind() of client socket failed");
	}

	cacheLocalSockaddr();

	auto session = makeClientSession(address);
	session->peer_id = PEER_ID_SERVER;
	m_local_peer_id = PEER_ID_SERVER;

	auto *raw = session.get();
	m_sessions[PEER_ID_SERVER] = std::move(session);

	writeAndSendPackets(*raw);
}

bool QuicConnection::Connected()
{
	std::lock_guard<std::mutex> lk(m_mutex);
	auto it = m_sessions.find(PEER_ID_SERVER);
	return it != m_sessions.end() && it->second->handshake_completed;
}

void QuicConnection::writeConnectionClose(QuicSession &s)
{
	if (!s.conn || m_socket_fd < 0 || m_local_slen == 0)
		return;

	uint8_t buf[NGTCP2_MAX_UDP_PAYLOAD_SIZE];
	struct sockaddr_storage remote_ss;
	socklen_t remote_slen = to_sockaddr(s.remote, &remote_ss);

	ngtcp2_path_storage ps;
	ngtcp2_path_storage_init(&ps,
		reinterpret_cast<ngtcp2_sockaddr *>(&m_local_ss), m_local_slen,
		reinterpret_cast<ngtcp2_sockaddr *>(&remote_ss), remote_slen,
		nullptr);

	ngtcp2_pkt_info pi;
	ngtcp2_ccerr ccerr;
	ngtcp2_ccerr_default(&ccerr);
	ngtcp2_ssize n = ngtcp2_conn_write_connection_close(s.conn, &ps.path,
		&pi, buf, sizeof(buf), &ccerr, timestamp_ns());
	if (n > 0) {
		(void)::sendto(m_socket_fd, buf, n, 0,
			reinterpret_cast<struct sockaddr *>(&remote_ss), remote_slen);
	}
}

void QuicConnection::Disconnect()
{
	// Move sessions out of the map under the lock, then notify the handler and free
	// them with the lock released. (We need to do this because PeerHandler could
	// re-enter QuicConnection e.g. via DeleteClient and might need the mutex.)
	std::map<session_t, std::unique_ptr<QuicSession>> drained;
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		// Send a CONNECTION_CLOSE for each live session before tearing it down.
		// Without this the other side would have to wait until the idle timeout.
		for (auto &kv : m_sessions)
			writeConnectionClose(*kv.second);
		drained.swap(m_sessions);
	}
	for (auto &kv : drained) {
		if (m_handler && kv.second->peer_added_emitted) {
			QuicPeer p(kv.second->peer_id, kv.second->remote);
			m_handler->deletingPeer(&p, false);
		}
	}
	drained.clear();
}

void QuicConnection::DisconnectPeer(session_t peer_id)
{
	std::lock_guard<std::mutex> lk(m_mutex);
	auto it = m_sessions.find(peer_id);
	if (it == m_sessions.end())
		throw PeerNotFoundException("Peer not found");

	// Send the CONNECTION_CLOSE frame to the peer
	writeConnectionClose(*it->second);

	// We mark the session as draining. reapDeadSessions() will get called in stepIO()
	it->second->draining = true;
	// This is an explicit disconnection
	it->second->timed_out = false;
}

Address QuicConnection::GetPeerAddress(session_t peer_id)
{
	std::lock_guard<std::mutex> lk(m_mutex);
	auto it = m_sessions.find(peer_id);
	if (it == m_sessions.end())
		throw PeerNotFoundException("Peer not found");
	return it->second->remote;
}

void QuicConnection::writeAndSendPackets(QuicSession &s)
{
	if (!s.conn || s.draining || m_local_slen == 0)
		return;

	uint8_t buf[NGTCP2_MAX_UDP_PAYLOAD_SIZE];
	struct sockaddr_storage remote_ss;
	socklen_t remote_slen = to_sockaddr(s.remote, &remote_ss);

	ngtcp2_path_storage ps;
	ngtcp2_path_storage_init(&ps,
		reinterpret_cast<ngtcp2_sockaddr *>(&m_local_ss), m_local_slen,
		reinterpret_cast<ngtcp2_sockaddr *>(&remote_ss), remote_slen, nullptr);

	for (;;) {
		ngtcp2_pkt_info pi;
		ngtcp2_ssize ndatalen = 0;
		int64_t stream_id = -1;
		uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
		ngtcp2_vec vec_storage;
		const ngtcp2_vec *vecs = nullptr;
		size_t vec_count = 0;

		// Open the bidi stream on the client after the handshake is complete
		if (m_connecting && s.handshake_completed && s.app_stream_id < 0) {
			int64_t sid;
			int rv = ngtcp2_conn_open_bidi_stream(s.conn, &sid, &s);
			if (rv == 0) {
				s.app_stream_id = sid;
				s.stream_open_sent = true;
			}
		}

		size_t unread = s.tx_unread_size();
		if (s.app_stream_id >= 0 && unread > 0) {
			vec_storage.base = const_cast<uint8_t *>(s.tx_unread());
			vec_storage.len = unread;
			vecs = &vec_storage;
			vec_count = 1;
			stream_id = s.app_stream_id;
		} else {
			flags = 0;
		}

		ngtcp2_ssize n = ngtcp2_conn_writev_stream(s.conn, &ps.path, &pi,
			buf, sizeof(buf), &ndatalen, flags, stream_id, vecs, vec_count,
			timestamp_ns());

		if (n == 0)
			break;
		if (n == NGTCP2_ERR_WRITE_MORE) {
			if (ndatalen > 0)
				s.tx_advance(static_cast<size_t>(ndatalen));
			continue;
		}
		if (n < 0) {
			fprintf(stderr, "QUIC: writev_stream error: %s\n",
				ngtcp2_strerror(static_cast<int>(n)));
			s.draining = true;
			return;
		}

		if (ndatalen > 0)
			s.tx_advance(static_cast<size_t>(ndatalen));

		// Send the packet on the wire
		ssize_t sent = ::sendto(m_socket_fd, buf, n, 0,
			reinterpret_cast<struct sockaddr *>(&remote_ss), remote_slen);
		if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			fprintf(stderr, "QUIC: sendto failed: %s\n", std::strerror(errno));
	}

	ngtcp2_conn_update_pkt_tx_time(s.conn, timestamp_ns());
}

int QuicConnection::handleIncomingPacket(QuicSession &s, const uint8_t *pkt, size_t pktlen)
{
	struct sockaddr_storage remote_ss;
	socklen_t remote_slen = to_sockaddr(s.remote, &remote_ss);

	ngtcp2_path path;
	path.local.addr = reinterpret_cast<ngtcp2_sockaddr *>(&m_local_ss);
	path.local.addrlen = m_local_slen;
	path.remote.addr = reinterpret_cast<ngtcp2_sockaddr *>(&remote_ss);
	path.remote.addrlen = remote_slen;
	path.user_data = nullptr;

	ngtcp2_pkt_info pi;
	std::memset(&pi, 0, sizeof(pi));

	int rv = ngtcp2_conn_read_pkt(s.conn, &path, &pi, pkt, pktlen,
		timestamp_ns());
	if (rv != 0) {
		if (rv == NGTCP2_ERR_DRAINING || rv == NGTCP2_ERR_CLOSING) {
			s.draining = true;
			return rv;
		}
		fprintf(stderr, "QUIC: read_pkt error: %s\n", ngtcp2_strerror(rv));
	}
	return rv;
}

QuicSession *QuicConnection::getOrCreateServerSession(const Address &remote,
	const uint8_t *pkt, size_t pktlen)
{
	for (auto &kv : m_sessions) {
		if (kv.second->remote == remote)
			return kv.second.get();
	}
	auto s = makeServerSession(remote, pkt, pktlen);
	if (!s)
		return nullptr;
	session_t pid = m_next_server_peer_id++;
	s->peer_id = pid;
	auto *raw = s.get();
	m_sessions.emplace(pid, std::move(s));
	return raw;
}

std::unique_ptr<QuicSession> QuicConnection::makeServerSession(
	const Address &remote, const uint8_t *initial_pkt, size_t initial_pktlen)
{
	ngtcp2_pkt_hd hd;
	int rv = ngtcp2_accept(&hd, initial_pkt, initial_pktlen);
	if (rv != 0)
		return nullptr;

	auto s = std::make_unique<QuicSession>();
	s->remote = remote;

	s->ssl = SSL_new(m_server_tls->get());
	if (!s->ssl)
		throw ConnectionException("SSL_new (server) failed");
	SSL_set_accept_state(s->ssl);
	if (ngtcp2_crypto_ossl_configure_server_session(s->ssl) != 0)
		throw ConnectionException("configure_server_session failed");
	if (ngtcp2_crypto_ossl_ctx_new(&s->ossl_ctx, s->ssl) != 0)
		throw ConnectionException("ngtcp2_crypto_ossl_ctx_new (server) failed");

	auto *ref_impl = new ConnRefImpl;
	std::memset(ref_impl, 0, sizeof(*ref_impl));
	ref_impl->ref.get_conn = &QuicConnection::get_conn_from_ref;
	ref_impl->session = s.get();
	s->conn_ref = &ref_impl->ref;
	SSL_set_app_data(s->ssl, &ref_impl->ref);

	ngtcp2_cid scid;
	scid.datalen = 8;
	rand_bytes(scid.data, scid.datalen);
	s->scid.assign(scid.data, scid.data + scid.datalen);

	struct sockaddr_storage remote_ss;
	socklen_t remote_slen = to_sockaddr(remote, &remote_ss);

	ngtcp2_path path;
	path.local.addr = reinterpret_cast<ngtcp2_sockaddr *>(&m_local_ss);
	path.local.addrlen = m_local_slen;
	path.remote.addr = reinterpret_cast<ngtcp2_sockaddr *>(&remote_ss);
	path.remote.addrlen = remote_slen;
	path.user_data = nullptr;

	ngtcp2_settings settings;
	make_default_settings(settings);
	settings.token = nullptr;
	settings.tokenlen = 0;

	ngtcp2_transport_params params;
	make_transport_params(params);
	params.original_dcid = hd.dcid;
	params.original_dcid_present = 1;

	rv = ngtcp2_conn_server_new(&s->conn, &hd.scid, &scid, &path,
		hd.version, &server_callbacks(), &settings, &params, nullptr, s.get());
	if (rv != 0) {
		throw ConnectionException(
			(std::string("ngtcp2_conn_server_new: ") + ngtcp2_strerror(rv)).c_str());
	}
	ngtcp2_conn_set_tls_native_handle(s->conn, s->ossl_ctx);
	return s;
}

void QuicConnection::drainSocket()
{
	// Defer peer callbacks until after we drop m_mutex so the
	// PeerHandler can safely call back into the connection
	struct PendingPeerAdded {
		session_t pid;
		Address remote;
	};
	std::vector<PendingPeerAdded> pending_added;

	uint8_t buf[2048];
	for (;;) {
		struct sockaddr_storage from_ss;
		socklen_t from_slen = sizeof(from_ss);
		ssize_t n = ::recvfrom(m_socket_fd, buf, sizeof(buf), 0,
			reinterpret_cast<struct sockaddr *>(&from_ss), &from_slen);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			fprintf(stderr, "QUIC: recvfrom failed: %s\n", std::strerror(errno));
			break;
		}
		if (n == 0)
			continue;

		Address remote = from_sockaddr(&from_ss);
		size_t plen = static_cast<size_t>(n);

		QuicSession *target = nullptr;
		if (m_serving) {
			target = getOrCreateServerSession(remote, buf, plen);
			if (!target) {
				fprintf(stderr, "QUIC: dropping packet from unknown peer\n");
				continue;
			}
			if (m_handler && !target->peer_added_emitted) {
				pending_added.push_back({target->peer_id, target->remote});
				target->peer_added_emitted = true;
			}
		} else {
			auto it = m_sessions.find(PEER_ID_SERVER);
			if (it == m_sessions.end())
				continue;
			target = it->second.get();
		}

		handleIncomingPacket(*target, buf, plen);
		extractFrames(*target);

		if (!m_serving && m_handler && target->handshake_completed
				&& !target->peer_added_emitted) {
			pending_added.push_back({target->peer_id, target->remote});
			target->peer_added_emitted = true;
		}
	}

	if (m_handler && !pending_added.empty()) {
		m_mutex.unlock();
		for (auto &p : pending_added) {
			QuicPeer peer(p.pid, p.remote);
			m_handler->peerAdded(&peer);
		}
		m_mutex.lock();
	}
}

void QuicConnection::flushAllSessions()
{
	for (auto &kv : m_sessions)
		writeAndSendPackets(*kv.second);
}

void QuicConnection::handleAllExpiries()
{
	const uint64_t now = timestamp_ns();
	for (auto &kv : m_sessions) {
		QuicSession &s = *kv.second;
		if (!s.conn || s.draining)
			continue;

		ngtcp2_tstamp expiry = ngtcp2_conn_get_expiry(s.conn);
		if (expiry == UINT64_MAX || expiry > now)
			continue;

		int rv = ngtcp2_conn_handle_expiry(s.conn, now);
		if (rv != 0) {
			// IDLE_CLOSE / DRAINING / CLOSING all end the session
			s.draining = true;
			s.timed_out = (rv == NGTCP2_ERR_IDLE_CLOSE);
		}
	}
}

// Removes sessions marked as closing or draining by ngtcp2 and returns them in a vector
// The caller must release m_mutex before invoking any m_handler callbacks
std::vector<QuicConnection::PendingDelete> QuicConnection::reapDeadSessions()
{
	std::vector<PendingDelete> reaped;
	for (auto it = m_sessions.begin(); it != m_sessions.end();) {
		QuicSession *s = it->second.get();
		bool dead = !s->conn
			|| s->draining
			|| ngtcp2_conn_in_closing_period(s->conn)
			|| ngtcp2_conn_in_draining_period(s->conn);
		if (dead) {
			PendingDelete pd;
			pd.pid = s->peer_id;
			pd.remote = s->remote;
			pd.timed_out = s->timed_out;
			pd.emit_callback = m_handler && s->peer_added_emitted;
			pd.session = std::move(it->second);
			reaped.push_back(std::move(pd));
			it = m_sessions.erase(it);
		} else {
			++it;
		}
	}
	// If the map is now empty, we can recycle peer IDs from 2 again
	if (m_serving && m_sessions.empty())
		m_next_server_peer_id = 2;
	return reaped;
}

bool QuicConnection::stepIO(int timeout_ms)
{
	struct pollfd pfd;
	pfd.fd = m_socket_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	int rc = ::poll(&pfd, 1, timeout_ms);
	if (rc < 0)
		// EINTR, etc.
		return false;

	if (pfd.revents & POLLIN)
		drainSocket();

	// Run ngtcp2's internal timers for closing/draining sessions
	handleAllExpiries();

	// Flush any pending bytes
	flushAllSessions();

	// Reap sessions that ngtcp2 considers dead
	auto reaped = reapDeadSessions();
	if (!reaped.empty() && m_handler) {
		m_mutex.unlock();
		for (auto &pd : reaped) {
			if (pd.emit_callback) {
				QuicPeer p(pd.pid, pd.remote);
				m_handler->deletingPeer(&p, pd.timed_out);
			}
		}
		reaped.clear();
		m_mutex.lock();
	}

	// Return true if something arrived to process
	for (auto &kv : m_sessions) {
		if (!kv.second->rx_frames.empty())
			return true;
	}
	return false;
}

bool QuicConnection::ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms)
{
	std::unique_lock<std::mutex> lk(m_mutex);

	auto next_frame = [&]() -> bool {
		for (auto &kv : m_sessions) {
			if (!kv.second->rx_frames.empty()) {
				auto frame = std::move(kv.second->rx_frames.front());
				kv.second->rx_frames.pop_front();
				const auto &payload = frame.second;
				if (payload.size() < 2)
					return false;
				pkt->putRawPacket(payload.data(),
					static_cast<u32>(payload.size()), kv.first);
				return true;
			}
		}
		return false;
	};

	if (next_frame())
		return true;

	// I/O loop until either we have a frame or the timeout expires
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
	for (;;) {
		auto now = std::chrono::steady_clock::now();
		int remaining_ms = (timeout_ms == 0) ? 0
			: static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
				deadline - now).count());
		if (remaining_ms < 0)
			remaining_ms = 0;

		stepIO(remaining_ms);

		if (next_frame())
			return true;
		if (timeout_ms == 0 || std::chrono::steady_clock::now() >= deadline)
			return false;
	}
}

void QuicConnection::Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable)
{
	std::lock_guard<std::mutex> lk(m_mutex);
	auto it = m_sessions.find(peer_id);
	if (it == m_sessions.end())
		throw PeerNotFoundException("Peer not found");

	auto buf = pkt->oldForgePacket();
	std::string_view sv = static_cast<std::string_view>(buf);
	it->second->send_app_frame(channelnum, reliable,
		reinterpret_cast<const uint8_t *>(sv.data()), sv.size());

	writeAndSendPackets(*it->second);
}

IConnection *createQUIC(float timeout, bool ipv6, PeerHandler *handler)
{
	return new QuicConnection(timeout, ipv6, handler);
}

} // namespace con
