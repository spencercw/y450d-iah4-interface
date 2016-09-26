#include "TunerInterface.h"

#include <arduino.h>

static const int TUNER_START_PIN = 10;
static const int TUNER_KEY_PIN = 7;
static const int JUMP_PIN = 6;
static const int STATUS_PIN = 4;
static const int TXR_SLAVE_SELECT = 5;

static const unsigned long TXR_START_TIMEOUT_MS = 500;
static const unsigned long TUNER_START_TIMEOUT_MS = 500;
static const unsigned long TUNE_TIMEOUT_MS = 10000;
static const unsigned long TUNE_FINISH1_TIMEOUT_MS = 100;
static const unsigned long TUNE_FINISH2_TIMEOUT_MS = 500;

static const char STATUS_FLASHES[] = { 1, 1, 5 };
static const unsigned long STATUS_FLASH_DURATION_MS[] = { 100, 1000, 100 };

TunerInterface::TunerInterface():
m_state(STATE_IDLE),
m_timeout(0),
m_starting(0),
m_functionTx(0),
m_mode(0),
m_meter(0),
m_power(0),
m_statusCountdown(0),
m_statusHigh(false),
m_statusTime(0),
m_statusDuration(0)
{
}

TunerInterface::~TunerInterface()
{
}

void TunerInterface::init()
{
	pinMode(TUNER_START_PIN, OUTPUT);
	pinMode(TUNER_KEY_PIN, INPUT_PULLUP);
	pinMode(JUMP_PIN, INPUT_PULLUP);
	pinMode(STATUS_PIN, OUTPUT);

	digitalWrite(TUNER_START_PIN, HIGH);
	digitalWrite(STATUS_PIN, LOW);

	Serial.begin(38400);
	if (!m_txrSerial.begin(TXR_SLAVE_SELECT))
	{
		status(0, STATUS_FATAL_ERROR);
	}
	m_txrTunerSerial.begin(4800);

	// Flash the status LED to indicate initialisation has completed
	digitalWrite(STATUS_PIN, HIGH);
	delay(100);
	digitalWrite(STATUS_PIN, LOW);
	delay(100);
	digitalWrite(STATUS_PIN, HIGH);
	delay(100);
	digitalWrite(STATUS_PIN, LOW);
}

void TunerInterface::poll()
{
	unsigned long now = millis();

	CatMessage pcMessage, txrMessage, txrTunerMessage;
	doSerial(now, pcMessage, txrMessage, txrTunerMessage);
	doLogic(now, pcMessage, txrMessage, txrTunerMessage);
	doStatus(now);
}

void TunerInterface::status(unsigned long now, Status status)
{
	// For a fatal error just enter an infinite loop flashing the status LED
	if (status == STATUS_FATAL_ERROR)
	{
		for (;;)
		{
			digitalWrite(STATUS_PIN, HIGH);
			delay(STATUS_FLASH_DURATION_MS[STATUS_INTERNAL_ERROR]);
			digitalWrite(STATUS_PIN, LOW);
			delay(STATUS_FLASH_DURATION_MS[STATUS_INTERNAL_ERROR]);
		}
	}

	m_statusCountdown = STATUS_FLASHES[status];
	m_statusHigh = true;
	m_statusTime = now;
	m_statusDuration = STATUS_FLASH_DURATION_MS[status];
	digitalWrite(STATUS_PIN, HIGH);
}

void TunerInterface::restoreTransceiver()
{
	char buffer[64];
	sprintf(buffer, "FT%u;MD0%X;MS%u;PC%03u;",
		static_cast<unsigned>(m_functionTx),
		static_cast<unsigned>(m_mode),
		static_cast<unsigned>(m_meter),
		static_cast<unsigned>(m_power));
	m_txrSerial.write(buffer);
}

void TunerInterface::doSerial(unsigned long now, CatMessage &pcMessage, CatMessage &txrMessage,
	CatMessage &txrTunerMessage)
{
	pcMessage = {};
	if (Serial.available())
	{
		char ch = static_cast<char>(Serial.read());
		pcMessage = m_pcParser.process(ch, now);
	}

	txrMessage = {};
	{
		int ch = m_txrSerial.read();
		if (ch != -1)
		{
			txrMessage = m_txrParser.process(static_cast<char>(ch), now);
		}
	}

	txrTunerMessage = {};
	if (m_txrTunerSerial.available())
	{
		char ch = static_cast<char>(m_txrTunerSerial.read());
		txrTunerMessage = m_txrTunerParser.process(ch, now);
	}
}

void TunerInterface::doLogic(unsigned long now, CatMessage &pcMessage, CatMessage &txrMessage,
	CatMessage &txrTunerMessage)
{
	switch (m_state)
	{
	case STATE_IDLE:
		// Pass messages from the PC to the transceiver
		if (pcMessage.command != CAT_NONE)
		{
			m_txrSerial.write(pcMessage.raw);
		}

		// Pass messages from the transceiver to the PC
		if (txrMessage.command != CAT_NONE)
		{
			Serial.write(txrMessage.raw);
		}

		// Check for a start command
		if (txrTunerMessage.command == CAT_FREQUENCY_A || digitalRead(JUMP_PIN) == LOW)
		{
			m_state = STATE_TXR_START;
			m_timeout = now + TXR_START_TIMEOUT_MS;

			// Request required data from the transceiver
			m_starting = STARTING_MASK;
			m_txrSerial.write("FT;MD0;MS;PC;");
		}
		break;

	case STATE_TXR_START:
		// Check for responses from the transceiver with our required data
		switch (txrMessage.command)
		{
		case CAT_FUNCTION_TX:
			m_functionTx = txrMessage.value;
			m_starting &= ~STARTING_FUNCTION_TX;
		case CAT_MODE:
			m_mode = txrMessage.value;
			m_starting &= ~STARTING_MODE;
			break;
		case CAT_METER:
			m_meter = txrMessage.value;
			m_starting &= ~STARTING_METER;
			break;
		case CAT_POWER:
			m_power = txrMessage.value;
			m_starting &= ~STARTING_POWER;
			break;
		}

		if (m_starting == 0)
		{
			// Put the transceiver in the appropriate state for tuning
			m_txrSerial.write("FT0;MD03;MS3;PC010;");

			// Assert the start pin
			digitalWrite(TUNER_START_PIN, LOW);
			m_state = STATE_TUNER_START;
			m_timeout = now + TUNER_START_TIMEOUT_MS;
		}
		else if (now > m_timeout)
		{
			// Transceiver did not respond in time
			status(now, STATUS_INTERNAL_ERROR);
			m_state = STATE_IDLE;
		}
		break;

	case STATE_TUNER_START:
		if (digitalRead(TUNER_KEY_PIN) == LOW)
		{
			// Key the transceiver
			digitalWrite(TUNER_START_PIN, HIGH);
			m_txrSerial.write("TX1;");
			m_state = STATE_TUNING;
			m_timeout = now + TUNE_TIMEOUT_MS;
		}
		else if (now > m_timeout)
		{
			// Tuner did not respond in time
			digitalWrite(TUNER_START_PIN, HIGH);
			restoreTransceiver();
			status(now, STATUS_INTERNAL_ERROR);
			m_state = STATE_IDLE;
		}
		break;

	case STATE_TUNING:
		if (digitalRead(TUNER_KEY_PIN) == HIGH)
		{
			// Tuning has finished. If tuning failed then the tuner will briefly reassert the KEY line after a short
			// delay
			m_txrSerial.write("TX0;");
			m_state = STATE_FINISH_WAIT1;
			m_timeout = now + TUNE_FINISH1_TIMEOUT_MS;
		}
		else if (now > m_timeout)
		{
			// Tuning took too long
			m_txrSerial.write("TX0;");
			restoreTransceiver();
			status(now, STATUS_INTERNAL_ERROR);
			m_state = STATE_IDLE;
		}
		break;

	case STATE_FINISH_WAIT1:
		if (digitalRead(TUNER_KEY_PIN) == LOW)
		{
			// Tuning failed. Wait for the KEY line to be deasserted again
			m_state = STATE_FINISH_WAIT2;
			m_timeout = now + TUNE_FINISH2_TIMEOUT_MS;
		}
		else if (now > m_timeout)
		{
			// Tuning succeeded
			restoreTransceiver();
			status(now, STATUS_SUCCESS);
			m_state = STATE_IDLE;
		}
		break;

	case STATE_FINISH_WAIT2:
		if (digitalRead(TUNER_KEY_PIN) == HIGH)
		{
			// Signal the tuning failure
			restoreTransceiver();
			status(now, STATUS_FAIL);
			m_state = STATE_IDLE;
		}
		else if (now > m_timeout)
		{
			// KEY line was not deasserted
			restoreTransceiver();
			status(now, STATUS_INTERNAL_ERROR);
			m_state = STATE_IDLE;
		}
		break;
	}
}

void TunerInterface::doStatus(unsigned long now)
{
	if (m_statusCountdown == 0)
	{
		return;
	}

	unsigned long delta = now - m_statusTime;
	if (delta >= m_statusDuration)
	{
		m_statusHigh = !m_statusHigh;
		digitalWrite(STATUS_PIN, m_statusHigh ? HIGH : LOW);

		m_statusTime = now;
		if (!m_statusHigh)
		{
			--m_statusCountdown;
		}
	}
}
