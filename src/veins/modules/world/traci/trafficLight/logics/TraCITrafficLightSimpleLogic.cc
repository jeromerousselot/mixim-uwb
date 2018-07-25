//
// Copyright (C) 2015-2018 Dominik Buse <dbuse@mail.uni-paderborn.de>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <veins/modules/world/traci/trafficLight/logics/TraCITrafficLightSimpleLogic.h>

using Veins::TraCITrafficLightSimpleLogic;

Define_Module(Veins::TraCITrafficLightSimpleLogic);

void TraCITrafficLightSimpleLogic::handleApplMsg(cMessage *msg) {
	delete msg; // just drop it
}


void TraCITrafficLightSimpleLogic::handleTlIfMsg(TraCITrafficLightMessage *tlMsg) {
	delete tlMsg; // just drop it
}

void TraCITrafficLightSimpleLogic::handlePossibleSwitch() {
	// do nothing - just let it happen
}
