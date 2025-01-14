// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CBoneSceneNode.h"

#include <optional>

namespace irr
{
namespace scene
{

void CBoneSceneNode::OnAnimate(u32 timeMs)
{
	if (IsVisible) {
		// update absolute position
		// updateAbsolutePosition();

		// perform the post render process on all children
		ISceneNodeList::iterator it = Children.begin();
		for (; it != Children.end(); ++it)
			(*it)->OnAnimate(timeMs);
	}
}

void CBoneSceneNode::helper_updateAbsolutePositionOfAllChildren(ISceneNode *Node)
{
	Node->updateAbsolutePosition();

	ISceneNodeList::const_iterator it = Node->getChildren().begin();
	for (; it != Node->getChildren().end(); ++it) {
		helper_updateAbsolutePositionOfAllChildren((*it));
	}
}

void CBoneSceneNode::updateAbsolutePositionOfAllChildren()
{
	helper_updateAbsolutePositionOfAllChildren(this);
}

} // namespace scene
} // namespace irr
