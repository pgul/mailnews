#define BATCHSIZE	500*1024
#define RNEWS "/usr/bin/rnews"
#define LOCKDIR "/var/lock"
#ifndef TIMEOUT
#define TIMEOUT (30*60)
#endif
#ifndef MAX_ARTICLES
#define MAX_ARTICLES 4096
#endif
#undef NONETDB
#undef LOGFILE /* dont make log for nntpserv */
