#!/bin/sh

### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: AESD Socket Server
# Description:       Manages the AESD Socket Server daemon
### END INIT INFO

DAEMON="/usr/bin/aesdsocket"
DAEMON_OPTS="-d"
NAME="aesdsocket"
DESC="AESD Socket Server"
PID_FILE="/var/run/$NAME.pid"
SCRIPTNAME=$(basename "$0")

log_daemon_msg() {
    echo "$1: $2"
}

log_end_msg() {
    if [ $1 -eq 0 ]; then
        echo " [ OK ]"
    else
        echo " [ FAIL ]"
    fi
}

start() {
    log_daemon_msg "Starting $DESC" "$NAME"
    if [ -f "$PID_FILE" ]; then
        if kill -0 $(cat "$PID_FILE") > /dev/null 2>&1; then
            echo "$DESC is already running."
            return 0
        else
            echo "Warning: PID file exists but process is not running. Removing stale PID file."
            rm -f "$PID_FILE"
        fi
    fi

    start-stop-daemon --start --quiet --background --exec "$DAEMON" -- $DAEMON_OPTS
    status=$?
    if [ $status -eq 0 ]; then
        log_end_msg 0
    else
        log_end_msg 1
    fi
}

stop() {
    log_daemon_msg "Stopping $DESC" "$NAME"
    if [ -f "$PID_FILE" ]; then
        start-stop-daemon --stop --quiet --pidfile "$PID_FILE"
        status=$?
        if [ $status -eq 0 ]; then
            rm -f "$PID_FILE"
            log_end_msg 0
        else
            log_end_msg 1
        fi
    else
        echo "$DESC is not running (PID file not found)"
        log_end_msg 0
    fi
}

status() {
    if [ -f "$PID_FILE" ] && kill -0 $(cat "$PID_FILE") > /dev/null 2>&1; then
        echo "$DESC is running."
        return 0
    else
        echo "$DESC is not running."
        return 1
    fi
}

restart() {
    stop
    sleep 1
    start
}

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart|force-reload)
        restart
        ;;
    status)
        status
        ;;
    *)
        echo "Usage: $SCRIPTNAME {start|stop|restart|force-reload|status}" >&2
        exit 3
        ;;
esac

exit 0