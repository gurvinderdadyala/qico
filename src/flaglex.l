/**********************************************************
 * File: flaglex.l
 * Created at Thu Jul 15 16:14:46 1999 by pk // aaz@ruxy.org.ru
 * Base version of this file was taken from Eugene Crosser's ifcico 
 * $Id: flaglex.l,v 1.2 2000/07/18 12:50:33 lev Exp $
 **********************************************************/
%{

static char *yyPTR = NULL;
static int yyBUFL;

#ifndef yywrap
#define yywrap() 1
#endif

#ifdef FLEX_SCANNER
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ((yyPTR == NULL) || (*yyPTR == '\0')) \
		result=YY_NULL; \
	else \
	{ \
		yyBUFL=strlen(yyPTR); \
		if (yyBUFL > max_size) yyBUFL=max_size; \
		memcpy(buf,yyPTR,yyBUFL); \
		yyPTR += yyBUFL; \
		result=yyBUFL; \
	}

#else /* this is most probably native lex? */

#undef input
#undef output
#undef unput

#define input() (((yyPTR == NULL) || (*yyPTR == '\0'))?NULL:*yyPTR++)
#define output(c)
#define unput(c) (*--yyPTR = (c))

#endif

int flagfor(char *);
int chkaddr(char*);
int chktime(char*);
int chkdate(char*);
int chkgaps(char*);
int yylval;
%}

Blank		[ \t\n]+
Speed	[Ss][Pp][Ee][Ee][Dd]
Phone	[Pp][Hh][Oo][Nn][Ee]
Time	[Tt][Ii][Mm][Ee]
iTime	[Ii][Tt][Ii][Mm][Ee]
Address	[Aa][Dd][Dd][Rr][Ee][Ss][Ss]
Port    [Pp][Oo][Rr][Tt]
Connect [Cc][Oo][Nn][Nn][Ee][Cc][Tt]
Cid     [Cc][Ii][Dd]
Sun		[Ss][Uu][Nn]
Mon		[Mm][Oo][Nn]
Tue		[Tt][Uu][Ee]
Wed		[Ww][Ee][Dd]
Thu		[Tt][Hh][Uu]
Fri		[Ff][Rr][Ii]
Sat		[Ss][Aa][Tt]
Any		[Aa][Nn][Yy]
Wk		[Ww][Kk]
We		[Ww][Ee]
Jan             [Jj][Aa][Nn]
Feb             [Ff][Ee][Bb]
Mar             [Mm][Aa][Rr]
Apr             [Aa][Pp][Rr]
May             [Mm][Aa][Yy]
Jun             [Jj][Uu][Nn]
Jul             [Jj][Uu][Ll]
Aug             [Aa][Uu][Gg]
Sep             [Se][Ee][Pp]
Oct             [Oo][Cc][Tt]
Nov             [Nn][Oo][Vv]
Dec             [Dd][Ee][Cc]
File			[Ff][Ii][Ll][Ee]
Eq		=(=)?
Ne		!=
Gt		\>
Ge		\>=
Lt		\<
Le		\<=
And		\&
Or		\|
Not		\!
Xor		\^
Lb		\(
Rb		\)
Comma		\,
Asterisk	\*
Dow		({Sun}|{Mon}|{Tue}|{Wed}|{Thu}|{Fri}|{Sat}|{Any}|{Wk}|{We})
Month           ({Jan}|{Feb}|{Mar}|{Apr}|{May}|{Jun}|{Jul}|{Aug}|{Sep}|{Oct}|{Nov}|{Dec}|{Any})
Day		(([0-2]?[0-9])|(3[01]))
iHour		(([0-1][0-9])|(2[0-3]))
Minute		[0-5][0-9]
Decimal		[1-9][0-9]*
Numeric		[0-9][0-9]*
Phstr		[0-9-]*-[0-9-]*
Ident		[A-Za-z][A-Za-z0-9]*
Pathstr		\/([^\/]+\/)*[^\/]+
Timestr		{Dow}({iHour}{Minute}\-{iHour}{Minute})?
Hour        (([0-1]?[0-9])|(2[0-3]))
Gapstr      (([0-7].)?{Hour}:{Minute}\-{Hour}:{Minute}|(([0-7].)?{Hour}\-{Hour})|([0-7].{Hour}:{Minute}\-[0-7].{Hour}:{Minute})|([0-7].{Hour}\-[0-7].{Hour})|CM|T[a-xA-X][a-xA-X])
Addrstr		(({Decimal}|\*):)?(({Decimal}|\*)\/)?({Numeric}|\*)(.({Numeric}|\*))?(@({Ident}|\*))?
Datestr         {Month}({Day}-{Day})?
%%
{Blank}		;
{Decimal}	{yylval=strtol(yytext,NULL,0);return(NUMBER);}
{Timestr}	{yylval=chktime(yytext);return(TIMESTR);}
{Datestr}       {yylval=chkdate(yytext);return(DATESTR);}
{Gapstr}	{yylval=chkgaps(yytext);return(GAPSTR);}
{Phstr}		{yylval=PHSTR;return(PHSTR);}
{Eq}		{yylval=EQ;return(AROP);}
{Ne}		{yylval=NE;return(AROP);}
{Gt}		{yylval=GT;return(AROP);}
{Ge}		{yylval=GE;return(AROP);}
{Lt}		{yylval=LT;return(AROP);}
{Le}		{yylval=LE;return(AROP);}
{And}		{yylval=AND;return(LOGOP);}
{Or}		{yylval=OR;return(LOGOP);}
{Not}		{yylval=NOT;return(NOT);}
{Xor}		{yylval=XOR;return(LOGOP);}
{Lb}		{yylval=LB;return(LB);}
{Rb}		{yylval=RB;return(RB);}
{Comma}		{yylval=COMMA;return(COMMA);}
{Asterisk}	{yylval=ASTERISK;return(ASTERISK);}
{Connect}	{yylval=CONNECT;return(CONNECT);}
{Speed}		{yylval=SPEED;return(SPEED);}
{File}		{yylval=FLFILE;return(FLFILE);}
{Pathstr}	{yylval=PATHSTR;return(PATHSTR);}
{Phone}		{yylval=PHONE;return(PHONE);}
{Cid}		{yylval=CID;return(CID);}
{Port}		{yylval=PORT;return(PORT);}
{Time}		{yylval=TIME;return(TIME);}
{iTime}		{yylval=ITIME;return(ITIME);}
{Address}	{yylval=ADDRESS;return(ADDRESS);}
{Ident}		{yylval=flagfor(yytext);return(IDENT);} 
{Addrstr}	{yylval=chkaddr(yytext);return(ADDRSTR);}
%%

int flagfor(char *str)
{
	char *p, *q;
	if(!rnode) return 0;
 	if(!strncasecmp(str,"list",4))
		return rnode->options&O_LST;
	if(!strncasecmp(str,"prot",4))
		return rnode->options&O_PWD;
	if(!strncasecmp(str,"in",2))
		return rnode->options&O_INB;
	if(!strncasecmp(str,"out",3))
		return !(rnode->options&O_INB);
	if(!strncasecmp(str,"tcp",3))
		return rnode->options&O_TCP;
	if(!strncasecmp(str,"bad",3))
		return rnode->options&O_BAD;
	if(rnode->flags) {
		q=strdup(rnode->flags);p=strtok(q,",");
		while(p) {
			if(!strcasecmp(str,p)) {
				free(q);
				return 1;
			}
			p=strtok(NULL,",");
		}
		free(q);
	}
	return 0;
}

int chkaddr(char *str)
{
	ftnaddr_t addr;
	falist_t *i;

	if (!parseftnaddr(str, &addr, &DEFADDR, 1))	{
#ifdef Y_DEBUG
		log("unparsable address \"%s\" in expression",str);
#endif	
		return 0;
	}
#ifdef Y_DEBUG
	log("chkaddr: does spec %s match remote address ?",
		ftnaddrtoa(&addr));
#endif	
	if(!rnode) return 0;

	for(i=rnode->addrs;i;i=i->next) 
		if((addr.z<0 || i->addr.z==addr.z) &&
		   (addr.n<0 || i->addr.n==addr.n) &&
		   (addr.f<0 || i->addr.f==addr.f) &&
		   (addr.p<0 || i->addr.p==addr.p))
			return 1;
	return 0;
}


int chkgaps(char *str)
{
	return checktimegaps(str);
}

int chktime(str)
char *str;
{
	int h1,h2,m1,m2,beg,end,cur,dayok,day;

	if (strncasecmp(str,"Sun",3) == 0) day=0;
	else if (strncasecmp(str,"Mon",3) == 0) day=1;
	else if (strncasecmp(str,"Tue",3) == 0) day=2;
	else if (strncasecmp(str,"Wed",3) == 0) day=3;
	else if (strncasecmp(str,"Thu",3) == 0) day=4;
	else if (strncasecmp(str,"Fri",3) == 0) day=5;
	else if (strncasecmp(str,"Sat",3) == 0) day=6;
	else if (strncasecmp(str,"Any",3) == 0) day=-1;
	else if (strncasecmp(str,"Wk",2) == 0) day=-2;
	else if (strncasecmp(str,"We",2) == 0) day=-3;
	else day=-4;

#ifdef Y_DEBUG
	log("chkday: does day %d match spec %d ?",now->tm_wday,day);
#endif
	if (day >= 0) dayok=(now->tm_wday == day);
	else switch (day)
	{
	case -3: dayok=((now->tm_wday == 0) || (now->tm_wday == 6)); break;
	case -2: dayok=((now->tm_wday != 0) && (now->tm_wday != 6)); break;
	case -1: dayok=1; break;
	default: 
#ifdef Y_DEBUG
                log("bad day specified!",day);
#endif	
                dayok=0; break;
	}

	if (dayok == 0) return 0;

	while (*str && ((*str < '0') || (*str > '9'))) str++;
	if (*str == '\0')
	{
		return 1;
	}
	if (sscanf(str,"%02d%02d-%02d%02d",&h1,&m1,&h2,&m2) != 4)
	{
#ifdef Y_DEBUG
		log("invalid time string \"%s\" in expression",str);
#endif	
		return 0;
	}
#ifdef Y_DEBUG	
	log("chktime: is %02d:%02d between %02d:%02d and %02d:%02d ?",
		now->tm_hour,now->tm_min,h1,m1,h2,m2);
#endif
	cur=now->tm_hour*60+now->tm_min;
	beg=h1*60+m1;
	end=h2*60+m2;
	if (end > beg) return ((cur >= beg) && (cur <= end));
	return ((cur >= beg) || (cur <= end));
}

int chkdate(str)
char *str;
{
	int d1,d2,m;
	char *mnths[13]={"Jan","Feb","Mar","Apr","May","Jun",
					 "Jul","Aug","Sep","Oct","Nov","Dec","Any"};

	char *p;
	for(m=0;m<13;m++) if(!strncasecmp(str,mnths[m],3)) break;
	if(m>12 || (m!=12 && m!=now->tm_mon)) return 0;

	str+=3;while(*str==' '||*str=='\t') str++;
	if(!(p=strchr(str,'-'))) {
		if(sscanf(str,"%d", &d1)!=1) return 0;
		d2=d1;
	} else
		if(sscanf(str,"%d-%d", &d1, &d2)!=1) return 0;
	if(d2>d1) return 0;
	return now->tm_yday>=d1 && now->tm_yday<=d2;
}