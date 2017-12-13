#!/bin/sh

export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

echo "Boot."

# Even board de tijd geven om z'n opstart dingetje te doen..
sleep 10

while sleep 1; do
	echo "Start @"
	date
	/opt/ve_player/start-film.sh
	echo "stop @"
	date

	sleep 2
done
