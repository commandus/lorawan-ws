#ifndef DB_HELPER_H_
#define DB_HELPER_H_	1

#include <string>

#include "lorawan-ws.h"

/**
 * Establish configured database connection
 * 	dbconnect(&config);
	if (!config.db)
		return START_FETCH_DB_CONNECT_FAILED;
	sqlite3_close((sqlite3 *) config.db);
	config.db = NULL;

 */
void *dbconnect(const std::string &dbfn);

bool checkDbConnection(const std::string &dbfn);

bool createTables(const std::string &dbfn);

#endif
