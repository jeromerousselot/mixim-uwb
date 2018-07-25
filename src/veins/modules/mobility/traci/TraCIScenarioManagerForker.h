//
// Copyright (C) 2006-2016 Christoph Sommer <christoph.sommer@uibk.ac.at>
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

#ifndef WORLD_TRACI_TRACISCENARIOMANAGERFORKER_H
#define WORLD_TRACI_TRACISCENARIOMANAGERFORKER_H

#include <omnetpp.h>

#include "veins/modules/mobility/traci/TraCIScenarioManager.h"
#include "veins/modules/mobility/traci/TraCILauncher.h"

/**
 * @brief
 *
 * Extends the TraCIScenarioManager to automatically fork an instance of SUMO when needed.
 *
 * All other functionality is provided by the TraCIScenarioManager.
 *
 * See the Veins website <a href="http://veins.car2x.org/"> for a tutorial, documentation, and publications </a>.
 *
 * @author Christoph Sommer, Florian Hagenauer
 *
 * @see TraCIMobility
 * @see TraCIScenarioManager
 *
 */
namespace Veins {
class TraCIScenarioManagerForker : public TraCIScenarioManager
{
	public:

		TraCIScenarioManagerForker();
		virtual ~TraCIScenarioManagerForker();
		virtual void initialize(int stage);
		virtual void finish();

	protected:

		std::string commandLine; /**< command line for running TraCI server (substituting $configFile, $seed, $port) */
		std::string configFile; /**< substitution for $configFile parameter */
		int seed; /**< substitution for $seed parameter (-1: current run number) */

		TraCILauncher* server;

		virtual void startServer();
		virtual void killServer();
};
}

namespace Veins {
class TraCIScenarioManagerForkerAccess
{
	public:
		TraCIScenarioManagerForker* get() {
			return FindModule<TraCIScenarioManagerForker*>::findGlobalModule();
		};
};
}

#endif
