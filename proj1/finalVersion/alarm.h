#include <stdio.h>
#define FALSE 				0
#define TRUE 				1


typedef struct {
  int retries;
  int resend;
  int received;
} Alarm;

extern Alarm *alarm;

void setAlarm();
