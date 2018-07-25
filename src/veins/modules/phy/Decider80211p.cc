//
// Copyright (C) 2011 David Eckhoff <eckhoff@cs.fau.de>
// Copyright (C) 2012 Bastian Bloessl, Stefan Joerer, Michele Segata <{bloessl,joerer,segata}@ccs-labs.org>
// Copyright (C) 2018 Fabian Bronner <fabian.bronner@ccs-labs.org>
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

/*
 * Based on Decider80211.cc from Karl Wessel
 * and modifications by Christopher Saloman
 */

#include "veins/modules/phy/Decider80211p.h"
#include "veins/modules/phy/DeciderResult80211.h"
#include "veins/modules/messages/Mac80211Pkt_m.h"
#include "veins/base/toolbox/Signal.h"
#include "veins/modules/messages/AirFrame11p_m.h"
#include "veins/modules/phy/NistErrorRate.h"
#include "veins/modules/utility/ConstsPhy.h"

#include "veins/base/toolbox/MathHelper.h"

#ifndef DBG_D11P
#define DBG_D11P EV
#endif
//#define DBG_D11P std::cerr << "[" << simTime().raw() << "] " << myPath << ".Dec "


using namespace Veins;

using Veins::AirFrame;
using Veins::Radio;

simtime_t Decider80211p::processNewSignal(AirFrame* msg) {

	AirFrame11p *frame = check_and_cast<AirFrame11p *>(msg);

	// get the receiving power of the Signal at start-time and center frequency
	Signal& signal = frame->getSignal();

	signalStates[frame] = EXPECT_END;

	if(signal.smallerAtCenterFrequency(sensitivity)) {


		//annotate the frame, so that we won't try decoding it at its end
		frame->setUnderSensitivity(true);
		//check channel busy status. a superposition of low power frames might turn channel status to busy
		if (cca(simTime(), NULL) == false) {
			setChannelIdleStatus(false);
		}
		return signal.getReceptionEnd();
	}
	else {


	    //This value might be just an intermediate result (due to short circuiting)
	    double recvPower = signal.getAtCenterFrequency();
		setChannelIdleStatus(false);

		if (phy11p->getRadioState() == Radio::TX) {
			frame->setBitError(true);
			frame->setWasTransmitting(true);
			DBG_D11P << "AirFrame: " << frame->getId() << " (" << recvPower << ") received, while already sending. Setting BitErrors to true" << std::endl;
		}
		else {

			if (!currentSignal.first) {
				//NIC is not yet synced to any frame, so lock and try to decode this frame
				currentSignal.first = frame;
				DBG_D11P << "AirFrame: " << frame->getId() << " with (" << recvPower << " > " << sensitivity << ") -> Trying to receive AirFrame." << std::endl;
				if (notifyRxStart) {
					phy->sendControlMsgToMac(new cMessage("RxStartStatus",MacToPhyInterface::PHY_RX_START));
				}
			}
			else {
				//NIC is currently trying to decode another frame. this frame will be simply treated as interference
				DBG_D11P << "AirFrame: " << frame->getId() << " with (" << recvPower << " > " << sensitivity << ") -> Already synced to another AirFrame. Treating AirFrame as interference." << std::endl;
			}


			//channel turned busy
			//measure communication density
			myBusyTime += signal.getDuration().dbl();
		}
		return signal.getReceptionEnd();
	}
}

int Decider80211p::getSignalState(AirFrame* frame) {

	if (signalStates.find(frame) == signalStates.end()) {
		return NEW;
	}
	else {
		return signalStates[frame];
	}
}

DeciderResult* Decider80211p::checkIfSignalOk(AirFrame* frame) {

	Signal& s = frame->getSignal();
	simtime_t start = s.getReceptionStart();
	simtime_t end = s.getReceptionEnd();

	//compute receive power
	double recvPower_dBm = 10*log10(s.getAtCenterFrequency());

	start = start + PHY_HDR_PREAMBLE_DURATION; //its ok if something in the training phase is broken

	AirFrameVector airFrames;
	getChannelInfo(start, end, airFrames);

	double noise = phy->getThermalNoiseValue();

	//Make sure to use the adjusted starting-point (which ignores the preamble)
	double sinrMin = MathHelper::getMinSINR(start, end, frame, airFrames, noise);
	double snrMin;
	if (collectCollisionStats) {
		//snrMin = MathHelper::getMinSNR(start, end, frame, noise);
		snrMin = s.getDataMin() / noise;
	}
	else {
		//just set to any value. if collectCollisionStats != true
		//it will be ignored by packetOk
		snrMin = 1e200;
	}

	double payloadBitrate = s.getBitrate();

	DeciderResult80211* result = 0;

	switch (packetOk(sinrMin, snrMin, frame->getBitLength(), payloadBitrate)) {

		case DECODED:
			DBG_D11P << "Packet is fine! We can decode it" << std::endl;
			result = new DeciderResult80211(true, payloadBitrate, sinrMin, recvPower_dBm, false);
			break;

		case NOT_DECODED:
			if (!collectCollisionStats) {
				DBG_D11P << "Packet has bit Errors. Lost " << std::endl;
			}
			else {
				DBG_D11P << "Packet has bit Errors due to low power. Lost " << std::endl;
			}
			result = new DeciderResult80211(false, payloadBitrate, sinrMin, recvPower_dBm, false);
			break;

		case COLLISION:
			DBG_D11P << "Packet has bit Errors due to collision. Lost " << std::endl;
			collisions++;
			result = new DeciderResult80211(false, payloadBitrate, sinrMin, recvPower_dBm, true);
			break;

		default:
			ASSERT2(false, "Impossible packet result returned by packetOk(). Check the code.");
			break;

	}

	return result;
}

enum Decider80211p::PACKET_OK_RESULT Decider80211p::packetOk(double sinrMin, double snrMin, int lengthMPDU, double bitrate) {

	//the lengthMPDU includes the PHY_SIGNAL_LENGTH + PHY_PSDU_HEADER + Payload, while the first is sent with PHY_HEADER_BANDWIDTH

	double packetOkSinr;
	double packetOkSnr;

	//compute success rate depending on mcs and bw
	packetOkSinr = NistErrorRate::getChunkSuccessRate(bitrate, BW_OFDM_10_MHZ, sinrMin, lengthMPDU);

	//check if header is broken
	double headerNoError = NistErrorRate::getChunkSuccessRate(PHY_HDR_BITRATE, BW_OFDM_10_MHZ, sinrMin, PHY_HDR_PLCPSIGNAL_LENGTH);

	double headerNoErrorSnr;
	//compute PER also for SNR only
	if (collectCollisionStats) {

		packetOkSnr = NistErrorRate::getChunkSuccessRate(bitrate, BW_OFDM_10_MHZ, snrMin, lengthMPDU);
		headerNoErrorSnr = NistErrorRate::getChunkSuccessRate(PHY_HDR_BITRATE, BW_OFDM_10_MHZ, snrMin, PHY_HDR_PLCPSIGNAL_LENGTH);

		//the probability of correct reception without considering the interference
		//MUST be greater or equal than when consider it
		assert(packetOkSnr >= packetOkSinr);
		assert(headerNoErrorSnr >= headerNoError);

	}

	//probability of no bit error in the PLCP header

	double rand = RNGCONTEXT dblrand();

	if (!collectCollisionStats) {
		if (rand > headerNoError)
			return NOT_DECODED;
	}
	else {

		if (rand > headerNoError) {
			//ups, we have a header error. is that due to interference?
			if (rand > headerNoErrorSnr) {
				//no. we would have not been able to receive that even
				//without interference
				return NOT_DECODED;
			}
			else {
				//yes. we would have decoded that without interference
				return COLLISION;
			}

		}

	}

	//probability of no bit error in the rest of the packet

	rand = RNGCONTEXT dblrand();

	if (!collectCollisionStats) {
		if (rand > packetOkSinr) {
			return NOT_DECODED;
		}
		else {
			return DECODED;
		}
	}
	else {

		if (rand > packetOkSinr) {
			//ups, we have an error in the payload. is that due to interference?
			if (rand > packetOkSnr) {
				//no. we would have not been able to receive that even
				//without interference
				return NOT_DECODED;
			}
			else {
				//yes. we would have decoded that without interference
				return COLLISION;
			}

		}
		else {
			return DECODED;
		}

	}
}


bool Decider80211p::cca(simtime_t_cref time, AirFrame* exclude) {

	AirFrameVector airFrames;

	// collect all AirFrames that intersect with [start, end]
	getChannelInfo(time, time, airFrames);

	//In the reference implementation only centerFrequenvy - 5e6 (half bandwidth) is checked!
	//Although this is wrong, the same is done here to reproduce original results
	double minPower = phy->getThermalNoiseValue();
	bool isChannelIdle = minPower < ccaThreshold;
	if(airFrames.size()>0)
	{
	    size_t usedFreqIndex = airFrames.front()->getSignal().getSpectrum()->indexOf(centerFrequency - 5e6);
	    isChannelIdle = MathHelper::smallerAtFreqIndex(time, time, airFrames, usedFreqIndex, ccaThreshold - minPower, exclude);
	}

	return isChannelIdle;
}


simtime_t Decider80211p::processSignalEnd(AirFrame* msg) {

	AirFrame11p *frame = check_and_cast<AirFrame11p *>(msg);

	// here the Signal is finally processed
	Signal& signal = frame->getSignal();

	double recvPower_dBm = 10*log10(signal.getRelativeMax());

	bool whileSending = false;

	//remove this frame from our current signals
	signalStates.erase(frame);

	DeciderResult* result;

	if (frame->getUnderSensitivity()) {
		//this frame was not even detected by the radio card
		result = new DeciderResult80211(false,0,0,recvPower_dBm);
	}
	else if (frame->getWasTransmitting() || phy11p->getRadioState() == Radio::TX) {
		//this frame was received while sending
		whileSending = true;
		result = new DeciderResult80211(false,0,0,recvPower_dBm);
	}
	else {

		//first check whether this is the frame NIC is currently synced on
		if (frame == currentSignal.first) {
			// check if the snr is above the Decider's specific threshold,
			// i.e. the Decider has received it correctly
			result = checkIfSignalOk(frame);

			//after having tried to decode the frame, the NIC is no more synced to the frame
			//and it is ready for syncing on a new one
			currentSignal.first = 0;
		}
		else {
			//if this is not the frame we are synced on, we cannot receive it
			result = new DeciderResult80211(false, 0, 0,recvPower_dBm);
		}
	}

	if (result->isSignalCorrect()) {
		DBG_D11P << "packet was received correctly, it is now handed to upper layer...\n";
		// go on with processing this AirFrame, send it to the Mac-Layer
		if (notifyRxStart) {
			phy->sendControlMsgToMac(new cMessage("RxStartStatus",MacToPhyInterface::PHY_RX_END_WITH_SUCCESS));
		}
		phy->sendUp(frame, result);
	}
	else {
		if (frame->getUnderSensitivity()) {
			DBG_D11P << "packet was not detected by the card. power was under sensitivity threshold\n";
		}
		else if (whileSending) {
			DBG_D11P << "packet was received while sending, sending it as control message to upper layer\n";
			phy->sendControlMsgToMac(new cMessage("Error",RECWHILESEND));
		}
		else {
			DBG_D11P << "packet was not received correctly, sending it as control message to upper layer\n";
			if (notifyRxStart) {
				phy->sendControlMsgToMac(new cMessage("RxStartStatus",MacToPhyInterface::PHY_RX_END_WITH_FAILURE));
			}

			if (((DeciderResult80211 *)result)->isCollision()) {
				phy->sendControlMsgToMac(new cMessage("Error", Decider80211p::COLLISION));
			}
			else {
				phy->sendControlMsgToMac(new cMessage("Error",BITERROR));
			}
		}
		delete result;
	}

	if (phy11p->getRadioState() == Radio::TX) {
		DBG_D11P << "I'm currently sending\n";
	}
	//check if channel is idle now
	//we declare channel busy if CCA tells us so, or if we are currently
	//decoding a frame
	else if (cca(simTime(), frame) == false || currentSignal.first != 0) {
		DBG_D11P << "Channel not yet idle!\n";
	}
	else {
		//might have been idle before (when the packet rxpower was below sens)
		if (isChannelIdle != true) {
			DBG_D11P << "Channel idle now!\n";
			setChannelIdleStatus(true);
		}
	}
	return notAgain;
}

void Decider80211p::setChannelIdleStatus(bool isIdle) {
	isChannelIdle = isIdle;
	if (isIdle) phy->sendControlMsgToMac(new cMessage("ChannelStatus",Mac80211pToPhy11pInterface::CHANNEL_IDLE));
	else phy->sendControlMsgToMac(new cMessage("ChannelStatus",Mac80211pToPhy11pInterface::CHANNEL_BUSY));
}

void Decider80211p::changeFrequency(double freq) {
	centerFrequency = freq;
}

double Decider80211p::getCCAThreshold() {
	return 10 * log10(ccaThreshold);
}

void Decider80211p::setCCAThreshold(double ccaThreshold_dBm) {
	ccaThreshold = pow(10, ccaThreshold_dBm / 10);
}

void Decider80211p::setNotifyRxStart(bool enable) {
	notifyRxStart = enable;
}

void Decider80211p::switchToTx() {
	if (currentSignal.first != 0) {
		//we are currently trying to receive a frame.
		if (allowTxDuringRx) {
			//if the above layer decides to transmit anyhow, we need to abort reception
			AirFrame11p *currentFrame = dynamic_cast<AirFrame11p *>(currentSignal.first);
			assert(currentFrame);
			//flag the frame as "while transmitting"
			currentFrame->setWasTransmitting(true);
			currentFrame->setBitError(true);
			//forget about the signal
			currentSignal.first = 0;
		}
		else {
			throw cRuntimeError("Decider80211p: mac layer requested phy to transmit a frame while currently receiving another");
		}
	}
}

void Decider80211p::finish() {
	simtime_t totalTime = simTime() - myStartTime;
	phy->recordScalar("busyTime", myBusyTime / totalTime.dbl());
	if (collectCollisionStats) {
		phy->recordScalar("ncollisions", collisions);
	}
}

Decider80211p::~Decider80211p() {}
;
