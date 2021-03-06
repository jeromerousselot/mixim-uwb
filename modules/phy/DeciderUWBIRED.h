/* -*- mode:c++ -*- ********************************************************
 * file:        DeciderUWBIRED.h
 *
 * author:      Jerome Rousselot <jerome.rousselot@csem.ch>
 *
 * copyright:   (C) 2008-2009 Centre Suisse d'Electronique et Microtechnique (CSEM) SA
 * 				Systems Engineering
 *              Real-Time Software and Networking
 *              Jaquet-Droz 1, CH-2002 Neuchatel, Switzerland.
 *
 *              This program is free software; you can redistribute it
 *              and/or modify it under the terms of the GNU General Public
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later
 *              version.
 *              For further information see file COPYING
 *              in the top level directory
 * description: this Decider models an energy-detection receiver with soft-decision
 * acknowledgment: this work was supported (in part) by the National Competence
 * 			    Center in Research on Mobile Information and Communication Systems
 * 				NCCR-MICS, a center supported by the Swiss National Science
 * 				Foundation under grant number 5005-67322.
 ***************************************************************************/

//
// This class implements a model of an energy detection receiver
// that demodulates UWB-IR burst position modulation as defined
// in the IEEE802154A standard (mandatory mode, high PRF).
// The code modeling the frame clock synchronization is implemented
// in attemptSync(). Simply subclass this class and redefine attemptSync()
// if you wish to consider more sophisticated synchronization models.
// To implement a coherent receiver, the easiest way to start is to copy-paste
// this code into a new class and rename it accordingly. Then, redefine
// decodePacket().

#ifndef _UWBIRENERGYDETECTIONDECIDERV2_H
#define	_UWBIRENERGYDETECTIONDECIDERV2_H

#include <vector>
#include <map>
#include <math.h>

#include "Signal_.h"
#include "Mapping.h"
#include "AirFrame_m.h"
#include "Decider.h"
#include "DeciderResultUWBIR.h"
#include "AlohaMacLayer.h"
#include "IEEE802154A.h"
#include "UWBIRPacket.h"
#include "BaseUtility.h"
//#include "RadioUWBIR.h"
//#include "PhyUtils.h"

using namespace std;

class PhyLayerUWBIR;


class DeciderUWBIRED: public Decider {
private:
	bool trace, stats;
	long nbRandomBits;
	long nbFailedSyncs, nbSuccessfulSyncs;
	double nbSymbols, allThresholds;
	double vsignal2, vnoise2, snirs, snirEvals, pulseSnrs;
	double packetSNIR, packetSignal, packetNoise, packetSamples;

protected:
	double syncThreshold;
	bool syncAlwaysSucceeds;
	UWBIRPacket packet;
	int catUWBIRPacket;
	BaseUtility* utility;
	double epulseAggregate, enoiseAggregate;

public:
	const static double noiseVariance = 101.085E-12; // P=-116.9 dBW // 404.34E-12;   v²=s²=4kb T R B (T=293 K)
	const static double peakPulsePower = 1.3E-3; //1.3E-3 W peak power of pulse to reach  0dBm during burst; // peak instantaneous power of the transmitted pulse (A=0.6V) : 7E-3 W. But peak limit is 0 dBm

	DeciderUWBIRED(DeciderToPhyInterface* iface,
			PhyLayerUWBIR* _uwbiface,
			double _syncThreshold, bool _syncAlwaysSucceeds, bool _stats,
			bool _trace, bool alwaysFailOnDataInterference=false) :
		Decider(iface), trace(_trace),
				stats(_stats), nbRandomBits(0), nbFailedSyncs(0),
				nbSuccessfulSyncs(0), nbSymbols(0), syncThreshold(_syncThreshold),
				syncAlwaysSucceeds(_syncAlwaysSucceeds), uwbiface(_uwbiface), tracking(0),
				channelSensing(false), synced(false), vsignal2(0), vnoise2(0), snirEvals(0), pulseSnrs(0),
				alwaysFailOnDataInterference(alwaysFailOnDataInterference) {

		receivedPulses.setName("receivedPulses");
		syncThresholds.setName("syncThresholds");
		utility = iface->getUtility();
		catUWBIRPacket = utility->getCategory(&packet);
	};

	virtual simtime_t processSignal(AirFrame* frame);

	long getNbRandomBits() {
		return nbRandomBits;
	};

	double getAvgThreshold() {
		if (nbSymbols > 0)
			return allThresholds / nbSymbols;
		else
			return 0;
	};

	long getNbFailedSyncs() {
		return nbFailedSyncs;
	};

	long getNbSuccessfulSyncs() {
		return nbSuccessfulSyncs;
	};

	double getNoiseValue() {
		 return normal(0, sqrt(noiseVariance));
	}

	void cancelReception();

protected:
	map<Signal*, int> currentSignals;
	cOutVector receivedPulses;
	cOutVector syncThresholds;

	PhyLayerUWBIR* uwbiface;
	Signal* tracking;
	enum {
		FIRST, HEADER_OVER, SIGNAL_OVER
	};

	bool channelSensing;
	bool synced;
	bool alwaysFailOnDataInterference;

	vector<ConstMapping*> receivingPowers;
	ConstMapping* signalPower; // = signal->getReceivingPower();
	// store relative offsets between signals starts
	vector<simtime_t> offsets;
	vector<AirFrame*> airFrameVector;
	// Create an iterator for each potentially colliding airframe
	vector<AirFrame*>::iterator airFrameIter;

	typedef ConcatConstMapping<std::multiplies<double> > MultipliedMapping;

	bool decodePacket(Signal* signal, vector<bool> * receivedBits);
	simtime_t handleNewSignal(Signal* s);
	simtime_t handleHeaderOver(map<Signal*, int>::iterator& it);
	virtual bool attemptSync(Signal* signal);
	simtime_t
			handleSignalOver(map<Signal*, int>::iterator& it, AirFrame* frame);
	// first value is energy from signal, other value is total window energy
	pair<double, double> integrateWindow(int symbol, simtime_t now,
			simtime_t burst, Signal* signal);

	simtime_t handleChannelSenseRequest(ChannelSenseRequest* request);

};

#endif	/* _UWBIRENERGYDETECTIONDECIDERV2_H */

