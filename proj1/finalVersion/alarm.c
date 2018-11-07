#include "alarm.h"

Alarm *alarm;

static void alarmHandler(int sig)
{
  printf("Failed to receive frame! \n");
  (*alarm).retries++;
  (*alarm).resend = TRUE;
  (*alarm).received = FALSE;
}

void setAlarm() {

}
