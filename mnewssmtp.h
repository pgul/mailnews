#define MAILHUB		"localhost"
#define SMTPPORT	25
//#define MAILHUB2	"uucp.lucky.net"
//#define SMTPPORT2	925
#define SMTP_TIMEOUT	120
#define TRIES		30
#define SLEEPTIME	10

struct mailnewsreq { 
	char* mailhub;
	unsigned short port;
#ifdef MAILHUB2
	char* mailhub2;
	unsigned short port2;
#endif
	char* Iam;
	char** users;
};
extern int opensmtpsession(struct mailnewsreq* req, int newsession);
extern int closesmtpdata(int fd);
extern int closesmtpsession(int fd);
extern int abortsmtpsession(int fd);

extern void gethost(char *buf, int bufsize);

