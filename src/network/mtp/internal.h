// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "network/mtp/impl.h"

// Constant that differentiates the protocol from random data and other protocols
#define PROTOCOL_ID 0x4f457403

#define MAX_UDP_PEERS 65535

/*
=== NOTES ===

A packet is sent through a channel to a peer with a basic header:
	Header (7 bytes):
	[0] u32 protocol_id
	[4] session_t sender_peer_id
	[6] u8 channel
sender_peer_id:
	Unique to each peer.
	value 0 (PEER_ID_INEXISTENT) is reserved for making new connections
	value 1 (PEER_ID_SERVER) is reserved for server
	these constants are defined in constants.h
channel:
	Channel numbers have no intrinsic meaning. Currently only 0, 1, 2 exist.
*/
#define BASE_HEADER_SIZE 7
#define CHANNEL_COUNT 3

/*
Packet types:

PACKET_TYPE_CONTROL: This is a packet used by the protocol.
- When this is processed, nothing is handed to the user.
	Header (2 byte):
	[0] u8 type
	[1] u8 controltype
controltype and data description:
	CONTROLTYPE_ACK
		[2] u16 seqnum
	CONTROLTYPE_SET_PEER_ID
		[2] session_t peer_id_new
	CONTROLTYPE_PING
	- There is no actual reply, but this can be sent in a reliable
	  packet to get a reply
	CONTROLTYPE_DISCO
*/

/*
PACKET_TYPE_ORIGINAL: This is a plain packet with no control and no error
checking at all.
- When this is processed, it is directly handed to the user.
	Header (1 byte):
	[0] u8 type
*/
#define ORIGINAL_HEADER_SIZE 1

/*
PACKET_TYPE_SPLIT: These are sequences of packets forming one bigger piece of
data.
- When processed and all the packet_nums 0...packet_count-1 are
  present (this should be buffered), the resulting data shall be
  directly handed to the user.
- If the data fails to come up in a reasonable time, the buffer shall
  be silently discarded.
- These can be sent as-is or atop of a RELIABLE packet stream.
	Header (7 bytes):
	[0] u8 type
	[1] u16 seqnum
	[3] u16 chunk_count
	[5] u16 chunk_num
*/

/*
PACKET_TYPE_RELIABLE: Delivery of all RELIABLE packets shall be forced by ACKs,
and they shall be delivered in the same order as sent. This is done
with a buffer in the receiving and transmitting end.
- When this is processed, the contents of each packet is recursively
  processed as packets.
	Header (3 bytes):
	[0] u8 type
	[1] u16 seqnum

*/
#define RELIABLE_HEADER_SIZE 3
#define SEQNUM_INITIAL 65500
#define SEQNUM_MAX 65535

/****/

template<typename T>
class ConstSharedPtr {
public:
	ConstSharedPtr(T *ptr) : ptr(ptr) {}
	ConstSharedPtr(const std::shared_ptr<T> &ptr) : ptr(ptr) {}

	const T* get() const noexcept { return ptr.get(); }
	const T& operator*() const noexcept { return *ptr.get(); }
	const T* operator->() const noexcept { return ptr.get(); }

private:
	std::shared_ptr<T> ptr;
};

namespace con
{

enum PacketType : u8 {
	PACKET_TYPE_CONTROL = 0,
	PACKET_TYPE_ORIGINAL = 1,
	PACKET_TYPE_SPLIT = 2,
	PACKET_TYPE_RELIABLE = 3,
	PACKET_TYPE_MAX
};

enum ControlType : u8 {
	CONTROLTYPE_ACK = 0,
	CONTROLTYPE_SET_PEER_ID = 1,
	CONTROLTYPE_PING = 2,
	CONTROLTYPE_DISCO = 3,
};

inline bool seqnum_higher(u16 totest, u16 base)
{
	if (totest > base)
	{
		if ((totest - base) > (SEQNUM_MAX/2))
			return false;

		return true;
	}

	if ((base - totest) > (SEQNUM_MAX/2))
		return true;

	return false;
}

inline bool seqnum_in_window(u16 seqnum, u16 next,u16 window_size)
{
	u16 window_start = next;
	u16 window_end   = ( next + window_size ) % (SEQNUM_MAX+1);

	if (window_start < window_end) {
		return ((seqnum >= window_start) && (seqnum < window_end));
	}


	return ((seqnum < window_end) || (seqnum >= window_start));
}

inline float CALC_DTIME(u64 lasttime, u64 curtime)
{
	float value = (curtime - lasttime) / 1000.0f;
	return MYMAX(MYMIN(value, 0.1f), 0.0f);
}

/* Exceptions */

class NotFoundException : public BaseException
{
public:
	NotFoundException(const char *s) : BaseException(s) {}
};

class ProcessedSilentlyException : public BaseException
{
public:
	ProcessedSilentlyException(const char *s) : BaseException(s) {}
};

class ProcessedQueued : public BaseException
{
public:
	ProcessedQueued(const char *s) : BaseException(s) {}
};

class IncomingDataCorruption : public BaseException
{
public:
	IncomingDataCorruption(const char *s) : BaseException(s) {}
};


/*
	Struct for all kinds of packets. Includes following data:
		BASE_HEADER
		u8[] packet data (usually copied from SharedBuffer<u8>)
*/
struct BufferedPacket {
	BufferedPacket(u32 a_size)
	{
		m_data.resize(a_size);
		data = &m_data[0];
	}

	DISABLE_CLASS_COPY(BufferedPacket)

	u16 getSeqnum() const;
	void setSenderPeerId(session_t id);

	inline size_t size() const { return m_data.size(); }

	u8 *data; // Direct memory access
	float time = 0.0f; // Seconds from buffering the packet or re-sending
	float totaltime = 0.0f; // Seconds from buffering the packet
	u64 absolute_send_time = -1;
	Address address; // Sender or destination
	unsigned int resend_count = 0;

private:
	std::vector<u8> m_data; // Data of the packet, including headers
};


// This adds the base headers to the data and makes a packet out of it
BufferedPacketPtr makePacket(const Address &address, const SharedBuffer<u8> &data,
		u32 protocol_id, session_t sender_peer_id, u8 channel);

// Depending on size, make a TYPE_ORIGINAL or TYPE_SPLIT packet
// Increments split_seqnum if a split packet is made
void makeAutoSplitPacket(const SharedBuffer<u8> &data, u32 chunksize_max,
		u16 &split_seqnum, std::list<SharedBuffer<u8>> *list);

// Add the TYPE_RELIABLE header to the data
SharedBuffer<u8> makeReliablePacket(const SharedBuffer<u8> &data, u16 seqnum);

struct IncomingSplitPacket
{
	IncomingSplitPacket(u32 cc, bool r):
		chunk_count(cc), reliable(r) {}

	IncomingSplitPacket() = delete;

	float time = 0.0f; // Seconds from adding
	u32 chunk_count;
	bool reliable; // If true, isn't deleted on timeout

	bool allReceived() const
	{
		return (chunks.size() == chunk_count);
	}
	bool insert(u32 chunk_num, SharedBuffer<u8> &chunkdata);
	SharedBuffer<u8> reassemble();

private:
	// Key is chunk number, value is data without headers
	std::map<u16, SharedBuffer<u8>> chunks;
};

/*
	A buffer which stores reliable packets and sorts them internally
	for fast access to the smallest one.
*/

class ReliablePacketBuffer
{
public:
	bool getFirstSeqnum(u16 &result);

	BufferedPacketPtr popFirst();
	BufferedPacketPtr popSeqnum(u16 seqnum);
	void insert(BufferedPacketPtr &p_ptr, u16 next_expected);
	/// Adjusts the sender peer ID for all packets
	void fixPeerId(session_t id);

	void incrementTimeouts(float dtime);
	u32 getTimedOuts(float timeout);
	// timeout relative to last resend
	std::vector<ConstSharedPtr<BufferedPacket>> getResend(float timeout, u32 max_packets);

	void print();
	bool empty();
	u32 size();


private:
	typedef std::list<BufferedPacketPtr>::iterator FindResult;

	FindResult findPacketNoLock(u16 seqnum);

	std::list<BufferedPacketPtr> m_list;

	u16 m_oldest_non_answered_ack;

	std::mutex m_list_mutex;
};

/*
	A buffer for reconstructing split packets
*/

class IncomingSplitBuffer
{
public:
	~IncomingSplitBuffer();

	/*
		Returns a reference counted buffer of length != 0 when a full split
		packet is constructed. If not, returns one of length 0.
	*/
	SharedBuffer<u8> insert(BufferedPacketPtr &p_ptr, bool reliable);

	void removeUnreliableTimedOuts(float dtime, float timeout);

private:
	// Key is seqnum
	std::map<u16, IncomingSplitPacket*> m_buf;

	std::mutex m_map_mutex;
};

enum ConnectionCommandType{
	CONNCMD_NONE,
	CONNCMD_SERVE,
	CONNCMD_CONNECT,
	CONNCMD_DISCONNECT,
	CONNCMD_DISCONNECT_PEER,
	CONNCMD_SEND,
	CONNCMD_SEND_TO_ALL,
	CONCMD_ACK,
	CONCMD_CREATE_PEER,
	CONNCMD_RESEND_ONE,
	CONNCMD_PEER_ID_SET
};

// This is very similar to ConnectionEvent
struct ConnectionCommand
{
	const ConnectionCommandType type;
	Address address;
	session_t peer_id = PEER_ID_INEXISTENT;
	u8 channelnum = 0;
	Buffer<u8> data;
	bool reliable = false;
	bool raw = false;

	DISABLE_CLASS_COPY(ConnectionCommand);

	static ConnectionCommandPtr serve(Address address);
	static ConnectionCommandPtr connect(Address address);
	static ConnectionCommandPtr disconnect();
	static ConnectionCommandPtr disconnect_peer(session_t peer_id);
	static ConnectionCommandPtr resend_one(session_t peer_id);
	static ConnectionCommandPtr peer_id_set(session_t own_peer_id);
	static ConnectionCommandPtr send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable);
	static ConnectionCommandPtr ack(session_t peer_id, u8 channelnum, const Buffer<u8> &data);
	static ConnectionCommandPtr createPeer(session_t peer_id, const Buffer<u8> &data);

private:
	ConnectionCommand(ConnectionCommandType type_) :
		type(type_) {}

	static ConnectionCommandPtr create(ConnectionCommandType type);
};

/*
 * Window sizes to use, in packets (not bytes!).
 * 0xFFFF is theoretical maximum. don't think about
 * touching it, the less you're away from it the more likely data corruption
 * will occur
 *
 * Note: window sizes directly translate to maximum possible throughput, e.g.
 *       (2048 * 512 bytes) / 33ms = 15 MiB/s
 */

// Due to backwards compatibility we have different window sizes for what we'll
// accept from peers vs. what we use for sending.
#define MAX_RELIABLE_WINDOW_SIZE 0x8000
#define MAX_RELIABLE_WINDOW_SIZE_SEND 2048
/* starting value for window size */
#define START_RELIABLE_WINDOW_SIZE 64
/* minimum value for window size */
#define MIN_RELIABLE_WINDOW_SIZE 32

class Channel
{

public:
	u16 readNextIncomingSeqNum();
	u16 incNextIncomingSeqNum();

	u16 getOutgoingSequenceNumber(bool& successful);
	u16 readOutgoingSequenceNumber();
	bool putBackSequenceNumber(u16);

	u16 readNextSplitSeqNum();
	void setNextSplitSeqNum(u16 seqnum);

	// This is for buffering the incoming packets that are coming in
	// the wrong order
	ReliablePacketBuffer incoming_reliables;
	// This is for buffering the sent packets so that the sender can
	// re-send them if no ACK is received
	ReliablePacketBuffer outgoing_reliables_sent;

	//queued reliable packets
	std::queue<BufferedPacketPtr> queued_reliables;

	//queue commands prior splitting to packets
	std::deque<ConnectionCommandPtr> queued_commands;

	IncomingSplitBuffer incoming_splits;

	Channel() = default;
	~Channel() = default;

	void UpdatePacketLossCounter(unsigned int count);
	void UpdatePacketTooLateCounter();
	void UpdateBytesSent(unsigned int bytes,unsigned int packages=1);
	void UpdateBytesLost(unsigned int bytes);
	void UpdateBytesReceived(unsigned int bytes);

	void UpdateTimers(float dtime);

	float getCurrentDownloadRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return cur_kbps; };
	float getMaxDownloadRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return max_kbps; };

	float getCurrentLossRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return cur_kbps_lost; };
	float getMaxLossRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return max_kbps_lost; };

	float getCurrentIncomingRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return cur_incoming_kbps; };
	float getMaxIncomingRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return max_incoming_kbps; };

	float getAvgDownloadRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return avg_kbps; };
	float getAvgLossRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return avg_kbps_lost; };
	float getAvgIncomingRateKB()
		{ MutexAutoLock lock(m_internal_mutex); return avg_incoming_kbps; };

	u16 getWindowSize() const { return m_window_size; };

	void setWindowSize(long size)
	{
		m_window_size = (u16)rangelim(size, MIN_RELIABLE_WINDOW_SIZE, MAX_RELIABLE_WINDOW_SIZE_SEND);
	}

private:
	std::mutex m_internal_mutex;
	u16 m_window_size = MIN_RELIABLE_WINDOW_SIZE;

	u16 next_incoming_seqnum = SEQNUM_INITIAL;

	u16 next_outgoing_seqnum = SEQNUM_INITIAL;
	u16 next_outgoing_split_seqnum = SEQNUM_INITIAL;

	unsigned int current_packet_loss = 0;
	unsigned int current_packet_too_late = 0;
	unsigned int current_packet_successful = 0;
	float packet_loss_counter = 0.0f;

	unsigned int current_bytes_transfered = 0;
	unsigned int current_bytes_received = 0;
	unsigned int current_bytes_lost = 0;
	float max_kbps = 0.0f;
	float cur_kbps = 0.0f;
	float avg_kbps = 0.0f;
	float max_incoming_kbps = 0.0f;
	float cur_incoming_kbps = 0.0f;
	float avg_incoming_kbps = 0.0f;
	float max_kbps_lost = 0.0f;
	float cur_kbps_lost = 0.0f;
	float avg_kbps_lost = 0.0f;
	float bpm_counter = 0.0f;

	unsigned int rate_samples = 0;
};


class UDPPeer final : public Peer
{
public:

	friend class PeerHelper;
	friend class ConnectionReceiveThread;
	friend class ConnectionSendThread;
	friend class Connection;

	UDPPeer(session_t id, const Address &address, Connection *connection);
	virtual ~UDPPeer() = default;

	void PutReliableSendCommand(ConnectionCommandPtr &c,
							unsigned int max_packet_size) override;

	virtual const Address &getAddress() const override {
		return address;
	}

	u16 getNextSplitSequenceNumber(u8 channel) override;
	void setNextSplitSequenceNumber(u8 channel, u16 seqnum) override;

	SharedBuffer<u8> addSplitPacket(u8 channel, BufferedPacketPtr &toadd,
		bool reliable) override;

	bool isTimedOut(float timeout, std::string &reason) override;

protected:
	/*
		Calculates avg_rtt and resend_timeout.
		rtt=-1 only recalculates resend_timeout
	*/
	void reportRTT(float rtt) override;

	void RunCommandQueues(
					unsigned int max_packet_size,
					unsigned int maxtransfer);

	float getResendTimeout()
		{ MutexAutoLock lock(m_exclusive_access_mutex); return resend_timeout; }

	void setResendTimeout(float timeout)
		{ MutexAutoLock lock(m_exclusive_access_mutex); resend_timeout = timeout; }

	bool Ping(float dtime, SharedBuffer<u8>& data) override;

	Channel channels[CHANNEL_COUNT];
	bool m_pending_disconnect = false;
private:
	// This is changed dynamically
	float resend_timeout = 0.5;

	bool processReliableSendCommand(
					ConnectionCommandPtr &c_ptr,
					unsigned int max_packet_size);
};

}
