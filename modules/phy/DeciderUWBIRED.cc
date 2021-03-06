/* -*- mode:c++ -*- ********************************************************
 * file:        DeciderUWBIRED.cc
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
 * description: this Decider models a non-coherent energy-detection receiver.
 * acknowledgment: this work was supported (in part) by the National Competence
 * 			    Center in Research on Mobile Information and Communication Systems
 * 				NCCR-MICS, a center supported by the Swiss National Science
 * 				Foundation under grant number 5005-67322.
 ***************************************************************************/

#include "DeciderUWBIRED.h"
#include "PhyLayerUWBIR.h"


simtime_t DeciderUWBIRED::processSignal(AirFrame* frame) {
	Signal* s = &frame->getSignal();
	map<Signal*, int>::iterator it = currentSignals.find(s);

	if (it == currentSignals.end()) {
		return handleNewSignal(s);
	} else {
		switch (it->second) {
		case HEADER_OVER:
			return handleHeaderOver(it);
		case SIGNAL_OVER:
			return handleSignalOver(it, frame);
		default:
			break;
		}
	}
	//we should never get here!
	assert(false);
	return 0;
}

simtime_t DeciderUWBIRED::handleNewSignal(Signal* s) {

	int currState = uwbiface->getRadioState();
	if (tracking == 0 && currState == RadioUWBIR::SYNC) {
		  simtime_t endOfHeader = s->getSignalStart()
				+ IEEE802154A::mandatory_preambleLength;
		  currentSignals[s] = HEADER_OVER;
		  assert(endOfHeader> 0);
		  return endOfHeader;
	} else {
		// we are already tracking an airframe, this is noise
		// or we are transmitting, switching, or sleeping
		currentSignals[s] = SIGNAL_OVER;
		simtime_t endOfSignal = s->getSignalStart() + s->getSignalLength();
		assert(endOfSignal> 0);
		return endOfSignal;
	}

}


// We just left reception state ; we must update our information on this frame accordingly

void DeciderUWBIRED::cancelReception() {
	if(tracking != NULL) {
   	  tracking = NULL;
   	  synced = false;
	}
}

simtime_t DeciderUWBIRED::handleHeaderOver(map<Signal*, int>::iterator& it) {

	int currState = uwbiface->getRadioState();
	Signal* s = it->first;

	if (tracking == 0 && currState == RadioUWBIR::SYNC) {
		// We are not tracking a signal currently.
		// Can we synchronize on this one ?

		bool isSyncSignalHigherThanThreshold;
		if(syncAlwaysSucceeds) {
			isSyncSignalHigherThanThreshold = true;
		} else {
			isSyncSignalHigherThanThreshold = attemptSync(s);
		}

		packet.setNbSyncAttempts(packet.getNbSyncAttempts() + 1);

		if(isSyncSignalHigherThanThreshold) {
			nbSuccessfulSyncs = nbSuccessfulSyncs + 1;
			tracking = s;
			synced = true;
			currentSignals[s] = SIGNAL_OVER;
			uwbiface->switchRadioToRX();
			packet.setNbSyncSuccesses(packet.getNbSyncSuccesses() + 1);
		} else {
			nbFailedSyncs = nbFailedSyncs + 1;
		}
		utility->publishBBItem(catUWBIRPacket, &packet, -1); // scope = all == host

	}
	// in any case, look at that frame again when it is finished
	it->second = SIGNAL_OVER;
	simtime_t startOfSignal = it->first->getSignalStart();
	simtime_t lengthOfSignal = it->first->getSignalLength();
	simtime_t endOfSignal = startOfSignal + lengthOfSignal;
	assert(endOfSignal> 0);
	return endOfSignal;
}

bool DeciderUWBIRED::attemptSync(Signal* s) {
	double snrValue;
	ConstMapping* power = s->getReceivingPower();
	ConstMappingIterator* mIt = power->createConstIterator();

	vector<AirFrame*> syncVector;
	// Retrieve all potentially colliding airFrames
	phy->getChannelInfo(s->getSignalStart(), simTime(), syncVector);

	if(syncVector.size() > 1) {
		// do not accept interferers
		return false;
	}
	Argument posFirstPulse(IEEE802154A::tFirstSyncPulseMax + s->getSignalStart());
	mIt->jumpTo(posFirstPulse);
	snrValue = std::abs(mIt->getValue()/getNoiseValue());
    syncThresholds.record(snrValue);
    if(snrValue > syncThreshold) {
    	return true;
    } else {
    	return false;
    }
}

simtime_t DeciderUWBIRED::handleSignalOver(map<Signal*, int>::iterator& it, AirFrame* frame) {
	if (it->first == tracking) {
		vector<bool>* receivedBits = new vector<bool>();
		bool isCorrect = decodePacket(it->first, receivedBits);
		// we cannot compute bit error rate here
		// so we send the packet to the MAC layer which will compare receivedBits
		// with the actual bits sent (stored in the encapsulated UWBIRMacPkt object).
		DeciderResultUWBIR * result = new DeciderResultUWBIR(isCorrect, receivedBits);
		if(isCorrect) {
		  phy->sendUp(frame, result);
		} else {
			delete frame;
			delete result;
		}
		currentSignals.erase(it);
		tracking = 0;
		synced = false;
		uwbiface->switchRadioToSync();
		return -1;
	} else {
		// reached end of noisy signal
		currentSignals.erase(it);
		return -1;
	}
}

/*
 * @brief Returns false if the packet is incorrect. If true,
 * the MAC layer must still compare bit values to validate the frame.
 */
bool DeciderUWBIRED::decodePacket(Signal* signal,
		vector<bool> * receivedBits) {

	simtime_t now, offset;
	simtime_t aSymbol, shift, burst;

	packetSNIR = 0;
	packetNoise = 0;
	packetSignal = 0;
	packetSamples = 0;

	// Retrieve all potentially colliding airFrames
	phy->getChannelInfo(signal->getSignalStart(), signal->getSignalStart()
			+ signal->getSignalLength(), airFrameVector);

	for (airFrameIter = airFrameVector.begin(); airFrameIter
			!= airFrameVector.end(); ++airFrameIter) {
		Signal & aSignal = (*airFrameIter)->getSignal();
		offsets.push_back(signal->getSignalStart() - aSignal.getSignalStart());
		ConstMapping* currPower = aSignal.getReceivingPower();
		receivingPowers.push_back(currPower);
		if (aSignal.getSignalStart() == signal->getSignalStart()
				&& aSignal.getSignalLength() == signal->getSignalLength()) {
			signalPower = currPower;
		}
	}

	// times are absolute
	offset = signal->getSignalStart() + IEEE802154A::mandatory_preambleLength;
	shift = IEEE802154A::mandatory_timeShift;
	aSymbol = IEEE802154A::mandatory_symbol;
	burst = IEEE802154A::mandatory_burst;
	now = offset + IEEE802154A::mandatory_pulse / 2;
	std::pair<double, double> energyZero, energyOne;

	epulseAggregate = 0;
	enoiseAggregate = 0;

	// debugging information (start)
	if (trace) {
		ConstMappingIterator* iteratorDbg = signalPower->createConstIterator();
		iteratorDbg->jumpToBegin();
		while (iteratorDbg->hasNext()) {
			iteratorDbg->next();
			receivedPulses.recordWithTimestamp(
			  iteratorDbg->getPosition().getTime(),
			  signalPower->getValue(iteratorDbg->getPosition())
			);
		}
		delete iteratorDbg;
	}
	// debugging information (end)

	int symbol;
	// Loop to decode each bit value
	for (symbol = 0; IEEE802154A::mandatory_preambleLength + symbol
			* aSymbol < signal->getSignalLength(); symbol++) {

		int hoppingPos = IEEE802154A::getHoppingPos(symbol);
		int decodedBit;

		if (stats) {
			nbSymbols = nbSymbols + 1;
		}

		// sample in window zero
		now = now + IEEE802154A::getHoppingPos(symbol)*IEEE802154A::mandatory_burst;
		energyZero = integrateWindow(symbol, now, burst, signal);
		// sample in window one
		now = now + shift;
		energyOne = integrateWindow(symbol, now, burst, signal);

		if (energyZero.second > energyOne.second) {
		  decodedBit = 0;
	    } else {
	      decodedBit = 1;
		//packetSNIR = packetSNIR + energyOne.first;
	    }

		packetSNIR = packetSNIR + energyZero.first; // valid only if the payload consists of zeros
		receivedBits->push_back(static_cast<bool>(decodedBit));
		//packetSamples = packetSamples + 16; // 16 EbN0 evaluations per bit

		now = offset + (symbol + 1) * aSymbol + IEEE802154A::mandatory_pulse / 2;

	}
    symbol = symbol + 1;

	bool isCorrect = true;
	if(airFrameVector.size() > 1 && alwaysFailOnDataInterference) {
		isCorrect = false;
	}

	airFrameVector.clear();
	receivingPowers.clear();
	offsets.clear();

	return isCorrect;
}

/*
 * @brief Returns a pair with as first value the SNIR (if the signal is not nul in this window, and 0 otherwise)
 * and as second value a "score" associated to this window. This score is equals to the sum for all
 * 16 pulse peak positions of the voltage measured by the receiver ADC.
 */
pair<double, double> DeciderUWBIRED::integrateWindow(int symbol,
		simtime_t now, simtime_t burst, Signal* signal) {
	std::pair<double, double> energy;
	energy.first = 0; // stores SNIR
	energy.second = 0; // stores total captured window energy
	vector<ConstMapping*>::iterator mappingIter;
	Argument arg;
	simtime_t windowEnd = now + burst;

	double burstsnr = 0;
	// Triangular baseband pulses
	// we sample at each pulse peak
	// get the interpolated values of amplitude for each interferer
	// and add these to the peak with a random phase

	// we sample one point per pulse
	// caller has already set our time reference ("now") at the peak of the pulse
	for (; now < windowEnd; now += IEEE802154A::mandatory_pulse) {
		double signalValue = 0;	// electric field from tracked signal [V/m²]
		double resPower = 0;		// electric field at antenna = combination of all arriving electric fields [V/m²]
		double vEfield = 0;		// voltage at antenna caused by electric field Efield [V]
		double vmeasured = 0;	// voltage measured by energy-detector [V], including thermal noise
		double vmeasured_square = 0; // to the square [V²]
		double vsignal_square = 0;	// square of the voltage that would be induced on the antenna if there were no interferers (Efield=signalValue)
		double vnoise_square = 0;	// (thermal noise + interferers noise)²
		double snir = 0;			// burst SNIR estimate
		double vThermalNoise = 0;	// thermal noise realization
		arg.setTime(now);		// loop variable: begin by considering the first pulse
		int currSig = 0;

		// consider all interferers at this point in time
		for (airFrameIter = airFrameVector.begin(); airFrameIter
				!= airFrameVector.end(); ++airFrameIter) {
			Signal & aSignal = (*airFrameIter)->getSignal();
			ConstMapping* currPower = aSignal.getReceivingPower();
			double measure = currPower->getValue(arg)*peakPulsePower; //TODO: de-normalize (peakPulsePower should be in AirFrame or in Signal, to be set at run-time)
//			measure = measure * uniform(0, +1); // random point of Efield at sampling (due to pulse waveform and self interference)
			if (currPower == signalPower) {
				signalValue = measure*0.5; // we capture half of the maximum possible pulse energy to account for self  interference
				resPower = resPower + signalValue;
			} else {
				// take a random point within pulse envelope for interferer
				resPower = resPower + measure * uniform(-1, +1);
			}
			++currSig;
		}

		double attenuatedPower = resPower / 10; // 10 dB = 6 dB implementation loss + 5 dB noise factor
		vEfield = sqrt(50*resPower); // P=V²/R
		// add thermal noise realization
		vThermalNoise = getNoiseValue();
		vnoise2 = pow(vThermalNoise, 2);    // for convenience
		vmeasured = vEfield + vThermalNoise;
		vmeasured_square = pow(vmeasured, 2);
		// signal + interference + noise
		energy.second = energy.second + vmeasured_square;  // collect this contribution

		// Now evaluates signal to noise ratio
		// signal converted to antenna voltage squared
		vsignal_square = 50*signalValue;
		vnoise_square = pow(sqrt(vmeasured_square) - sqrt(vsignal_square), 2); // everything - signal = noise + interfence
		snir = signalValue / 2.0217E-12;
		epulseAggregate = epulseAggregate + pow(vEfield, 2);
		enoiseAggregate = enoiseAggregate + vnoise2;
		packetSignal = packetSignal + vsignal_square;
		packetNoise = packetNoise + vnoise_square;
		snirs = snirs + snir;
		snirEvals = snirEvals + 1;
		if(signalValue > 0) {
		  double pulseSnr = signalValue / (vnoise_square/50);
		  burstsnr = burstsnr + pulseSnr;
		}
		energy.first = energy.first + snir;

	} // consider next point in time
	return energy;
}

simtime_t DeciderUWBIRED::handleChannelSenseRequest(
		ChannelSenseRequest* request) {
	if (channelSensing) {
		// send back the channel state
		request->setResult(new ChannelState(synced, 0)); // bogus rssi value (0)
		phy->sendControlMsg(request);
		channelSensing = false;
		return -1; // do not call me back ; I have finished
	} else {
		channelSensing = true;
		return -1; //phy->getSimTime() + request->getSenseDuration();
	}
}
