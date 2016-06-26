#ifndef CAT_PARSER_H
#define CAT_PARSER_H

enum CatCommand
{
	CAT_NONE = 0,
	CAT_FREQUENCY_A = 0x4641,
	CAT_FUNCTION_TX = 0x4654,
	CAT_MODE = 0x4d44,
	CAT_METER = 0x4d53,
	CAT_POWER = 0x5043
};

struct CatMessage
{
	static const int MAX_MSG_SIZE = 32;

	CatCommand command;
	unsigned long value;
	char raw[MAX_MSG_SIZE];
};

//! Class for parsing CAT messages from the Yaesu FT-450D.
class CatParser
{
public:
	//! Default constructor.
	CatParser();

	//! Destructor.
	~CatParser();

	//! Processes the given character.
	/**
	 * \param ch The character from the stream.
	 * \param timeMs The time the character was received in milliseconds. A gap in the stream of characters will reset
	 * the parser state to reduce interference from garbage characters.
	 * \return The received message. The command will be set to CAT_NONE if no complete message was received.
	 */
	CatMessage process(char ch, unsigned long timeMs);

private:
	enum State
	{
		STATE_READY,
		STATE_COMMAND,
		STATE_UNKNOWN_COMMAND,
		STATE_SIMPLE,
		STATE_MODE1,
		STATE_MODE2,
		STATE_MODE3
	};

	State m_state;
	unsigned long m_lastTime;
	int m_command;
	unsigned long m_value;

	char m_raw[CatMessage::MAX_MSG_SIZE];
	int m_length;

	void reset();

	// Disabled operations
	CatParser(const CatParser &) = delete;
	CatParser & operator=(const CatParser &) = delete;
};

#endif
