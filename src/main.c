/**********************************************************
 * File: main.c
 * Created at Thu Jul 15 16:14:17 1999 by pk // aaz@ruxy.org.ru
 * qico main
 * $Id: main.c,v 1.2 2000/07/18 12:56:17 lev Exp $
 **********************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/utsname.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "ftn.h"
#include "ver.h"
#include "qconf.h"
#include "tty.h"
#include "mailer.h"
#include "qipc.h"
#include "globals.h"

#ifdef Q_DEBUG
#define sline log
#endif

#define IP_D 0

char *configname=CONFIG;
subst_t *psubsts;

void usage(char *ex)
{
	printf("%s-%s copyright (c) pavel kurnosoff, 1999 // 2:5030/1061@fidonet\n"
		   "usage: %s [<options>] [<node>] [<files>]\n"
 		   "<node>       must be in ftn-style (i.e. zone:net/node[.point])!\n" 
		   "-h           this help screen\n"
		   "-v           be verbose\n"
		   "-I<config>   override default config\n\n"  
		   "-d           start in daemon (originate) mode\n"
 		   "-a<type>     start in answer mode with <type> session, type can be:\n"
		   "                       auto - autodetect\n"
		   "             **EMSI_INQC816 - EMSI session without init phase\n"
		   "                      tsync - FTS-0001 session (unsuppported)\n"
		   "                     yoohoo - YOOHOO session (unsuppported)\n"
		   "                      binkp - BinkP session (unsuppported)\n"
 		   "-i<host>     start TCP/IP connection to <host> (node must be specified!)\n"
 		   "-q           kill existing daemon\n"
 		   "-R           reread config\n"
		   "-n           compile nodelists\n"
		   "-f           query info about <node>\n"
		   "-p           poll <node>\n"
		   "-r           freq from <node> files <files>\n"
		   "-s[n|c|d|h]  attach files <files> to <node> with specified flavor\n"
		   "             flavors: <n>ormal, <c>rash, <d>irect, <h>old\n"
		   "-k           kill attached files after transmission (for -s)\n"
		   "-x[UuWwIi]   set[UWI]/reset[uwi] <node> state(s)\n"
		   "             <u>ndialable, <i>mmediate, <w>ait\n"
/* 		   "-K           kill all outbound .?lo and .?ut for <node>\n" */
		   "\n", progname, version, ex);
	exit(0);
}

void stopit(int rc)
{
	vidle();qqreset();
	log("exiting with rc=%d", rc);log_done();
#if IP_D	
	if(is_ip) qlerase();
#endif	
	qipc_done();
	exit(rc);
}

void sigerr(int sig)
{
	char *sigs[]={"","HUP","INT","QUIT","ILL","TRAP","IOT","BUS","FPE",
				  "KILL","USR1","SEGV","USR2","PIPE","ALRM","TERM"};
	signal(sig, SIG_DFL);
	bso_done();
	log("got SIG%s signal",sigs[sig]);
#if IP_D	
	if(is_ip) qlerase();
#endif
	if(getpid()==islocked(cfgs(CFG_PIDFILE))) lunlink(ccs);
	log_done();
	tty_close();
	qipc_done();
	switch(sig) {
	case SIGSEGV:
	case SIGFPE:
	case SIGBUS:
		abort();
	default:
		exit(1);
	}
}

void sigchild(int sig)
{
	int rc, wr;
	signal(SIGCHLD, sigchild);
	wr=wait(&rc);
	if(wr<0) log("wait() returned %d (%d)", wr, rc);
	rc=WEXITSTATUS(rc)&S_MASK;
	if(rc==S_OK || rc==S_REDIAL) {
		do_rescan=1;
	}
}

void sighup(int sig)
{
	signal(SIGHUP, sighup);
	log("got SIGHUP, trying to reread configs...");
	killsubsts(&psubsts);
	killconfig();
	if(!readconfig(configname)) {
		log("there was some errors parsing config, exiting");
		stopit(0);
	}
	psubsts=parsesubsts(cfgfasl(CFG_SUBST));
	do_rescan=1;
}


void daemon_mode()
{
	int t_dial=0, t_rescan=0; 
	int rc=1, dable, f, w;
	char *port;
	sts_t sts;
	pid_t chld;
	qitem_t *current=q_queue, *i;

	if(getppid()!=1) {
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		if((rc=fork())>0) 
			exit(0);
		if(rc<0) {
			fprintf(stderr, "can't spawn daemon!\n");
			exit(1);
		}
		setsid();
	}

	if(!log_init(cfgs(CFG_MASTERLOG),NULL)) {
		printf("can't open master log %s!\n", ccs);
		exit(0);
	}
	signal(SIGINT, sigerr);	
	signal(SIGTERM, sigerr);
//	signal(SIGSEGV, sigerr);
	signal(SIGFPE, sigerr);
	signal(SIGCHLD, sigchild);
	signal(SIGHUP, sighup);
	
	if(!lockpid(cfgs(CFG_PIDFILE))) {
		log("another daemon exists or can't create pid file!");
		stopit(1);
	}

	log("%s-%s/%s daemon started",progname,version,osname);
	if(!bso_init(cfgs(CFG_OUTBOUND), cfgal(CFG_ADDRESS)->addr.z)) {
		log("can't init BSO");stopit(1);
	}

	t_rescan=cfgi(CFG_RESCANPERIOD);
	while(1) {
		title("Queue manager [%d]", cfgi(CFG_RESCANPERIOD)-t_rescan);
		if(t_rescan>=cci || do_rescan) {
			do_rescan=0;
			sline("Rescanning outbound...");
			if(!q_rescan(&current)) 
				log("can't rescan outbound %s!", cfgs(CFG_OUTBOUND));
			t_rescan=0;
		}
		sline("Waiting %d...", cfgi(CFG_DIALDELAY)-t_dial);
		if(t_dial>=cci) {
			t_dial=0;
			dable=0;

			port=tty_findport(cfgsl(CFG_PORT),cfgs(CFG_NODIAL));			
			if(!port || !q_queue) dable=1;
			i=q_queue;
#ifdef Q_DEBUG			
			log("dabl");
#endif
			while(!dable && i) {
				f=current->flv;
				w=current->what;
				if(f&Q_ANYWAIT) 
					if(t_exp(current->onhold)) {
						current->onhold=0;
						f&=~Q_ANYWAIT;
					}
				if(falist_find(cfgal(CFG_ADDRESS), &current->addr) ||
				   f&Q_UNDIAL ||
				   !(f&Q_NORM) ||
				   (f&Q_WAITR && !(w&(~T_REQ))) ||
				   (f&Q_WAITX && !(w&(~T_ARCMAIL))) ||
				   (f&Q_WAITA)) {
					current=current->next;
					if(!current) current=q_queue;
					i=i->next;
					continue;
				}
#ifdef Q_DEBUG			
				log("quering");
#endif
				rc=query_nodelist(&current->addr,cfgs(CFG_NLPATH),&rnode);
#ifdef Q_DEBUG			
				log("querynl");
#endif
				switch(rc) {
				case 1:log("can't query nodelist, index error");break;
				case 2:log("can't query nodelist, nodelist error");break;
				case 3:log("index is older than the list, need recompile");break;
				}
				if(!rnode) {
					rnode=calloc(1,sizeof(ninfo_t));
					falist_add(&rnode->addrs, &current->addr);
					rnode->name=strdup("Unknown");
					rnode->phone=strdup("");
				}
				phonetrans(rnode->phone, cfgsl(CFG_PHONETR));
#ifdef Q_DEBUG			
				log("%s %s %s [%d]", ftnaddrtoa(&current->addr),
					rnode?rnode->phone:"$",rnode->haswtime?rnode->wtime:"$",rnode->hidnum);
#endif
				applysubst(rnode, psubsts);
				rnode->tty=strdup(baseport(port));
				if(can_dial(rnode, current->flv&Q_IMM) &&
				   checktimegaps(cfgs(CFG_CANCALL))) {
					dable=1;current->flv|=Q_DIAL;
					chld=fork();
#ifdef Q_DEBUG			
					log("forking %s",ftnaddrtoa(&current->addr));
#endif
					
					if(chld==0) {
						if(!bso_locknode(&current->addr)) exit(S_BUSY);
						log_done();
						if(!log_init(cfgs(CFG_LOG),rnode->tty)) {
							fprintf(stderr, "can't init log %s!",ccs);
						}
						if(rnode->hidnum) {
							title("Calling %s #%d, %s",
								  rnode->name, rnode->hidnum,
								  ftnaddrtoa(&current->addr));
							log("calling %s #%d, %s (%s)", rnode->name, rnode->hidnum,	
								ftnaddrtoa(&current->addr),
								rnode->phone);
						} else {								
							title("Calling %s, %s",
								  rnode->name, ftnaddrtoa(&current->addr));
							log("calling %s, %s (%s)", rnode->name,
								ftnaddrtoa(&current->addr),
								rnode->phone);
						}								
						rc=do_call(&current->addr, rnode->phone,
								   port);
						log_done();
							
						if(!log_init(cfgs(CFG_MASTERLOG),NULL)) {
							fprintf(stderr, "can't init log %s.%s!",
									ccs, port);
						}
							
						if(rc&S_ANYHOLD) {
							log("calls to %s delayed for %d min",
								ftnaddrtoa(&current->addr), cfgi(CFG_WAITHRQ));
							bso_getstatus(&current->addr, &sts);
							if(rc&S_HOLDA) sts.flags|=Q_WAITA;
							if(rc&S_HOLDR) sts.flags|=Q_WAITR;
							if(rc&S_HOLDX) sts.flags|=Q_WAITX;
							sts.htime=t_set(cci*60);
							bso_setstatus(&current->addr, &sts);
						}
						if(rc!=S_BUSY) t_rescan=cfgi(CFG_RESCANPERIOD)-1;
						switch(rc&S_MASK) {
						case S_BUSY: break;
						case S_OK:
							bso_getstatus(&current->addr, &sts);
							sts.try=0;
							bso_setstatus(&current->addr, &sts);
							break;
						case S_UNDIAL:
							bso_getstatus(&current->addr, &sts);
							sts.flags|=Q_UNDIAL;
							bso_setstatus(&current->addr, &sts);
							break;
						case S_REDIAL:
							bso_getstatus(&current->addr, &sts);
							if(++sts.try>=cfgi(CFG_MAX_FAILS)) {
								sts.flags|=Q_UNDIAL;
								log("maximum tries count reached, %s undialable",
									ftnaddrtoa(&current->addr));
							}
							bso_setstatus(&current->addr, &sts);
							break;
						}
						bso_unlocknode(&current->addr);
						vidle();log_done();
/* 						qipc_done(); */
						exit(rc);
					}
					if(chld<0) log("can't fork() caller!");
/* 						if(chld>0) { */
							
/* 							while((wr=wait(&rc))!=-1 && wr!=chld) { */
/* 								log("wait ret %d, %d %d[%s]", wr, rc, */
/* 									errno, strerror(errno)); */
/* 							} */
/* 							rc=WEXITSTATUS(rc); */
/* 							if(wr<0) rc=S_REDIAL; */
/* 						} */
				} else current->flv&=~Q_DIAL;
				nlkill(&rnode);
#ifdef Q_DEBUG
				log("nlkill");
#endif
				current=current->next;
				if(!current) current=q_queue;
				i=i->next;
			} 
		}		
		sleep(1);t_dial++;t_rescan++;
		if(do_rescan) t_dial=0;
	}
}

void killdaemon(int sig)
{
	FILE *f=fopen(cfgs(CFG_PIDFILE), "rt");
	pid_t pid;
	if(!f) {
		fprintf(stderr, "can't open pid file - no daemon killed!\n");
		return;
	}		
	fscanf(f, "%d", &pid);fclose(f);
	if(kill(pid, sig))
		fprintf(stderr, "can't send signal!\n");
	else
		fprintf(stderr, "ok!\n");
}

void getsysinfo()
{
	struct utsname uts;
	char tmp[MAX_STRING];
	if(uname(&uts)) return;
	sprintf(tmp, "%s-%s (%s)", uts.sysname, uts.release, uts.machine);
	osname=strdup(tmp);
}


void answer_mode(int type)
{
	int rc, spd;char *cs;
	struct sockaddr_in sa;int ss=sizeof(sa);

	rnode=calloc(1, sizeof(ninfo_t));
	is_ip=!isatty(0);
#if IP_D	
	sprintf(ip_id, "ip%d", getpid());
#else
	sprintf(ip_id, "ipd");
#endif
	rnode->tty=strdup(is_ip?"tcpip":basename(ttyname(0)));
	rnode->options|=O_INB;
	if(!log_init(cfgs(CFG_LOG),rnode->tty)) {
		printf("can't open log %s!\n", ccs);
		exit(0);
	}
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, sigerr);
	signal(SIGSEGV, sigerr);
	signal(SIGFPE, sigerr);
	
	if(!bso_init(cfgs(CFG_OUTBOUND), cfgal(CFG_ADDRESS)->addr.z)) {
		log("can't init BSO");stopit(1);
	}

	log("answering incoming call");vidle();
	if(is_ip && !getsockname(0,(struct sockaddr *)&sa,&ss)) {
		log("remote is %s", inet_ntoa(sa.sin_addr));
		spd=TCP_SPEED;
	} else {	
		cs=getenv("CONNECT");spd=cs?atoi(cs):0;
		if(cs && spd) {
			log("*** CONNECT %s", cs);
		} else {
			log("*** CONNECT Unknown");spd=300;
		}
	}
	if((cs=getenv("CALLER_ID")) && strcasecmp(cs,"none"))
	   log("caller-id: %s", cs);
	tty_setattr(0);
	tty_nolocal();
	rc=session(0, type, NULL, spd);
	tty_cooked();
	title("Waiting...");
	vidle();
	sline("");
	bso_done();
	stopit(rc);
}	

char *flvs[]={"error", "normal", "hold", "direct", "crash"};

int main(int argc, char *argv[], char *envp[])
{
	int c, daemon=-1, rc,
		flv=F_NORM,
		kfs=0, verb=0, set=0, res=0,
		sesstype=SESSION_AUTO;
	char *hostname=NULL, *str=NULL;
	ftnaddr_t fa;
	slist_t *sl=NULL, *l;
	sts_t sts;

#ifndef FREE_BSD
	setargspace(argv,envp);
#endif
 	setlocale(LC_ALL, "");	 

	while((c=getopt(argc, argv, "hI:da:qni:s:rpz:x:fkR"))!=EOF) {
		switch(c) {
		case 'v':
			verb=1;
			break;
		case 'k':
			kfs=1;
			break;
		case 'x':
			daemon=7;
			str=optarg;
			while(*str) {
				switch(*str) {
				case 'W': set|=Q_WAITA;break;
				case 'I': set|=Q_IMM;break;
				case 'U': set|=Q_UNDIAL;break;
				case 'w': res|=Q_ANYWAIT;break;
				case 'i': res|=Q_IMM;break;
				case 'u': res|=Q_UNDIAL;break;
				default: log("unknown action: %c", *str);exit(0);
				}
				str++;
			}
			break;
		case 's':
			daemon=5;
			switch(toupper(*optarg)) {
			case 'N': flv=F_NORM;break;
			case 'C': flv=F_CRSH;break;
			case 'D': flv=F_DIR;break;
			case 'H': flv=F_HOLD;break;
			default: log("unknown flavour: %c", *optarg);exit(0);
			}
			break;
		case 'p':
			daemon=3;
			break;
		case 'f':
			daemon=8;
			break;
		case 'r':
			daemon=4;
			break;
		case 'i':
			hostname=optarg;
			break;
		case 'h':
			usage(argv[0]);
		case 'I':
			configname=optarg;
			break;
		case 'd':
			daemon=1;
			break;
		case 'a':
			daemon=0;
			sesstype=SESSION_AUTO;
			if(!strncasecmp(optarg, "**emsi", 6)) sesstype=SESSION_EMSI;
			if(!strncasecmp(optarg, "tsync", 5)) sesstype=SESSION_FTS0001;
			if(!strncasecmp(optarg, "yoohoo", 6)) sesstype=SESSION_YOOHOO;
			if(!strncasecmp(optarg, "binkp", 5)) sesstype=SESSION_BINKP;
			break;
		case 'n':
			daemon=2;
			break;
		case 'q':
			daemon=10;
			break;
		case 'R':
			daemon=11;
			break;
		}
	}

	if(!hostname && daemon<0) usage(argv[0]);

	getsysinfo();
	if(!readconfig(configname)) {
		log("there was some errors parsing %s, aborting",
				configname);
		exit(EXC_BADCONFIG);
	}

	switch(daemon) {
	case 10: 
		killdaemon(SIGTERM);
		exit(0);
	case 11:
		killdaemon(SIGHUP);
		exit(0);
	}
	
	psubsts=parsesubsts(cfgfasl(CFG_SUBST));
#ifdef C_DEBUG
	dumpconfig();
	{
		subst_t *s;
		dialine_t *l;
		for(s=psubsts;s;s=s->next) {
			printf("subst for %s [%d]\n", ftnaddrtoa(&s->addr), s->nhids);				
			for(l=s->hiddens;l;l=l->next)
				printf(" * %s,%s,%d\n",l->phone,l->timegaps,l->num);
		}
	}
	printf("...press any key...\n");getchar();
#endif	

	if(daemon==0 || daemon==1) qipc_init(0);

#ifdef STDERRLOG	
	freopen("/usr/src/qico/stderr.out","at",stderr);
	setbuf(stderr, NULL);
#endif
	if(hostname || (daemon>=3 && daemon<=8)) 
		if(!parseftnaddr(argv[optind], &fa, &DEFADDR, 0)) {
			log("%s: can't parse address '%s'!\n", argv[0],
				argv[optind]);
			exit(1);
		}

	if(hostname) {
		is_ip=1;
		rnode=calloc(1,sizeof(ninfo_t));
#if IP_D	
		sprintf(ip_id, "ip%d", getpid());
#else
		sprintf(ip_id, "ipd");
#endif
		rnode->tty="tcpip";
		if(!log_init(cfgs(CFG_LOG),rnode->tty)) {
			log("can't open log %s!\n", ccs);
			exit(1);
		}
		signal(SIGINT, sigerr);
		signal(SIGTERM, sigerr);
		signal(SIGSEGV, sigerr);
		
		if(!bso_init(cfgs(CFG_OUTBOUND), cfgal(CFG_ADDRESS)->addr.z)) {
			log("can't init BSO");stopit(1);
		}
		tcp_call(hostname, &fa);
		
		bso_done();
		stopit(0);
	}

	if(daemon>=3 && daemon<=8) 
		if(!bso_init(cfgs(CFG_OUTBOUND), cfgal(CFG_ADDRESS)->addr.z)) {
			log("%s: can't init bso!\n", argv[0]);
			exit(1);
		}
	
	switch(daemon) {
	case 1:
		daemon_mode();break;
	case 0:
		answer_mode(sesstype);break;
	case 2:
		compile_nodelists();break;
	case 3:
		if(bso_locknode(&fa)) {
			if(verb) log("poll %s\n", ftnaddrtoa(&fa));
			rc=bso_poll(&fa);
			bso_unlocknode(&fa);
		} else
			rc=0;
		if(!rc) {
			log("%s: can't create poll for %s!\n", argv[0],
				ftnaddrtoa(&fa));
		}
		break;
	case 4:
		for(c=optind+1;c<argc;c++) {
			slist_add(&sl, argv[c]);
		}
		if(bso_locknode(&fa)) {
			if(verb) {
				log("requesting ");
				for(l=sl;l;l=l->next) printf("%s ", l->str);
				log("from %s\n", ftnaddrtoa(&fa));
			}
			rc=bso_request(&fa, sl);
			bso_unlocknode(&fa);
		} else
			rc=0;
		slist_kill(&sl);
		if(!rc) {
			log("%s: can't create .req for %s!\n", argv[0],
				ftnaddrtoa(&fa));
		}
		break;
	case 5:
		str=malloc(MAX_PATH);
		for(c=optind+1;c<argc;c++) {
			str[0]=kfs?'^':0;str[1]=0;
			if(argv[c][0]!='/') {
				getcwd(str+kfs, MAX_PATH-1);strcat(str, "/");
			}
			strcat(str, argv[c]);
			slist_add(&sl, str);
		}
		sfree(str);
		if(bso_locknode(&fa)) {
			if(verb) {
				log("attaching ");
				for(l=sl;l;l=l->next) printf("%s ", l->str);
				log("to %s as %s%s\n", ftnaddrtoa(&fa), flvs[flv],
					kfs?" (k/s)":"");
			}
			rc=bso_attach(&fa, flv, sl);
			bso_unlocknode(&fa);
		} else
			rc=0;
		slist_kill(&sl);
		if(!rc) {
			log("%s: can't create .?lo for %s!\n", argv[0],
				ftnaddrtoa(&fa));
		}
		break;
	case 7:
		bso_getstatus(&fa, &sts);
		sts.flags|=set;sts.flags&=~res;
		if(set&Q_WAITA && !(res&Q_ANYWAIT)) sts.htime=t_set(cfgi(CFG_WAITHRQ)*60);
		if(res&Q_UNDIAL) sts.try=0;
		bso_setstatus(&fa, &sts);
		break;
	case 8:
		rc=query_nodelist(&fa,cfgs(CFG_NLPATH),&rnode);
		if(rc) {
			log("%s: nodelist query error!", argv[0]);
			break;
		}
		if(rnode) {
			printf("Address: %s\n"
				   "Station: %s\n"
				   "  Place: %s\n"
				   "  Sysop: %s\n"
				   "  Phone: %s\n"
				   "  Flags: %s\n"
				   "  Speed: %d\n"
				   ,ftnaddrtoa(&fa), rnode->name, rnode->place,
				   rnode->sysop, rnode->phone, rnode->flags, rnode->speed
				);
			{
				char *u, *p;
				time_t tm=time(NULL);
				long tz;
				struct tm *tt=localtime(&tm);
				tz=tt->tm_gmtoff/3600; 
	
				u=rnode->flags;while((p=strsep(&u, ","))) {
					if(!strcmp(p,"CM")) {
						printf(" WkTime: 00:00-24:00\n"); 
						break;
					}
					if(p[0]=='T' && p[3]==0) {
						printf(" WkTime: %02ld:%02d-%02ld:%02d\n",
							   (toupper(p[1])-'A'+tz)%24, 
							   islower(p[1]) ? 30:0, 
							   (toupper(p[2])-'A'+tz)%24, 
							   islower(p[2]) ? 30:0); 
						break;
					}
				}
				nlkill(&rnode);
			}
		} else printf("%s not found in nodelist!\n", ftnaddrtoa(&fa));
		break;
	}
	
	if(daemon>=3 && daemon<=8) bso_done();
	return 0;
}
	
	