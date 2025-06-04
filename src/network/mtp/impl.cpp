// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <iomanip>
#include <cerrno>
#include <algorithm>
#include <cmath>
#include "network/mtp/internal.h"
#include "serialization.h"
#include "log.h"
#include "porting.h"
#include "network/mtp/threads.h"
#include "network/peerhandler.h"
#include "network/networkpacket.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/string.h"
#include "settings.h"
#include "profiler.h"

namespace con
{

/******************************************************************************/
/* defines used for debugging and profiling                                   */
/******************************************************************************/
#ifdef NDEBUG
	#define PROFILE(a)
#else
	#define PROFILE(a) a
#endif

// TODO: Clean this up.
#define LOG(a) a

#define PING_TIMEOUT 5.0f

// exponent base
#define RESEND_SCALE_BASE 1.5f

// since spacing is exponential the numbers here shouldn't be too high
// (it's okay to start out quick)
#define RESEND_TIMEOUT_MIN 0.1f
#define RESEND_TIMEOUT_MAX 2.0f
#define RESEND_TIMEOUT_FACTOR 2

u16 BufferedPacket::getSeqnum() const
{
	if (size() < BASE_HEADER_SIZE + 3)
		return 0; // should never happen

	return readU16(&data[BASE_HEADER_SIZE + 1]);
}

void BufferedPacket::setSenderPeerId(session_t id)
{
	if (size() < BASE_HEADER_SIZE) {
		assert(false); // should never happen
		return;
	}
	writeU16(&data[4], id);
}

BufferedPacketPtr makePacket(const Address &address, const SharedBuffer<u8> &data,
		u32 protocol_id, session_t sender_peer_id, u8 channel)
{
	u32 packet_size = data.getSize() + BASE_HEADER_SIZE;

	auto p = std::make_shared<BufferedPacket>(packet_size);
	p->address = address;

	writeU32(&p->data[0], protocol_id);
	writeU16(&p->data[4], sender_peer_id);
	writeU8(&p->data[6], channel);

	memcpy(&p->data[BASE_HEADER_SIZE], *data, data.getSize());

	return p;
}

SharedBuffer<u8> makeOriginalPacket(const SharedBuffer<u8> &data)
{
	u32 header_size = 1;
	u32 packet_size = data.getSize() + header_size;
	SharedBuffer<u8> b(packet_size);

	writeU8(&(b[0]), PACKET_TYPE_ORIGINAL);
	if (data.getSize() > 0) {
		memcpy(&(b[header_size]), *data, data.getSize());
	}
	return b;
}

// Split data in chunks and add TYPE_SPLIT headers to them
void makeSplitPacket(const SharedBuffer<u8> &data, u32 chunksize_max, u16 seqnum,
		std::list<SharedBuffer<u8>> *chunks)
{
	// Chunk packets, containing the TYPE_SPLIT header
	const u32 chunk_header_size = 7;
	const u32 maximum_data_size = chunksize_max - chunk_header_size;
	u32 start = 0, end = 0;
	u16 chunk_num = 0;
	do {
		end = start + maximum_data_size - 1;
		if (end > data.getSize() - 1)
			end = data.getSize() - 1;

		u32 payload_size = end - start + 1;
		u32 packet_size = chunk_header_size + payload_size;

		SharedBuffer<u8> chunk(packet_size);

		writeU8(&chunk[0], PACKET_TYPE_SPLIT);
		writeU16(&chunk[1], seqnum);
		// [3] u16 chunk_count is written at next stage
		writeU16(&chunk[5], chunk_num);
		memcpy(&chunk[chunk_header_size], &data[start], payload_size);

		chunks->push_back(chunk);

		start = end + 1;
		sanity_check(chunk_num < 0xFFFF); // overflow
		chunk_num++;
	}
	while (end != data.getSize() - 1);

	for (auto &chunk : *chunks) {
		// Write chunk_count
		writeU16(&chunk[3], chunk_num);
	}
}

void makeAutoSplitPacket(const SharedBuffer<u8> &data, u32 chunksize_max,
		u16 &split_seqnum, std::list<SharedBuffer<u8>> *list)
{
	u32 original_header_size = 1;

	if (data.getSize() + original_header_size > chunksize_max) {
		makeSplitPacket(data, chunksize_max, split_seqnum, list);
		split_seqnum++;
		return;
	}

	list->push_back(makeOriginalPacket(data));
}

SharedBuffer<u8> makeReliablePacket(const SharedBuffer<u8> &data, u16 seqnum)
{
	u32 header_size = 3;
	u32 packet_size = data.getSize() + header_size;
	SharedBuffer<u8> b(packet_size);

	writeU8(&b[0], PACKET_TYPE_RELIABLE);
	writeU16(&b[1], seqnum);

	memcpy(&b[header_size], *data, data.getSize());

	return b;
}

/*
	ReliablePacketBuffer
*/

void ReliablePacketBuffer::print()
{
	MutexAutoLock listlock(m_list_mutex);
	LOG(dout_con<<"Dump of ReliablePacketBuffer:" << std::endl);
	unsigned int index = 0;
	for (BufferedPacketPtr &packet : m_list) {
		LOG(dout_con<<index<< ":" << packet->getSeqnum() << std::endl);
		index++;
	}
}

bool ReliablePacketBuffer::empty()
{
	MutexAutoLock listlock(m_list_mutex);
	return m_list.empty();
}

u32 ReliablePacketBuffer::size()
{
	MutexAutoLock listlock(m_list_mutex);
	return m_list.size();
}

ReliablePacketBuffer::FindResult ReliablePacketBuffer::findPacketNoLock(u16 seqnum)
{
	for (auto it = m_list.begin(); it != m_list.end(); ++it) {
		if ((*it)->getSeqnum() == seqnum)
			return it;
	}
	return m_list.end();
}

bool ReliablePacketBuffer::getFirstSeqnum(u16& result)
{
	MutexAutoLock listlock(m_list_mutex);
	if (m_list.empty())
		return false;
	result = m_list.front()->getSeqnum();
	return true;
}

BufferedPacketPtr ReliablePacketBuffer::popFirst()
{
	MutexAutoLock listlock(m_list_mutex);
	if (m_list.empty())
		throw NotFoundException("Buffer is empty");

	BufferedPacketPtr p(m_list.front());
	m_list.pop_front();

	if (m_list.empty()) {
		m_oldest_non_answered_ack = 0;
	} else {
		m_oldest_non_answered_ack = m_list.front()->getSeqnum();
	}
	return p;
}

BufferedPacketPtr ReliablePacketBuffer::popSeqnum(u16 seqnum)
{
	MutexAutoLock listlock(m_list_mutex);
	auto r = findPacketNoLock(seqnum);
	if (r == m_list.end()) {
		LOG(dout_con<<"Sequence number: " << seqnum
				<< " not found in reliable buffer"<<std::endl);
		throw NotFoundException("seqnum not found in buffer");
	}

	BufferedPacketPtr p(*r);
	m_list.erase(r);

	if (m_list.empty()) {
		m_oldest_non_answered_ack = 0;
	} else {
		m_oldest_non_answered_ack = m_list.front()->getSeqnum();
	}
	return p;
}

void ReliablePacketBuffer::insert(BufferedPacketPtr &p_ptr, u16 next_expected)
{
	MutexAutoLock listlock(m_list_mutex);
	const BufferedPacket &p = *p_ptr;

	if (p.size() < BASE_HEADER_SIZE + 3) {
		errorstream << "ReliablePacketBuffer::insert(): Invalid data size for "
			"reliable packet" << std::endl;
		return;
	}
	u8 type = readU8(&p.data[BASE_HEADER_SIZE + 0]);
	if (type != PACKET_TYPE_RELIABLE) {
		errorstream << "ReliablePacketBuffer::insert(): type is not reliable"
			<< std::endl;
		return;
	}
	const u16 seqnum = p.getSeqnum();

	if (!seqnum_in_window(seqnum, next_expected, MAX_RELIABLE_WINDOW_SIZE)) {
		errorstream << "ReliablePacketBuffer::insert(): seqnum is outside of "
			"expected window " << std::endl;
		return;
	}
	if (seqnum == next_expected) {
		errorstream << "ReliablePacketBuffer::insert(): seqnum is next expected"
			<< std::endl;
		return;
	}

	sanity_check(m_list.size() <= SEQNUM_MAX); // FIXME: Handle the error?

	// Find the right place for the packet and insert it there
	// If list is empty, just add it
	if (m_list.empty()) {
		m_list.push_back(p_ptr);
		m_oldest_non_answered_ack = seqnum;
		// Done.
		return;
	}

	// Otherwise find the right place
	auto it = m_list.begin();
	// Find the first packet in the list which has a higher seqnum
	u16 s = (*it)->getSeqnum();

	/* case seqnum is smaller then next_expected seqnum */
	/* this is true e.g. on wrap around */
	if (seqnum < next_expected) {
		while(((s < seqnum) || (s >= next_expected)) && (it != m_list.end())) {
			++it;
			if (it != m_list.end())
				s = (*it)->getSeqnum();
		}
	}
	/* non wrap around case (at least for incoming and next_expected */
	else
	{
		while(((s < seqnum) && (s >= next_expected)) && (it != m_list.end())) {
			++it;
			if (it != m_list.end())
				s = (*it)->getSeqnum();
		}
	}

	if (s == seqnum) {
		/* nothing to do this seems to be a resent packet */
		/* for paranoia reason data should be compared */
		auto &i = *it;
		if (
			(i->getSeqnum() != seqnum) ||
			(i->size() != p.size()) ||
			(i->address != p.address)
			)
		{
			/* if this happens your maximum transfer window may be to big */
			char buf[200];
			snprintf(buf, sizeof(buf),
					"Duplicated seqnum %d non matching packet detected:\n",
					seqnum);
			warningstream << buf;
			snprintf(buf, sizeof(buf),
					"Old: seqnum: %05d size: %04zu, address: %s\n",
					i->getSeqnum(), i->size(),
					i->address.serializeString().c_str());
			warningstream << buf;
			snprintf(buf, sizeof(buf),
					"New: seqnum: %05d size: %04zu, address: %s\n",
					p.getSeqnum(), p.size(),
					p.address.serializeString().c_str());
			warningstream << buf << std::flush;
			throw IncomingDataCorruption("duplicated packet isn't same as original one");
		}
	}
	/* insert or push back */
	else if (it != m_list.end()) {
		m_list.insert(it, p_ptr);
	} else {
		m_list.push_back(p_ptr);
	}

	/* update last packet number */
	m_oldest_non_answered_ack = m_list.front()->getSeqnum();
}

void ReliablePacketBuffer::fixPeerId(session_t new_id)
{
	MutexAutoLock listlock(m_list_mutex);
	for (auto &packet : m_list)
		packet->setSenderPeerId(new_id);
}

void ReliablePacketBuffer::incrementTimeouts(float dtime)
{
	MutexAutoLock listlock(m_list_mutex);
	for (auto &packet : m_list) {
		packet->time += dtime;
		packet->totaltime += dtime;
	}
}

u32 ReliablePacketBuffer::getTimedOuts(float timeout)
{
	MutexAutoLock listlock(m_list_mutex);
	u32 count = 0;
	for (auto &packet : m_list) {
		if (packet->totaltime >= timeout)
			count++;
	}
	return count;
}

std::vector<ConstSharedPtr<BufferedPacket>>
	ReliablePacketBuffer::getResend(float timeout, u32 max_packets)
{
	MutexAutoLock listlock(m_list_mutex);
	std::vector<ConstSharedPtr<BufferedPacket>> timed_outs;
	for (auto &packet : m_list) {
		// resend time scales exponentially with each cycle
		const float pkt_timeout = timeout * powf(RESEND_SCALE_BASE, packet->resend_count);

		if (packet->time < pkt_timeout)
			continue;

		// caller will resend packet so reset time and increase counter
		packet->time = 0.0f;
		packet->resend_count++;

		timed_outs.emplace_back(packet);

		if (timed_outs.size() >= max_packets)
			break;
	}
	return timed_outs;
}

/*
	IncomingSplitPacket
*/

bool IncomingSplitPacket::insert(u32 chunk_num, SharedBuffer<u8> &chunkdata)
{
	sanity_check(chunk_num < chunk_count);

	// If chunk already exists, ignore it.
	// Sometimes two identical packets may arrive when there is network
	// lag and the server re-sends stuff.
	if (chunks.find(chunk_num) != chunks.end())
		return false;

	// Set chunk data in buffer
	chunks[chunk_num] = chunkdata;

	return true;
}

SharedBuffer<u8> IncomingSplitPacket::reassemble()
{
	sanity_check(allReceived());

	// Calculate total size
	u32 totalsize = 0;
	for (const auto &chunk : chunks)
		totalsize += chunk.second.getSize();

	SharedBuffer<u8> fulldata(totalsize);

	// Copy chunks to data buffer
	u32 start = 0;
	for (u32 chunk_i = 0; chunk_i < chunk_count; chunk_i++) {
		const SharedBuffer<u8> &buf = chunks[chunk_i];
		memcpy(&fulldata[start], *buf, buf.getSize());
		start += buf.getSize();
	}

	return fulldata;
}

/*
	IncomingSplitBuffer
*/

IncomingSplitBuffer::~IncomingSplitBuffer()
{
	MutexAutoLock listlock(m_map_mutex);
	for (auto &i : m_buf) {
		delete i.second;
	}
}

SharedBuffer<u8> IncomingSplitBuffer::insert(BufferedPacketPtr &p_ptr, bool reliable)
{
	MutexAutoLock listlock(m_map_mutex);
	const BufferedPacket &p = *p_ptr;

	u32 headersize = BASE_HEADER_SIZE + 7;
	if (p.size() < headersize) {
		errorstream << "Invalid data size for split packet" << std::endl;
		return SharedBuffer<u8>();
	}
	u8 type = readU8(&p.data[BASE_HEADER_SIZE+0]);
	u16 seqnum = readU16(&p.data[BASE_HEADER_SIZE+1]);
	u16 chunk_count = readU16(&p.data[BASE_HEADER_SIZE+3]);
	u16 chunk_num = readU16(&p.data[BASE_HEADER_SIZE+5]);

	if (type != PACKET_TYPE_SPLIT) {
		errorstream << "IncomingSplitBuffer::insert(): type is not split"
			<< std::endl;
		return SharedBuffer<u8>();
	}
	if (chunk_num >= chunk_count) {
		errorstream << "IncomingSplitBuffer::insert(): chunk_num=" << chunk_num
				<< " >= chunk_count=" << chunk_count << std::endl;
		return SharedBuffer<u8>();
	}

	// Add if doesn't exist
	IncomingSplitPacket *sp;
	if (m_buf.find(seqnum) == m_buf.end()) {
		sp = new IncomingSplitPacket(chunk_count, reliable);
		m_buf[seqnum] = sp;
	} else {
		sp = m_buf[seqnum];
	}

	if (chunk_count != sp->chunk_count) {
		errorstream << "IncomingSplitBuffer::insert(): chunk_count="
				<< chunk_count << " != sp->chunk_count=" << sp->chunk_count
				<< std::endl;
		return SharedBuffer<u8>();
	}
	if (reliable != sp->reliable)
		LOG(derr_con<<"Connection: WARNING: reliable="<<reliable
				<<" != sp->reliable="<<sp->reliable
				<<std::endl);

	// Cut chunk data out of packet
	u32 chunkdatasize = p.size() - headersize;
	SharedBuffer<u8> chunkdata(chunkdatasize);
	memcpy(*chunkdata, &(p.data[headersize]), chunkdatasize);

	if (!sp->insert(chunk_num, chunkdata))
		return SharedBuffer<u8>();

	// If not all chunks are received, return empty buffer
	if (!sp->allReceived())
		return SharedBuffer<u8>();

	SharedBuffer<u8> fulldata = sp->reassemble();

	// Remove sp from buffer
	m_buf.erase(seqnum);
	delete sp;

	return fulldata;
}

void IncomingSplitBuffer::removeUnreliableTimedOuts(float dtime, float timeout)
{
	MutexAutoLock listlock(m_map_mutex);
	std::vector<u16> remove_queue;
	{
		for (const auto &i : m_buf) {
			IncomingSplitPacket *p = i.second;
			// Reliable ones are not removed by timeout
			if (p->reliable)
				continue;
			p->time += dtime;
			if (p->time >= timeout)
				remove_queue.push_back(i.first);
		}
	}
	for (u16 j : remove_queue) {
		LOG(dout_con<<"NOTE: Removing timed out unreliable split packet"<<std::endl);
		auto it = m_buf.find(j);
		delete it->second;
		m_buf.erase(it);
	}
}

/*
	ConnectionCommand
 */

ConnectionCommandPtr ConnectionCommand::create(ConnectionCommandType type)
{
	return ConnectionCommandPtr(new ConnectionCommand(type));
}

ConnectionCommandPtr ConnectionCommand::serve(Address address)
{
	auto c = create(CONNCMD_SERVE);
	c->address = address;
	return c;
}

ConnectionCommandPtr ConnectionCommand::connect(Address address)
{
	auto c = create(CONNCMD_CONNECT);
	c->address = address;
	return c;
}

ConnectionCommandPtr ConnectionCommand::disconnect()
{
	return create(CONNCMD_DISCONNECT);
}

ConnectionCommandPtr ConnectionCommand::disconnect_peer(session_t peer_id)
{
	auto c = create(CONNCMD_DISCONNECT_PEER);
	c->peer_id = peer_id;
	return c;
}

ConnectionCommandPtr ConnectionCommand::resend_one(session_t peer_id)
{
	auto c = create(CONNCMD_RESEND_ONE);
	c->peer_id = peer_id;
	c->channelnum = 0; // must be same as createPeer
	c->reliable = true;
	return c;
}

ConnectionCommandPtr ConnectionCommand::peer_id_set(session_t own_peer_id)
{
	auto c = create(CONNCMD_PEER_ID_SET);
	c->peer_id = own_peer_id;
	return c;
}

ConnectionCommandPtr ConnectionCommand::send(session_t peer_id, u8 channelnum,
	NetworkPacket *pkt, bool reliable)
{
	auto c = create(CONNCMD_SEND);
	c->peer_id = peer_id;
	c->channelnum = channelnum;
	c->reliable = reliable;
	c->data = pkt->oldForgePacket();
	return c;
}

ConnectionCommandPtr ConnectionCommand::ack(session_t peer_id, u8 channelnum, const Buffer<u8> &data)
{
	auto c = create(CONCMD_ACK);
	c->peer_id = peer_id;
	c->channelnum = channelnum;
	c->reliable = false;
	data.copyTo(c->data);
	return c;
}

ConnectionCommandPtr ConnectionCommand::createPeer(session_t peer_id, const Buffer<u8> &data)
{
	auto c = create(CONCMD_CREATE_PEER);
	c->peer_id = peer_id;
	c->channelnum = 0;
	c->reliable = true;
	c->raw = true;
	data.copyTo(c->data);
	return c;
}

/*
	Channel
*/

u16 Channel::readNextIncomingSeqNum()
{
	MutexAutoLock internal(m_internal_mutex);
	return next_incoming_seqnum;
}

u16 Channel::incNextIncomingSeqNum()
{
	MutexAutoLock internal(m_internal_mutex);
	u16 retval = next_incoming_seqnum;
	next_incoming_seqnum++;
	return retval;
}

u16 Channel::readNextSplitSeqNum()
{
	MutexAutoLock internal(m_internal_mutex);
	return next_outgoing_split_seqnum;
}
void Channel::setNextSplitSeqNum(u16 seqnum)
{
	MutexAutoLock internal(m_internal_mutex);
	next_outgoing_split_seqnum = seqnum;
}

u16 Channel::getOutgoingSequenceNumber(bool& successful)
{
	MutexAutoLock internal(m_internal_mutex);

	u16 retval = next_outgoing_seqnum;
	successful = false;

	/* shortcut if there ain't any packet in outgoing list */
	if (outgoing_reliables_sent.empty()) {
		successful = true;
		next_outgoing_seqnum++;
		return retval;
	}

	u16 lowest_unacked_seqnumber;
	if (outgoing_reliables_sent.getFirstSeqnum(lowest_unacked_seqnumber)) {
		if (lowest_unacked_seqnumber < next_outgoing_seqnum) {
			// ugly cast but this one is required in order to tell compiler we
			// know about difference of two unsigned may be negative in general
			// but we already made sure it won't happen in this case
			if (((u16)(next_outgoing_seqnum - lowest_unacked_seqnumber)) > m_window_size) {
				return 0;
			}
		} else {
			// ugly cast but this one is required in order to tell compiler we
			// know about difference of two unsigned may be negative in general
			// but we already made sure it won't happen in this case
			if ((next_outgoing_seqnum + (u16)(SEQNUM_MAX - lowest_unacked_seqnumber)) >
					m_window_size) {
				return 0;
			}
		}
	}

	successful = true;
	next_outgoing_seqnum++;
	return retval;
}

u16 Channel::readOutgoingSequenceNumber()
{
	MutexAutoLock internal(m_internal_mutex);
	return next_outgoing_seqnum;
}

bool Channel::putBackSequenceNumber(u16 seqnum)
{
	if (((seqnum + 1) % (SEQNUM_MAX+1)) == next_outgoing_seqnum) {

		next_outgoing_seqnum = seqnum;
		return true;
	}
	return false;
}

void Channel::UpdateBytesSent(unsigned int bytes, unsigned int packets)
{
	MutexAutoLock internal(m_internal_mutex);
	current_bytes_transfered += bytes;
	current_packet_successful += packets;
}

void Channel::UpdateBytesReceived(unsigned int bytes) {
	MutexAutoLock internal(m_internal_mutex);
	current_bytes_received += bytes;
}

void Channel::UpdateBytesLost(unsigned int bytes)
{
	MutexAutoLock internal(m_internal_mutex);
	current_bytes_lost += bytes;
}


void Channel::UpdatePacketLossCounter(unsigned int count)
{
	MutexAutoLock internal(m_internal_mutex);
	current_packet_loss += count;
}

void Channel::UpdatePacketTooLateCounter()
{
	MutexAutoLock internal(m_internal_mutex);
	current_packet_too_late++;
}

void Channel::UpdateTimers(float dtime)
{
	bpm_counter += dtime;
	packet_loss_counter += dtime;

	if (packet_loss_counter > 1.0f) {
		packet_loss_counter -= 1.0f;

		unsigned int packet_loss;
		unsigned int packets_successful;
		unsigned int packet_too_late;

		bool reasonable_amount_of_data_transmitted = false;

		{
			MutexAutoLock internal(m_internal_mutex);
			packet_loss = current_packet_loss;
			packet_too_late = current_packet_too_late;
			packets_successful = current_packet_successful;

			// has half the window even been used?
			if (current_bytes_transfered > (unsigned int) (m_window_size*512/2)) {
				reasonable_amount_of_data_transmitted = true;
			}
			current_packet_loss = 0;
			current_packet_too_late = 0;
			current_packet_successful = 0;
		}

		// Packets too late means either packet duplication along the way
		// or we were too fast in resending it (which should be self-regulating).
		// Count this a signal of congestion, like packet loss.
		packet_loss = std::min(packet_loss + packet_too_late, packets_successful);

		/* dynamic window size */
		float successful_to_lost_ratio = 0.0f;
		bool done = false;

		if (packets_successful > 0) {
			successful_to_lost_ratio = packet_loss/packets_successful;
		} else if (packet_loss > 0) {
			setWindowSize(m_window_size - 10);
			done = true;
		}

		if (!done) {
			if (successful_to_lost_ratio < 0.01f) {
				/* don't even think about increasing if we didn't even
				 * use major parts of our window */
				if (reasonable_amount_of_data_transmitted)
					setWindowSize(m_window_size + 100);
			} else if (successful_to_lost_ratio < 0.05f) {
				/* don't even think about increasing if we didn't even
				 * use major parts of our window */
				if (reasonable_amount_of_data_transmitted)
					setWindowSize(m_window_size + 50);
			} else if (successful_to_lost_ratio > 0.15f) {
				setWindowSize(m_window_size - 100);
			} else if (successful_to_lost_ratio > 0.1f) {
				setWindowSize(m_window_size - 50);
			}
		}
	}

	if (bpm_counter > 10.0f) {
		{
			MutexAutoLock internal(m_internal_mutex);
			cur_kbps                 =
					(((float) current_bytes_transfered)/bpm_counter)/1024.0f;
			current_bytes_transfered = 0;
			cur_kbps_lost            =
					(((float) current_bytes_lost)/bpm_counter)/1024.0f;
			current_bytes_lost       = 0;
			cur_incoming_kbps        =
					(((float) current_bytes_received)/bpm_counter)/1024.0f;
			current_bytes_received   = 0;
			bpm_counter              = 0.0f;
		}

		if (cur_kbps > max_kbps) {
			max_kbps = cur_kbps;
		}

		if (cur_kbps_lost > max_kbps_lost) {
			max_kbps_lost = cur_kbps_lost;
		}

		if (cur_incoming_kbps > max_incoming_kbps) {
			max_incoming_kbps = cur_incoming_kbps;
		}

		rate_samples       = MYMIN(rate_samples+1,10);
		float old_fraction = ((float) (rate_samples-1) )/( (float) rate_samples);
		avg_kbps           = avg_kbps * old_fraction +
				cur_kbps * (1.0 - old_fraction);
		avg_kbps_lost      = avg_kbps_lost * old_fraction +
				cur_kbps_lost * (1.0 - old_fraction);
		avg_incoming_kbps  = avg_incoming_kbps * old_fraction +
				cur_incoming_kbps * (1.0 - old_fraction);
	}
}


/*
	Peer
*/

PeerHelper::~PeerHelper()
{
	if (m_peer)
		m_peer->DecUseCount();

	m_peer = nullptr;
}

PeerHelper& PeerHelper::operator=(Peer* peer)
{
	if (m_peer)
		m_peer->DecUseCount();
	m_peer = peer;
	if (peer && !peer->IncUseCount())
		m_peer = nullptr;
	return *this;
}

bool Peer::IncUseCount()
{
	MutexAutoLock lock(m_exclusive_access_mutex);

	if (!m_pending_deletion) {
		this->m_usage++;
		return true;
	}

	return false;
}

void Peer::DecUseCount()
{
	{
		MutexAutoLock lock(m_exclusive_access_mutex);
		sanity_check(m_usage > 0);
		m_usage--;

		if (!((m_pending_deletion) && (m_usage == 0)))
			return;
	}
	delete this;
}

void Peer::RTTStatistics(float rtt, const std::string &profiler_id,
		unsigned int num_samples) {

	if (m_last_rtt > 0) {
		/* set min max values */
		if (rtt < m_rtt.min_rtt)
			m_rtt.min_rtt = rtt;
		if (rtt >= m_rtt.max_rtt)
			m_rtt.max_rtt = rtt;

		/* do average calculation */
		if (m_rtt.avg_rtt < 0.0)
			m_rtt.avg_rtt  = rtt;
		else
			m_rtt.avg_rtt  = m_rtt.avg_rtt * (num_samples/(num_samples-1)) +
								rtt * (1/num_samples);

		/* do jitter calculation */

		//just use some neutral value at beginning
		float jitter = m_rtt.jitter_min;

		if (rtt > m_last_rtt)
			jitter = rtt-m_last_rtt;

		if (rtt <= m_last_rtt)
			jitter = m_last_rtt - rtt;

		if (jitter < m_rtt.jitter_min)
			m_rtt.jitter_min = jitter;
		if (jitter >= m_rtt.jitter_max)
			m_rtt.jitter_max = jitter;

		if (m_rtt.jitter_avg < 0.0)
			m_rtt.jitter_avg  = jitter;
		else
			m_rtt.jitter_avg  = m_rtt.jitter_avg * (num_samples/(num_samples-1)) +
								jitter * (1/num_samples);

		if (!profiler_id.empty()) {
			g_profiler->graphAdd(profiler_id + " RTT [ms]", rtt * 1000.f);
			g_profiler->graphAdd(profiler_id + " jitter [ms]", jitter * 1000.f);
		}
	}
	/* save values required for next loop */
	m_last_rtt = rtt;
}

bool Peer::isTimedOut(float timeout, std::string &reason)
{
	MutexAutoLock lock(m_exclusive_access_mutex);

	{
		u64 current_time = porting::getTimeMs();
		float dtime = CALC_DTIME(m_last_timeout_check, current_time);
		m_last_timeout_check = current_time;
		m_timeout_counter += dtime;
	}
	if (m_timeout_counter > timeout) {
		reason = "timeout counter";
		return true;
	}

	return false;
}

void Peer::Drop()
{
	{
		MutexAutoLock usage_lock(m_exclusive_access_mutex);
		m_pending_deletion = true;
		if (m_usage != 0)
			return;
	}

	PROFILE(std::stringstream peerIdentifier1);
	PROFILE(peerIdentifier1 << "runTimeouts[" << m_connection->getDesc()
			<< ";" << id << ";RELIABLE]");
	PROFILE(g_profiler->remove(peerIdentifier1.str()));
	PROFILE(std::stringstream peerIdentifier2);
	PROFILE(peerIdentifier2 << "sendPackets[" << m_connection->getDesc()
			<< ";" << id << ";RELIABLE]");
	PROFILE(ScopeProfiler peerprofiler(g_profiler, peerIdentifier2.str(), SPT_AVG));

	delete this;
}

UDPPeer::UDPPeer(session_t id, const Address &address, Connection *connection) :
	Peer(id, address, connection)
{
	for (Channel &channel : channels)
		channel.setWindowSize(START_RELIABLE_WINDOW_SIZE);
}

bool UDPPeer::isTimedOut(float timeout, std::string &reason)
{
	if (Peer::isTimedOut(timeout, reason))
		return true;

	MutexAutoLock lock(m_exclusive_access_mutex);

	for (int i = 0; i < CHANNEL_COUNT; i++) {
		Channel &channel = channels[i];
		if (channel.outgoing_reliables_sent.getTimedOuts(timeout) > 0) {
			reason = "outgoing reliables channel=" + itos(i);
			return true;
		}
	}

	return false;
}

void UDPPeer::reportRTT(float rtt)
{
	if (rtt < 0)
		return;
	RTTStatistics(rtt,"rudp",MAX_RELIABLE_WINDOW_SIZE*10);

	// use this value to decide the resend timeout
	const float rtt_stat = getStat(AVG_RTT);
	if (rtt_stat < 0)
		return;
	float timeout = rtt_stat * RESEND_TIMEOUT_FACTOR;
	if (timeout < RESEND_TIMEOUT_MIN)
		timeout = RESEND_TIMEOUT_MIN;
	if (timeout > RESEND_TIMEOUT_MAX)
		timeout = RESEND_TIMEOUT_MAX;

	float timeout_old = getResendTimeout();
	setResendTimeout(timeout);

	if (std::abs(timeout - timeout_old) >= 0.001f) {
		dout_con << m_connection->getDesc() << " set resend timeout " << timeout
			<< " (rtt=" << rtt_stat << ") for peer id: " << id << std::endl;
	}
}

bool UDPPeer::Ping(float dtime,SharedBuffer<u8>& data)
{
	m_ping_timer += dtime;
	if (!isHalfOpen() && m_ping_timer >= PING_TIMEOUT)
	{
		// Create and send PING packet
		writeU8(&data[0], PACKET_TYPE_CONTROL);
		writeU8(&data[1], CONTROLTYPE_PING);
		m_ping_timer = 0.0f;
		return true;
	}
	return false;
}

void UDPPeer::PutReliableSendCommand(ConnectionCommandPtr &c,
		unsigned int max_packet_size)
{
	if (m_pending_disconnect)
		return;

	Channel &chan = channels[c->channelnum];

	if (chan.queued_commands.empty() &&
			/* don't queue more packets then window size */
			(chan.queued_reliables.size() + 1 < chan.getWindowSize() / 2)) {
		LOG(dout_con<<m_connection->getDesc()
				<<" processing reliable command for peer id: " << c->peer_id
				<<" data size: " << c->data.getSize() << std::endl);
		if (processReliableSendCommand(c, max_packet_size))
			return;
	} else {
		LOG(dout_con<<m_connection->getDesc()
				<<" Queueing reliable command for peer id: " << c->peer_id
				<<" data size: " << c->data.getSize() <<std::endl);

		if (chan.queued_commands.size() + 1 >= chan.getWindowSize() / 2) {
			LOG(derr_con << m_connection->getDesc()
					<< "Possible packet stall to peer id: " << c->peer_id
					<< " queued_commands=" << chan.queued_commands.size()
					<< std::endl);
		}
	}
	chan.queued_commands.push_back(c);
}

bool UDPPeer::processReliableSendCommand(
				ConnectionCommandPtr &c_ptr,
				unsigned int max_packet_size)
{
	if (m_pending_disconnect)
		return true;

	const auto &c = *c_ptr;
	Channel &chan = channels[c.channelnum];

	const u32 chunksize_max = max_packet_size
							- BASE_HEADER_SIZE
							- RELIABLE_HEADER_SIZE;

	std::list<SharedBuffer<u8>> originals;

	if (c.raw) {
		originals.emplace_back(c.data);
	} else {
		u16 split_seqnum = chan.readNextSplitSeqNum();
		makeAutoSplitPacket(c.data, chunksize_max, split_seqnum, &originals);
		chan.setNextSplitSeqNum(split_seqnum);
	}

	sanity_check(originals.size() < MAX_RELIABLE_WINDOW_SIZE);

	bool have_sequence_number = false;
	bool have_initial_sequence_number = false;
	std::queue<BufferedPacketPtr> toadd;
	u16 initial_sequence_number = 0;

	for (SharedBuffer<u8> &original : originals) {
		u16 seqnum = chan.getOutgoingSequenceNumber(have_sequence_number);

		/* oops, we don't have enough sequence numbers to send this packet */
		if (!have_sequence_number)
			break;

		if (!have_initial_sequence_number)
		{
			initial_sequence_number = seqnum;
			have_initial_sequence_number = true;
		}

		SharedBuffer<u8> reliable = makeReliablePacket(original, seqnum);

		// Add base headers and make a packet
		BufferedPacketPtr p = con::makePacket(address, reliable,
				m_connection->GetProtocolID(), m_connection->GetPeerID(),
				c.channelnum);

		toadd.push(p);
	}

	if (have_sequence_number) {
		while (!toadd.empty()) {
			BufferedPacketPtr p = toadd.front();
			toadd.pop();
//			LOG(dout_con<<connection->getDesc()
//					<< " queuing reliable packet for peer_id: " << c.peer_id
//					<< " channel: " << (c.channelnum&0xFF)
//					<< " seqnum: " << readU16(&p.data[BASE_HEADER_SIZE+1])
//					<< std::endl)
			chan.queued_reliables.push(p);
		}
		sanity_check(chan.queued_reliables.size() < 0xFFFF);
		return true;
	}

	u16 packets_available = toadd.size();
	/* we didn't get a single sequence number no need to fill queue */
	if (!have_initial_sequence_number) {
		dout_con << m_connection->getDesc() << " No sequence numbers available!" << std::endl;
		return false;
	}

	while (!toadd.empty()) {
		/* remove packet */
		toadd.pop();

		bool successfully_put_back_sequence_number
			= chan.putBackSequenceNumber(
				(initial_sequence_number+toadd.size() % (SEQNUM_MAX+1)));

		FATAL_ERROR_IF(!successfully_put_back_sequence_number, "error");
	}

	u32 n_queued = chan.outgoing_reliables_sent.size();

	LOG(dout_con<<m_connection->getDesc()
			<< " Windowsize exceeded on reliable sending "
			<< c.data.getSize() << " bytes"
			<< std::endl << "\t\tinitial_sequence_number: "
			<< initial_sequence_number
			<< std::endl << "\t\tgot at most            : "
			<< packets_available << " packets"
			<< std::endl << "\t\tpackets queued         : "
			<< n_queued
			<< std::endl);

	return false;
}

void UDPPeer::RunCommandQueues(
							unsigned int max_packet_size,
							unsigned int maxtransfer)
{

	for (Channel &channel : channels) {

		if ((!channel.queued_commands.empty()) &&
				(channel.queued_reliables.size() < maxtransfer)) {
			try {
				ConnectionCommandPtr c = channel.queued_commands.front();

				LOG(dout_con << m_connection->getDesc()
						<< " processing queued reliable command " << std::endl);

				// Packet is processed, remove it from queue
				if (processReliableSendCommand(c, max_packet_size)) {
					channel.queued_commands.pop_front();
				} else {
					LOG(dout_con << m_connection->getDesc()
							<< " Failed to queue packets for peer_id: " << c->peer_id
							<< ", delaying sending of " << c->data.getSize()
							<< " bytes" << std::endl);
				}
			}
			catch (ItemNotFoundException &e) {
				// intentionally empty
			}
		}
	}
}

u16 UDPPeer::getNextSplitSequenceNumber(u8 channel)
{
	assert(channel < CHANNEL_COUNT); // Pre-condition
	return channels[channel].readNextSplitSeqNum();
}

void UDPPeer::setNextSplitSequenceNumber(u8 channel, u16 seqnum)
{
	assert(channel < CHANNEL_COUNT); // Pre-condition
	channels[channel].setNextSplitSeqNum(seqnum);
}

SharedBuffer<u8> UDPPeer::addSplitPacket(u8 channel, BufferedPacketPtr &toadd,
	bool reliable)
{
	assert(channel < CHANNEL_COUNT); // Pre-condition
	return channels[channel].incoming_splits.insert(toadd, reliable);
}

/*
	ConnectionEvent
*/

const char *ConnectionEvent::describe() const
{
	switch(type) {
	case CONNEVENT_NONE:
		return "CONNEVENT_NONE";
	case CONNEVENT_DATA_RECEIVED:
		return "CONNEVENT_DATA_RECEIVED";
	case CONNEVENT_PEER_ADDED:
		return "CONNEVENT_PEER_ADDED";
	case CONNEVENT_PEER_REMOVED:
		return "CONNEVENT_PEER_REMOVED";
	case CONNEVENT_BIND_FAILED:
		return "CONNEVENT_BIND_FAILED";
	}
	return "Invalid ConnectionEvent";
}


ConnectionEventPtr ConnectionEvent::create(ConnectionEventType type)
{
	return std::shared_ptr<ConnectionEvent>(new ConnectionEvent(type));
}

ConnectionEventPtr ConnectionEvent::dataReceived(session_t peer_id, const Buffer<u8> &data)
{
	auto e = create(CONNEVENT_DATA_RECEIVED);
	e->peer_id = peer_id;
	data.copyTo(e->data);
	return e;
}

ConnectionEventPtr ConnectionEvent::peerAdded(session_t peer_id, Address address)
{
	auto e = create(CONNEVENT_PEER_ADDED);
	e->peer_id = peer_id;
	e->address = address;
	return e;
}

ConnectionEventPtr ConnectionEvent::peerRemoved(session_t peer_id, bool is_timeout, Address address)
{
	auto e = create(CONNEVENT_PEER_REMOVED);
	e->peer_id = peer_id;
	e->timeout = is_timeout;
	e->address = address;
	return e;
}

ConnectionEventPtr ConnectionEvent::bindFailed()
{
	return create(CONNEVENT_BIND_FAILED);
}

/*
	Connection
*/

Connection::Connection(u32 max_packet_size, float timeout,
		bool ipv6, PeerHandler *peerhandler) :
	m_udpSocket(ipv6),
	m_protocol_id(PROTOCOL_ID),
	m_sendThread(new ConnectionSendThread(max_packet_size, timeout)),
	m_receiveThread(new ConnectionReceiveThread()),
	m_bc_peerhandler(peerhandler)

{
	/* Amount of time Receive() will wait for data, this is entirely different
	 * from the connection timeout */
	m_udpSocket.setTimeoutMs(500);

	m_sendThread->setParent(this);
	m_receiveThread->setParent(this);

	m_sendThread->start();
	m_receiveThread->start();
}


Connection::~Connection()
{
	m_shutting_down = true;
	// request threads to stop
	m_sendThread->stop();
	m_receiveThread->stop();

	// wait for threads to finish
	m_sendThread->wait();
	m_receiveThread->wait();

	// Delete peers
	for (auto &peer : m_peers) {
		delete peer.second;
	}
}

/* Internal stuff */

void Connection::putEvent(ConnectionEventPtr e)
{
	assert(e->type != CONNEVENT_NONE); // Pre-condition
	m_event_queue.push_back(e);
}

void Connection::TriggerSend()
{
	m_sendThread->Trigger();
}

PeerHelper Connection::getPeerNoEx(session_t peer_id)
{
	MutexAutoLock peerlock(m_peers_mutex);
	std::map<session_t, Peer *>::iterator node = m_peers.find(peer_id);

	if (node == m_peers.end()) {
		return PeerHelper(NULL);
	}

	// Error checking
	FATAL_ERROR_IF(node->second->id != peer_id, "Invalid peer id");

	return PeerHelper(node->second);
}

/* find peer_id for address */
session_t Connection::lookupPeer(const Address& sender)
{
	MutexAutoLock peerlock(m_peers_mutex);
	for (auto &it: m_peers) {
		Peer *peer = it.second;
		if (peer->isPendingDeletion())
			continue;

		if (peer->getAddress() == sender)
			return peer->id;
	}

	return PEER_ID_INEXISTENT;
}

u32 Connection::getActiveCount()
{
	MutexAutoLock peerlock(m_peers_mutex);
	u32 count = 0;
	for (auto &it : m_peers) {
		Peer *peer = it.second;
		if (peer->isPendingDeletion())
			continue;
		if (peer->isHalfOpen())
			continue;
		count++;
	}
	return count;
}

bool Connection::deletePeer(session_t peer_id, bool timeout)
{
	Peer *peer = 0;

	/* lock list as short as possible */
	{
		MutexAutoLock peerlock(m_peers_mutex);
		if (m_peers.find(peer_id) == m_peers.end())
			return false;
		peer = m_peers[peer_id];
		m_peers.erase(peer_id);
		auto it = std::find(m_peer_ids.begin(), m_peer_ids.end(), peer_id);
		m_peer_ids.erase(it);
	}

	// Create event
	putEvent(ConnectionEvent::peerRemoved(peer_id, timeout, peer->getAddress()));

	peer->Drop();
	return true;
}

/* Interface */

ConnectionEventPtr Connection::waitEvent(u32 timeout_ms)
{
	try {
		return m_event_queue.pop_front(timeout_ms);
	} catch(ItemNotFoundException &ex) {
		return ConnectionEvent::create(CONNEVENT_NONE);
	}
}

void Connection::putCommand(ConnectionCommandPtr c)
{
	if (!m_shutting_down) {
		m_command_queue.push_back(c);
		m_sendThread->Trigger();
	}
}

void Connection::Serve(Address bind_addr)
{
	putCommand(ConnectionCommand::serve(bind_addr));
}

void Connection::Connect(Address address)
{
	putCommand(ConnectionCommand::connect(address));
}

bool Connection::Connected()
{
	MutexAutoLock peerlock(m_peers_mutex);

	if (m_peers.size() != 1)
		return false;

	std::map<session_t, Peer *>::iterator node = m_peers.find(PEER_ID_SERVER);
	if (node == m_peers.end())
		return false;

	if (m_peer_id == PEER_ID_INEXISTENT)
		return false;

	return true;
}

void Connection::Disconnect()
{
	putCommand(ConnectionCommand::disconnect());
}

bool Connection::ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms)
{
	/*
		Note that this function can potentially wait infinitely if non-data
		events keep happening before the timeout expires.
		This is not considered to be a problem (is it?)
	*/
	for(;;) {
		ConnectionEventPtr e_ptr = waitEvent(timeout_ms);
		const ConnectionEvent &e = *e_ptr;

		if (e.type != CONNEVENT_NONE) {
			LOG(dout_con << getDesc() << ": Receive: got event: "
					<< e.describe() << std::endl);
		}

		switch (e.type) {
		case CONNEVENT_NONE:
			return false;
		case CONNEVENT_DATA_RECEIVED:
			// Data size is lesser than command size, ignoring packet
			if (e.data.getSize() < 2) {
				continue;
			}

			pkt->putRawPacket(*e.data, e.data.getSize(), e.peer_id);
			return true;
		case CONNEVENT_PEER_ADDED: {
			UDPPeer tmp(e.peer_id, e.address, this);
			if (m_bc_peerhandler)
				m_bc_peerhandler->peerAdded(&tmp);
			continue;
		}
		case CONNEVENT_PEER_REMOVED: {
			UDPPeer tmp(e.peer_id, e.address, this);
			if (m_bc_peerhandler)
				m_bc_peerhandler->deletingPeer(&tmp, e.timeout);
			continue;
		}
		case CONNEVENT_BIND_FAILED:
			throw ConnectionBindFailed("Failed to bind socket "
					"(port already in use?)");
		}
	}
	return false;
}

void Connection::Send(session_t peer_id, u8 channelnum,
		NetworkPacket *pkt, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	// approximate check similar to UDPPeer::processReliableSendCommand()
	// to get nicer errors / backtraces if this happens.
	if (reliable && pkt->getSize() > MAX_RELIABLE_WINDOW_SIZE*512) {
		std::ostringstream oss;
		oss << "Packet too big for window, peer_id=" << peer_id
			<< " command=" << pkt->getCommand() << " size=" << pkt->getSize();
		FATAL_ERROR(oss.str().c_str());
	}

	putCommand(ConnectionCommand::send(peer_id, channelnum, pkt, reliable));
}

Address Connection::GetPeerAddress(session_t peer_id)
{
	PeerHelper peer = getPeerNoEx(peer_id);

	if (!peer)
		throw PeerNotFoundException("No address for peer found!");
	return peer->getAddress();
}

float Connection::getPeerStat(session_t peer_id, rtt_stat_type type)
{
	PeerHelper peer = getPeerNoEx(peer_id);
	if (!peer)
		return -1;
	return peer->getStat(type);
}

float Connection::getLocalStat(rate_stat_type type)
{
	PeerHelper peer = getPeerNoEx(PEER_ID_SERVER);

	FATAL_ERROR_IF(!peer, "Connection::getLocalStat we couldn't get our own peer? are you serious???");

	float retval = 0;

	for (Channel &channel : dynamic_cast<UDPPeer *>(&peer)->channels) {
		switch(type) {
			case CUR_DL_RATE:
				retval += channel.getCurrentDownloadRateKB();
				break;
			case AVG_DL_RATE:
				retval += channel.getAvgDownloadRateKB();
				break;
			case CUR_INC_RATE:
				retval += channel.getCurrentIncomingRateKB();
				break;
			case AVG_INC_RATE:
				retval += channel.getAvgIncomingRateKB();
				break;
			case AVG_LOSS_RATE:
				retval += channel.getAvgLossRateKB();
				break;
			case CUR_LOSS_RATE:
				retval += channel.getCurrentLossRateKB();
				break;
		default:
			FATAL_ERROR("Connection::getLocalStat Invalid stat type");
		}
	}
	return retval;
}

session_t Connection::createPeer(const Address &sender, int fd)
{
	// Somebody wants to make a new connection

	// Get a unique peer id
	const session_t minimum = 2;
	const session_t overflow = MAX_UDP_PEERS;

	/*
		Find an unused peer id
	*/

	MutexAutoLock lock(m_peers_mutex);
	session_t peer_id_new;
	for (int tries = 0; tries < 100; tries++) {
		peer_id_new = myrand_range(minimum, overflow - 1);
		if (m_peers.find(peer_id_new) == m_peers.end())
			break;
	}
	if (m_peers.find(peer_id_new) != m_peers.end()) {
		errorstream << getDesc() << " ran out of peer ids" << std::endl;
		return PEER_ID_INEXISTENT;
	}

	// Create a peer
	Peer *peer = 0;
	peer = new UDPPeer(peer_id_new, sender, this);

	m_peers[peer->id] = peer;
	m_peer_ids.push_back(peer->id);

	LOG(dout_con << getDesc()
			<< "createPeer(): giving peer_id=" << peer_id_new << std::endl);

	{
		Buffer<u8> reply(4);
		writeU8(&reply[0], PACKET_TYPE_CONTROL);
		writeU8(&reply[1], CONTROLTYPE_SET_PEER_ID);
		writeU16(&reply[2], peer_id_new);
		putCommand(ConnectionCommand::createPeer(peer_id_new, reply));
	}

	// Create peer addition event
	putEvent(ConnectionEvent::peerAdded(peer_id_new, sender));

	// We're now talking to a valid peer_id
	return peer_id_new;
}

const std::string Connection::getDesc()
{
	MutexAutoLock _(m_info_mutex);
	return std::string("con(")+
			itos(m_udpSocket.GetHandle())+"/"+itos(m_peer_id)+")";
}

void Connection::DisconnectPeer(session_t peer_id)
{
	putCommand(ConnectionCommand::disconnect_peer(peer_id));
}

void Connection::SetPeerID(session_t id)
{
	m_peer_id = id;
	// fix peer id in existing queued reliable packets
	if (id != PEER_ID_INEXISTENT)
		putCommand(ConnectionCommand::peer_id_set(id));
}

void Connection::doResendOne(session_t peer_id)
{
	assert(peer_id != PEER_ID_INEXISTENT);
	putCommand(ConnectionCommand::resend_one(peer_id));
}

void Connection::sendAck(session_t peer_id, u8 channelnum, u16 seqnum)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	LOG(dout_con<<getDesc()
			<<" Queuing ACK command to peer_id: " << peer_id <<
			" channel: " << (channelnum & 0xFF) <<
			" seqnum: " << seqnum << std::endl);

	SharedBuffer<u8> ack(4);
	writeU8(&ack[0], PACKET_TYPE_CONTROL);
	writeU8(&ack[1], CONTROLTYPE_ACK);
	writeU16(&ack[2], seqnum);

	putCommand(ConnectionCommand::ack(peer_id, channelnum, ack));
	m_sendThread->Trigger();
}

UDPPeer* Connection::createServerPeer(const Address &address)
{
	if (ConnectedToServer())
		throw ConnectionException("Already connected to a server");

	UDPPeer *peer = new UDPPeer(PEER_ID_SERVER, address, this);
	peer->SetFullyOpen();

	{
		MutexAutoLock lock(m_peers_mutex);
		m_peers[peer->id] = peer;
		m_peer_ids.push_back(peer->id);
	}

	return peer;
}

} // namespace
