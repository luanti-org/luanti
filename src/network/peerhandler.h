/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

namespace con
{
class Peer;

class PeerHandler
{
public:

	PeerHandler() = default;

	virtual ~PeerHandler() = default;

	/*
		This is called after the Peer has been inserted into the
		Connection's peer container.
	*/
	virtual void peerAdded(Peer *peer) = 0;

	/*
		This is called before the Peer has been removed from the
		Connection's peer container.
	*/
	virtual void deletingPeer(Peer *peer, bool timeout) = 0;
};

enum PeerChangeType
{
	PEER_ADDED,
	PEER_REMOVED
};
struct PeerChange
{
	PeerChange(PeerChangeType t, u16 _peer_id, bool _timeout):
		type(t), peer_id(_peer_id), timeout(_timeout) {}
	PeerChange() = delete;

	PeerChangeType type;
	u16 peer_id;
	bool timeout;
};

}
