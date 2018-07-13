#include "Core.h"

Core core;

void setup() {
  core.setup();
}

void loop() {
  WorkStatus wstatus;

  wstatus = core.updateAll();
  
  switch (wstatus)
  {
    case WorkStatus::Unknown:
      return;
    case WorkStatus::FailedToSensor:    
      return;
    case WorkStatus::FailedToFirebase:
      return;
  }

  
  
  delay(3000);
}
