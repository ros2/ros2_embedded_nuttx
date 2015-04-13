#include "os.h"
#include <unistd.h>
void os_printf(const char* fmt, ...)
{

}

void os_sleep(unsigned int period)
{
	usleep(period);
}