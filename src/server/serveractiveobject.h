// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <cassert>
#include <unordered_set>
#include <optional>
#include "irrlichttypes_bloated.h"
#include "activeobject.h"
#include "itemgroup.h"
#include "util/container.h"


/*

Some planning
-------------

* Server environment adds an active object, which gets the id 1
* The active object list is scanned for each client once in a while,
  and it finds out what objects have been added that are not known
  by the client yet. This scan is initiated by the Server class and
  the result ends up directly to the server.
* A network packet is created with the info and sent to the client.
* Environment converts objects to static data and static data to
  objects, based on how close players are to them.

*/

class ServerEnvironment;
struct ItemStack;
struct ToolCapabilities;
struct ObjectProperties;
struct PlayerHPChangeReason;
class Inventory;
struct InventoryLocation;

class ServerActiveObject : public ActiveObject
{
public:
	/*
		NOTE: m_env can be NULL, but step() isn't called if it is.
		Prototypes are used that way.
	*/
	ServerActiveObject(ServerEnvironment *env, v3f pos);
	virtual ~ServerActiveObject() = default;

	virtual ActiveObjectType getSendType() const
	{ return getType(); }

	// Called after id has been set and has been inserted in environment
	virtual void addedToEnvironment(u32 dtime_s){};
	// Called before removing from environment
	virtual void removingFromEnvironment(){};

	// Safely mark the object for removal or deactivation
	void markForRemoval();
	void markForDeactivation();

	/*
		Some simple getters/setters
	*/
	v3f getBasePosition() const { return m_base_position; }
	void setBasePosition(v3f pos);
	ServerEnvironment* getEnv(){ return m_env; }

	/*
		Some more dynamic interface
	*/

	virtual void setPos(const v3f &pos)
		{ setBasePosition(pos); }
	virtual void addPos(const v3f &added_pos)
		{ setBasePosition(m_base_position + added_pos); }
	// continuous: if true, object does not stop immediately at pos
	virtual void moveTo(v3f pos, bool continuous)
		{ setBasePosition(pos); }
	// If object has moved less than this and data has not changed,
	// saving to disk may be omitted
	virtual float getMinimumSavedMovement();

	virtual std::string getDescription(){return "SAO";}

	/*
		Step object in time.
		Messages added to messages are sent to client over network.

		send_recommended:
			True at around 5-10 times a second, same for all objects.
			This is used to let objects send most of the data at the
			same time so that the data can be combined in a single
			packet.
	*/
	virtual void step(float dtime, bool send_recommended){}

	/*
		The return value of this is passed to the client-side object
		when it is created
	*/
	virtual std::string getClientInitializationData(u16 protocol_version) {return "";}

	/*
		The return value of this is passed to the server-side object
		when it is created (converted from static to active - actually
		the data is the static form)
	*/
	virtual void getStaticData(std::string *result) const
	{
		assert(isStaticAllowed());
		*result = "";
	}

	/*
		Return false in here to never save and instead remove object
		on unload. getStaticData() will not be called in that case.
	*/
	virtual bool isStaticAllowed() const
	{return true;}

	/*
		Return false here to never unload the object.
		isStaticAllowed && shouldUnload -> unload when out of active block range
		!isStaticAllowed && shouldUnload -> unload when block is unloaded
	*/
	virtual bool shouldUnload() const
	{ return true; }

	// Returns added tool wear
	virtual u32 punch(v3f dir,
			const ToolCapabilities *toolcap = nullptr,
			ServerActiveObject *puncher = nullptr,
			float time_from_last_punch = 1000000.0f,
			u16 initial_wear = 0)
	{ return 0; }
	virtual void rightClick(ServerActiveObject *clicker)
	{}
	virtual void setHP(s32 hp, const PlayerHPChangeReason &reason)
	{}
	virtual u16 getHP() const
	{ return 0; }

	/// @brief Returns an unique ID for this object (persistent across unload, server restarts).
	/// @note Because these strings are very short, copying them is not expensive.
	virtual std::string getGUID() const = 0;

	virtual void setArmorGroups(const ItemGroupList &armor_groups)
	{}
	virtual const ItemGroupList &getArmorGroups() const
	{ static ItemGroupList rv; return rv; }
	virtual void setAnimation(v2f frames, float frame_speed, float frame_blend, bool frame_loop)
	{}
	virtual void getAnimation(v2f *frames, float *frame_speed, float *frame_blend, bool *frame_loop)
	{}
	virtual void setAnimationSpeed(float frame_speed)
	{}
	virtual void setBoneOverride(const std::string &bone, const BoneOverride &props)
	{}
	virtual BoneOverride getBoneOverride(const std::string &bone)
	{ BoneOverride props; return props; }
	virtual const BoneOverrideMap &getBoneOverrides() const
	{ static BoneOverrideMap rv; return rv; }
	virtual const std::unordered_set<object_t> &getAttachmentChildIds() const
	{ static std::unordered_set<object_t> rv; return rv; }
	virtual ServerActiveObject *getParent() const { return nullptr; }
	virtual ObjectProperties *accessObjectProperties()
	{ return NULL; }
	virtual void notifyObjectPropertiesModified()
	{}

	// Inventory and wielded item
	virtual Inventory *getInventory() const
	{ return NULL; }
	virtual InventoryLocation getInventoryLocation() const;
	virtual void setInventoryModified()
	{}
	virtual std::string getWieldList() const
	{ return ""; }
	virtual u16 getWieldIndex() const
	{ return 0; }
	virtual ItemStack getWieldedItem(ItemStack *selected,
			ItemStack *hand = nullptr) const;
	virtual bool setWieldedItem(const ItemStack &item);
	inline void attachParticleSpawner(u32 id)
	{
		m_attached_particle_spawners.insert(id);
	}
	inline void detachParticleSpawner(u32 id)
	{
		m_attached_particle_spawners.erase(id);
	}

	std::string generateUpdateInfantCommand(u16 infant_id, u16 protocol_version);

	void dumpAOMessagesToQueue(std::queue<ActiveObjectMessage> &queue);

	/*
		Number of players which know about this object. Object won't be
		deleted until this is 0 to keep the id preserved for the right
		object.
	*/
	u16 m_known_by_count = 0;

	/*
		A getter that unifies the above to answer the question:
		"Can the environment still interact with this object?"
	*/
	inline bool isGone() const
	{ return m_pending_removal || m_pending_deactivation; }

	inline bool isPendingRemoval() const
	{ return m_pending_removal; }

	/*
		Whether the object's static data has been stored to a block

		Note that `!isStaticAllowed() && m_static_exists` is a valid state
		(though it usually doesn't persist long) and you need to be careful
		about handling it.
	*/
	bool m_static_exists = false;
	/*
		The block from which the object was loaded from, and in which
		a copy of the static data resides.
	*/
	v3s16 m_static_block = v3s16(1337,1337,1337);

	// Names of players to whom the object is to be sent, not considering parents.
	using Observers = std::optional<std::unordered_set<std::string>>;
	Observers m_observers;

	/// Invalidate final observer cache. This needs to be done whenever
	/// the observers of this object or any of its ancestors may have changed.
	void invalidateEffectiveObservers();
	/// Cache `m_effective_observers` with the names of all observers,
	/// also indirect observers (object attachment chain).
	const Observers &getEffectiveObservers();
	/// Force a recalculation of final observers (including all parents).
	const Observers &recalculateEffectiveObservers();
	/// Whether the object is sent to `player_name`
	bool isEffectivelyObservedBy(const std::string &player_name);

protected:
	// Cached intersection of m_observers of this object and all its parents.
	std::optional<Observers> m_effective_observers;

	virtual void onMarkedForDeactivation() {}
	virtual void onMarkedForRemoval() {}

	ServerEnvironment *m_env;
	std::unordered_set<u32> m_attached_particle_spawners;

	/*
		Same purpose as m_pending_removal but for deactivation.
		deactvation = save static data in block, remove active object

		If this is set alongside with m_pending_removal, removal takes
		priority.
		Note: Do not assign this directly, use markForDeactivation() instead.
	*/
	bool m_pending_deactivation = false;

	/*
		- Whether this object is to be removed when nobody knows about
		  it anymore.
		- Removal is delayed to preserve the id for the time during which
		  it could be confused to some other object by some client.
		- This is usually set to true by the step() method when the object wants
		  to be deleted but can be set by anything else too.
		Note: Do not assign this directly, use markForRemoval() instead.
	*/
	bool m_pending_removal = false;

	/*
		Queue of messages to be sent to the client
	*/
	std::queue<ActiveObjectMessage> m_messages_out;

private:
	v3f m_base_position; // setBasePosition updates index and MUST be called
};
