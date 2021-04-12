
#include <libe/libe.h>

int main(void)
{
	while (1) {
		os_wdt_reset();
	}
	return 0;
}