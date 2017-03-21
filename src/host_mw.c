/*
 * host_mw.c
 */

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <ctype.h>
#include <poll.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>

#include <util.h>

#include "defc.h"

extern Engine_reg engine;


static int socket_fd = -1;
static int stdin_fd = -1;
static int stdout_fd = -1;
static pid_t child_pid = -1;
static int mode = 2;
static int busy = 0;
static int dtr = 1;
static int pending_connect = -1;


struct search {
	word16 address;
	char *string;
	int length;
	int state;
	struct search *next;
};


struct search *search_head = NULL;


struct url {
	int schema;
	char *user;
	char *password;
	char *host;
	char *port;
	int port_number;

};


static int parse_url(char *s, struct url *url) {
	memset(url, 0, sizeof(struct url));
	if (!strncmp(s, "telnet://", 9)) {
		s += 9;
		url->schema = url->port_number = 23;
	} else if (!strncmp(s, "ssh://", 6)) {
		s += 6;
		url->schema = url->port_number = 22;
	} else {
		return 0;
	}


	// telnet://<user>:<password>@<host>:<port>/

	char c = 0;

	char *cp = strchr(s, '@');
	if (cp != NULL) {

		url->user = s;

		for(;;++s) {
			char c = *s;
			if ( c == '@' || c== ':') break; 
		}
		*s++ = 0;

		if (c == ':') {
			url->password = s;
			for(;;++s) {
				char c = *s;
				if (c == '@') break;
			}
			*s++ = 0;
		}
	}

	url->host = s;
	for(;;++s) {
		c = *s;
		if (c == 0 || c == ':') break;
	}
	*s++ = 0;
	if (c == ':') {
		// remainder is the port.

		char *tmp;
		unsigned long l = strtoul(s, &tmp, 10);
		if (tmp != s && *s == 0 && l <= 0xffff) {
			url->port_number = l;
			url->port = s;
		}
	}

	return url->schema;
}




// OMM

#define MSG_INIT 0       // initialize module
#define MSG_QUIT 1       // quit (shutdown) module
#define MSG_AMPR 2       // execute ampersand service routine
#define MSG_USER 3       // user (non-applesoft) service request
#define MSG_REL1 4       // alert module before relocation
#define MSG_REL2 5       // alert module after relocation
#define MSG_KILL 6       // death notice (sent before actual death)
#define MSG_DIED 7       // obituary (sent after death)
#define MSG_BORN 8       // birth notice
#define MSG_IDLE 9       // idle event for module
#define MSG_INFO 10      // get modules info string
#define MSG_NEXT 11


// Console Tool

#define CT_ID	7463 // Console Tool ("ct") ID

#define CTOpen 0
#define CTClose 1
#define CTReset 2
#define CTControl 3
#define CTStatus 4
#define CTGetXY 5
#define CTWriteChar 6
#define CTWriteBuffer 7
#define CTTestChar 8
#define CTReadChar 9
#define CTFlushInQ 10
#define CTShowCursor 11
#define CTHideCursor 12
#define CTSetBellAttr 13
#define CTSetTermcap 14
#define CTGotoXY 15

// ModemTool

#define MT_ID 0x746d //Modem Tool ('mt') ID

#define InitModem 0
#define ModemExit 1
#define IsOnline 2
#define HasMNP 3
#define DialNumber 4
#define SetBusy 5
#define HandleConnect 6
#define AnswerLine 7
#define HangUp 8
#define IsRinging 9
#define SetMNP 10
#define OrigAnsLine 11
#define ResetModem 12
#define SetSpeaker 13
#define GetMode 14
#define ModemType 15
#define ConnectSpeed 16
#define SetModem 17
#define SetModemSpeed 18
// 19? 
#define GetModemSpeed 20

// PortTool

#define PT_ID	 0x7470 //Port Tool ("pt") ID

#define SerOpen 0
#define SerClose 1
#define SerReset 2
#define SerSendBreak 3
#define SerSetDTR 4
#define SerClearDTR 5
#define SerSetPortBits 6
#define SerSetSpeed 7
#define SerGetSpeed 8
#define SerGetDCD 9
#define SerWriteChar 10
#define SerWriteBuffer 11
#define SerReadChar 12
#define SerReadBuffer 13
#define SerFlushInQ 14
#define SerGetInQ 15
#define SerGetInBuf 16
#define SerSetInBuf 17
#define SerSetFlow 18
#define SerAddCompVec 19
#define SerDelCompVec 20
#define SerClearCompVec 21
#define SerAddSearch 22
#define SerDelSearch 23
#define SerClearSearch 26
#define SerGetSearch 24
#define SerShowSearch 25
#define SerGetTimedByte 27
#define SerOutBuffering 28
#define SerSetDCD 29



// &FN fnMode results and &PICKUP selectors
#define modeAns 	0
#define modeOrig	1
#define modeQuiet	2

// & FN fnModemType results
#define noModem 	0
#define intModem	1
#define extModem	2

// & WAIT FOR CARRIER results
#define wfcConnect	0	
#define wfcCancelled	1
#define wfcNoConnect	2
#define wfcBusy 	3
#define wfcNoDialTone	4
#define wfcNoAnswer	5
#define wfcVoice	6


#define prmtbl 0xe0


#define SEC() engine.psr |= 1
#define CLC() engine.psr &= ~1


#define SEV() engine.psr |= 0x40
#define CLV() engine.psr &= ~0x40


/*
 * modemworks in a nutshell:
 *
 * out:
 * &call "number" : &wait for carrier : ...
 *
 * in:
 * &fn fnRing, X : ... : &pickup : &wait for carrier
 * & wait for call : &pickup : &wait for carrier
 *
 * &call -> DialNumber
 * & wait for carrier -> HandleConnect
 * &pickup -> AnswerLine / OrigAnsLine / SetBusy
 * & wait for call -> while (!IsRinging() && !CtTestChar())  ;
 * &fn fnRing -> IsRinging
 */

static int server_socket(int port, int backlog) {
	struct sockaddr_in sa;

	int ok;
	int flags;
	int opt = 1;

	int fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0) { warn("socket"); return -1; }

	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(fd, F_GETFD);
	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

	ok = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ok < 0) { warn("setsockopt(..., SO_REUSEADDR, ...)"); close(fd); return -1; }

	memset(&sa,0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	ok = bind(fd, (struct sockaddr *) &sa, sizeof(sa));
	if (ok < 0) { warn("bind"); close(fd); return -1; }

	ok = listen(fd, backlog);
	if (ok < 0) { warn("listen"); close(fd); return -1; }

	return fd;
}

static void reset() {
	if (stdin_fd >= 0) close(stdin_fd);
	if (stdout_fd >= 0 && stdout_fd != stdin_fd) close(stdout_fd);
	if (socket_fd >= 0) close(socket_fd);
	stdin_fd = stdout_fd = socket_fd = -1;
	busy = 0;
	mode = 0;
}

static void hangup() {
	if (stdin_fd >= 0) close(stdin_fd);
	if (stdout_fd >= 0 && stdout_fd != stdin_fd) close(stdout_fd);
	stdin_fd = stdout_fd = -1;


	// maybe these should be collected ina a SIGCHLD signal trap instead?
	if (child_pid >= 0) {
		for(;;) {
			int ok;
			int st;
			ok = waitpid(child_pid, &st, 0);
			if (ok == -1) {
				if (errno == EINTR) continue;
				warn("waitpid");
			}
			break;
		}
		child_pid = -1;
	}
	busy = 0;
	mode = 0;	
}

// like forkpty.

static int fork_pipe(int *stdin_fd, int *stdout_fd) {
	int pipe_in[2];
	int pipe_out[2];

	if (pipe(pipe_in) < 0) {
		return -1;
	}

	if (pipe(pipe_out) < 0) {
		close(pipe_in[0]);
		close(pipe_in[1]);
		return -1;
	}

	// write to 1, read from 0.

	int pid = fork();
	if (pid < 0) {
		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
		return -1;
	}



	if (pid == 0) {
		// child.

		// stdin_fd, stdout_fd are not updated.

		dup2(pipe_in[0], STDIN_FILENO);
		dup2(pipe_out[1], STDOUT_FILENO);
		dup2(pipe_out[1], STDERR_FILENO);

		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
	}
	else {
		// parent

		*stdin_fd = pipe_out[0];
		*stdout_fd = pipe_in[1];

		close(pipe_in[0]);
		close(pipe_out[1]);
	}

	return pid;
}

static void mt() {

	word16 a = engine.acc & 0xff;
	word16 x = engine.xreg;
	word16 y = engine.yreg;


	if (a == MSG_USER) {

		if (y != IsRinging) fprintf(stderr, "MT User Call: %d\n", y);
		switch (y) {
			case InitModem: {
				// 1 = fail, 0 = success. (mwtr is backwards)
				hangup();
				engine.acc = 0;

				break;
			}
			case ModemExit: {
				hangup();

				break;
			}
			case IsOnline: {
				engine.acc = 0; // offline
				if (stdin_fd >= 0) engine.acc = 1;

				// poll and check POLLHUP?
				// todo -- check if child pid still active.
				break;
			}
			case HasMNP: {
				// mnp error correction. sure.
				engine.acc = 1;
				break;
			}
			case DialNumber: {

				unsigned length = get_memory_c(prmtbl, 0);
				word16 address = get_memory16_c(prmtbl+1, 0);
				unsigned tt = get_memory_c(prmtbl+3, 0);

				char *str = malloc(length + 1);
				for (unsigned i = 0; i < length; ++i) {
					str[i] = get_memory_c(address + i, 0);
				}
				str[length] = 0;
				fprintf(stderr, "Dial: %s\n", str);

				mode = modeOrig; // ?

				pending_connect = wfcCancelled; // so it doesn't try repeatedly.

				// ! uses pipes instead of a pty.. any point?
				if (str[0] == '!') {

					int pid = fork_pipe(&stdin_fd, &stdout_fd);

					if (pid < 0) {
						warn("fork_pipe");
						free(str);
						return;
					}
					if (pid == 0) {
						// child.

						execl("/bin/sh","sh", "-c", str+1, NULL);
						warn("execl");
						_exit(255);
					}

					// parent
					fcntl(stdin_fd, F_SETFL, fcntl(stdin_fd, F_GETFL) | O_NONBLOCK);
					fcntl(stdout_fd, F_SETFL, fcntl(stdout_fd, F_GETFL) | O_NONBLOCK);

					child_pid = pid;
					pending_connect = wfcConnect;

					free(str);
					break;
				}


				struct url url;
				int url_type = parse_url(str, &url);

				if (url_type) {
				 	if (!url.host || ! *url.host) {
						free(str);
						break;
					}
					fprintf(stderr, "%d - %s : %s @ %s : %s\n",
						url.schema,
						url.user ? url.user : "",
						url.password ? url.password : "",
						url.host ? url.host : "",
						url.port ? url.port : ""
					);
				}
				// default

				{
					char *argv[10];
					char *program;

					switch(url_type) {
						case 23: { // telnet
							int i = 0;
							program = "/usr/bin/telnet";
							argv[i++] = "telnet";
							argv[i++] = "-E"; // disable ^] escape.

							if (url.user) {
								argv[i++] = "-l";
								argv[i++] = url.user;
							}

							argv[i++] = url.host;
							if (url.port) argv[i++] = url.port;
							argv[i++] = NULL;

							break;
						}
						case 22: { // telnet
							int i = 0;
							program = "/usr/bin/ssh";
							argv[i++] = "ssh";

							argv[i++] = "-e";
							argv[i++] = "none"; // disable escape char.

							if (url.user) {
								argv[i++] = "-l";
								argv[i++] = url.user;
							}

							if (url.port) {
								argv[i++] = "-p";
								argv[i++] = url.port;
							}

							argv[i++] = url.host;

							argv[i++] = NULL;

							break;
						}
						default: {
							program = "/bin/sh";
							argv[0] = "sh";
							argv[1] = "-c";
							argv[2] = str;
							argv[3] = NULL;
							break;
						}
					}
					/* pseudo terminal */
					int fd;
					int pid = forkpty(&fd, NULL, NULL, NULL);
					if (pid < 0) {
						warn("forkpty");
						free(str);
						return;
					}
					if (pid == 0) {
						/* child */
						execv(program, argv);
						warn("execv");
						_exit(255);						
					}

					/* parent */
					fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
					stdin_fd = fd;
					stdout_fd = fd;
					child_pid = pid;
					pending_connect = wfcConnect;

					free(str);
					break;
				}

				free(str);
				break;
			}
			case SetBusy: {
				busy = get_memory_c(prmtbl, 0);
				// 
				break;
			}
			// this handles both inbound and outbound.
			case HandleConnect: {
				word16 seconds = get_memory16_c(prmtbl, 0);


				if (pending_connect >= 0) {
					engine.acc = pending_connect;
					pending_connect = -1;
					break;
				}

				if (socket_fd < 0) {
					engine.acc = wfcNoDialTone; // no dial tone!
					break;
				}


				struct pollfd fds;
				fds.fd = socket_fd;
				fds.events = POLLIN;
				fds.revents = 0;

				int ok = poll(&fds, 1, seconds * 1000);
				if (ok < 1) {
					engine.acc = wfcNoConnect;
					break;
				}

				struct sockaddr_in addr;
				socklen_t length = sizeof(addr);

				int fd = accept(socket_fd, (struct sockaddr *)&addr, &length);
				if (fd < 0) {
					engine.acc = wfcNoConnect; // no connection
					break;
				}
				fprintf(stderr, "connection from: %s\n", inet_ntoa(addr.sin_addr));
				// should inherit non-blocking from socket_fd.
				stdin_fd = stdout_fd = fd;
				engine.acc = wfcConnect; // connection established.
				break;
			}
			case AnswerLine: {
				mode = modeAns;
				break;
			}
			case OrigAnsLine: {
				mode = modeOrig;
				// shouldn't be called. not sure how to handle...
				break;
			}

			case HangUp: {
				hangup();
				break;
			}
			case IsRinging: {
				// c = 0/1.
				CLC();
				if (busy) break;
				if (socket_fd < 0) break;

				struct pollfd fds;
				fds.fd = socket_fd;
				fds.events = POLLIN;
				fds.revents = 0;

				// .25 second check
				int ok = poll(&fds, 1, 250);
				if (ok == 1 && fds.revents == POLLIN) {
					SEC();
				}

				break;
			}
			case SetMNP: {
				unsigned value = get_memory_c(prmtbl, 0);
				break;
			}
			case ResetModem: {

				hangup();
				engine.acc = 0;
				break;
			}
			case SetSpeaker: {
				unsigned value = get_memory_c(prmtbl, 0);
				break;
			}
			case GetMode: {
				// 0 = answer, 1 = originate, 2 = quiet/offhook
				set_memory_c(prmtbl, busy ? modeQuiet : mode, 0);
				break;
			}
			case ModemType: {
				// internal modem!
				set_memory_c(prmtbl, intModem, 0);
				break;	
			}
			case ConnectSpeed: {
				set_memory_c(prmtbl, 15, 0); // 19200!
				break;					
			}
			case SetModem: {
				word16 address = get_memory16_c(prmtbl, 0);
				break;
			}
			case SetModemSpeed: {
				unsigned speed = get_memory_c(prmtbl, 0);
				break;
			}
			case GetModemSpeed: {
				const char speed[] = "19200";
				word16 address = get_memory16_c(prmtbl, 0);
				for (int i = 0; i < sizeof(speed); ++i) {
					set_memory_c(address + i, speed[i], 0);
				}
				break;
			}
			default: {
				fprintf(stderr, "Unsupported MT user call: %d\n", y);
			}

		}
		return;
	}

	switch(a) {
		default: {
			fprintf(stderr, "Unsupported MT system call: %d\n", a);
		}
	}
}

static int get_timed_byte(int ticks) {

	CLC();
	CLV();

	if (stdin_fd < 0) {
		SEV();
		return -1;
	}

	int ok;

	struct pollfd fds;
	fds.fd = stdin_fd;
	fds.events = POLLIN;
	fds.revents = 0;

	ok = poll(&fds, 1, ticks * 1000.0 / 60.0);
	if (ok < 0) {
		warn("poll");
		return -1;
	}
	if (ok == 0) return -1;


	if (fds.revents & POLLIN) {
		byte c;
		ok = read(stdin_fd, &c, 1);
		if (ok == 1) {
			fprintf(stderr, "<- %02x %c\n", c, isprint(c) ? c : '.');
			engine.acc = c;
			SEC();
			return c;
		}

		if (ok < 0) warn("read");
	}

	if (fds.revents & (POLLHUP | POLLERR)) {
		warn("pollerr? %d", fds.revents);
		SEV(); // carrier lost?
		hangup();
		return -1;
	}

	return -1;
}

static void pt() {

	word16 a = engine.acc & 0xff;
	word16 x = engine.xreg;
	word16 y = engine.yreg;

	if (a == MSG_USER) {

		switch(y) {
			case SerReadChar:
			case SerGetDCD:
			case SerWriteChar:
			case SerGetSearch:
			case SerShowSearch:
				break;
			default:
				fprintf(stderr, "PT User Call: %d\n", y);
		}

		switch (y) {
			case SerOpen: {
				unsigned slot = get_memory_c(prmtbl, 0);
				break;
			}
			case SerClose: {
				break;
			}
			case SerReset: {
				break;
			}
			case SerSendBreak: {
				// thought ... if it's a ssh or telnet connection,
				// break could send the escape character (would require doubling it elsehwere)
				// or a signal of some sort?
				if (stdin_fd >= 0)
					tcsendbreak(stdin_fd, 0);
				break;
			}
			case SerSetDTR: {
				dtr = 1;
				break;
			}
			case SerClearDTR: {
				dtr = 0;
				break;
			}
			case SerSetPortBits: {
				unsigned data_stop = get_memory_c(prmtbl, 0);
				unsigned parity = get_memory_c(prmtbl+1,0);
				break;
			}
			case SerSetSpeed: {
				unsigned speed = get_memory_c(prmtbl, 0);
				break;
			}
			case SerGetSpeed: {
				// could call cfgetspeed and remap but why bother.
				engine.acc = 15; // 19200.
				break;
			}
			case SerGetDCD: {
				CLC();
				if (stdin_fd >= 0) SEC();
				break;
			}
			case SerWriteChar: {
				if (stdout_fd < 0) break;

				byte c = get_memory_c(prmtbl, 0);
				int ok = write(stdout_fd, &c, 1);
				if (ok < 0) hangup();

				engine.acc = c; // SX depends on this.

				//fprintf(stderr, "-> %02x %c\n", c, isprint(c) ? c : '.');

				break;
			}
			case SerWriteBuffer: {
				if (stdout_fd < 0) break;

				word16 count = get_memory16_c(prmtbl, 0);
				word16 buffer = get_memory16_c(prmtbl+2, 0);
				fprintf(stderr, "-> %d bytes from $%04x\n", count, buffer);
				if (count) {
					byte *tmp = malloc(count);
					for (unsigned i = 0; i < count; ++i)
						tmp[i] = get_memory_c(buffer+i,0);
					int ok = write(stdout_fd, tmp, count);
					if (ok < 0) hangup();
					free(tmp);
				}

				break;
			}
			case SerReadChar: {
				CLC();
				if (stdin_fd < 0) break;

				byte c;
				int ok;
				ok = read(stdin_fd, &c, 1);
				if (ok == 1) {
					//fprintf(stderr, "%02x %c\n", c, isprint(c) ? c : '.');
					engine.acc = c;
					SEC();
				}
				if (ok == 0) { /* eof */ hangup(); }
				break;
			}
			case SerReadBuffer: {
				word16 count = get_memory16_c(prmtbl, 0);
				word16 buffer = get_memory16_c(prmtbl+2, 0);

				if (stdin_fd < 0) {
					for (unsigned i = 0; i < count; ++i)
						set_memory_c(buffer+i, 0, 0);
					break;
				}

				// n.b. - this is a blocking call.
				if (count) {
					char *tmp = malloc(count);
					memset(tmp, 0, count);
					unsigned offset = 0;
					unsigned remaining = count;

					while (remaining) {

						struct pollfd fds;
						fds.fd = stdin_fd;
						fds.events = POLLIN;
						fds.revents = 0;
						int ok = poll(&fds, 1, 0);
						if (ok < 0) {
							if (errno == EINTR || errno == EAGAIN) continue;
							break;
						}
						if (ok == 0) continue;
						if (ok == 1) {
							if (fds.revents & (POLLHUP | POLLERR)) {
								hangup();
								break;
							}

							ok = read(stdin_fd, tmp + offset, remaining);
							if (ok < 0) {
								if (errno == EINTR || errno == EAGAIN) continue;
							}
							if (ok > 0) {
								remaining -= ok;
								offset += ok;
								continue;
							}
							break; // eof, etc.
						}
					}

					for (unsigned i = 0; i < count; ++i)
						set_memory_c(buffer + i, tmp[i], 0);

					free(tmp);
				}


				break;
			}
			case SerFlushInQ: {
				// flush any buffered input...
				if (stdin_fd >= 0)
					tcflush(stdin_fd, TCIFLUSH);
				break;
			}
			case SerGetInQ: {
				// pending input..
				int size;

				if (stdin_fd < 0) {
					set_memory16_c(prmtbl, 0, 0);
					break;
				}


				int ok = ioctl(stdin_fd, FIONREAD, &size);
				if (ok < 0) size = 0;
				if (size > 0xffff) size = 0xffff;
				set_memory16_c(prmtbl, size, 0);
				break;
			}
			case SerGetInBuf: {
				warnx("SerGetInBuf not supported yet");
				// return the serial input buffer...
				break;
			}
			case SerSetInBuf: {
				warnx("SerSetInBuf not supported yet");
				// set the serial input buffer....
				break;
			}
			case SerSetFlow: {
				unsigned flow = get_memory_c(prmtbl, 0);
				// tcflow? 0x04 is xon/xoff (?)
				break;
			}
			// todo...
			case SerAddCompVec: {
				warnx("SerAddCompVec not supported yet");
				break;
			}
			case SerDelCompVec: {
				warnx("SerDelCompVec not supported yet");
				break;
			}
			case SerClearCompVec: {
				warnx("SerClearCompVec not supported yet");
				break;
			}
			case SerAddSearch: {
				word16 address = get_memory16_c(prmtbl, 0);
				if (!address) break;

				unsigned length = 0;
				while (get_memory_c(address + length, 0)) ++length;

				if (!length) break;

				/* check if already on file... */
				struct search *s = search_head;
				while (s) {
					if (s->address == address) return;
					s = s->next;
				}

				char *str = malloc(length + 1);
				for (unsigned i = 0; i < length; ++i)
					str[i] = toupper(get_memory_c(address + i, 0));
				str[length] = 0;

				s = malloc(sizeof(struct search));
				memset(s, 0, sizeof(struct search));
				s->address = address;
				s->next = search_head;
				s->string = str;
				s->length = length;
				search_head = s;

				fprintf(stderr, "Adding %04x %s to search\n", address, str);

				break;
			}
			case SerDelSearch: {
				word16 address = get_memory16_c(prmtbl, 0);
				if (!address) break;

				struct search *s = search_head;
				struct search *prev = NULL;
				while (s) {
					if (s->address == address) {
						if (prev) prev->next = s->next;
						else search_head = s->next;
						free(s->string);
						free(s);
						break;
					}
					prev = s;
					s = s->next;
				}
				break;
			}
			case SerClearSearch: {
				struct search *s = search_head;
				while (s) {
					struct search *tmp = s->next;
					free(s->string);
					free(s);
					s = tmp;
				}
				search_head = NULL;
				break;
			}
			// show search displays it too...
			/*
			 * address of matched string is returned in prmtbl[0,1]
			 * to support SerShowSearch (which calls the console tool 
			 * to display the character), it also uses the same returns
			 * as SerGetTimedByte (A/C/V)
			 */
			case SerGetSearch: 
			case SerShowSearch: {
				struct search *s = search_head;

				set_memory16_c(prmtbl, 0, 0);
				if (!s) break;


				int c = get_timed_byte(60); // 1 second
				if (c < 0) break;

				c = toupper(c);

				for(s = search_head; s; s = s->next) {
					int state = s->state;

					if (c == s->string[state]) {
						state++;
					} else if (c == s->string[0]) {
						// retry first char if state and failed match.
						// there is probably a Knuthy algorithm but I 
						// doubt the original Serial did that.
						state = 1;
					} else {
						state = 0;
					}
					s->state = state;

					if (state == s->length) {
						s->state = 0;
						break;
					}
				}

				if (s) {
					fprintf(stderr, "SerGetSearch found %04x %s\n", s->address, s->string);
					set_memory16_c(prmtbl, s->address, 0);
					// reset all states back to 0 
					s = search_head;
					for(s = search_head; s; s = s->next)
						s->state = 0;
				}

				break;
			}

			case SerGetTimedByte: {
				unsigned ticks = get_memory_c(prmtbl, 0);
				// tick = 1/60 of a second. = 16.6666 microseconds.

				get_timed_byte(ticks);

				break;
			}
			case SerOutBuffering: {
				unsigned buffering = get_memory_c(prmtbl, 0);
				break;
			}
			case SerSetDCD: {
				unsigned spoof = get_memory_c(prmtbl, 0);
				break;
			}
		}
		return;
	}

	fprintf(stderr, "PT %d\n", a);
	switch(a) {
		case MSG_INIT: {
			reset();
			socket_fd = server_socket(6809, 1);
			break;
		}
		case MSG_QUIT: {
			reset();
			break;
		}
		default: {
			fprintf(stderr, "Unsupported PT system call: %d\n", a);
		}
	}

}

void host_mw(void) {
	/*
	 * x = id
	 * a = omm message
	 * y = user message (for MSG_USER)
	 */

	word16 a = engine.acc & 0xff;
	word16 x = engine.xreg;
	word16 y = engine.yreg;

	switch(x) {
		// should be endian safe...
		case PT_ID: pt(); break;
		case MT_ID: mt(); break;
		default:
			fprintf(stderr, "Unsupported ModemWorks call: (a=%04x, x=%04x, y=%04x)\n", a, x, y);
			break;
	}

}
