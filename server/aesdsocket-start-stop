#!/bin/sh

#-- specifies the end of the command and treats that as an input to the process being called
# -n specifies the name of what we are running
# -a tells us where we can expect to find it
case "$1" in
    start) 
        echo "Starting aesdsocket server"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket server"
        start-stop-daemon -K --pidfile /var/run/aesdsocket.pid
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac
