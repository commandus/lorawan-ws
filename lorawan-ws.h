/*
 * @file lorawan-ws.h
 */

#ifndef LORAWANWS_H_
#define LORAWANWS_H_

#include <iostream>
#include <fstream>

typedef struct 
{
	// sqlite3
	const char *dbfilename;
	// listener port
	int port;
	// last error code
	int lasterr;
	// html root
	const char* dirRoot;
	// log verbosity
	int verbosity;
	// web server descriptor
	void *descriptor;
	// sqlite3 descriptor
	void *db;
} WSConfig;

void setLogFile(std::ostream* value);

/**
 * @param threadCount threads count, e.g. 2
 * @param connectionLimit mex connection limit, e.g. 1000
 * @param flags e.g. MHD_SUPPRESS_DATE_NO_CLOCK | MHD_USE_DEBUG | MHD_USE_SELECT_INTERNALLY
 */ 
bool startWS(
	unsigned int threadCount,
	unsigned int connectionLimit,
	unsigned int flags,
	WSConfig &config
);

void doneWS(
	WSConfig &config
);

bool checkDbConnection(WSConfig *config);

bool createTables(WSConfig &config);

#endif
