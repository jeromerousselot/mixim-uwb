/*
 * CrankshaftBase
 *
 * A "reverse TDMA" mac protocol.
 * We cheat on the time synchronisation part, by having all nodes be in sync
 * from the start. However, if they haven't heard a neighbour they will
 * listen as if they are unsynchronised.
 */


//#include "mixim.h"
#include "message.h"

#include "crankshaft-base.h"

#define CLOCK_SKEW_ALLOWANCE 2
#define CONTEND_TIME 100
#define ACK_CONTEND_TIME 5
/* Time for sending a (maximum length) message, receiving the ACK, clock skew
   allowence and contention time. */
#define SLOT_TIME	(int)( ( frameTotalTime(max_header_length + data_length) + \
	frameTotalTime(max_header_length) + 2 * EXTRA_TRANSMIT_TIME ) * 32768.0 + \
	CLOCK_SKEW_ALLOWANCE + CONTEND_TIME + ACK_CONTEND_TIME)

int CrankshaftBase::data_length, CrankshaftBase::backoff_max, CrankshaftBase::max_retries;
bool CrankshaftBase::useScp, CrankshaftBase::parametersInitialised = false, CrankshaftBase::useSift, CrankshaftBase::rerouteOnFail, CrankshaftBase::quickAbort;
double CrankshaftBase::retryChance;

/* FIXME:

	- sync frequency
	- clock:
		- ticks to next frame
		- hops to sink
		- [ frame id ] (may fit elsewhere, only 2 bits)
		Does the above fit in 2 bytes? [ FIXED: set to 3 ]
	- sync only in bcast?
*/

/* Notes:
	- For normal routing/unicast: the problems are 2 fold:
		- the sink needs to be awake all the time
		- retries need to be spread [for more complex routing protocols, the
			retries will be different anyway, so no need to do anything special]
*/

void CrankshaftBase::initialize() {
	EyesMacLayer::initialize();
	printfNoInfo("\t\tCRANKSHAFT initializing...");
	
	tx_msg = NULL;
	current_slot = -1;
	current_slot_state = SSTATE_NONE;
	initialized = INIT_NONE;
	send_state = SEND_NONE;
	
	if (!parametersInitialised) {
		parametersInitialised = true;
		data_length = getLongParameter("dataLength", 64);
		backoff_max = getLongParameter("backoffMax", 16);
		max_retries = getLongParameter("maxRetries", 3);
		useScp = getBoolParameter("useScp", true);
		useSift = getBoolParameter("useSift", true);
		retryChance = getDoubleParameter("retryChance", 0.7);
		rerouteOnFail = getBoolParameter("rerouteOnFail", false);
		quickAbort = getBoolParameter("quickAbort", false);
	}

	backoff = 0;
	
	if (macid() == 0) {
		backoff = 3;
		initialized = INIT_DO_NOTIFIY;
	}

	setTimeout(2, SLOT_TIMER);
	setRadioListen();
}

void CrankshaftBase::finish() {
	EyesMacLayer::finish();
	printfNoInfo("\t\tCRANKSHAFT ending...");
}

CrankshaftBase::~CrankshaftBase() {
	parametersInitialised = false;
	if (tx_msg)
		delete tx_msg;
}	

void CrankshaftBase::txPacket(MacPacket * msg){
	assert(msg);
	if(tx_msg) {
		printf("MAC busy! dropping at tx_packet");
		++stat_tx_drop;
		delete msg;
		return;
	}
	tx_msg = msg;
	retries = 0;
	if (rerouteOnFail) {
		if (tx_msg->getData() == NULL ||
				((MessageInfo *) tx_msg->getData())->id != macid()) {
			/* Tag this message with our data */
			MessageInfo messageInfo(macid());
			tx_msg->setData(&messageInfo, sizeof(messageInfo), 0);
		}
	}
}

void CrankshaftBase::rxFrame(MacPacket * msg) {
	assert(msg);
	Header *header = newHeader(msg->getData());
	
	if (initialized == INIT_NONE) {
		backoff = (int) intuniform(0, backoff_max, RNG_MAC);
		initialized = INIT_DO_NOTIFIY;
	}

	switch (header->getType()) {
		case MSG_NOTIFY:
			reg_rx_overhead(msg);
			printf("notify received");
			delete msg;
			break;
		case MSG_DATA:
			if(msg->local_to == macid()) {
				printf("unicast frame received");	
				ack_to = msg->local_from;

				reg_rx_data(msg);
				msg->discardData();
				rxPacket(msg);
				++stat_rx;

				send_state = SEND_ACK;
				precontend = false;
				setTimeout(ACK_CONTEND_TIME, CONTENTION_TIMER);
				/* Do listen and avoid setRadioSleep at the end. */
				setRadioListen();

				return;
			} else {
				printf("overheard frame, not for me");
				reg_rx_overhear(msg);
				send_state = SEND_NONE;
				delete msg;
			}
			break;
		case MSG_BCAST:
			printf("local broadcast received");
			reg_rx_data(msg);
			msg->discardData();
			rxPacket(msg);
			++stat_rx;
			break;
		case MSG_ACK:
			if (msg->local_to == macid() && send_state == SEND_WFACK && tx_msg->local_to == msg->local_from) {
				printf("Received ack from %d", msg->local_from);
				txPacketDone(tx_msg); // report success
				tx_msg = NULL;

				++stat_tx;
				reg_rx_overhead(msg);
			} else {
				printf("Received unexpected ack from %d", msg->local_from);
				reg_rx_overhear(msg);
			}
			delete msg;
			send_state = SEND_NONE;
			break;
		default:
			assert(0);
	}
	setRadioSleep();
	delete header;
}

void CrankshaftBase::rxFailed() {
	if (send_state == SEND_ACK)
		return;
	setRadioSleep();
}

void CrankshaftBase::rxStarted() {
	if (initialized == INIT_NONE || send_state == SEND_ACK)
		return;
	cancelTimeout(MSG_TIMEOUT);
	cancelTimeout(ACK_TIMEOUT);
	cancelTimeout(CONTENTION_TIMER);
	if (useScp)
		cancelTimeout(POLL_TIMER);
	/* If we're not contending, or this is the broadcast slot, stay awake.
	   Otherwise: go to sleep. */
	assert(current_slot_state != SSTATE_SLEEP);
	if (current_slot_state == SSTATE_SEND && send_state != SEND_WFACK)
		setRadioSleep();

	if (rerouteOnFail && send_state == SEND_DATA && tx_msg->local_to != BROADCAST) {
		printf("Lost contention, returning message to routing");
		tx_msg->setKind(TX_CONTEND_LOSE);
		txPacketDone(tx_msg);
		tx_msg = NULL;
	}
	
	if (send_state != SEND_WFACK)
		send_state = SEND_NONE;
}

void CrankshaftBase::rxHeader(MacPacket * msg) {
	if (quickAbort && msg->local_to != macid() && msg->local_to != BROADCAST)
		setRadioSleep();
}

void CrankshaftBase::transmitDone() {
	switch (send_state) {
		case SEND_NOTIFY:
			assert(initialized == INIT_DO_NOTIFIY);
			initialized = INIT_DONE;
			break;
		case SEND_ACK:
			break;
		case SEND_DATA:
			if (tx_msg->local_to != BROADCAST) {
				send_state = SEND_WFACK;
				setRadioListen();
				setTimeout(ACK_CONTEND_TIME + 10, ACK_TIMEOUT);
				return;
			} else {
				txPacketDone(tx_msg); // report success
				tx_msg = NULL;

				++stat_tx;
			}
			break;
		default:
			assert(0);
	}
	send_state = SEND_NONE;
	setRadioSleep();
}

void CrankshaftBase::wrapSlotCounter() {}

void CrankshaftBase::timeout(int which) {
	
	switch(which) {
		case SLOT_TIMER:
			setTimeout(SLOT_TIME, SLOT_TIMER);
			current_slot++;
			wrapSlotCounter();

			if (macid() == 0)
				printf("==== Slot %d ====", current_slot);

			if (initialized == INIT_NONE)
				break;

			if (initialized == INIT_DO_NOTIFIY) {
				printf("Will send notify in %d slots", backoff);
				if (backoff == 0) {
					send_state = SEND_NOTIFY;
					contend();
					if (useScp)
						setTimeout(CLOCK_SKEW_ALLOWANCE + CONTEND_TIME - 5, POLL_TIMER);
				} else {
					backoff--;
				}
				break;
			}

			assert(send_state == SEND_NONE);

			current_slot_state = getCurrentSlotState();

			if ((current_slot_state == SSTATE_SEND || current_slot_state == SSTATE_SEND_RECEIVE) &&
					(retries != 0 || (rerouteOnFail && ((MessageInfo *) tx_msg->getData())->last_failed)) &&
					uniform(0, 1, RNG_MAC) > retryChance) {
				if (rerouteOnFail) {
					printf("retry skipped, returning message to routing");
					tx_msg->setKind(TX_RETRY_SKIPPED);
					txPacketDone(tx_msg);
					tx_msg = NULL;
				}

				if (current_slot_state == SSTATE_SEND)
					current_slot_state = SSTATE_SLEEP;
				else
					current_slot_state = SSTATE_RECEIVE;
			}

			switch (current_slot_state) {
				case SSTATE_SEND_RECEIVE:
					msgTimeoutState = CHECK_ACTIVITY;
					setTimeout(CLOCK_SKEW_ALLOWANCE + CONTEND_TIME, MSG_TIMEOUT);
					/* FALLTHROUGH */
				case SSTATE_SEND:
					if (rerouteOnFail)
						((MessageInfo *) tx_msg->getData())->last_failed = false;
					send_state = SEND_DATA;
					contend();
					if (useScp)
						setTimeout(CLOCK_SKEW_ALLOWANCE + CONTEND_TIME - 5, POLL_TIMER);
					break;
				case SSTATE_SLEEP:
					setRadioSleep();
					break;
				case SSTATE_RECEIVE:
					msgTimeoutState = CHECK_ACTIVITY;
					setTimeout(CLOCK_SKEW_ALLOWANCE + CONTEND_TIME, MSG_TIMEOUT);
					if (useScp) {
						setTimeout(CLOCK_SKEW_ALLOWANCE + CONTEND_TIME - 10, POLL_TIMER);
						setRadioSleep();
					} else {
						setRadioListen();
					}
					break;
				default:
					assert(0);
			}
			break;
		case CONTENTION_TIMER: {
			Header *header;

			if (precontend) {
				setRadioListen();
				precontend = false;
				setTimeout(10, CONTENTION_TIMER);
				return;
			}
			
			if (getRssi() > 0.5 && send_state != SEND_ACK) {
				cancelTimeout(POLL_TIMER);
				if (rerouteOnFail && send_state == SEND_DATA && tx_msg->local_to != BROADCAST) {
					printf("Lost contention, returning message to routing");
					tx_msg->setKind(TX_CONTEND_LOSE);
					txPacketDone(tx_msg);
					tx_msg = NULL;
				}
				send_state = SEND_NONE;
				if (SSTATE_SEND_RECEIVE)
					break;
				setRadioSleep();
				break;
			}

			cancelTimeout(MSG_TIMEOUT);
			setRadioTransmit();

			switch (send_state) {
				case SEND_NOTIFY:
					assert(initialized == INIT_DO_NOTIFIY);
					packet = new MacPacket(this,"init_notify");
					packet->local_to = BROADCAST;
					header = newHeader(MSG_NOTIFY);
					reg_tx_overhead(packet);
					printf("Sending notify now");
					break;
				case SEND_DATA:
					assert(tx_msg);

					header = newHeader(tx_msg->local_to == BROADCAST ? MSG_BCAST : MSG_DATA );
					//now do real send
					reg_tx_data(tx_msg); // statistics
					packet = (MacPacket *) tx_msg->dup();;
					printf("Sending message to %d", tx_msg->local_to);
					break;
				case SEND_ACK:
					packet = new MacPacket(this,"ack");
					packet->local_to = ack_to;
					header = newHeader(MSG_ACK);
					printf("Sending ack to %d", ack_to);
					reg_tx_overhead(packet);
					break;
				default:
					assert(0);
			}
			packet->setData(header->data(), header->dataSize(), header->extraLength());
			delete header;
			packet->local_from = macid();
			
			if (!useScp || send_state == SEND_ACK)
				startTransmit(packet);
			return;
		}
		case MSG_TIMEOUT:
			/* Check that there is no usable activity. */
			if (msgTimeoutState == CHECK_ACTIVITY && getRssi() > 0.5) {
				//~ printf("Detected activity, waiting for startsym");
				msgTimeoutState = CHECK_STARTSYM;
				setTimeout((int)(frameTotalTime(0) * 32768.0), MSG_TIMEOUT);
			} else {
				/* No activity or no startsymbol received */
				//~ if (msgTimeoutState == CHECK_STARTSYM)
					//~ printf("No startsymbol, going to sleep");
				setRadioSleep();
			}
			break;
		case ACK_TIMEOUT:
			printf("ack-timeout");

			if (rerouteOnFail) {
				printf("No ack, returning message to routing");
				((MessageInfo *) tx_msg->getData())->last_failed = true;
				tx_msg->setKind(TX_NO_ACK);
				txPacketDone(tx_msg);
				tx_msg = NULL;
			} else {
				retries++;
				if (retries >= max_retries) {
					txPacketFail(tx_msg);
					tx_msg = NULL;
					stat_tx_drop++;
				}
			}
			send_state = SEND_NONE;
			setRadioSleep();
			break;
		case POLL_TIMER:
			assert(useScp);
			if (send_state == SEND_NONE)
				setRadioListen();
			else {
				assert(packet);
				startTransmit(packet);
			}
			break;
		default:
			assert(false); // illegal state
	}
}

int CrankshaftBase::headerLength() {
	return min_header_length;
}

void CrankshaftBase::contend(void) {
	int slots;

	if (useSift)
		slots = siftSlot(CLOCK_SKEW_ALLOWANCE/2, CONTEND_TIME - 10);
	else
		slots = intuniform(CLOCK_SKEW_ALLOWANCE/2, CONTEND_TIME - 10);

	if (slots <= 10 || (!useScp && current_slot_state == SSTATE_SEND_RECEIVE)) {
		precontend = false;
		setRadioListen();
	} else {
		precontend = true;
		slots -= 10;
	}	
	setTimeout(slots, CONTENTION_TIMER);
}

int CrankshaftBase::firstToWake(std::vector<int> *nodes) {
	int i, currentLowest = INT_MAX, node = -1;

	for (i = nodes->size() - 1; i >= 0; i--) {
		int slots;

#ifdef SINK_ALWAYS_ON // Assumes that node 0 is sink!	
		if ((*nodes)[i] == 0)
			slots = -1;
		else
#endif
			slots = slotsUntilWake((*nodes)[i]);

		printf("node %d @ %d", (*nodes)[i], slots);
		
		if (slots < currentLowest) {
			node = (*nodes)[i];
			currentLowest = slots;
		}
	}

	return node == -1 ? (*nodes)[0] : node;
}
