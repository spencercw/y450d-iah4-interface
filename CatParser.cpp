#include "CatParser.h"

#include <string.h>

static const unsigned long MAX_TIME_DELTA_MS = 10;

CatParser::CatParser():
m_state(STATE_READY),
m_lastTime(0),
m_command(0),
m_value(0),
m_raw(),
m_length(0)
{
}

CatParser::~CatParser()
{
}

CatMessage CatParser::process(char ch, unsigned long timeMs)
{
	CatMessage message = {};

	// If the time since the last character is too great then reset the state. This reduces interference from garbage
	// data
	unsigned long delta = timeMs - m_lastTime;
	if (delta > MAX_TIME_DELTA_MS)
	{
		reset();
	}
	m_lastTime = timeMs;

	// Append the character to the raw message buffer
	if (ch == '\0' || m_length == CatMessage::MAX_MSG_SIZE - 1)
	{
		reset();
		m_state = STATE_READY;
		return message;
	}
	m_raw[m_length++] = ch;

	// Handle the character
	switch (m_state)
	{
	case STATE_READY:
		if (ch >= 'A' && ch <= 'Z')
		{
			m_command = ch << 8;
			m_state = STATE_COMMAND;
		}
		break;

	case STATE_COMMAND:
		if (ch >= 'A' && ch <= 'Z')
		{
			m_command |= ch;
			switch (m_command)
			{
			case CAT_FREQUENCY_A:
			case CAT_FUNCTION_TX:
			case CAT_METER:
			case CAT_POWER:
				m_state = STATE_SIMPLE;
				break;
			case CAT_MODE:
				m_state = STATE_MODE1;
				break;
			default:
				m_state = STATE_UNKNOWN_COMMAND;
			}
		}
		else
		{
			reset();
		}
		break;

	case STATE_UNKNOWN_COMMAND:
		if (ch == ';')
		{
			message.command = static_cast<CatCommand>(m_command);
			memcpy(message.raw, m_raw, sizeof(m_raw));
			reset();
		}
		else if (ch < ' ' || ch > '~')
		{
			reset();
		}
		break;

	case STATE_SIMPLE:
		if (ch >= '0' && ch <= '9')
		{
			m_value = m_value * 10 + (ch - '0');
		}
		else if (ch == ';')
		{
			message.command = static_cast<CatCommand>(m_command);
			message.value = m_value;
			memcpy(message.raw, m_raw, sizeof(m_raw));
			reset();
		}
		else
		{
			reset();
		}
		break;

	case STATE_MODE1:
		if (ch == '0')
		{
			m_state = STATE_MODE2;
		}
		else
		{
			reset();
		}
		break;

	case STATE_MODE2:
		if (ch >= '0' && ch <= '9')
		{
			m_value = ch - '0';
			m_state = STATE_MODE3;
		}
		else if (ch >= 'A' && ch <= 'F')
		{
			m_value = ch - 'A' + 10;
			m_state = STATE_MODE3;
		}
		else if (ch == ';')
		{
			message.command = static_cast<CatCommand>(m_command);
			memcpy(message.raw, m_raw, sizeof(m_raw));
			reset();
		}
		else
		{
			reset();
		}
		break;

	case STATE_MODE3:
		if (ch == ';')
		{
			message.command = static_cast<CatCommand>(m_command);
			message.value = m_value;
			memcpy(message.raw, m_raw, sizeof(m_raw));
			reset();
		}
		else
		{
			reset();
		}
		break;
	}

	return message;
}

void CatParser::reset()
{
	m_state = STATE_READY;
	m_command = 0;
	m_value = 0;
	memset(m_raw, 0, sizeof(m_raw));
	m_length = 0;
}
