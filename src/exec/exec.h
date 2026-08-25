#ifndef EXEC_EXEC_H
#define EXEC_EXEC_H

#include "exec/types.h"

struct Library {
    const char *name;
};

struct Port {
    int unused;
};

struct IOStdReq {
    int unused;
};

void *OpenLibrary(const char *name,long version);
void CloseLibrary(void *library);
struct IOStdReq *CreateStdIO(struct Port *port);
struct Port *CreatePort(const char *name,long pri);

#endif
