#ifndef SVSTATUS_H
#define SVSTATUS_H

enum svstatus {
  svstatus_stopped,
  svstatus_starting,
  svstatus_started,
  svstatus_running,
  svstatus_stopping,
  svstatus_failed,
  svstatus_orphanage,
};

#endif
