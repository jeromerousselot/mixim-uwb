//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//


package org.mixim.modules.mobility;

import org.mixim.base.modules.BaseMobility;
//
// Uses the <position_change> elements of the ANSim tool's
// trace file.
//
// NOTE: Does not yet support 3-dimensional movement.
// @author Andras Varga
//
simple ANSimMobility extends BaseMobility
{
    parameters:
        @class(ANSimMobility);
        bool debug; // debug switch
        xml ansimTrace; // the ANSim trace file in XML
        double nodeId; // <position_change> elements to match; 
                               // -1 gets substituted to parent module's index
        double updateInterval @unit(s); // time interval to update the hosts position (seconds)
}



