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
#include <fcntl.h>
#include <alsa/asoundlib.h>

// set timezone
const char *tz_current = "Australia/Sydney";

// check /proc/asound
const char* sound_card = "hw:0";

// check /sys/class/power_supply
const char* battery = "/sys/class/power_supply/BAT0";

void settz(const char *tzname) {
  setenv( "TZ", tzname, 1 );
}

void setstatus( const char *str, Display *dpy ) {
    if( dpy == NULL ) return;
    XStoreName( dpy, DefaultRootWindow(dpy), str );
    XSync( dpy, False );
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
    static char ret[128];
    const char *co;
    char status;
    int descap = -1;
    int remcap = -1;

    co = readfile( base, "status" );
    if( co == NULL ) {
        status = '?';
    } else if( strncmp(co, "Discharging", 11) == 0 ) {
        status = '-';
    } else if( strncmp(co, "Charging", 8) == 0 ) {
        status = '+';
    } else if( strncmp(co, "Full", 4 ) == 0 ) {
        return "Charged";
    } else {
        status = '?';
    }

    co = readfile( base, "charge_full" );
    if( co == NULL ) co = readfile( base, "energy_full" );
    if( co == NULL ) {
        status = '?';
    } else {
        sscanf( co, "%d", &descap );
    }

    co = readfile( base, "charge_now" );
    if( co == NULL ) co = readfile( base, "energy_now" );
    if( co == NULL ) {
        status = '?';
    } else {
        sscanf( co, "%d", &remcap );
    }

    if( remcap < 0 || descap < 0 ) {
        status = '?';
    }

    if( status == '?' ) return "battery fault";

    snprintf( ret, sizeof(ret), "%c%.0lf%%",
              status, ((double)remcap / (double)descap * 100) );

    return ret;
}

int getvol( long* outvol ) {
    snd_mixer_t* handle;
    snd_mixer_elem_t* elem;
    snd_mixer_selem_id_t* sid;

    static const char* mix_name = "Master";
    static int mix_index = 0;

    snd_mixer_selem_id_alloca(&sid);

    //sets simple-mixer index and name
    snd_mixer_selem_id_set_index(sid, mix_index);
    snd_mixer_selem_id_set_name(sid, mix_name);

    if ((snd_mixer_open(&handle, 0)) < 0) return -1;

    if ((snd_mixer_attach(handle, sound_card)) < 0) {
        snd_mixer_close(handle);
        return -2;
    }
    if ((snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
        snd_mixer_close(handle);
        return -3;
    }
    if( snd_mixer_load(handle) < 0 ) {
        snd_mixer_close(handle);
        return -4;
    }
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        snd_mixer_close(handle);
        return -5;
    }

    long minv, maxv;
    snd_mixer_selem_get_playback_volume_range( elem, &minv, &maxv );
    if(snd_mixer_selem_get_playback_volume(elem, 0, outvol) < 0) {
        snd_mixer_close(handle);
        return -6;
    }
    snd_mixer_close(handle);

    maxv -= minv;
    *outvol -= minv;
    *outvol *= 100;
    *outvol /= maxv;

    return 0;
}

const char * mktimes( const char *fmt, const char *tzname ) {
    static char buf[128];
    time_t tim;
    struct tm *timtm;

    settz( tzname );
    tim = time(NULL);
    timtm = localtime(&tim);
    if( timtm == NULL ) { perror("localtime"); exit(1); }

    if( !strftime(buf, sizeof(buf)-1, fmt, timtm) ) {
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
    long _vol;

    Display* dpy;
    dpy = XOpenDisplay( NULL );

    for( ;;sleep(5) ) {
        _batt = getbattery( battery );
        _ram  = getram();
        getvol( &_vol );
        _time = mktimes( "%a-%d-%b %H:%M", tz_current );

        snprintf( status, 512, "%s %s ♪%ld %s", _batt, _ram, _vol, _time );
        setstatus( status, dpy );
  }

    XCloseDisplay( dpy );
    return 0;
}
