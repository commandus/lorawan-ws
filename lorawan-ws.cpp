/**
 *
 */

#include <cstdlib>
#include <cstdbool>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "platform.h"

#include <sys/stat.h>

#ifndef _MSC_VER
#include <sys/time.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <sqlite3.h>
#include <microhttpd.h>
#if MHD_VERSION <= 0x00096600
#define MHD_Result int
#endif

#include "lorawan-ws.h"

// static struct WSConfig config;

std::ostream *logstream = NULL;

void setLogFile(std::ostream* value)
{
	if ((value == NULL) && (logstream))
		delete logstream;
	logstream = value;
}

/**
 * Establish configured database connection
 */
static sqlite3 *dbconnect(WSConfig *config)
{
	config->db = NULL;
	if (config && (config->dbfilename) && (strlen(config->dbfilename)))
	{
		config->lasterr = sqlite3_open_v2(config->dbfilename, (sqlite3 **) &config->db, SQLITE_OPEN_READWRITE, NULL);	// SQLITE_OPEN_READWRITE
		if (config->lasterr != SQLITE_OK)
			config->db = NULL;
	}
	return (sqlite3 *) config->db;
}

const char *CREATE_STATEMENTS[2] = {
	"CREATE TABLE \"logger_raw\"(\"id\" INTEGER PRIMARY KEY, \"raw\" text, devname text, loraaddr text, received text);",
	"CREATE TABLE \"logger_lora\"(\"id\" INTEGER PRIMARY KEY, \"kosa\" integer, \"year\" integer, \"no\" integer, "
		"\"measured\" integer, \"parsed\" integer, \"vcc\" float, \"vbat\" float, \"t\" text, \"raw\" text, "
		"devname text, loraaddr text, received text);"
};

bool createTables(WSConfig &config)
{
	sqlite3 *db;
	config.lasterr = sqlite3_open_v2(config.dbfilename, &db, SQLITE_OPEN_READWRITE  | SQLITE_OPEN_CREATE, NULL);
	if (!db)
		return false;
	for (int i = 0; i < 2; i++) {
		char *errmsg = NULL;
		config.lasterr = sqlite3_exec(db, CREATE_STATEMENTS[i], NULL, NULL, &errmsg);
		if (config.lasterr) {
			sqlite3_free(errmsg);
			return false;
		}
	}
	sqlite3_close(db);
	return config.lasterr == SQLITE_OK;	
}

bool checkDbConnection(WSConfig *config)
{
	sqlite3 *r = dbconnect(config);
	if (r) {
		sqlite3_close(r);
		return true;
	}
	return false;
}

#define PATH_COUNT 2

typedef enum
{
	RT_RAW = 0,			//< List of coordinates
	RT_T = 1,			//< List of device and their states
	RT_UNKNOWN = 100	//< file request

} RequestType;


const char *paths[PATH_COUNT] = {
	"/raw",
	"/t"
};

const char *pathSelect[PATH_COUNT] = {
	"SELECT id, raw, devname, loraaddr, received FROM logger_raw "
	"ORDER BY id LIMIT ?1, ?2;",
	"SELECT id, kosa, year, no, measured, parsed, vcc, vbat, t, raw, devname, loraaddr, received FROM logger_lora "
	"ORDER BY id LIMIT ?1, ?2;"
};

#define QUERY_PARAMS_SIZE 15
#define QUERY_PARAMS_MAX 5

static const char* queryParamNames[QUERY_PARAMS_SIZE] = {
	"o",	    "s",		"id",		"start",	"finish",
	"sensor",	"kosa", 	"year",		"name",		"t",
	"vcc",		"vbat",		"devname",	"loraaddr",	"received"
};

#define EOP -1

const int pathParams[PATH_COUNT][QUERY_PARAMS_MAX] = {
	{ 0, 1, EOP, 0, 0 },
	{ 0, 1, EOP, 0, 0 }
};

const static char *MSG404 = "404 not found";
const static char *dateformat = "%FT%T";
const static char *dateformatview = "%F %T";
const static char *CT_HTML = "text/html;charset=UTF-8";
const static char *CT_JSON = "text/javascript;charset=UTF-8";
const static char *CT_KML = "application/vnd.google-earth.kml+xml";
const static char *CT_TILE = "image/png";
const static char *CT_CSS = "text/css";
const static char *CT_TEXT = "text/plain;charset=UTF-8";
const static char *CT_TTF = "font/ttf";
const static char *CT_BIN = "application/octet";

typedef enum {
	START_FETCH_DB_OK = 0,
	START_FETCH_DB_CONNECT_FAILED = 1,
	START_FETCH_DB_PREPARE_FAILED = 2,
	START_FETCH_DB_NO_PARAM = 3,
	START_FETCH_DB_BIND_PARAM = 4
} START_FETCH_DB_RESULT;

const static char *MSG500[5] = {
	"200 OK",
	"Database connection not established",
	"SQL statement preparation failed",
	"Required parameter missed",
	"Binding parameter failed"
};

typedef struct 
{
	RequestType requestType;
} RequestParams;

typedef struct{
	int state;
} OutputState;

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

typedef struct 
{
	RequestParams request;
	sqlite3 *db;		// each request in separate connection
	sqlite3_stmt *stmt;	// SQL statement
	WSConfig *config;
	OutputState state;
} RequestEnv;

static RequestType parseRequestType(const char *url)
{
	int i;
	for (i = 0; i < PATH_COUNT; i++)
	{
		if (strcmp(paths[i], url) == 0)
			return (RequestType) i;
	}
	return RT_UNKNOWN;
}

const char *requestTypeString(RequestType value)
{
	if (value == RT_UNKNOWN)
		return "Unknown";
	if ((value >= 0) && (value < PATH_COUNT))
		return paths[(int) value];
	else
		return "???";
}

void *uri_logger_callback (void *cls, const char *uri)
{
	if (logstream)
		*logstream <<  uri << std::endl;
	return NULL;
}

static int doneFetch(
	RequestEnv *env
)
{
	if (!env->stmt)
		return 0;
	sqlite3_finalize(env->stmt);
	env->stmt = NULL;
	return 0;
}

const char *NULLSTR = "";

static ssize_t file_reader_callback(void *cls, uint64_t pos, char *buf, size_t max)
{
	FILE *file = (FILE *) cls;
	(void) fseek (file, (long) pos, SEEK_SET);
	return fread (buf, 1, max, file);
}

static void free_file_reader_callback(void *cls)
{
	fclose ((FILE *) cls);
}

static const char *mimeTypeByFileExtention(const std::string &filename)
{
	std::string ext = filename.substr(filename.find_last_of(".") + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	if (ext == "html")
		return CT_HTML;
	else
		if (ext == "htm")
			return CT_HTML;
	else
		if (ext == "js")
			return CT_JSON;
	else
		if (ext == "css")
			return CT_CSS;
	else
		if (ext == "txt")
			return CT_TEXT;
	else
		if (ext == "ttf")
			return CT_TTF;
	else
		return CT_BIN;

}

static MHD_Result processFile(struct MHD_Connection *connection, const std::string &filename)
{
	struct MHD_Response *response;
	MHD_Result ret;
	FILE *file;
	struct stat buf;

	const char *fn = filename.c_str();

	if (stat(fn, &buf) == 0)
		file = fopen(fn, "rb");
	else
		file = NULL;
	if (file == NULL) {
		if (logstream)
			*logstream << "E404: " << fn << std::endl;

		response = MHD_create_response_from_buffer(strlen(MSG404), (void *) MSG404, MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response (connection, MHD_HTTP_NOT_FOUND, response);
		MHD_destroy_response (response);
	} else {
		response = MHD_create_response_from_callback(buf.st_size, 32 * 1024,
			&file_reader_callback, file, &free_file_reader_callback);
		if (NULL == response)
		{
			fclose (file);
			return MHD_NO;
		}

		MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mimeTypeByFileExtention(filename));
		ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	return ret;
}

/**
 * Translate Url to the file name
 */
static std::string buildFileName(const char *dirRoot, const char *url)
{
	std::stringstream r;
	r << dirRoot;
	if (url)
	{
		r << url;
		int l = strlen(url);
		if (l && (url[l - 1] == '/'))
			r << "index.html";
	}
	return r.str();
}

static START_FETCH_DB_RESULT startFetchDb(
	struct MHD_Connection *connection,
	RequestEnv *env
)
{
	int r = sqlite3_prepare_v2(env->db,
		pathSelect[env->request.requestType], -1, &env->stmt, NULL);
	if (r) {
		if (logstream)
			*logstream << "Error " << r 
				<< ": " << sqlite3_errmsg(env->db)
				<< " db " << env->config->dbfilename 
				<< " SQL " << pathSelect[env->request.requestType]
				<< std::endl;
		return START_FETCH_DB_PREPARE_FAILED;
	}
	for (int i = 0; i < QUERY_PARAMS_MAX; i++)	{
		int v = pathParams[env->request.requestType][i];
		if (v == EOP)
			break; // no more parameters
		const char *c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, queryParamNames[v]);
		if (!c) {
			if (logstream)
				*logstream << "Binding parameter " 
						<< "#" << i + 1
						<< " " << queryParamNames[v]
						<< " empty" << std::endl;
			sqlite3_finalize(env->stmt);
			return START_FETCH_DB_NO_PARAM;
		} else {	
			r = sqlite3_bind_text(env->stmt, i + 1, c, -1, SQLITE_STATIC);
			// r = sqlite3_bind_int(env->stmt, i + 1, 1);
			if (r) {
				if (logstream)
					*logstream << "Bind parameter error " 
						<< r << ": " << sqlite3_errstr(r)
						<< ", parameter #" << i + 1
						<< " " << queryParamNames[v]
						<< " = " << c << std::endl;
				sqlite3_finalize(env->stmt);
				return START_FETCH_DB_BIND_PARAM;
			}
		}
	}
	return START_FETCH_DB_OK;
}

static size_t result2json(
	char *buf,
	size_t bufSize,
	RequestEnv *env
)
{
	std::stringstream ss;
	int st = env->state.state;
	size_t sz;

	switch (st)
	{
		case 0:
			env->state.state = 1;
			ss << "[{";
			break;
		case 1:
			ss << "}, {";
			break;
		case 2:
			sz = snprintf(buf, bufSize, "}]");
			return sz;
		case 3:
			sz = snprintf(buf, bufSize, "[]");
			return sz;
		default:
			sz = snprintf(buf, bufSize, "]");
			return sz;
	}
	int columns = sqlite3_column_count(env->stmt);
	
	bool isFirst = true;
	for (int c = 0; c < columns; c++) {
		const char *n = sqlite3_column_name(env->stmt, c);
		const unsigned char *v = sqlite3_column_text(env->stmt, c);
		int t = sqlite3_column_type(env->stmt, c);
		if (v == NULL)
			 continue;
		if (isFirst)
			isFirst = false;
		else
			ss << ", ";
		ss << "\"" << n << "\": ";
		if (t == SQLITE_TEXT) 
			ss << "\"" << v << "\"";
		else
			ss << v;
	}
	std::string s = ss.str();
	sz = s.size();
	memmove(buf, s.c_str(), sz > bufSize ? bufSize : sz);
	return sz;
}

static ssize_t chunk_callbackFetchDb(void *cls, uint64_t pos, char *buf, size_t max)
{
	RequestEnv *env = (RequestEnv*) cls;
	if (env->state.state >= 2)
		return MHD_CONTENT_READER_END_OF_STREAM;

	int r = sqlite3_step(env->stmt);
	if (r != SQLITE_ROW) {
		env->state.state = env->state.state == 0 ? 3 : 2;
		return result2json(buf, max, env);
	}
	ssize_t sz = result2json(buf, max, env);
	return sz;
}

static void chunk_done_callback(void *cls)
{
	RequestEnv *e = (RequestEnv*) cls;
	if (e != NULL)
	{
		doneFetch(e);
		free(e);
	}
}

static MHD_Result request_callback(
	void *cls,			// struct WSConfig*
	struct MHD_Connection *connection,
	const char *url,
	const char *method,
	const char *version,
	const char *upload_data,
	size_t *upload_data_size,
	void **ptr
)
{
	static int aptr;
	struct MHD_Response *response;
	MHD_Result ret;

	if (&aptr != *ptr) {
		// do never respond on first call
		*ptr = &aptr;
		return MHD_YES;
	}

	if (strcmp(method, "GET") != 0)
		return MHD_NO;              // unexpected method
	*ptr = NULL;					// reset when done

	RequestEnv *requestenv = (RequestEnv *) malloc(sizeof(RequestEnv));
	requestenv->state.state = 0;
	requestenv->config = (WSConfig*) cls;
	requestenv->db = (sqlite3 *) requestenv->config->db;
	requestenv->request.requestType = parseRequestType(url);

	char *buf;
	if (requestenv->request.requestType == RT_UNKNOWN) {
			std::string fn = buildFileName(requestenv->config->dirRoot, url);
			return processFile(connection, fn);
	}

	int r = (int) startFetchDb(connection, requestenv);
	char *msg;
	int hc;
	if (r) {
		hc = MHD_HTTP_INTERNAL_SERVER_ERROR;
		response = MHD_create_response_from_buffer(strlen(MSG500[r]), (void *) MSG500[r], MHD_RESPMEM_PERSISTENT);
	} else {
		hc = MHD_HTTP_OK;
		response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024, &chunk_callbackFetchDb, requestenv, &chunk_done_callback);
	}
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CT_JSON);
	ret = MHD_queue_response(connection, hc, response);
	MHD_destroy_response(response);
	return ret;
}

bool startWS(
	unsigned int threadCount,
	unsigned int connectionLimit,
	unsigned int flags,
	WSConfig &config
) {
	dbconnect(&config);
	if (!config.db)
		return START_FETCH_DB_CONNECT_FAILED;

	struct MHD_Daemon *d = MHD_start_daemon(
		flags, config.port, NULL, NULL, 
		&request_callback, &config,
		MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
                MHD_OPTION_THREAD_POOL_SIZE, threadCount,
                MHD_OPTION_URI_LOG_CALLBACK, &uri_logger_callback, NULL,
                MHD_OPTION_CONNECTION_LIMIT, connectionLimit,
                MHD_OPTION_END
	);
	config.descriptor = (void *) d;
	return config.descriptor != NULL;
}

void doneWS(
	WSConfig &config
) {
	if (config.descriptor)
		MHD_stop_daemon((struct MHD_Daemon *) config.descriptor);
	config.descriptor = NULL;
	setLogFile(NULL);
	sqlite3_close((sqlite3 *) config.db);
	config.db = NULL;
}
