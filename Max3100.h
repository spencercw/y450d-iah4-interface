#ifndef MAX3100_H
#define MAX3100_H

//! Class for interacting with a Maxim MAX3100 UART.
class Max3100
{
public:
	//! Default constructor.
	Max3100();

	//! Destructor.
	~Max3100();

	//! Performs initialisation functions.
	/**
	 * \param slaveSelect The slave select (SS) pin number.
	 * \return Whether the device was initialised successfully.
	 */
	bool begin(int slaveSelect);

	//! Gets the next byte of received data.
	/**
	 * \return The next data byte, or -1 if not available.
	 */
	int read();

	//! Writes a string.
	void write(const char *str);

private:
	static const int BUFFER_SIZE = 64;

	int m_slaveSelect;

	char m_rxBuffer[BUFFER_SIZE];
	unsigned m_rxBegin;
	unsigned m_rxSize;

	// Disabled operations
	Max3100(const Max3100 &) = delete;
	Max3100 & operator=(const Max3100 &) = delete;
};

#endif
