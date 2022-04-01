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

#include "sqlite/sqlite3.h"

#include <sys/stat.h>

#ifndef _MSC_VER
#include <sys/time.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "lorawan-ws.h"

// static struct wsConfig config;

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
static sqlite3 *dbconnect(wsConfig *config)
{
	sqlite3 *r = NULL;
	if (config && (config->dbfilename) && (strlen(config->dbfilename)))
	{
		config->lasterr = sqlite3_open(config->dbfilename, &r);
		if (config->lasterr != SQLITE_OK)
			r = NULL;
	}
	return r;
}

bool checkDbConnection(wsConfig *config)
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
	RT_UNKNOWN = 100		//< file request

} RequestType;


const char *paths[PATH_COUNT] = {
	"/raw",
	"/t"
};

const char *pathSelect[PATH_COUNT] = {
	"SELECT id, raw, devname, loraaddr, received FROM logger_raw "
	"ORDER BY id LIMIT ?1, ?2",
	"SELECT id, kosa, year, no, measured, parsed, vcc, vbat, t, raw, devname, loraaddr, received FROM raw "
	"ORDER BY id LIMIT ?1, ?2"
};

		

#define QUERY_PARAMS_SIZE 15
#define QUERY_PARAMS_MAX 5

static const char* queryParamNames[QUERY_PARAMS_SIZE] = {
	"o",	    "s",		"id",		"start",	"finish",
	"sensor",	"kosa", 	"year",		"name",		"t",
	"vcc",		"vbat",		"devname",	"loraaddr",	"received"
};

const int pathParams[PATH_COUNT][QUERY_PARAMS_MAX] = {
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 }
};

const static char *MSG404 = "404 not found";
const static char *MSG500 = "500 Internal error";
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

const static char *FMT_URL_JSON_HISTORY = "http://iridium.ysn.ru/m.php?get=history&u=%s&p=%s&fmt=2&start=%lld";

typedef struct RequestParams
{
	RequestType requesttype;
	const char *params[QUERY_PARAMS_SIZE];
};

typedef struct OutputState
{
	int state;
	int buffersize;
	void *buffer;
};

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

typedef RequestEnv
{
	sqlite3 *db;		// each request in separate connection
	sqlite3_stmt *stmt;	// SQL statement
	struct wsConfig *config;
	OutputState state;
};

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

static int doneFetch
(
	RequestEnv *env
)
{
	if (!env->stmt)
		return 0;
	sqlite3_finalize(env->stmt);
	env->stmt = NULL;
	if (!env->db)
		return 0;
	sqlite3_close(env->db);
	env->db = NULL;
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

static int processFile(struct MHD_Connection *connection, const std::string &filename)
{
	struct MHD_Response *response;
	int ret;
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

static int processDb(struct MHD_Connection *connection, const std::string &filename)
{
	struct MHD_Response *response;
	int ret;
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

static int startFetchDb
(
	struct MHD_Connection *connection,
	RequestEnv *env
)
{
	env->db = dbconnect(env->config);
	if (env->db == NULL)
		return 1;

	int r = sqlite3_prepare_v2(env->db,
		pathSelect[env->request.requesttype], -1, &env->stmt, NULL);
	if (r)
		return 2;
	for (int i = 0; i < QUERY_PARAMS_MAX; i++)	{
		int v = pathParams[env->request.requesttype][i];
		if (!v)
			break; // no more parameters
		const char *c = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, queryParamNames[v]);
		if (!c)
			r = 1;
		else	
			r = sqlite3_bind_text(env->stmt, i + 1, c, -1, SQLITE_STATIC);
		if (r)
			break;
	}
	if (r) {
		sqlite3_close(env->db);
		env->db = NULL;
		return 3;
	}
	return 0;
}

static size_t result2json(
	char *buf,
	size_t bufSize,
	RequestEnv *env
)
{
	std::stringstream ss;
	int columns = sqlite3_column_count(env->stmt);

	int st = env->state.state;
	size_t sz;

	switch (st)
	{
		case 0:
			ss << "[{";
			break;
		case 1:
			ss << ", {";
			break;
		default:
			sz = snprintf(buf, bufSize, "]");
			return sz;
	}
	
	bool isFirst = true;
	for (int c = 0; c < columns; c++) {
		const char *n = sqlite3_column_name(env->stmt, c);
		const unsigned char *v = sqlite3_column_text(env->stmt, c);
		if (isFirst)
			isFirst = false;
		else
			ss << ", ";
		ss << "\"" << n << "\": " 
			<< "\"" << v << "\"";
	}
	ss << "}";
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

	if (r != SQLITE_ROW)
	{
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

static int request_callback
(
	void *cls,			// struct wsConfig*
	struct MHD_Connection *connection,
	const char *url,
	const char *method,
	const char *version,
	const char *upload_data,
	size_t *upload_data_size,
	void **ptr)
{
	static int aptr;
	struct MHD_Response *response;
	int ret;

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
	requestenv->config = (wsConfig*) cls;

	requestenv->requesttype = parseRequestType(url);

	char *buf;
	if (requestenv->request.requesttype == RT_UNKNOWN) {
			std::string fn = buildFileName(requestenv->config->dirRoot, url);
			return processFile(connection, fn);
	}

	int r = startFetchDb(connection, requestenv);
	char *msg;
	if (r)
		response = MHD_create_response_from_buffer(strlen(MSG500), (void *) MSG500, MHD_RESPMEM_PERSISTENT);
	else
		response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 1024, &chunk_callbackFetchDb, requestenv, &chunk_done_callback);
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CT_JSON);
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}


bool startWS(
	unsigned int threadCount,
	unsigned int connectionLimit,
	unsigned int flags,
	wsConfig &config
) {
	(struct MHD_Daemon *) config.descriptor = MHD_start_daemon(
		flags, config.port, NULL, NULL, &request_callback, &config,
		MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
                MHD_OPTION_THREAD_POOL_SIZE, threadCount,
                MHD_OPTION_URI_LOG_CALLBACK, &uri_logger_callback, NULL,
                MHD_OPTION_CONNECTION_LIMIT, connectionLimit,
                MHD_OPTION_END
	);
	return config.descriptor != NULL;
}

void* doneWS(
	wsConfig &config
) {
	if (config.descriptor)
		MHD_stop_daemon((struct MHD_Daemon *) config.descriptor);
	config.descriptor = NULL;
	setLogFile(NULL);
}
