// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "network/connection.h"
#include "network/socket.h"
#include "constants.h"
#include "util/pointer.h"
#include "util/container.h"
#include "util/numeric.h"
#include "porting.h"
#include "network/networkprotocol.h"
#include <atomic>
#include <iostream>
#include <vector>
#include <memory>
#include <map>

namespace con
{

class ConnectionReceiveThread;
class ConnectionSendThread;

class Peer;

class PeerHelper
{
public:
	PeerHelper() = default;
	inline PeerHelper(Peer *peer) { *this = peer; }
	~PeerHelper();

	PeerHelper& operator=(Peer *peer);
	inline Peer* operator->() const { return m_peer; }
	inline Peer* operator&() const { return m_peer; }

	inline bool operator!() { return !m_peer; }
	inline bool operator!=(std::nullptr_t) { return !!m_peer; }

private:
	Peer *m_peer = nullptr;
};

/*
	Connection
*/

enum ConnectionEventType {
	CONNEVENT_NONE,
	CONNEVENT_DATA_RECEIVED,
	CONNEVENT_PEER_ADDED,
	CONNEVENT_PEER_REMOVED,
	CONNEVENT_BIND_FAILED,
};

struct ConnectionEvent;
typedef std::shared_ptr<ConnectionEvent> ConnectionEventPtr;

// This is very similar to ConnectionCommand
struct ConnectionEvent
{
	const ConnectionEventType type;
	session_t peer_id = 0;
	Buffer<u8> data;
	bool timeout = false;
	Address address;

	// We don't want to copy "data"
	DISABLE_CLASS_COPY(ConnectionEvent);

	static ConnectionEventPtr create(ConnectionEventType type);
	static ConnectionEventPtr dataReceived(session_t peer_id, const Buffer<u8> &data);
	static ConnectionEventPtr peerAdded(session_t peer_id, Address address);
	static ConnectionEventPtr peerRemoved(session_t peer_id, bool is_timeout, Address address);
	static ConnectionEventPtr bindFailed();

	const char *describe() const;

private:
	ConnectionEvent(ConnectionEventType type_) :
		type(type_) {}
};

struct ConnectionCommand;
typedef std::shared_ptr<ConnectionCommand> ConnectionCommandPtr;

struct BufferedPacket;
typedef std::shared_ptr<BufferedPacket> BufferedPacketPtr;

class Connection;
class PeerHandler;

class Peer : public IPeer {
	public:
		friend class PeerHelper;

		virtual ~Peer() {
			MutexAutoLock usage_lock(m_exclusive_access_mutex);
			FATAL_ERROR_IF(m_usage != 0, "Reference counting failure");
		}

		void Drop();

		virtual void PutReliableSendCommand(ConnectionCommandPtr &c,
						unsigned int max_packet_size) {};

		bool isPendingDeletion() const {
			MutexAutoLock lock(m_exclusive_access_mutex);
			return m_pending_deletion;
		}
		void ResetTimeout() {
			MutexAutoLock lock(m_exclusive_access_mutex);
			m_timeout_counter = 0;
		}

		bool isHalfOpen() const {
			MutexAutoLock lock(m_exclusive_access_mutex);
			return m_half_open;
		}
		void SetFullyOpen() {
			MutexAutoLock lock(m_exclusive_access_mutex);
			m_half_open = false;
		}

		virtual bool isTimedOut(float timeout, std::string &reason);

		unsigned int m_increment_packets_remaining = 0;

		virtual u16 getNextSplitSequenceNumber(u8 channel) { return 0; };
		virtual void setNextSplitSequenceNumber(u8 channel, u16 seqnum) {};
		virtual SharedBuffer<u8> addSplitPacket(u8 channel, BufferedPacketPtr &toadd,
				bool reliable)
		{
			FATAL_ERROR("unimplemented in abstract class");
		}

		virtual bool Ping(float dtime, SharedBuffer<u8>& data) { return false; };

		virtual float getStat(rtt_stat_type type) const {
			switch (type) {
				case MIN_RTT:
					return m_rtt.min_rtt;
				case MAX_RTT:
					return m_rtt.max_rtt;
				case AVG_RTT:
					return m_rtt.avg_rtt;
				case MIN_JITTER:
					return m_rtt.jitter_min;
				case MAX_JITTER:
					return m_rtt.jitter_max;
				case AVG_JITTER:
					return m_rtt.jitter_avg;
			}
			return -1;
		}

	protected:
		Peer(session_t id, const Address &address, Connection *connection) :
			IPeer(id),
			m_connection(connection),
			address(address),
			m_last_timeout_check(porting::getTimeMs())
		{
		}

		virtual void reportRTT(float rtt) {};

		void RTTStatistics(float rtt,
							const std::string &profiler_id = "",
							unsigned int num_samples = 1000);

		bool IncUseCount();
		void DecUseCount();

		mutable std::mutex m_exclusive_access_mutex;

		bool m_pending_deletion = false;

		Connection *m_connection;

		// Address of the peer
		Address address;

		// Ping timer
		float m_ping_timer = 0.0f;

	private:
		struct rttstats {
			float jitter_min = FLT_MAX;
			float jitter_max = 0.0f;
			float jitter_avg = -1.0f;
			float min_rtt = FLT_MAX;
			float max_rtt = 0.0f;
			float avg_rtt = -1.0f;
		};

		rttstats m_rtt;
		float m_last_rtt = -1.0f;

		/*
			Until the peer has communicated with us using their assigned peer id
			the connection is considered half-open.
			During this time we inhibit re-sending any reliables or pings. This
			is to avoid spending too many resources on a potential DoS attack
			and to make sure Minetest servers are not useful for UDP amplificiation.
		*/
		bool m_half_open = true;

		// current usage count
		unsigned int m_usage = 0;

		// Seconds from last receive
		float m_timeout_counter = 0.0f;

		u64 m_last_timeout_check;
};

class UDPPeer;

class Connection final : public IConnection
{
public:
	friend class ConnectionSendThread;
	friend class ConnectionReceiveThread;

	Connection(u32 max_packet_size, float timeout, bool ipv6,
			PeerHandler *peerhandler);
	~Connection();

	/* Interface */
	ConnectionEventPtr waitEvent(u32 timeout_ms);

	void putCommand(ConnectionCommandPtr c);

	void Serve(Address bind_addr);
	void Connect(Address address);
	bool Connected();
	void Disconnect();
	bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms);
	void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable);
	session_t GetPeerID() const { return m_peer_id; }
	Address GetPeerAddress(session_t peer_id);
	float getPeerStat(session_t peer_id, rtt_stat_type type);
	float getLocalStat(rate_stat_type type);
	u32 GetProtocolID() const { return m_protocol_id; };
	const std::string getDesc();
	void DisconnectPeer(session_t peer_id);

protected:
	PeerHelper getPeerNoEx(session_t peer_id);
	session_t   lookupPeer(const Address& sender);

	session_t createPeer(const Address& sender, int fd);
	UDPPeer*  createServerPeer(const Address& sender);
	bool deletePeer(session_t peer_id, bool timeout);

	void SetPeerID(session_t id);

	void doResendOne(session_t peer_id);

	void sendAck(session_t peer_id, u8 channelnum, u16 seqnum);

	std::vector<session_t> getPeerIDs()
	{
		MutexAutoLock peerlock(m_peers_mutex);
		return m_peer_ids;
	}

	u32 getActiveCount();

	UDPSocket m_udpSocket;
	// Command queue: user -> SendThread
	MutexedQueue<ConnectionCommandPtr> m_command_queue;

	void putEvent(ConnectionEventPtr e);

	void TriggerSend();

	bool ConnectedToServer()
	{
		return getPeerNoEx(PEER_ID_SERVER) != nullptr;
	}
private:
	// Event queue: ReceiveThread -> user
	MutexedQueue<ConnectionEventPtr> m_event_queue;

	session_t m_peer_id = 0;
	u32 m_protocol_id;

	std::map<session_t, Peer *> m_peers;
	std::vector<session_t> m_peer_ids;
	std::mutex m_peers_mutex;

	std::unique_ptr<ConnectionSendThread> m_sendThread;
	std::unique_ptr<ConnectionReceiveThread> m_receiveThread;

	mutable std::mutex m_info_mutex;

	// Backwards compatibility
	PeerHandler *m_bc_peerhandler;

	std::atomic<bool> m_shutting_down = false;
};

} // namespace
