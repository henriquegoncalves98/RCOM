#include "application.h"
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv) {

  if(argc < 6) {
    printf("Invalid number of arguments! \n");
    exit(1);
  }
  else {
    if( (strcmp("/dev/ttyS0", argv[1])!=0) && (strcmp("/dev/ttyS1", argv[1])!=0) ) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }
    else if( ((*argv[2]) != 0) && ((*argv[2]) != 1) ) {
      printf("Invalid mode \n");
      exit(1);
    }
  }


  startApplication(argv[1], (*argv[2]), (*argv[3]), (*argv[4]), (*argv[5]), argv[6]);

  return 0;
}
