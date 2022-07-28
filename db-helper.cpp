#include <cstring>
#include <sqlite3.h>

#include "db-helper.h"

/**
 * Establish configured database connection
 */
void *dbconnect(
	const std::string &dbfn
)
{
	void *db = NULL;
	if (!dbfn.empty())
	{
		int lasterr = sqlite3_open_v2(dbfn.c_str(), (sqlite3 **) &db, SQLITE_OPEN_READWRITE, NULL);	// SQLITE_OPEN_READWRITE
		if (lasterr != SQLITE_OK)
			db = NULL;
	}
	return db;
}

const char *CREATE_STATEMENTS[2] = {
	"CREATE TABLE \"logger_raw\"(\"id\" INTEGER PRIMARY KEY, \"raw\" text, devname text, loraaddr text, received text);",
	"CREATE TABLE \"logger_lora\"(\"id\" INTEGER PRIMARY KEY, \"kosa\" integer, \"year\" integer, \"no\" integer, "
		"\"measured\" integer, \"parsed\" integer, \"vcc\" float, \"vbat\" float, \"t\" text, \"tp\" text, \"th\" text, \"raw\" text, "
		"devname text, loraaddr text, received text);"
};

bool createTables(const std::string &dbfn)
{
	sqlite3 *db;
	int lasterr = sqlite3_open_v2(dbfn.c_str(), &db, SQLITE_OPEN_READWRITE  | SQLITE_OPEN_CREATE, NULL);
	if (!db)
		return false;
	for (int i = 0; i < 2; i++) {
		char *errmsg = NULL;
		lasterr = sqlite3_exec(db, CREATE_STATEMENTS[i], NULL, NULL, &errmsg);
		if (lasterr) {
			sqlite3_free(errmsg);
			return false;
		}
	}
	sqlite3_close(db);
	return lasterr == SQLITE_OK;	
}

bool checkDbConnection(
	const std::string &dbfn
)
{
	sqlite3 *r = (sqlite3*) dbconnect(dbfn);
	if (r) {
		sqlite3_close(r);
		return true;
	}
	return false;
}
