#include "Max3100.h"

#include <SPI.h>

static const uint32_t SPI_CLOCK = 4202000;
static const uint8_t SPI_BIT_ORDER = MSBFIRST;
static const uint8_t SPI_MODE = SPI_MODE0;

static const uint16_t TX_FIFO_MASK = 0x4000;
static const uint16_t RX_FIFO_MASK = 0x8000;

Max3100::Max3100():
m_slaveSelect(0),
m_rxBegin(0),
m_rxSize(0)
{
}

Max3100::~Max3100()
{
}

bool Max3100::begin(int slaveSelect)
{
	SPI.begin();

	m_slaveSelect = slaveSelect;
	pinMode(m_slaveSelect, OUTPUT);

	// Configure the device
	digitalWrite(m_slaveSelect, LOW);
	SPI.beginTransaction(SPISettings(SPI_CLOCK, SPI_BIT_ORDER, SPI_MODE));
	uint16_t rx = SPI.transfer16(0xc009);
	SPI.endTransaction();
	digitalWrite(m_slaveSelect, HIGH);

	// Check the returned value. The TX FIFO flag is the only bit that should be set
	return rx == TX_FIFO_MASK;
}

int Max3100::read()
{
	// Read as much as possible from the device without overflowing the buffer
	while (m_rxSize < BUFFER_SIZE)
	{
		digitalWrite(m_slaveSelect, LOW);
		SPI.beginTransaction(SPISettings(SPI_CLOCK, SPI_BIT_ORDER, SPI_MODE));
		uint16_t rx = SPI.transfer16(0x0000);
		SPI.endTransaction();
		digitalWrite(m_slaveSelect, HIGH);

		if ((rx & RX_FIFO_MASK) == 0)
		{
			break;
		}

		unsigned rxEnd = (m_rxBegin + m_rxSize) % BUFFER_SIZE;
		m_rxBuffer[rxEnd] = static_cast<char>(rx);
		++m_rxSize;
	}

	// Return a character from the buffer if available
	if (m_rxSize > 0)
	{
		char ch = m_rxBuffer[m_rxBegin];
		m_rxBegin = (m_rxBegin + 1) % BUFFER_SIZE;
		--m_rxSize;
		return ch;
	}

	return -1;
}

void Max3100::write(const char *str)
{
	for (const char *ch = str; *ch;)
	{
		digitalWrite(m_slaveSelect, LOW);
		SPI.beginTransaction(SPISettings(SPI_CLOCK, SPI_BIT_ORDER, SPI_MODE));
		uint16_t rx = SPI.transfer16(0x8000 + *ch);
		SPI.endTransaction();
		digitalWrite(m_slaveSelect, HIGH);

		// Advance the pointer if the character was written
		if (rx & TX_FIFO_MASK)
		{
			++ch;
		}

		// Save the received character, if any
		if (rx & RX_FIFO_MASK)
		{
			unsigned rxEnd = (m_rxBegin + m_rxSize) % BUFFER_SIZE;
			m_rxBuffer[rxEnd] = static_cast<char>(rx);
			if (m_rxSize == BUFFER_SIZE)
			{
				m_rxBegin = (m_rxBegin + 1) % BUFFER_SIZE;
			}
			else
			{
				++m_rxSize;
			}
		}
	}
}
