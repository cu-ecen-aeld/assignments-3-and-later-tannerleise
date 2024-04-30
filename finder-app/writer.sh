#!/bin/bash
writefile=$1
writestr=$2
if [ $# -ne 2 ]
then
 	echo "Must Specify 2 Arguments!!"
	exit 1
fi

#I think we can do this without the if statement and just use:
#echo "$writestr" > "$writefile" for both cases
#But I feel like this makes it more readable for when I come back to it
if [ -f "$writefile" ]
then
	echo "$writestr" > "$writefile"
	exit 0
#Assumes with the file or directory wasn't found
else
	dir_name=$(dirname "$writefile")
	mkdir -p "$dir_name"
	touch "$writefile"
	echo "$writestr" > "$writefile"
	exit 0
fi
