#ifndef TUNER_INTERFACE_H
#define TUNER_INTERFACE_H

#include <AltSoftSerial.h>

#include "CatParser.h"
#include "Max3100.h"

//! Application class for the Icom AH-4 tuner to Yaesu FT-450D transceiver interface.
class TunerInterface
{
public:
	//! Default constructor.
	TunerInterface();

	//! Destructor.
	~TunerInterface();

	//! Performs initialisation functions.
	void init();

	//! Executes the application logic.
	void poll();

private:
	enum Status
	{
		STATUS_SUCCESS,
		STATUS_FAIL,
		STATUS_INTERNAL_ERROR,
		STATUS_FATAL_ERROR
	};

	enum State
	{
		STATE_IDLE,
		STATE_TXR_START,
		STATE_TUNER_START,
		STATE_TUNING,
		STATE_FINISH_WAIT1,
		STATE_FINISH_WAIT2
	};

	enum StartingFlags
	{
		STARTING_FUNCTION_TX = 1 << 0,
		STARTING_MODE = 1 << 1,
		STARTING_METER = 1 << 2,
		STARTING_POWER = 1 << 3,
		STARTING_MASK = 0x0f
	};

	Max3100 m_txrSerial;
	AltSoftSerial m_txrTunerSerial;

	CatParser m_pcParser;
	CatParser m_txrParser;
	CatParser m_txrTunerParser;

	State m_state;
	unsigned long m_timeout;

	char m_starting;
	char m_functionTx;
	char m_mode;
	char m_meter;
	char m_power;

	char m_statusCountdown;
	bool m_statusHigh;
	unsigned long m_statusTime;
	unsigned long m_statusDuration;

	// Signals a status condition by flashing an LED
	void status(unsigned long now, Status status);

	// Restores the transceiver to its pre-tuning state
	void restoreTransceiver();

	// Processes serial inputs
	void doSerial(unsigned long now, CatMessage &pcMessage, CatMessage &txrMessage, CatMessage &txrTunerMessage);

	// Processes the application logic
	void doLogic(unsigned long now, CatMessage &pcMessage, CatMessage &txrMessage, CatMessage &txrTunerMessage);

	// Updates the flashing status LED
	void doStatus(unsigned long now);

	// Disabled operations
	TunerInterface(const TunerInterface &) = delete;
	TunerInterface & operator=(const TunerInterface &) = delete;
};

#endif

