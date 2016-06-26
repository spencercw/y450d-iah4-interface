#include <AltSoftSerial.h>
#include <SPI.h>

#include "TunerInterface.h"

static TunerInterface app;

void setup()
{
	app.init();
}

void loop()
{
	app.poll();
}
