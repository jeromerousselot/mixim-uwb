#include "veins/modules/analogueModel/BreakpointPathlossModel.h"

#include "veins/base/messages/AirFrame_m.h"

using namespace Veins;
using Veins::AirFrame;

#define debugEV EV << "PhyLayer(BreakpointPathlossModel): "

void BreakpointPathlossModel::filterSignal(Signal* signal, const Coord& sendersPos, const Coord& receiverPos)
{

    /** Calculate the distance factor */
    double distance = useTorus ? receiverPos.sqrTorusDist(sendersPos, playgroundSize) : receiverPos.sqrdist(sendersPos);
    distance = sqrt(distance);
    debugEV << "distance is: " << distance << endl;

    if (distance <= 1.0) {
        // attenuation is negligible
        return;
    }

    double attenuation = 1;
    // PL(d) = PL0 + 10 alpha log10 (d/d0)
    // 10 ^ { PL(d)/10 } = 10 ^{PL0 + 10 alpha log10 (d/d0)}/10
    // 10 ^ { PL(d)/10 } = 10 ^ PL0/10 * 10 ^ { 10 log10 (d/d0)^alpha }/10
    // 10 ^ { PL(d)/10 } = 10 ^ PL0/10 * 10 ^ { log10 (d/d0)^alpha }
    // 10 ^ { PL(d)/10 } = 10 ^ PL0/10 * (d/d0)^alpha
    if (distance < breakpointDistance) {
        attenuation = attenuation * PL01_real;
        attenuation = attenuation * pow(distance, alpha1);
    }
    else {
        attenuation = attenuation * PL02_real;
        attenuation = attenuation * pow(distance / breakpointDistance, alpha2);
    }
    attenuation = 1 / attenuation;
    debugEV << "attenuation is: " << attenuation << endl;

    if (debug) {
        pathlosses.record(10 * log10(attenuation)); // in dB
    }

    signal->addUniformAttenuation(attenuation);
}
