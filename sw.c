/* simple wallet */

#define _XOPEN_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arg.h"
char *argv0;

typedef struct Movement Movement;
struct Movement {
	int id;
	int ts;
	float amount;
	char note[64];
	Movement *next;
};

/* function declarations */
int addmov(char *date, float amount, char *note);
void attach(Movement *m);
void attach_sorted_desc(Movement **head, Movement *m);
void deletemov(int id);
void detach(Movement *m);
void die(const char *errstr, ...);
void *ecalloc(size_t nmemb, size_t size);
void freemovs(void);
void loadmovs(void);
void savemovs(void);
void showmovs(void);
void sortmovs(void);
int strtots(char *s);
void usage(void);

/* variables */
Movement *movs;
FILE *movsfile;
char movsfilename[256];

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
	float tot = 0;
	int nmovs = 0;
	char time[32];

	printf("%3s | %16s | %8s | %s\n", "id", "date", "amount", "note");
	for(m = movs; m; m = m->next) {
		ts = m->ts;
		strftime(time, sizeof time, "%d/%m/%Y %H:%M", localtime(&ts));
		printf("%3d | %16s | %8.2f | %s\n", m->id, time, m->amount, m->note);
		tot += m->amount;
		++nmovs;
	}
	printf("%3s | %17s: %8.2f |\n", "", "Wallet balance", tot);
	printf("%3s | %17s: %8d |\n", "", "Total movements", nmovs);
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
	struct tm tm = {0};

	if(!strcmp(s, "now"))
		return time(NULL);
	strptime(s, "%d/%m/%Y %H:%M", &tm);
	return mktime(&tm);
}

void
usage(void) {
	die("Usage: %s [-v] [-d <id>] [-f <file>] [<date> <amount> <note>]\n", argv0);
}

int
main(int argc, char *argv[]) {
	int delid = 0;

	ARGBEGIN {
	case 'd': delid = atoi(EARGF(usage())); break;
	case 'f':
		snprintf(movsfilename, sizeof movsfilename, "%s", EARGF(usage()));
		break;
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
		freemovs();
		return 0;
	}
	sortmovs();
	showmovs();
	freemovs();
	return 0;
}
