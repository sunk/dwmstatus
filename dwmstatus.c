#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <sys/sysinfo.h>

const char *tz_current = "Australia/Sydney";

void settz(const char *tzname) {
	setenv( "TZ", tzname, 1 );
}

void setstatus( char *str, Display *dpy ) {
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

const char * readfile( const char *base, const char *file ) {
  static char line[512];
  char path[512];
  FILE *fd;

  snprintf( path, sizeof(path), "%s/%s", base, file );
  fd = fopen( path, "r" );
  if (fd == NULL) return NULL;

  memset( line, 0, sizeof(line) );
  if ( fgets(line, sizeof(line)-1, fd) == NULL )
    return NULL;
  fclose(fd);

  return line;
}

const char * getbattery( const char *base ) {
    static char ret[512];
    const char *co;
    char *stat;
    char status;
    int descap;
    int remcap;
    double remaining;
    double using;
    double voltage;
    double current;

    descap = -1;
    remcap = -1;
    using  = -1;
    remaining = -1;
    stat = "Not Charging";

    co = readfile( base, "status" );
    if ( co == NULL ) { co = "Not Charging"; }
    sscanf( co, "%s", &status );

    co = readfile( base, "charge_full_design" );
    if (co == NULL) {
        co = readfile( base, "energy_full_design" );
        if ( co == NULL ) return "";
    }
    sscanf( co, "%d", &descap );

    co = readfile( base, "charge_now" );
    if (co == NULL) {
        co = readfile( base, "energy_now" );
        if (co == NULL) return "";
    }
    sscanf( co, "%d", &remcap );

    co = readfile( base, "power_now" ); /* µWattage being used */
    if (co == NULL) {
        co = readfile( base, "voltage_now" );
        sscanf( co, "%lf", &voltage );

        co = readfile( base, "current_now" );
        sscanf( co, "%lf", &current );

        remcap  = (voltage / 1000.0) * ((double)remcap / 1000.0);
        descap  = (voltage / 1000.0) * ((double)descap / 1000.0);
        using   = (voltage / 1000.0) * ((double)current / 1000.0);

    } else {
        sscanf( co, "%lf", &using );
    }

    if (remcap < 0 || descap < 0) return "invalid";

    if (status == 'D') {
        remaining = (double)remcap / using;
        stat = "B";

    } else if (status == 'C') {
        remaining = ((double)descap - (double)remcap) /using;
        stat = "C";

    } else {
        remaining = 0;
        stat = "F";
    }

    if( stat[0] == 'F' )
        snprintf( ret, sizeof(ret), "Charged" );
    else {
        if ( remaining < 0 )
            snprintf( ret, sizeof(ret), "%s: Calculating...", stat );
        else
            snprintf( ret, sizeof(ret), "%s: %.0lf%%(%.01fH)", stat,
                (((double)remcap / (double)descap) * 100), remaining );
    }

    return ret;
}

const char * mktimes( const char *fmt, const char *tzname ) {
	static char buf[128];
	time_t tim;
	struct tm *timtm;

	settz( tzname );
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

    return buf;
}

char * getram(void) {
    static char ram[128];
    struct sysinfo s;
    sysinfo(&s);

    snprintf( ram,sizeof(ram),"%.1f/%.1fG",((double)(s.totalram-s.freeram))/1073741824., ((double)s.totalram)/1073741824. );
    return ram;
}

int main(void) {
	char status[512];
	const char *_time = NULL;
    const char *_batt = NULL;
    const char *_ram = NULL;
    Display *dpy;

	if( !(dpy = XOpenDisplay(NULL)) ) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

    for( ;;sleep(5) ) {
        _batt = getbattery( "/sys/class/power_supply/BAT0" );
        _ram  = getram();
        _time = mktimes( "%a %d %b %H:%M", tz_current );

        snprintf( status, 512, "%s %s %s", _batt, _ram, _time );
        setstatus( status, dpy );
	}

	XCloseDisplay(dpy);
	return 0;
}
