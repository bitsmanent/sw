/* See LICENSE file for copyright and license details.
 * sw is a simple wallet management tool for the terminal. */

#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "arg.h"
char *argv0;

typedef struct Movement Movement;
struct Movement {
	int id;
	int ts;
	int filtered;
	float amount;
	char note[64];
	Movement *next;
};

typedef struct {
	float amount, pamount;
	float income, pincome;
	float expense, pexpense;
	int count, pcount;
} Totals;

/* function declarations */
int addmov(char *date, float amount, char *note);
void attach(Movement *m);
void attach_sorted_desc(Movement **head, Movement *m);
void deletemov(int id);
void detach(Movement *m);
void die(const char *errstr, ...);
void *ecalloc(size_t nmemb, size_t size);
int filtermovs(int from, int to, char *txt);
void freemovs(void);
void loadmovs(void);
void refresh(void);
void savemovs(void);
void showmovs(void);
void sortmovs(void);
int strtots(char *s);
void usage(void);

/* variables */
Movement *movs;
Totals totals;
FILE *movsfile;
char movsfilename[256];
int limit = INT_MAX;
int filtered = 0;

/* function implementations */
int
addmov(char *date, float amount, char *note) {
	Movement *m;
	int id = 0;

	for(m = movs; m; m = m->next)
		if(m->id > id)
			id = m->id;
	++id;
	m = ecalloc(1, sizeof(Movement));
	m->id = id;
	m->ts = strtots(date);
	m->amount = amount;
	memcpy(m->note, note, sizeof(m->note));
	m->note[ sizeof(m->note) - 1] = '\0';
	attach(m);
	return 0;
}

void
attach(Movement *m) {
	m->next = movs;
	movs = m;
}

void
attach_sorted_desc(Movement **head, Movement *m) {
	Movement *t;

	if(!*head || m->ts > (*head)->ts) {
		m->next = *head;
		*head = m;
		return;
	}
	t = *head;
	while(t->next && m->ts <= t->next->ts)
		t = t->next;
	m->next = t->next;
	t->next = m;
}

void
deletemov(int id) {
	Movement *m;

	for(m = movs; m; m = m->next)
		if(m->id == id)
			break;
	if(m) {
		detach(m);
		free(m);
	}
}

void
detach(Movement *m) {
	Movement **tm;

	for (tm = &movs; *tm && *tm != m; tm = &(*tm)->next);
	*tm = m->next;
}

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size) {
	void *p;

	if(!(p = calloc(nmemb, size)))
		die("Cannot allocate memory.\n");
	return p;
}

int
filtermovs(int from, int to, char *txt) {
	Movement *m;
	int nf = 0;

	if(!(from || to || txt))
		return 0;
	for(m = movs; m; m = m->next) {
		m->filtered = (txt ? !!strcasestr(m->note, txt) : 1)
			&& (from ? m->ts >= from : 1)
			&& (to ? m->ts <= to : 1)
			? 0 : 1;
		if(m->filtered)
			++nf;
	}
	return nf;
}

void
freemovs(void) {
	Movement *m;

	while(movs) {
		m = movs;
		detach(m);
		free(m);
	}
}

void
loadmovs(void) {
	Movement *m;
	int r;

	rewind(movsfile);
	while(1) {
		m = ecalloc(1, sizeof(Movement));
		r = fscanf(movsfile, "%d %d %f %[^\n]", &m->id, &m->ts, &m->amount, &m->note[0]);
		if(r <= 0) {
			if(feof(movsfile))
				break;
			die("%s: file corrupted\n", movsfilename);
		}
		attach(m);
       }
}

void
refresh(void) {
	Movement *m;

	totals.amount = totals.income = totals.expense = totals.count = 0;
	totals.pamount = totals.pincome = totals.pexpense = totals.pcount = 0;
	for(m = movs; m; m = m->next) {
		totals.amount += m->amount;
		if(m->amount >= 0)
			totals.income += m->amount;
		else
			totals.expense += m->amount;
		++totals.count;

		if(m->filtered)
			continue;

		if(totals.pcount >= limit)
			continue;
		totals.pamount += m->amount;
		if(m->amount >= 0)
			totals.pincome += m->amount;
		else
			totals.pexpense += m->amount;
		++totals.pcount;
	}
}

void
savemovs(void) {
	Movement *m;

	movsfile = fopen(movsfilename, "w");
	for(m = movs; m; m = m->next)
		fprintf(movsfile, "%d %d %f %s\n", m->id, m->ts, m->amount, m->note);
	fclose(movsfile);
}

void
showmovs(void) {
	Movement *m;
	time_t ts;
	char time[32];
	int listcount = totals.count - filtered;
	int count = 0;

	if(limit < listcount)
		listcount = limit;

	if(listcount)
		printf("%5s | %16s | %8s | %s\n", "id", "date  time", "amount", "note");
	for(m = movs; m && count < limit; m = m->next) {
		if(m->filtered)
			continue;
		++count;
		ts = m->ts;
		strftime(time, sizeof time, "%d/%m/%Y %H:%M", localtime(&ts));
		printf("%5d | %16s | %8.2f | %s\n", m->id, time, m->amount, m->note);
	}
	if(listcount > 1 && listcount < totals.count)
		printf("%5s | %17s: %8.2f | income=%.2f expense=%.2f movements=%d\n", "",
			"Partial", totals.pamount, totals.pincome, totals.pexpense, totals.pcount);
	printf("%5s | %17s: %8.2f | income=%.2f expense=%.2f movements=%d\n",
		"", "Total", totals.amount, totals.income, totals.expense, totals.count);
}

void
sortmovs(void) {
	Movement *sorted = NULL, *m, *t;

	m = movs;
	while(m) {
		t = m;
		m = m->next;
		attach_sorted_desc(&sorted, t);
	}
	movs = sorted;
}

int
strtots(char *s) {
	struct tm tm = {.tm_isdst = - 1};

	if(!strcmp(s, "now"))
		return time(NULL);
	strptime(s, "%d/%m/%Y %H:%M", &tm);
	return mktime(&tm);
}

void
usage(void) {
	die("Usage: %s [-v] [-defit <arg>] [-l [limit]] [<date [time]> <amount> <note>]\n", argv0);
}

int
main(int argc, char *argv[]) {
	int delid = 0;
	int from = 0, to = 0;
	char *txt = NULL;
	char *num;

	ARGBEGIN {
	case 'd': delid = atoi(EARGF(usage())); break;
	case 'e': txt = EARGF(usage()); break;
	case 'f': from = strtots(EARGF(usage())); break;
	case 'i': snprintf(movsfilename, sizeof movsfilename, "%s", EARGF(usage())); break;
	case 'l':
		//limit = atoi(EARGF(usage()));
		num = ARGF();
		limit = num ? atoi(num) : 25;
		break;
	case 't': to = strtots(EARGF(usage())); break;
	case 'v': die("sw-"VERSION"\n");
	default: usage();
	} ARGEND;

	if(!*movsfilename)
		snprintf(movsfilename, sizeof movsfilename, "%s/%s", getenv("HOME"), ".sw");
	movsfile = fopen(movsfilename, "r");
	if(!movsfile)
		die("%s: cannot open the file\n", movsfilename);
	loadmovs();
	fclose(movsfile);

	if(delid) {
		deletemov(delid);
		savemovs();
		freemovs();
		return 0;
	}
	if(argc) {
		if(argc != 3)
			usage();
		addmov(argv[0], atof(argv[1]), argv[2]);
		savemovs();
	}
	filtered = filtermovs(from, to, txt);
	sortmovs();
	refresh();
	showmovs();
	freemovs();
	return 0;
}
