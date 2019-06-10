#define _BSD_SOURCE
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
#include <alsa/asoundlib.h>
#include <X11/Xlib.h>
#include <sys/sysinfo.h>

const char *tz_current = "Australia/Sydney";
#define VOL_MUTE    "M %d%%"
#define VOL "V %d%%"

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(const char *tzname)
{
	setenv( "TZ", tzname, 1 );
}

void
setstatus( char *str, Display *dpy )
{
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

char *
readfile(char *base, char *file)
{
  char *path, line[513];
  FILE *fd;

  memset(line, 0, sizeof(line));

  path = smprintf("%s/%s", base, file);
  fd = fopen(path, "r");
  if (fd == NULL)
    return NULL;
  free(path);

  if (fgets(line, sizeof(line)-1, fd) == NULL)
    return NULL;
  fclose(fd);

  return smprintf("%s", line);
}

char *
getbattery(char *base)
{
    char *co;
    char *stat;
    char *ret;
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
    if (co == NULL) {
        co = "Not Charging";
    }
    sscanf(co, "%s", &status);
    free(co);

    co = readfile(base, "charge_full_design");
    if (co == NULL) {
        co = readfile(base, "energy_full_design");
        if (co == NULL)
            return smprintf("");
    }
    sscanf(co, "%d", &descap);
    free(co);

    co = readfile(base, "charge_now");
    if (co == NULL) {
        co = readfile(base, "energy_now");
        if (co == NULL)
            return smprintf("");
    }
    sscanf(co, "%d", &remcap);
    free(co);

    co = readfile(base, "power_now"); /* µWattage being used */
    if (co == NULL) {
        co = readfile(base, "voltage_now");
        sscanf(co, "%lf", &voltage);
        free(co);
        co = readfile(base, "current_now");
        sscanf(co, "%lf", &current);
        free(co);
        remcap  = (voltage / 1000.0) * ((double)remcap / 1000.0);
        descap  = (voltage / 1000.0) * ((double)descap / 1000.0);
        using = (voltage / 1000.0) * ((double)current / 1000.0);
    } else {
        sscanf(co, "%lf", &using);
        free(co);
    }

    if (remcap < 0 || descap < 0)
        return smprintf("invalid");

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
        ret = smprintf( "Charged" );
    else {
        if ( remaining < 0 )
            ret = smprintf( "%s: Calculating...", stat );
        else
            ret = smprintf( "%s: %.0lf%%(%.01fH)", stat,
                (((double)remcap / (double)descap) * 100), remaining );
    }

    if(!stat) free(stat);
    return ret;
}

char *
get_vol(snd_mixer_t *handle, snd_mixer_elem_t *elem)
{
	int mute = 0;
	long vol, max, min;

	snd_mixer_handle_events(handle);
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &vol);
	snd_mixer_selem_get_playback_switch(elem, 0, &mute);

	return smprintf(mute == 0 ? VOL_MUTE : VOL, (vol * 100) / max);
}

static const char * mktimes( const char *fmt, const char *tzname )
{
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

static const char * getram(void) {
    static char ram[128];
    struct sysinfo s;
    sysinfo(&s);

    snprintf( ram,sizeof(ram),"%.1f/%.1fG",((double)(s.totalram-s.freeram))/1073741824., ((double)s.totalram)/1073741824. );
    return ram;
}

int
main(void)
{
	char *status;
	char *_time = NULL;
    char *_batt = NULL;
    char *_ram = NULL;
    Display *dpy;

	if( !(dpy = XOpenDisplay(NULL)) ) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

    for( ;;sleep(5) ) {
        _batt = getbattery( "/sys/class/power_supply/BAT0" );
        _ram  = (char *)getram();
        _time = (char *)mktimes( "%a %d %b %H:%M", tz_current );

        status = smprintf( "%s %s, %s", _batt, _ram, _time );
        setstatus( status, dpy );

        free( status );
        free( _batt );
	}

	XCloseDisplay(dpy);
	return 0;
}
