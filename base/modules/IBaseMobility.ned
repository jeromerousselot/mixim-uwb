package org.mixim.base.modules;

// Interface for mobility modules
moduleinterface IBaseMobility
{
    parameters:
        bool coreDebug; // debug switch for the core framework
        double x; // x coordinate of the nodes' position (-1 = random)
        double y; // y coordinate of the nodes' position (-1 = random)
        double z; // z coordinate of the nodes' position (-1 = random)
}

